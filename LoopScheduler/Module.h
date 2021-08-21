#pragma once

#include "LoopScheduler.dec.h"

namespace LoopScheduler
{
    class Module
    {
        friend Group;
    public:
        virtual ~Module();
    protected:
        virtual Module(TimeSpanPredictor ExecutionTimePredictor)
        virtual Run() = 0;

        class IdlingToken
        {
            friend Module;
        public:
            void Stop();
        private:
            Module Creator;
        }

        void Idle(double min_waiting_time);
        IdlingToken StartIdling(double max_waiting_time_after_stop, double total_max_waiting_time = 0);
    private:
        virtual _Run(); // Used by Group

        TimeSpanPredictor ExecutionTimePredictor; // Accessed by Group
        bool IsIdling; // To detect StartIdling being called more than once
    };
}
