#pragma once

#include "LoopScheduler.dec.h"
#include "TimeSpanPredictor.h"

namespace LoopScheduler
{
    class BiasedEMATimeSpanPredictor final : public TimeSpanPredictor
    {
    public:
        BiasedEMATimeSpanPredictor(double InitialValue, double IncrementAlpha, double DecrementAlpha);
        virtual void Initialize(double TimeSpan) override;
        virtual void ReportObservation(double TimeSpan) override;
        virtual double Predict() override;
    private:
        double IncrementAlpha;
        double DecrementAlpha;
        double TimeSpanBEMA;
    };
}
