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

#pragma once

#include "LoopScheduler.dec.h"

#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>

namespace LoopScheduler
{
    /// @brief To be derived to run a piece of code per iteration in a group, in a loop.
    ///
    /// To change the default settings,
    /// use the protected constructor in the derived class constructor.
    /// Example:
    ///   MyModule::MyModule(...) : Module(...) { ... }
    class Module
    {
    public:
        /// @brief To change the default settings in the derived class.
        ///
        /// Example:
        ///   MyModule::MyModule(...) : Module(...) { ... }
        ///
        /// @param CanRunInParallel Whether the module can run in another thread while it's already running.
        /// @param HigherExecutionTimePredictor Predictor to predict the higher timespan. nullptr to use default.
        /// @param LowerExecutionTimePredictor Predictor to predict the lower timespan. nullptr to use default.
        /// @param UseCustomCanRun Whether to use the virtual CanRun to check for availability before running too.
        ///                        Doesn't support IsAvailable or WaitForAvailability, IsAvailable may return true when CanRun returns false.
        ///                        Only use custom CanRun if really needed.
        Module(
            bool CanRunInParallel = false,
            std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor = nullptr,
            std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor = nullptr,
            bool UseCustomCanRun = false
        );
        virtual ~Module() = default;

        /// @brief Not thread-safe, use in a single thread.
        class RunningToken final
        {
            friend Module;
        public:
            RunningToken(RunningToken&) = delete;
            RunningToken(RunningToken&&);
            RunningToken& operator=(RunningToken&) = delete;
            RunningToken& operator=(RunningToken&&);
            ~RunningToken();
            /// @brief Whether running is permitted.
            ///
            /// Not thread-safe.
            bool CanRun();
            /// @brief This only works once per object, if CanRun() returns true.
            ///
            /// Won't check whether the Module object is destructed or not,
            /// call this immediately if CanRun() has returned true.
            ///
            /// Not thread-safe.
            void Run();
        private:
            /// @param Creator Should not be nullptr.
            RunningToken(Module * Creator);
            Module * Creator;
            bool _CanRun;
        };
        friend RunningToken;

        /// @brief Used by Group.
        ///        Gets a running token to check whether it's possible to run and then run,
        ///        while reserving that run until the token is destructed or used.
        RunningToken GetRunningToken();
        /// @brief Checks whether it's permitted to run the module.
        ///        May give false positive (return true when cannot run) if a custom CanRun code is used.
        bool IsAvailable();
        /// @brief Waits until it's permitted to run the module.
        ///        May give false positive (return when cannot run).
        /// @param MaxWaitingTime Maximum time to wait in seconds. No max time if 0 (default).
        void WaitForAvailability(double MaxWaitingTime = 0);
        /// @brief Returns the higher predicted timespan in seconds.
        ///
        /// Thread-safe
        double PredictHigherExecutionTime();
        /// @brief Returns the lower predicted timespan in seconds.
        ///
        /// Thread-safe
        double PredictLowerExecutionTime();

        /// @brief Should only be called by the Group that has this module as a member.
        /// @return Whether it was successful. Should fail the operation if false.
        bool SetParent(Group * Parent);
        /// @brief Should only be called by the Group that has this module as a member.
        /// @return Whether it was successful. Should fail the operation if false.
        bool SetLoop(Loop * LoopPtr);
        Group * GetParent();
        Loop * GetLoop();
    protected:
        virtual void OnRun() = 0;
        virtual bool CanRun();
        /// @brief To handle an exception that is derived from std::exception
        virtual void HandleException(const std::exception& e);
        /// @brief To handle an unknown exception
        virtual void HandleException(std::exception_ptr e_ptr);

        class IdlingToken final
        {
            friend Module;
        public:
            IdlingToken(IdlingToken&) = delete;
            IdlingToken(IdlingToken&&);
            IdlingToken& operator=(IdlingToken&) = delete;
            IdlingToken& operator=(IdlingToken&&);
            ~IdlingToken();
            /// @brief Stops idling. Only works once.
            ///        It's automatically done if the object is destructed.
            void Stop();
        private:
            IdlingToken();
            std::mutex * MutexPtr;
            bool * ShouldStopPtr;
            /// @brief Should not be nullptr by the time any member function is called.
            std::thread * ThreadPtr;
        };

        /// @brief To yield for other modules to possibly run meanwhile.
        ///        Recommended over StartIdling as it's faster and simpler.
        /// @param MinWaitingTime Minimum time to wait in seconds. Probably won't take much longer.
        void Idle(double MinWaitingTime);
        /// @brief To yield for other modules to possibly run meanwhile, in another thread.
        ///
        /// Do not call this a second time before stopping or destructing the first one's token.
        /// Do not call Idle after calling this, and before stopping or destructing the token.
        ///
        /// @param MaxWaitingTimeAfterStop Approximate maximum time to wait in seconds when calling the returned token's Stop().
        /// @param TotalMaxWaitingTime Approximate total maximum time to wait in seconds.
        ///                            If 0 (default), it will wait until the returned token's Stop() is called,
        ///                            or until the token is destructed.
        IdlingToken StartIdling(double MaxWaitingTimeAfterStop, double TotalMaxWaitingTime = 0);
    private:
        enum CanRunPolicyType
        {
            CannotRunInParallel = 0,
            CanRunInParallel = 1,
            CannotRunInParallelCustom = 2,
            CanRunInParallelCustom = 3,
        };
        const CanRunPolicyType CanRunPolicy;

        std::shared_mutex SharedMutex;

        /// @brief Cannot have 2 parents, only be able to set when parent is destructed.
        ///
        /// Note: With each module only having 1 parent,
        /// it's reliable to wait for availability by waiting for the module to be done running.
        /// Also having multiple children of the same module in a group is possible.
        Group * Parent;
        /// @brief Cannot be in 2 loops.
        Loop * LoopPtr;

        std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor;
        std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor;

        /// @brief Always true if CanRunInParallel
        bool _IsAvailable;
        std::mutex AvailabilityConditionMutex;
        /// @brief Notified when _IsAvailable is set to true.
        std::condition_variable AvailabilityConditionVariable;
    };
}
