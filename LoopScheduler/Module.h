#pragma once

#include "LoopScheduler.dec.h"

#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <shared_mutex>

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
        friend Loop; // To access Loop
        friend Group; // To access Parent
    public:
        /// @brief To change the default settings in the derived class.
        ///
        /// Example:
        ///   MyModule::MyModule(...) : Module(...) { ... }
        ///
        /// @param CanRunInParallel Whether the module can run in another thread while it's running.
        /// @param HigherExecutionTimePredictor Predictor to predict the higher timespan.
        /// @param LowerExecutionTimePredictor Predictor to predict the lower timespan.
        Module(
            bool CanRunInParallel = false,
            std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor = nullptr,
            std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor = nullptr
        );
        virtual ~Module() = default;

        /// @brief Not thread-safe, use in a single thread.
        class RunningToken
        {
            friend Module;
        public:
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
        bool IsAvailable();
        /// @brief Waits until it's permitted to run the module.
        ///        May give false positive (return when cannot run).
        /// @param MaxWaitingTime Maximum time to wait in seconds. No max time if 0 (default).
        void WaitForRunAvailability(double MaxWaitingTime = 0);
        /// @brief Returns the higher predicted timespan in seconds.
        ///
        /// Thread-safe
        double PredictHigherExecutionTime();
        /// @brief Returns the lower predicted timespan in seconds.
        ///
        /// Thread-safe
        double PredictLowerExecutionTime();
    protected:
        virtual void OnRun() = 0;
        /// @brief To handle an exception that is derived from std::exception
        virtual void HandleException(const std::exception& e);
        /// @brief To handle an unknown exception
        virtual void HandleException(std::exception_ptr e_ptr);

        class IdlingToken
        {
            friend Module;
        public:
            ~IdlingToken();
            /// @brief Stops idling. Only works once.
            ///        It's automatically done if the object is destructed.
            void Stop();
        private:
            IdlingToken(Module * Creator);
            Module * Creator;
        };

        /// @brief To yield for other modules to possibly run meanwhile.
        ///        Recommended over StartIdling.
        /// @param MinWaitingTime Minimum time to wait in seconds. Probably won't take much longer.
        void Idle(double MinWaitingTime);
        /// @brief To yield for other modules to possibly run meanwhile, in another thread.
        ///
        /// Calling this twice will give a dummy IdlingToken the second time.
        ///
        /// @param MaxWaitingTimeAfterStop Maximum time to wait in seconds calling IdlingToken.Stop().
        /// @param TotalMaxWaitingTime Maximum time to wait in seconds.
        ///                            If 0 (default), it will wait untill IdlingToken.Stop() is called.
        IdlingToken StartIdling(double MaxWaitingTimeAfterStop, double TotalMaxWaitingTime = 0);
    private:
        const bool CanRunInParallel;

        std::shared_mutex SharedMutex;

        /// @brief Cannot have 2 parents, only be able to set when parent is destructed.
        ///
        /// Note: With each module only having 1 parent,
        /// it's reliable to wait for availability by waiting for the module to be done running.
        /// Also having multiple children of the same module in a group is possible.
        Group * Parent;
        /// @brief Cannot be in 2 loops.
        Loop * Loop;

        std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor;
        std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor;

        /// @brief Always true if CanRunInParallel
        bool _IsAvailable;
        std::mutex AvailabilityConditionMutex;
        /// @brief Notified when _IsAvailable is set to true.
        std::condition_variable AvailabilityConditionVariable;

        bool IsIdling; // To detect StartIdling being called more than once
    };
}
