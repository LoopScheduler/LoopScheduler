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

    void SequentialGroup::WaitForNextEvent(double MaxEstimatedExecutionTime, double MaxWaitingTime)
    {
        std::unique_lock<std::mutex> lock(NextEventConditionMutex);

        std::chrono::time_point<std::chrono::steady_clock> start;
        if (MaxWaitingTime != 0)
            start = std::chrono::steady_clock::now();

        const auto predicate = [this, &MaxEstimatedExecutionTime, &MaxWaitingTime, &start] {
            std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
            if (ShouldIncrementCurrentMemberIndex()
                || ShouldRunNextModuleFromCurrentMemberIndex(MaxEstimatedExecutionTime)) // There is a next module to run.
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
                    member->WaitForNextEvent(max_exec_time);
                else
                {
                    auto stop = start + std::chrono::duration<double>(MaxWaitingTime);
                    std::chrono::duration<double> time = stop - std::chrono::steady_clock::now();
                    double t = time.count();
                    if (t > 0)
                        member->WaitForNextEvent(max_exec_time, t);
                }
                return true;
            }
            if ((CurrentMemberIndex == Members.size() - 1)
                && (RunningThreadsCount == 0)); // (And no next module to run) => IsDone=true.
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
            && (CurrentMemberIndex < Members.size() - 1)
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
                    duration.count() + LastModuleHigherPredictedTimeSpan,
                    0.000001
                );
            else
                return std::max(
                    duration.count() + LastModuleLowerPredictedTimeSpan,
                    0.000001
                );
            
        }
        else
        {
            // Double mutex lock can occur if there's a loop.
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
