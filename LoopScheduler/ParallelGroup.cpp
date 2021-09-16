#include "ParallelGroup.h"

#include <algorithm>

#include "Module.h"

namespace LoopScheduler
{
    ParallelGroup::ParallelGroup(std::vector<ParallelGroupMember> Members)
        : Members(Members), RunningThreadsCount(0), NotifyingCounter(0)
    {
        std::unique_lock<std::shared_mutex> lock(MembersSharedMutex);
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
    public:
        DoubleIncrementGuardLockingAndCountingOnDecrement(
            int& num1, int& num2, int& counter,
            std::unique_lock<std::shared_mutex>& lock) : num1(num1), num2(num2), counter(counter), lock(lock)
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
        }
    private:
        int& num1;
        int& num2;
        int& counter;
        std::unique_lock<std::shared_mutex>& lock;
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
                if (RunModule(m, lock))
                {
                    MainQueue.erase(i);
                    if (member.CanRunMoreThanOncePreCycle)
                        SecondaryQueue.push_back(*i);
                    return true;
                }
            }
            else
            {
                auto& g = std::get<std::shared_ptr<Group>>(member.Member);
                if (RunGroup(g, lock))
                {
                    if (g->IsDone())
                    {
                        MainQueue.erase(i);
                        if (member.CanRunMoreThanOncePreCycle)
                            SecondaryQueue.push_back(*i);
                    }
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
                if (RunModule(m, lock))
                {
                    SecondaryQueue.erase(i);
                    SecondaryQueue.push_back(*i);
                    return true;
                }
            }
            else
            {
                auto& g = std::get<std::shared_ptr<Group>>(member.Member);
                if (RunGroup(g, lock))
                {
                    SecondaryQueue.erase(i);
                    SecondaryQueue.push_back(*i);
                    return true;
                }
            }
        }
        return false;
    }
    inline bool ParallelGroup::RunModule(std::shared_ptr<Module>& m, std::unique_lock<std::shared_mutex>& lock)
    {
        auto token = m->GetRunningToken();
        if (token.CanRun())
        {
            auto& runinfo = ModulesRunCountsAndPredictedStopTimes[m];
            {
                DoubleIncrementGuardLockingAndCountingOnDecrement increment_guard(
                    runinfo.RunCount.value, RunningThreadsCount, NotifyingCounter, lock
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
        lock.lock();
        return false;
    }
    inline bool ParallelGroup::RunGroup(std::shared_ptr<Group>& g, std::unique_lock<std::shared_mutex>& lock)
    {
        auto& runcounts = GroupsRunCounts[g];
        bool success;
        {
            DoubleIncrementGuardLockingAndCountingOnDecrement increment_guard(
                runcounts.value, RunningThreadsCount, NotifyingCounter, lock
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
        for (auto& i : MainQueue)
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
        for (auto& i : SecondaryQueue)
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

        auto predicate = [this, start_notifying_counter] {
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
        // TODO
    }

    double ParallelGroup::PredictLowerRemainingExecutionTime()
    {
        // TODO
    }

    ParallelGroup::integer::integer() : value(0) {}
    ParallelGroup::integer::integer(int value) : value(value) {}
    ParallelGroup::integer::operator int() { return value; }
}
