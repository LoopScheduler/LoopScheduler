#include "ParallelGroup.h"

#include <algorithm>
#include <utility>

#include "Module.h"

namespace LoopScheduler
{
    ParallelGroup::ParallelGroup(std::vector<ParallelGroupMember> Members)
        : Members(Members), RunningThreadsCount(0), NotifyingCounter(0)
    {
        std::vector<std::shared_ptr<Group>> member_groups;
        std::vector<std::shared_ptr<Module>> member_modules;
        for (auto& member : Members)
            if (std::holds_alternative<std::shared_ptr<Group>>(member.Member))
                member_groups.push_back(std::get<std::shared_ptr<Group>>(member.Member));
            else
                member_modules.push_back(std::get<std::shared_ptr<Module>>(member.Member));

        IntroduceMemberModules(std::move(member_modules));
        IntroduceMemberGroups(std::move(member_groups));

        StartNextIterationForThisGroup();
        for (auto& member : Members)
            if (std::holds_alternative<std::shared_ptr<Group>>(member.Member))
                GroupMembers.push_back(std::get<std::shared_ptr<Group>>(member.Member));
    }

    /// Increments the 2 numbers on construction without locking.
    /// Decrements the 2 numbers on destruction with locking.
    /// Increments the counter on destruction too.
    class DoubleIncrementGuardLockingAndCountingOnDecrement
    {
    private:
        int& num1;
        int& num2;
        int& counter;
        std::unique_lock<std::shared_mutex>& lock;
        std::condition_variable& counter_cv;
    public:
        DoubleIncrementGuardLockingAndCountingOnDecrement(
            int& num1, int& num2, int& counter,
            std::unique_lock<std::shared_mutex>& lock,
            std::condition_variable& counter_cv) : num1(num1), num2(num2), counter(counter), lock(lock), counter_cv(counter_cv)
        {
            num1++;
            num2++;
        }
        ~DoubleIncrementGuardLockingAndCountingOnDecrement()
        {
            lock.lock();
            num2--;
            num1--;
            counter++;
            lock.unlock();
            counter_cv.notify_all();
        }
    };

