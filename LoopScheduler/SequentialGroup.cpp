#include "SequentialGroup.h"

#include <algorithm>
#include <utility>

#include "Module.h"

namespace LoopScheduler
{
    SequentialGroup::SequentialGroup(
            std::vector<SequentialGroupMember> Members
        ) : Members(Members), CurrentMemberIndex(-1), CurrentMemberRunsCount(0), RunningThreadsCount(0)
    {
        std::vector<std::shared_ptr<Group>> member_groups;
        std::vector<std::shared_ptr<Module>> member_modules;
        for (auto& member : Members)
            if (std::holds_alternative<std::shared_ptr<Group>>(member))
                member_groups.push_back(std::get<std::shared_ptr<Group>>(member));
            else
                member_modules.push_back(std::get<std::shared_ptr<Module>>(member));

        IntroduceMembers(std::move(member_groups), std::move(member_modules));

        for (auto& member : Members)
            if (std::holds_alternative<std::shared_ptr<Group>>(member))
                GroupMembers.push_back(std::get<std::shared_ptr<Group>>(member));
    }

    class IncrementGuardLockingOnDecrement
    {
    private:
        int& num;
        std::unique_lock<std::shared_mutex>& lock;
    public:
        IncrementGuardLockingOnDecrement(int& num, std::unique_lock<std::shared_mutex>& lock) : num(num), lock(lock)
        {
            num++;
        }
        ~IncrementGuardLockingOnDecrement()
        {
            lock.lock();
            num--;
            lock.unlock();
        }
    };

