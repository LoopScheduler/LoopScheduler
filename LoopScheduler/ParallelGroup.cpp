#include "ParallelGroup.h"

#include <algorithm>

#include "Module.h"

namespace LoopScheduler
{
    ParallelGroup::ParallelGroup(std::vector<ParallelGroupMember> Members)
        : Members(Members)
    {
        StartNextIteration();
    }

    class IncrementGuardLockingOnDecrement
    {
    public:
        IncrementGuardLockingOnDecrement(
            int& num,
            std::unique_lock<std::shared_mutex>& lock) : num(num), lock(lock)
        {
            num++;
        }
        ~IncrementGuardLockingOnDecrement()
        {
            lock.lock();
            num--;
            lock.unlock();
        }
    private:
        int& num;
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
                IncrementGuardLockingOnDecrement increment_guard(runinfo.RunCount.value, lock);
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
            IncrementGuardLockingOnDecrement increment_guard(runcounts.value, lock);
            lock.unlock();
            success = g->RunNextModule();
        }
        lock.lock();
        if (runcounts.value == 0)
            GroupsRunCounts.erase(g);
        return success;
    }

    void ParallelGroup::WaitForNextEvent(double MaxEstimatedExecutionTime, double MaxWaitingTime)
    {
        // TODO
    }

    bool ParallelGroup::IsDone()
    {
        return MainQueue.size() == 0;
    }

    void ParallelGroup::StartNextIteration()
    {
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
