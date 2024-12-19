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

#include "Module.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include "BiasedEMATimeSpanPredictor.h"
#include "Loop.h"
#include "Group.h"
#include "SmartCVWaiter.h"

namespace LoopScheduler
{
    Module::Module(
            bool CanRunInParallel,
            std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor,
            std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor,
            bool UseCustomCanRun,
            std::shared_ptr<SmartCVWaiter> CVWaiter
        ) : CanRunPolicy(CanRunInParallel ? (
                    (UseCustomCanRun ? CanRunPolicyType::CanRunInParallelCustom : CanRunPolicyType::CanRunInParallel)
                ) : (
                    (UseCustomCanRun ? CanRunPolicyType::CannotRunInParallelCustom : CanRunPolicyType::CannotRunInParallel)
            )),
            Parent(nullptr), LoopPtr(nullptr), _IsAvailable(true)
    {
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

    class SetToTrueGuard
    {
    private:
        bool& b;
        std::shared_mutex& m;
        std::condition_variable& c;
    public:
        SetToTrueGuard(bool& b, std::shared_mutex& m, std::condition_variable& c) : b(b), m(m), c(c) {}
        ~SetToTrueGuard()
        {
            std::unique_lock<std::shared_mutex> lock(m);
            b = true;
            lock.unlock();
            c.notify_all();
        }
    };

    Module::RunningToken::RunningToken(Module * Creator) : Creator(Creator)
    {
        switch (Creator->CanRunPolicy)
        {
            case CanRunPolicyType::CannotRunInParallel:
            {
                std::unique_lock<std::shared_mutex> lock(Creator->SharedMutex);
                if (Creator->_IsAvailable)
                {
                    Creator->_IsAvailable = false;
                    _CanRun = true;
                }
                else
                {
                    _CanRun = false;
                }
            }
            break;
            case CanRunPolicyType::CanRunInParallel:
            {
                _CanRun = true;
            }
            break;
            case CanRunPolicyType::CannotRunInParallelCustom:
            {
                
                std::unique_lock<std::shared_mutex> lock(Creator->SharedMutex);
                if (Creator->_IsAvailable)
                {
                    _CanRun = Creator->CanRun();
                    if (_CanRun)
                        Creator->_IsAvailable = false;
                }
                else
                {
                    _CanRun = false;
                }
            }
            break;
            case CanRunPolicyType::CanRunInParallelCustom:
            {
                _CanRun = Creator->CanRun();
            }
            break;
        }
    }
    Module::RunningToken::RunningToken(RunningToken&& op)
    {
        Creator = std::exchange(op.Creator, nullptr);
        _CanRun = std::exchange(op._CanRun, false);
    }
    Module::RunningToken& Module::RunningToken::operator=(RunningToken&& op)
    {
        Creator = std::exchange(op.Creator, nullptr);
        _CanRun = std::exchange(op._CanRun, false);
        return *this;
    }
    Module::RunningToken::~RunningToken()
    {
        if (Creator == nullptr)
            return;
        if (_CanRun && (
                Creator->CanRunPolicy == CanRunPolicyType::CannotRunInParallel
                || Creator->CanRunPolicy == CanRunPolicyType::CannotRunInParallelCustom))
        {
            std::unique_lock<std::shared_mutex> lock(Creator->SharedMutex);
            Creator->_IsAvailable = true;
            lock.unlock();
            Creator->AvailabilityConditionVariable.notify_all();
        }
    }
    bool Module::RunningToken::CanRun()
    {
        if (Creator == nullptr)
            return false;
        return _CanRun;
    }
    void Module::RunningToken::Run()
    {
        if (Creator == nullptr)
            return;
        if (_CanRun)
        {
            _CanRun = false;
            // Guarantees to set _IsAvailable to true at the end
            // Doesn't matter if can run in parallel
            SetToTrueGuard g(Creator->_IsAvailable, Creator->SharedMutex, Creator->AvailabilityConditionVariable);
            auto start = std::chrono::steady_clock::now();
            try
            {
                Creator->OnRun();
            }
            catch (const std::exception& e)
            {
                try
                {
                    Creator->HandleException(e);
                }
                catch (...) {}
            }
            catch (...)
            {
                try
                {
                    Creator->HandleException(std::current_exception());
                }
                catch (...) {}
            }
            std::chrono::duration<double> duration = std::chrono::steady_clock::now() - start;
            double time = duration.count();
            std::unique_lock<std::shared_mutex> lock(Creator->SharedMutex);
            Creator->HigherExecutionTimePredictor->ReportObservation(time);
            Creator->LowerExecutionTimePredictor->ReportObservation(time);
            lock.unlock();
        }
    }

    Module::RunningToken Module::GetRunningToken()
    {
        return RunningToken(this);
    }

    bool Module::IsAvailable()
    {
        std::shared_lock<std::shared_mutex> lock(SharedMutex);
        return _IsAvailable;
    }

    void Module::WaitForAvailability(double MaxWaitingTime)
    {
        std::chrono::time_point<std::chrono::steady_clock> start;
        if (MaxWaitingTime != 0)
            start = std::chrono::steady_clock::now();

        std::shared_lock<std::shared_mutex> lock(SharedMutex);
        if (_IsAvailable)
            return;
        lock.unlock();

        const auto predicate = [this] {
            std::shared_lock<std::shared_mutex> lock(SharedMutex);
            return _IsAvailable;
        };

        std::unique_lock<std::mutex> cv_lock(AvailabilityConditionMutex);
        if (MaxWaitingTime == 0)
        {
            AvailabilityConditionVariable.wait(cv_lock, predicate);
        }
        else if (MaxWaitingTime > 0)
        {
            auto stop = start + std::chrono::duration<double>(MaxWaitingTime);
            std::chrono::duration<double> time = stop - std::chrono::steady_clock::now();
#if LOOPSCHEDULER_USE_SMART_CV_WAITER
            CVWaiter->WaitFor(AvailabilityConditionVariable, cv_lock, time, predicate);
#else
            AvailabilityConditionVariable.wait_for(cv_lock, time, predicate);
#endif
        }
    }

    double Module::PredictHigherExecutionTime()
    {
        std::shared_lock<std::shared_mutex> lock(SharedMutex);
        return HigherExecutionTimePredictor->Predict();
    }

    double Module::PredictLowerExecutionTime()
    {
        std::shared_lock<std::shared_mutex> lock(SharedMutex);
        return LowerExecutionTimePredictor->Predict();
    }

    bool Module::SetParent(Group * Parent)
    {
        std::unique_lock<std::shared_mutex> lock(SharedMutex);
        if (this->Parent != nullptr && Parent != nullptr)
            return false;
        this->Parent = Parent;
        return true;
    }

    bool Module::SetLoop(Loop * LoopPtr)
    {
        std::unique_lock<std::shared_mutex> lock(SharedMutex);
        if (this->LoopPtr != nullptr && LoopPtr != nullptr)
            return false;
        this->LoopPtr = LoopPtr;
        return true;
    }

    Group * Module::GetParent()
    {
        std::shared_lock<std::shared_mutex> lock(SharedMutex);
        return Parent;
    }

    Loop * Module::GetLoop()
    {
        std::shared_lock<std::shared_mutex> lock(SharedMutex);
        return LoopPtr;
    }

    bool Module::CanRun() { return true; }
    void Module::HandleException(const std::exception& e) {}
    void Module::HandleException(std::exception_ptr e_ptr) {}

    Module::IdlingToken::IdlingToken() : MutexPtr(new std::mutex), ShouldStopPtr(new bool(false)), ThreadPtr(nullptr)
    {
    }
    Module::IdlingToken::IdlingToken(IdlingToken&& op)
    {
        MutexPtr = std::exchange(op.MutexPtr, nullptr);
        ShouldStopPtr = std::exchange(op.ShouldStopPtr, nullptr);
        ThreadPtr = std::exchange(op.ThreadPtr, nullptr);
    }
    Module::IdlingToken& Module::IdlingToken::operator=(IdlingToken&& op)
    {
        MutexPtr = std::exchange(op.MutexPtr, nullptr);
        ShouldStopPtr = std::exchange(op.ShouldStopPtr, nullptr);
        ThreadPtr = std::exchange(op.ThreadPtr, nullptr);
        return *this;
    }
    Module::IdlingToken::~IdlingToken()
    {
        if (MutexPtr == nullptr) // Has moved
            return;
        std::unique_lock<std::mutex> lock(*MutexPtr);
        (*ShouldStopPtr) = true;
        lock.unlock();
        if (ThreadPtr->joinable())
            ThreadPtr->join();
        delete MutexPtr;
        delete ShouldStopPtr;
        delete ThreadPtr;
    }
    void Module::IdlingToken::Stop()
    {
        if (MutexPtr == nullptr) // Has moved
            return;
        std::unique_lock<std::mutex> lock(*MutexPtr);
        (*ShouldStopPtr) = true;
        lock.unlock();
        if (ThreadPtr->joinable())
            ThreadPtr->join();
    }

    void Module::Idle(double MinWaitingTime)
    {
        auto start = std::chrono::steady_clock::now();
        double remaining_time = MinWaitingTime;
        while (remaining_time > 0)
        {
            auto architecture = LoopPtr->GetArchitecture();
            if (!architecture->RunNext(remaining_time))
                architecture->WaitForAvailability(remaining_time, remaining_time);
            remaining_time = MinWaitingTime - (
                    (std::chrono::duration<double>)(std::chrono::steady_clock::now() - start)
                ).count();
        }
    }

    Module::IdlingToken Module::StartIdling(double MaxWaitingTimeAfterStop, double TotalMaxWaitingTime)
    {
        IdlingToken token;
        auto MutexPtr = token.MutexPtr;
        auto ShouldStopPtr = token.ShouldStopPtr;
        token.ThreadPtr = new std::thread([this, MutexPtr, ShouldStopPtr, MaxWaitingTimeAfterStop, TotalMaxWaitingTime] {
            if (TotalMaxWaitingTime == 0)
            {
                if (MaxWaitingTimeAfterStop <= MNIMAL_TIME)
                    return; // Prevent unnecessary waiting or potential freezing
                while (true)
                {
                    auto architecture = LoopPtr->GetArchitecture();
                    if (!architecture->RunNext(MaxWaitingTimeAfterStop))
                        architecture->WaitForAvailability(MaxWaitingTimeAfterStop, MaxWaitingTimeAfterStop * 0.25);
                    std::unique_lock<std::mutex> lock(*MutexPtr);
                    if (*ShouldStopPtr)
                        return;
                }
            }
            else
            {
                auto start = std::chrono::steady_clock::now();
                double remaining_time = TotalMaxWaitingTime;
                while (remaining_time > 0)
                {
                    double time = std::min(remaining_time, MaxWaitingTimeAfterStop);
                    if (time <= MNIMAL_TIME)
                        return; // Prevent unnecessary waiting or potential freezing
                    auto architecture = LoopPtr->GetArchitecture();
                    if (!architecture->RunNext(time))
                        architecture->WaitForAvailability(time, time * 0.25);
                    std::unique_lock<std::mutex> lock(*MutexPtr);
                    if (*ShouldStopPtr)
                        return;
                    lock.unlock();
                    remaining_time = TotalMaxWaitingTime - (
                            (std::chrono::duration<double>)(std::chrono::steady_clock::now() - start)
                        ).count();
                }
            }
        });
        return token;
    }
}
