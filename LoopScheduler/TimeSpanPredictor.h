#pragma once

#include "LoopScheduler.dec.h"

namespace LoopScheduler
{
    class TimeSpanPredictor
    {
    public:
        virtual ~TimeSpanPredictor();
        virtual void Initialize(double TimeSpan) = 0;
        virtual void ReportObservation(double TimeSpan) = 0;
        virtual double Predict() = 0;
    };
}
