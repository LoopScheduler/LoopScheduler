#pragma once

#include "LoopScheduler.dec.h"

namespace LoopScheduler
{
    /// @brief An abstract class to predict the future timespans for modules' run times.
    class TimeSpanPredictor
    {
    public:
        virtual ~TimeSpanPredictor() = default;
        /// @brief Reinitializes, forgetting the past observations.
        virtual void Initialize(double TimeSpan) = 0;
        /// @brief Reports a new timespan observation.
        virtual void ReportObservation(double TimeSpan) = 0;
        /// @brief Returns the predicted timespan.
        virtual double Predict() = 0;
    };
}
