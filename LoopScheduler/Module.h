#pragma once

#include "LoopScheduler.dec.h"

#include <memory>

namespace LoopScheduler
{
    class Module
    {
    public:
        virtual ~Module();

        /// @brief Used by Group
        void Run();
        /// @brief Returns the higher predicted timespan in seconds.
        double PredictHigherExecutionTime();
        /// @brief Returns the lower predicted timespan in seconds.
        double PredictLowerExecutionTime();
    protected:
        Module(
            std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor = std::unique_ptr<TimeSpanPredictor>(),
            std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor = std::unique_ptr<TimeSpanPredictor>()
        );
        virtual void OnRun() = 0;

        class IdlingToken
        {
            friend Module;
        public:
            void Stop();
        private:
            std::weak_ptr<Module> Creator;
        };

        void Idle(double min_waiting_time);
        IdlingToken StartIdling(double max_waiting_time_after_stop, double total_max_waiting_time = 0);
    private:
        std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor;
        std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor;
        bool IsIdling; // To detect StartIdling being called more than once
    };
}
