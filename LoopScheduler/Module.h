#pragma once

#include "LoopScheduler.dec.h"

#include <memory>

namespace LoopScheduler
{
    class Module
    {
        friend Loop; // To access Loop
        friend Group; // To access Parent
    public:
        virtual ~Module();

        class RunningToken // TODO: Implement parallel run ability.
        {
            friend Module;
        public:
            ~RunningToken();
            /// @brief Whether running is permitted.
            bool CanRun();
            /// @brief This only works once per object, if CanRun() returns true.
            void Run();
        private:
            std::weak_ptr<Module> Creator;
        };

        /// @brief Used by Group.
        ///        Gets a running token to check whether it's possible to run and then run,
        ///        while reserving that run until the token is destructed or used.
        RunningToken GetRunningToken();
        /// @brief Checks whether it's permitted to run the module.
        bool IsAvailable();
        /// @brief Waits until it's permitted to run the module.
        ///        May give false positive (return when cannot run).
        void WaitForRunAvailability(double MaxWaitingTime = 0);
        /// @brief Whether the module is running.
        ///
        /// Thread-safe
        bool IsRunning();
        /// @brief Returns the higher predicted timespan in seconds.
        ///
        /// Thread-safe
        double PredictHigherExecutionTime();
        /// @brief Returns the lower predicted timespan in seconds.
        ///
        /// Thread-safe
        double PredictLowerExecutionTime();
    protected:
        /// @brief To change the default settings in the derived class.
        Module(
            bool CanRunInParallel = false,
            std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor = std::unique_ptr<TimeSpanPredictor>(),
            std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor = std::unique_ptr<TimeSpanPredictor>()
        );
        virtual void OnRun() = 0;

        class IdlingToken
        {
            friend Module;
        public:
            ~IdlingToken();
            /// @brief Stops idling. Only works once.
            ///        It's automatically done if the object is destructed.
            void Stop();
        private:
            std::weak_ptr<Module> Creator;
        };

        void Idle(double min_waiting_time);
        IdlingToken StartIdling(double max_waiting_time_after_stop, double total_max_waiting_time = 0);
    private:
        /// @brief Cannot have 2 parents, only be able to set when parent is destructed.
        ///        Check for loops using this.
        ///
        /// Note: With each module only having 1 parent,
        /// it's reliable to wait for availability by waiting for the module to be done running.
        /// Also having multiple children of the same module in a group is possible.
        Group * Parent;
        Loop * Loop;
        std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor;
        std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor;
        bool CanRunInParallel;
        bool IsIdling; // To detect StartIdling being called more than once
    };
}
