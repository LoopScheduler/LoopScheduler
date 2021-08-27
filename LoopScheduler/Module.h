#pragma once

#include "LoopScheduler.dec.h"

#include <memory>

namespace LoopScheduler
{
    class Module
    {
        friend Group;
    public:
        virtual ~Module();
    protected:
        Module(std::shared_ptr<TimeSpanPredictor> ExecutionTimePredictor = std::shared_ptr<TimeSpanPredictor>());
        virtual void Run() = 0;

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
        virtual void _Run(); // Used by Group

        std::shared_ptr<TimeSpanPredictor> ExecutionTimePredictor; // Accessed by Group
        bool IsIdling; // To detect StartIdling being called more than once
    };
}
