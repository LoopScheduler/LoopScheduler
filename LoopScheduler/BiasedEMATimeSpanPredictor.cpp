#include "BiasedEMATimeSpanPredictor.h"

namespace LoopScheduler
{
    BiasedEMATimeSpanPredictor::BiasedEMATimeSpanPredictor(double InitialValue, double IncrementAlpha, double DecrementAlpha)
        : TimeSpanBEMA(InitialValue), IncrementAlpha(IncrementAlpha), DecrementAlpha(DecrementAlpha)
    {}

    void BiasedEMATimeSpanPredictor::Initialize(double TimeSpan)
    {
        TimeSpanBEMA = TimeSpan;
    }

    void BiasedEMATimeSpanPredictor::ReportObservation(double TimeSpan)
    {
        if (TimeSpan > TimeSpanBEMA)
            TimeSpanBEMA = TimeSpanBEMA + IncrementAlpha * (TimeSpan - TimeSpanBEMA);
        else
            TimeSpanBEMA = TimeSpanBEMA + DecrementAlpha * (TimeSpan - TimeSpanBEMA);
    }

    double BiasedEMATimeSpanPredictor::Predict()
    {
        return TimeSpanBEMA;
    }
}
