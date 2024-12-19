// Copyright (c) 2021 Majidzadeh (hashpragmaonce@gmail.com)
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "SequentialGroup.h"

#include <algorithm>
#include <utility>

#include "Module.h"
#include "BiasedEMATimeSpanPredictor.h"
#include "SmartCVWaiter.h"

namespace LoopScheduler
{
    SequentialGroup::SequentialGroup(
            std::vector<SequentialGroupMember> Members,
            std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor,
            std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor,
            std::shared_ptr<SmartCVWaiter> CVWaiter
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

        if (HigherExecutionTimePredictor == nullptr)
            HigherExecutionTimePredictor = std::unique_ptr<BiasedEMATimeSpanPredictor>(
                new BiasedEMATimeSpanPredictor(
                    0,
                    BiasedEMATimeSpanPredictor::DEFAULT_FAST_ALPHA,
                    BiasedEMATimeSpanPredictor::DEFAULT_SLOW_ALPHA
                )
            );
        if (LowerExecutionTimePredictor == nullptr)
            LowerExecutionTimePredictor = std::unique_ptr<BiasedEMATimeSpanPredictor>(
                new BiasedEMATimeSpanPredictor(
                    0,
                    BiasedEMATimeSpanPredictor::DEFAULT_SLOW_ALPHA,
                    BiasedEMATimeSpanPredictor::DEFAULT_FAST_ALPHA
                )
            );
        if (CVWaiter == nullptr)
            CVWaiter = std::shared_ptr<SmartCVWaiter>(new SmartCVWaiter());

        this->HigherExecutionTimePredictor = std::move(HigherExecutionTimePredictor);
        this->LowerExecutionTimePredictor = std::move(LowerExecutionTimePredictor);
        this->CVWaiter = CVWaiter;
    }

    class IncrementGuard
    {
    private:
        int& num;
    public:
        IncrementGuard(int& num) : num(num)
        {
            num++;
        }
        ~IncrementGuard()
        {
            num--;
        }
    };

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