    bool ParallelGroup::RunNextModule(double MaxEstimatedExecutionTime)
    {
        std::unique_lock<std::shared_mutex> lock(MembersSharedMutex);
        for (std::list<int>::iterator i = MainQueue.begin(); i != MainQueue.end(); ++i)
        {
            auto& member = Members[*i];
            if (std::holds_alternative<std::shared_ptr<Module>>(member.Member))
            {
                auto& m = std::get<std::shared_ptr<Module>>(member.Member);
                if (MaxEstimatedExecutionTime != 0 && m->PredictHigherExecutionTime() > MaxEstimatedExecutionTime)
                    continue;
                if (RunModule(m, lock, MainQueue, SecondaryQueue, i, member.RunSharesAfterFirstRun))
                {
                    // Done in RunModule before running:
                    //MainQueue.erase(i);
                    //if (member.CanRunMoreThanOncePerCycle)
                        //SecondaryQueue.push_back(*i);
                    return true;
                }
            }
            else
            {
                auto& g = std::get<std::shared_ptr<Group>>(member.Member);
                if (g->IsDone())
                {
                    MainQueue.erase(i);
                    for (int j = 0; j < member.RunSharesAfterFirstRun; j++)
                        SecondaryQueue.push_back(*i);
                }
                else if (RunGroup(g, lock))
                {
                    return true;
                }
            }
        }
        for (std::list<int>::iterator i = SecondaryQueue.begin(); i != SecondaryQueue.end(); ++i)
        {
            auto& member = Members[*i];
            if (std::holds_alternative<std::shared_ptr<Module>>(member.Member))
            {
                auto& m = std::get<std::shared_ptr<Module>>(member.Member);
                if (MaxEstimatedExecutionTime != 0 && m->PredictHigherExecutionTime() > MaxEstimatedExecutionTime)
                    continue;
                if (RunModule(m, lock, SecondaryQueue, SecondaryQueue, i, 1))
                {
                    // Done in RunModule before running:
                    //SecondaryQueue.erase(i);
                    //SecondaryQueue.push_back(*i);
                    return true;
                }
            }
            else
            {
                auto& g = std::get<std::shared_ptr<Group>>(member.Member);
                SecondaryQueue.erase(i);
                SecondaryQueue.push_back(*i);
                if (RunGroup(g, lock))
                {
                    return true;
                }
            }
        }
        return false;
    }
    inline bool ParallelGroup::RunModule(std::shared_ptr<Module>& m,
                                         std::unique_lock<std::shared_mutex>& lock,
                                         std::list<int>& from_list,
                                         std::list<int>& to_list,
                                         std::list<int>::iterator& item_to_move,
                                         int move_to_to_list_count)
    {
        auto token = m->GetRunningToken();
        if (token.CanRun())
        {
            from_list.erase(item_to_move);
            for (int j = 0; j < move_to_to_list_count; j++)
                to_list.push_back(*item_to_move);
            auto& runinfo = ModulesRunCountsAndPredictedStopTimes[m];
            {
                DoubleIncrementGuardLockingAndCountingOnDecrement increment_guard(
                    runinfo.RunCount.value, RunningThreadsCount, NotifyingCounter, lock, NextEventConditionVariable
                );
                runinfo.StartTime = std::chrono::steady_clock::now();
                runinfo.HigherPredictedTimeSpan = m->PredictHigherExecutionTime();
                runinfo.HigherPredictedTimeSpan = m->PredictLowerExecutionTime();
                lock.unlock();
                token.Run();
            }
            lock.lock();
            if (runinfo.RunCount.value == 0)
                ModulesRunCountsAndPredictedStopTimes.erase(m);
            return true;
        }
        return false;
    }
    inline bool ParallelGroup::RunGroup(std::shared_ptr<Group>& g, std::unique_lock<std::shared_mutex>& lock)
    {
        auto& runcounts = GroupsRunCounts[g];
        bool success;
        {
            DoubleIncrementGuardLockingAndCountingOnDecrement increment_guard(
                runcounts.value, RunningThreadsCount, NotifyingCounter, lock, NextEventConditionVariable
            );
            lock.unlock();
            success = g->RunNextModule();
        }
        lock.lock();
        if (runcounts.value == 0)
            GroupsRunCounts.erase(g);
        return success;
    }

