#include "Module.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include "BiasedEMATimeSpanPredictor.h"
#include "Loop.h"
#include "Group.h"

namespace LoopScheduler
{
    Module::Module(
            bool CanRunInParallel,
            std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor,
            std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor
        ) : CanRunInParallel(CanRunInParallel), Parent(nullptr), Loop(nullptr), _IsAvailable(true)
    {
        if (HigherExecutionTimePredictor == nullptr)
            HigherExecutionTimePredictor = std::unique_ptr<BiasedEMATimeSpanPredictor>(new BiasedEMATimeSpanPredictor(0, 0.2, 0.05));
        if (LowerExecutionTimePredictor == nullptr)
            LowerExecutionTimePredictor = std::unique_ptr<BiasedEMATimeSpanPredictor>(new BiasedEMATimeSpanPredictor(0, 0.05, 0.2));
        this->HigherExecutionTimePredictor = std::move(HigherExecutionTimePredictor);
        this->LowerExecutionTimePredictor = std::move(LowerExecutionTimePredictor);
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
        if (Creator->CanRunInParallel)
        {
            _CanRun = true;
        }
        else
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
        if (!Creator->CanRunInParallel && _CanRun)
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
            return;
        _CanRun;
    }
    void Module::RunningToken::Run()
    {
        if (Creator == nullptr)
            return;
        if (_CanRun)
        {
            _CanRun = false;
            // Guarantees to set _IsAvailable to true at the end
            // Doesn't matter if Creator->CanRunInParallel
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

    void Module::WaitForRunAvailability(double MaxWaitingTime)
    {
        std::shared_lock<std::shared_mutex> lock(SharedMutex);
        if (_IsAvailable)
            return;
        lock.unlock();

        const auto predicate = [this] {
            std::shared_lock<std::shared_mutex> lock(SharedMutex);
            return _IsAvailable;
        };

        std::unique_lock<std::mutex> c_lock(AvailabilityConditionMutex);
        if (MaxWaitingTime == 0)
            AvailabilityConditionVariable.wait(c_lock, predicate);
        else if (MaxWaitingTime > 0)
            AvailabilityConditionVariable.wait_for(
                c_lock,
                std::chrono::duration<double>(MaxWaitingTime),
                predicate
            );
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
            if (!Loop->Architecture->RunNextModule(remaining_time))
                Loop->Architecture->WaitForAvailability(remaining_time, remaining_time);
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
                while (true)
                {
                    if (!Loop->Architecture->RunNextModule(MaxWaitingTimeAfterStop))
                        Loop->Architecture->WaitForAvailability(MaxWaitingTimeAfterStop, MaxWaitingTimeAfterStop * 0.25);
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
                    if (!Loop->Architecture->RunNextModule(time))
                        Loop->Architecture->WaitForAvailability(time, time * 0.25);
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

    Loop * Module::GetLoop()
    {
        return Loop;
    }
}