    bool SequentialGroup::RunNextModule(double MaxEstimatedExecutionTime)
    {
        std::unique_lock<std::shared_mutex> lock(MembersSharedMutex);
        if (ShouldIncrementCurrentMemberIndex())
        {
            CurrentMemberIndex++;
            CurrentMemberRunsCount = 0;
        }
        if (ShouldRunNextModuleFromCurrentMemberIndex(MaxEstimatedExecutionTime))
        {
            auto& member = std::get<std::shared_ptr<Module>>(Members[CurrentMemberIndex]);
            auto token = member->GetRunningToken();
            if (token.CanRun())
            {
                IncrementGuardLockingOnDecrement increment_guard(RunningThreadsCount, lock);
                CurrentMemberRunsCount++;
                LastModuleStartTime = std::chrono::steady_clock::now();
                LastModuleHigherPredictedTimeSpan = member->PredictHigherExecutionTime();
                LastModuleLowerPredictedTimeSpan = member->PredictLowerExecutionTime();
                lock.unlock();
                token.Run();
            }
            else
            {
                return false;
            }
            NextEventConditionVariable.notify_all();
            return true;
        }
        else if (double max_time; ShouldTryRunNextGroupFromCurrentMemberIndex(MaxEstimatedExecutionTime, max_time))
        {
            bool success = false;
            {
                IncrementGuardLockingOnDecrement increment_guard(RunningThreadsCount, lock);
                CurrentMemberRunsCount++;
                auto& member = std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex]);
                lock.unlock();

                success = member->RunNextModule(max_time);
            }
            NextEventConditionVariable.notify_all();
            return success;
        }
        return false;
    }

    bool SequentialGroup::IsAvailable(double MaxEstimatedExecutionTime)
    {
        if (ShouldIncrementCurrentMemberIndex()
            || ShouldRunNextModuleFromCurrentMemberIndex(MaxEstimatedExecutionTime))
        {
            return true;
        }
        if (double max_exec_time; ShouldTryRunNextGroupFromCurrentMemberIndex(MaxEstimatedExecutionTime, max_exec_time))
        {
            auto& member = std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex]);
            return member->IsAvailable(max_exec_time);
        }
        return false;
    }

    void SequentialGroup::WaitForAvailability(double MaxEstimatedExecutionTime, double MaxWaitingTime)
    {
        std::chrono::time_point<std::chrono::steady_clock> start;
        if (MaxWaitingTime != 0)
            start = std::chrono::steady_clock::now();

        std::unique_lock<std::mutex> lock(NextEventConditionMutex);

        const auto predicate = [this, &MaxEstimatedExecutionTime, &MaxWaitingTime, &start] {
            std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
            if (ShouldIncrementCurrentMemberIndex())
            {
                return true;
            }
            if (ShouldRunNextModuleFromCurrentMemberIndex(MaxEstimatedExecutionTime)) // There is a next module to run.
            {
                auto& member = std::get<std::shared_ptr<Module>>(Members[CurrentMemberIndex]);
                lock.unlock();
                if (MaxWaitingTime == 0)
                {
                    member->WaitForRunAvailability();
                }
                else
                {
                    auto stop = start + std::chrono::duration<double>(MaxWaitingTime);
                    std::chrono::duration<double> time = stop - std::chrono::steady_clock::now();
                    double t = time.count();
                    if (t > 0)
                        member->WaitForRunAvailability(t);
                }
                return true;
            }
            if (double max_exec_time; ShouldTryRunNextGroupFromCurrentMemberIndex(MaxEstimatedExecutionTime, max_exec_time)) // Can wait for the group.
            {
                auto& member = std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex]);
                lock.unlock();
                if (MaxWaitingTime == 0)
                    member->WaitForAvailability(max_exec_time);
                else
                {
                    auto stop = start + std::chrono::duration<double>(MaxWaitingTime);
                    std::chrono::duration<double> time = stop - std::chrono::steady_clock::now();
                    double t = time.count();
                    if (t > 0)
                        member->WaitForAvailability(max_exec_time, t);
                }
                return true;
            }
            if ((CurrentMemberIndex == (int)Members.size() - 1)
                && (RunningThreadsCount == 0)) // (And no next module to run) => IsDone=true.
            {
                return true;
            }
            return false;
        };

        if (MaxWaitingTime == 0)
            NextEventConditionVariable.wait(lock, predicate);
        else if (MaxWaitingTime > 0)
            NextEventConditionVariable.wait_for(lock, std::chrono::duration<double>(MaxWaitingTime), predicate);
    }

    bool SequentialGroup::IsDone()
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        return (CurrentMemberIndex == (int)Members.size() - 1)
               && (RunningThreadsCount == 0)
               && (
                    CurrentMemberIndex == -1
                    || (std::holds_alternative<std::shared_ptr<Module>>(Members[CurrentMemberIndex]) ?
                        (CurrentMemberRunsCount != 0)
                        : (std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->IsDone()))
                );
    }

    void SequentialGroup::StartNextIteration()
    {
        std::unique_lock<std::shared_mutex> lock(MembersSharedMutex);
        CurrentMemberIndex = -1;
        for (auto& group_member : GroupMembers)
            group_member->StartNextIteration();
    }

    double SequentialGroup::PredictHigherRemainingExecutionTime()
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        return PredictRemainingExecutionTimeNoLock<true>();
    }

    double SequentialGroup::PredictLowerRemainingExecutionTime()
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        return PredictRemainingExecutionTimeNoLock<false>();
    }

    bool SequentialGroup::UpdateLoop(Loop * LoopPtr)
    {
        for (int i = 0; i < Members.size(); i++)
        {
            if (std::holds_alternative<std::shared_ptr<Module>>(Members[i]))
            {
                auto& m = std::get<std::shared_ptr<Module>>(Members[i]);
                if (!m->SetLoop(LoopPtr))
                {
                    for (int j = 0; j < i; j++)
                        if (std::holds_alternative<std::shared_ptr<Module>>(Members[j]))
                            std::get<std::shared_ptr<Module>>(Members[j])->SetLoop(nullptr);
                    return false;
                }
            }
        }
        return true;
    }

    inline bool SequentialGroup::ShouldRunNextModuleFromCurrentMemberIndex(double MaxEstimatedExecutionTime)
    {
        // NO MUTEX LOCK
        return RunningThreadsCount == 0 && CurrentMemberRunsCount == 0
            && CurrentMemberIndex != -1
            && std::holds_alternative<std::shared_ptr<Module>>(Members[CurrentMemberIndex])
            && (MaxEstimatedExecutionTime == 0
                || std::get<std::shared_ptr<Module>>(Members[CurrentMemberIndex])->PredictHigherExecutionTime()
                    <= MaxEstimatedExecutionTime);
    }
    inline bool SequentialGroup::ShouldTryRunNextGroupFromCurrentMemberIndex(
            double InputMaxEstimatedExecutionTime,
            double& OutputMaxEstimatedExecutionTime)
    {
        // NO MUTEX LOCK
        if (CurrentMemberIndex != -1 && std::holds_alternative<std::shared_ptr<Group>>(Members[CurrentMemberIndex]))
        {
            if (std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->IsDone())
            {
                if (RunningThreadsCount != 0) // Else: either ShouldIncrement... or IsDone
                {
                    if (InputMaxEstimatedExecutionTime == 0)
                        OutputMaxEstimatedExecutionTime = PredictRemainingExecutionTimeNoLock<false>();
                    else
                        OutputMaxEstimatedExecutionTime = std::min(InputMaxEstimatedExecutionTime, PredictRemainingExecutionTimeNoLock<false>());
                    return true;
                }
            }
            else
            {
                OutputMaxEstimatedExecutionTime = InputMaxEstimatedExecutionTime;
                return true;
            }
        }
        return false;
    }
    inline bool SequentialGroup::ShouldIncrementCurrentMemberIndex()
    {
        // NO MUTEX LOCK
        return (RunningThreadsCount == 0)
            && (CurrentMemberIndex < (int)Members.size() - 1)
            && (
                CurrentMemberIndex == -1
                || (std::holds_alternative<std::shared_ptr<Module>>(Members[CurrentMemberIndex]) ?
                    (CurrentMemberRunsCount != 0)
                    : (std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->IsDone()))
            );
    }
    template <bool Higher>
    inline double SequentialGroup::PredictRemainingExecutionTimeNoLock()
    {
        // NO MUTEX LOCK
        if (RunningThreadsCount == 0) // else CurrentMemberIndex shouldn't be -1
            return 0;
        if (std::holds_alternative<std::shared_ptr<Module>>(Members[CurrentMemberIndex]))
        {
            std::chrono::duration<double> duration = std::chrono::steady_clock::now() - LastModuleStartTime;
            if constexpr (Higher)
                return std::max(
                    LastModuleHigherPredictedTimeSpan - duration.count(),
                    0.000001
                );
            else
                return std::max(
                    LastModuleLowerPredictedTimeSpan - duration.count(),
                    0.000001
                );
            
        }
        else
        {
            // Double mutex lock can occur if there's a group loop.
            if constexpr (Higher)
                return std::max(
                    std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->PredictHigherRemainingExecutionTime(),
                    0.000001
                );
            else
                return std::max(
                    std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->PredictLowerRemainingExecutionTime(),
                    0.000001
                );
        }
    }
}