    bool ParallelGroup::IsAvailable(double MaxEstimatedExecutionTime)
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        return IsAvailableNoLock(MaxEstimatedExecutionTime);
    }
    inline bool ParallelGroup::IsAvailableNoLock(double MaxEstimatedExecutionTime)
    {
        for (auto i : MainQueue)
        {
            if (std::holds_alternative<std::shared_ptr<Module>>(Members[i].Member))
            {
                auto& m = std::get<std::shared_ptr<Module>>(Members[i].Member);
                if (MaxEstimatedExecutionTime != 0 && m->PredictHigherExecutionTime() > MaxEstimatedExecutionTime)
                    continue;
                if (m->IsAvailable())
                    return true;
            }
            else
            {
                if (std::get<std::shared_ptr<Group>>(Members[i].Member)->IsAvailable(MaxEstimatedExecutionTime))
                    return true;
            }
        }
        for (auto i : SecondaryQueue)
        {
            if (std::holds_alternative<std::shared_ptr<Module>>(Members[i].Member))
            {
                auto& m = std::get<std::shared_ptr<Module>>(Members[i].Member);
                if (MaxEstimatedExecutionTime != 0 && m->PredictHigherExecutionTime() > MaxEstimatedExecutionTime)
                    continue;
                if (m->IsAvailable())
                    return true;
            }
            else
            {
                if (std::get<std::shared_ptr<Group>>(Members[i].Member)->IsAvailable(MaxEstimatedExecutionTime))
                    return true;
            }
        }
        return false;
    }

    void ParallelGroup::WaitForAvailability(double MaxEstimatedExecutionTime, double MaxWaitingTime)
    {
        std::chrono::time_point<std::chrono::steady_clock> start;
        if (MaxWaitingTime != 0)
            start = std::chrono::steady_clock::now();

        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        int start_notifying_counter = NotifyingCounter;

        if (RunningThreadsCount == 0)
            return;

        if (IsAvailableNoLock(MaxEstimatedExecutionTime))
            return;

        lock.unlock();

        const auto predicate = [this, start_notifying_counter] {
            std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
            return start_notifying_counter != NotifyingCounter;
        };

        std::unique_lock<std::mutex> cv_lock(NextEventConditionMutex);
        if (MaxWaitingTime == 0)
        {
            NextEventConditionVariable.wait(cv_lock, predicate);
        }
        else if (MaxWaitingTime > 0)
        {
            auto stop = start + std::chrono::duration<double>(MaxWaitingTime);
            std::chrono::duration<double> time = stop - std::chrono::steady_clock::now();
            NextEventConditionVariable.wait_for(cv_lock, time, predicate);
        }
    }

    bool ParallelGroup::IsDone()
    {
        return MainQueue.size() == 0;
    }

    void ParallelGroup::StartNextIteration()
    {
        std::unique_lock<std::shared_mutex> lock(MembersSharedMutex);
        StartNextIterationForThisGroup();
        for (auto& group_member : GroupMembers)
            group_member->StartNextIteration();
    }
    inline void ParallelGroup::StartNextIterationForThisGroup()
    {
        // NO MUTEX LOCK
        MainQueue.clear();
        SecondaryQueue.clear();
        for (int i = 0; i < Members.size(); i++)
            MainQueue.push_back(i);
    }

    double ParallelGroup::PredictHigherRemainingExecutionTime()
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        if (RunningThreadsCount == 0)
            return 0;
        double result = 0.000001;
        auto now = std::chrono::steady_clock::now();
        for (auto& item : ModulesRunCountsAndPredictedStopTimes)
        {
            std::chrono::duration<double> passed_time = now - item.second.StartTime;
            result = std::max(result, item.second.HigherPredictedTimeSpan - passed_time.count());
        }
        for (auto& item : GroupsRunCounts)
        {
            result = std::max(result, item.first->PredictHigherRemainingExecutionTime());
        }
        return result;
    }

    double ParallelGroup::PredictLowerRemainingExecutionTime()
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        if (RunningThreadsCount == 0)
            return 0;
        double result = 0.000001;
        auto now = std::chrono::steady_clock::now();
        for (auto& item : ModulesRunCountsAndPredictedStopTimes)
        {
            std::chrono::duration<double> passed_time = now - item.second.StartTime;
            result = std::max(result, item.second.LowerPredictedTimeSpan - passed_time.count());
        }
        for (auto& item : GroupsRunCounts)
        {
            result = std::max(result, item.first->PredictLowerRemainingExecutionTime());
        }
        return result;
    }

    bool ParallelGroup::UpdateLoop(Loop * LoopPtr)
    {
        for (int i = 0; i < Members.size(); i++)
        {
            if (std::holds_alternative<std::shared_ptr<Module>>(Members[i].Member))
            {
                auto& m = std::get<std::shared_ptr<Module>>(Members[i].Member);
                if (!m->SetLoop(LoopPtr))
                {
                    for (int j = 0; j < i; j++)
                        if (std::holds_alternative<std::shared_ptr<Module>>(Members[j].Member))
                            std::get<std::shared_ptr<Module>>(Members[j].Member)->SetLoop(nullptr);
                    return false;
                }
            }
        }
        return true;
    }

    ParallelGroup::integer::integer() : value(0) {}
    ParallelGroup::integer::integer(int value) : value(value) {}
    ParallelGroup::integer::operator int() { return value; }
}
