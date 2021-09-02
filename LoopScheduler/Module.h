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
        /// @brief Returns the predicted timespan.
        double PredictExecutionTime();
    protected:
        Module(std::shared_ptr<TimeSpanPredictor> ExecutionTimePredictor = std::shared_ptr<TimeSpanPredictor>());
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
        std::shared_ptr<TimeSpanPredictor> ExecutionTimePredictor; // Accessed by Group
        bool IsIdling; // To detect StartIdling being called more than once
    };
}