    bool SequentialGroup::RunNext(double MaxEstimatedExecutionTime)
    {
        std::unique_lock<std::shared_mutex> lock(MembersSharedMutex);
        std::unique_lock<std::mutex> cv_lock(NextEventConditionMutex, std::defer_lock);
        if (ShouldIncrementCurrentMemberIndex())
        {
            TimespanMeasurementStart();
            CurrentMemberIndex++;
            CurrentMemberRunsCount = 0;
        }
        if (ShouldRunNextModuleFromCurrentMemberIndex(MaxEstimatedExecutionTime))
        {
            auto& member = std::get<std::shared_ptr<Module>>(Members[CurrentMemberIndex]);
            auto token = member->GetRunningToken();
            if (token.CanRun())
            {
                IncrementGuard increment_guard(RunningThreadsCount);
                CurrentMemberRunsCount++;
                LastModuleStartTime = std::chrono::steady_clock::now();
                LastModuleHigherPredictedTimeSpan = member->PredictHigherExecutionTime();
                LastModuleLowerPredictedTimeSpan = member->PredictLowerExecutionTime();
                lock.unlock();
                token.Run();
                cv_lock.lock(); // Lock before MembersSharedMutex lock for modifications before notify_all()
                lock.lock(); // Lock for both increment_guard and TimespanMeasurementStop()
            }
            else
            {
                return false;
            }
            TimespanMeasurementStop();
            lock.unlock(); // Unlock after both increment_guard and TimespanMeasurementStop()
            cv_lock.unlock(); // Unlock after MembersSharedMutex unlock after modifications before notify_all()
            NextEventConditionVariable.notify_all();
            return true;
        }
        else if (double max_time; ShouldTryRunNextGroupFromCurrentMemberIndex(MaxEstimatedExecutionTime, max_time))
        {
            bool success = false;
            {
                IncrementGuard increment_guard(RunningThreadsCount);
                CurrentMemberRunsCount++;
                auto& member = std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex]);
                lock.unlock();

                success = member->RunNext(max_time);

                cv_lock.lock(); // Lock before MembersSharedMutex lock for modifications before notify_all()
                lock.lock(); // Lock for both increment_guard and TimespanMeasurementStop()
            }
            TimespanMeasurementStop();
            lock.unlock(); // Unlock after both increment_guard and TimespanMeasurementStop()
            cv_lock.unlock(); // Unlock after MembersSharedMutex unlock after modifications before notify_all()
            NextEventConditionVariable.notify_all();
            return success;
        }
        return false;
    }

    inline void SequentialGroup::TimespanMeasurementStart()
    {
        if (CurrentMemberIndex == -1)
        {
            IterationStartTime = std::chrono::steady_clock::now();
        }
    }
    inline void SequentialGroup::TimespanMeasurementStop()
    {
        if ((CurrentMemberIndex == (int)Members.size() - 1)
            && (RunningThreadsCount == 0) // Called after increment_guard is destructed => already decremented
            && (
                CurrentMemberIndex == -1
                || (std::holds_alternative<std::shared_ptr<Module>>(Members[CurrentMemberIndex]) ?
                    (CurrentMemberRunsCount != 0)
                    : (std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->IsDone()))
            )) // IsDone
        {
            std::chrono::duration<double> duration = std::chrono::steady_clock::now() - IterationStartTime;
            double time = duration.count();
            HigherExecutionTimePredictor->ReportObservation(time);
            LowerExecutionTimePredictor->ReportObservation(time);
        }
    }

    bool SequentialGroup::IsRunAvailable(double MaxEstimatedExecutionTime)
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        return IsRunAvailableNoLock(MaxEstimatedExecutionTime);
    }
    bool SequentialGroup::IsAvailable(double MaxEstimatedExecutionTime)
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        if (IsRunAvailableNoLock(MaxEstimatedExecutionTime))
        {
            return true;
        }
        if ((CurrentMemberIndex == (int)Members.size() - 1)
            && (RunningThreadsCount == 0)) // (And no next module to run) => IsDone=true.
        {
            return true;
        }
        return false;
    }
    inline bool SequentialGroup::IsRunAvailableNoLock(double MaxEstimatedExecutionTime)
    {
        if (ShouldIncrementCurrentMemberIndex()
            || ShouldRunNextModuleFromCurrentMemberIndex(MaxEstimatedExecutionTime))
        {
            return true;
        }
        if (double max_exec_time; ShouldTryRunNextGroupFromCurrentMemberIndex(MaxEstimatedExecutionTime, max_exec_time))
        {
            auto& member = std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex]);
            return member->IsRunAvailable(max_exec_time);
        }
        return false;
    }

    void SequentialGroup::WaitForRunAvailability(double MaxEstimatedExecutionTime, double MaxWaitingTime)
    {
        // Same because there's nothing left to do when IsDone=true.
        WaitForAvailabilityCommon(MaxEstimatedExecutionTime, MaxWaitingTime);
    }
    void SequentialGroup::WaitForAvailability(double MaxEstimatedExecutionTime, double MaxWaitingTime)
    {
        WaitForAvailabilityCommon(MaxEstimatedExecutionTime, MaxWaitingTime);
    }
    inline void SequentialGroup::WaitForAvailabilityCommon(double MaxEstimatedExecutionTime, double MaxWaitingTime)
    {
        std::chrono::time_point<std::chrono::steady_clock> start;
        if (MaxWaitingTime != 0)
            start = std::chrono::steady_clock::now();

        bool wait_for_next_module = false;
        bool wait_for_next_group = false;
        double max_exec_time;

        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        if (ShouldIncrementCurrentMemberIndex())
            return;
        if (ShouldRunNextModuleFromCurrentMemberIndex(MaxEstimatedExecutionTime)) // There is a next module to run.
            wait_for_next_module = true; // Wait outside condition_variable::wait later
        if (ShouldTryRunNextGroupFromCurrentMemberIndex(MaxEstimatedExecutionTime, max_exec_time)) // Can wait for the group.
            wait_for_next_group = true; // Wait outside condition_variable::wait later
        if ((CurrentMemberIndex == (int)Members.size() - 1)
                && (RunningThreadsCount == 0)) // (And no next module to run) => IsDone=true.
        {
            return;
        }

        const auto predicate = [this, &lock, &MaxEstimatedExecutionTime, &wait_for_next_module, &wait_for_next_group, &max_exec_time] {
            lock.lock(); // NextEventConditionMutex already locked before this MembersSharedMutex lock
            if (ShouldIncrementCurrentMemberIndex())
            {
                return true;
            }
            if (ShouldRunNextModuleFromCurrentMemberIndex(MaxEstimatedExecutionTime)) // There is a next module to run.
            {
                wait_for_next_module = true; // Wait outside condition_variable::wait
                return true;
            }
            if (ShouldTryRunNextGroupFromCurrentMemberIndex(MaxEstimatedExecutionTime, max_exec_time)) // Can wait for the group.
            {
                wait_for_next_group = true; // Wait outside condition_variable::wait
                return true;
            }
            if ((CurrentMemberIndex == (int)Members.size() - 1)
                && (RunningThreadsCount == 0)) // (And no next module to run) => IsDone=true.
            {
                return true;
            }
            lock.unlock();
            return false;
        }; // Locked when predicated returns true, should be unlocked at entry

        if (!wait_for_next_module && !wait_for_next_group)
        {
            lock.unlock(); // Locked after wait/wait_for
            std::unique_lock<std::mutex> cv_lock(NextEventConditionMutex);
            if (MaxWaitingTime == 0)
            {
                NextEventConditionVariable.wait(cv_lock, predicate);
            }
            else if (MaxWaitingTime > 0)
            {
                auto stop = start + std::chrono::duration<double>(MaxWaitingTime);
                std::chrono::duration<double> time = stop - std::chrono::steady_clock::now();
#if LOOPSCHEDULER_USE_SMART_CV_WAITER
                CVWaiter->WaitFor(NextEventConditionVariable, cv_lock, time, predicate);
#else
                NextEventConditionVariable.wait_for(cv_lock, time, predicate);
#endif
            }
        }

        if (wait_for_next_module)
        {
            auto& member = std::get<std::shared_ptr<Module>>(Members[CurrentMemberIndex]);
            if (member->IsAvailable())
                return;
            lock.unlock();
            if (MaxWaitingTime == 0)
            {
                member->WaitForAvailability();
            }
            else
            {
                auto stop = start + std::chrono::duration<double>(MaxWaitingTime);
                std::chrono::duration<double> time = stop - std::chrono::steady_clock::now();
                double t = time.count();
                if (t > 0)
                    member->WaitForAvailability(t);
            }
            return;
        }
        if (wait_for_next_group)
        {
            auto& member = std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex]);
            if (member->IsAvailable(max_exec_time))
                return;
            lock.unlock();
            if (MaxWaitingTime == 0)
                member->WaitForRunAvailability(max_exec_time);
            else
            {
                auto stop = start + std::chrono::duration<double>(MaxWaitingTime);
                std::chrono::duration<double> time = stop - std::chrono::steady_clock::now();
                double t = time.count();
                if (t > 0)
                    member->WaitForRunAvailability(max_exec_time, t);
            }
            return;
        }
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

    double SequentialGroup::PredictHigherExecutionTime()
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        return HigherExecutionTimePredictor->Predict();
    }
    double SequentialGroup::PredictLowerExecutionTime()
    {
        std::shared_lock<std::shared_mutex> lock(MembersSharedMutex);
        return LowerExecutionTimePredictor->Predict();
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
                    // Return false when there's no time left
                    // to prevent RunningThreadsCount to increase pointlessly and block the loop.
                    return OutputMaxEstimatedExecutionTime > MNIMAL_TIME;
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
                    MNIMAL_TIME
                );
            else
                return std::max(
                    LastModuleLowerPredictedTimeSpan - duration.count(),
                    MNIMAL_TIME
                );
            
        }
        else
        {
            // Double mutex lock can occur if there's a group loop.
            if constexpr (Higher)
                return std::max(
                    std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->PredictHigherRemainingExecutionTime(),
                    MNIMAL_TIME
                );
            else
                return std::max(
                    std::get<std::shared_ptr<Group>>(Members[CurrentMemberIndex])->PredictLowerRemainingExecutionTime(),
                    MNIMAL_TIME
                );
        }
    }
}
