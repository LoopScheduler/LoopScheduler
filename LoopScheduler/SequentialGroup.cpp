#include "SequentialGroup.h"

#include <algorithm>
#include <thread>

#include "Module.h"

namespace LoopScheduler
{
    SequentialGroup::SequentialGroup(
            std::vector<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>> Members
        ) : Members(Members), CurrentMemberIndex(-1), CurrentMemberRunsCount(0), RunningThreadsCount(0)
    {
    }

    class IncrementGuardLockingOnDecrement
    {
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
    private:
        int& num;
        std::unique_lock<std::shared_mutex>& lock;
    };

    bool SequentialGroup::RunNextModule(double MaxEstimatedExecutionTime)
    {
        std::unique_lock<std::shared_mutex> lock(MembersSharedMutex);
        if (ShouldIncrementCurrentMemberIndex())
        {
            CurrentMemberIndex++;
            CurrentMemberRunsCount = 0;
        }
        double max_time;
        if (ShouldRunNextModuleFromCurrentMemberIndex(MaxEstimatedExecutionTime))
        {
            {
                IncrementGuardLockingOnDecrement increment_guard(RunningThreadsCount, lock);
                CurrentMemberRunsCount++;
                auto& member = std::get<std::shared_ptr<Module>>(Members[CurrentMemberIndex]);
                LastModuleStartTime = std::chrono::steady_clock::now();
                LastModulePredictedTimeSpan = member->PredictLowerExecutionTime();
                lock.unlock();
                member->Run();
            }
            NextEventConditionVariable.notify_all();
            return true;
        }
        else if (ShouldTryRunNextGroupFromCurrentMemberIndex(MaxEstimatedExecutionTime, max_time))
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

    double SequentialGroup::PredictRemainingExecutionTime()
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        return PredictRemainingExecutionTimeNoLock();
    }

    void SequentialGroup::WaitForNextEvent(double MaxEstimatedExecutionTime)
    {
        std::unique_lock<std::mutex> lock(NextEventConditionMutex);
        NextEventConditionVariable.wait(lock, [this, &MaxEstimatedExecutionTime] {
            std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
            if (ShouldIncrementCurrentMemberIndex()
                || ShouldRunNextModuleFromCurrentMemberIndex(MaxEstimatedExecutionTime)) // There is a next module to run.
            {
                return true;
            }
            double max_time;
            if (ShouldTryRunNextGroupFromCurrentMemberIndex(MaxEstimatedExecutionTime, max_time)) // Can wait for the group.
            {
                auto& member = std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex]);
                lock.unlock();
                member->WaitForNextEvent(max_time);
                return true;
            }
            if ((CurrentMemberIndex == Members.size() - 1)
                && (RunningThreadsCount == 0)); // (And no next module to run) => IsDone=true.
            {
                return true;
            }
            return false;
        });
    }

    bool SequentialGroup::IsDone()
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        return (CurrentMemberIndex == Members.size() - 1)
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
                        OutputMaxEstimatedExecutionTime = PredictRemainingExecutionTimeNoLock();
                    else
                        OutputMaxEstimatedExecutionTime = std::min(InputMaxEstimatedExecutionTime, PredictRemainingExecutionTimeNoLock());
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
            && (CurrentMemberIndex < Members.size() - 1)
            && (
                CurrentMemberIndex == -1
                || (std::holds_alternative<std::shared_ptr<Module>>(Members[CurrentMemberIndex]) ?
                    (CurrentMemberRunsCount != 0)
                    : (std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->IsDone()))
            );
    }
    inline double SequentialGroup::PredictRemainingExecutionTimeNoLock()
    {
        // NO MUTEX LOCK
        if (RunningThreadsCount == 0) // else CurrentMemberIndex shouldn't be -1
            return 0;
        if (std::holds_alternative<std::shared_ptr<Module>>(Members[CurrentMemberIndex]))
        {
            std::chrono::duration<double> duration = std::chrono::steady_clock::now() - LastModuleStartTime;
            return std::max(
                duration.count() + LastModulePredictedTimeSpan,
                0.000001
            );
        }
        else
        {
            // Double mutex lock can occur if there's a loop.
            return std::max(
                std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->PredictRemainingExecutionTime(),
                0.000001
            );
        }
    }
}
