// Copyright (c) 2021 Majidzadeh (hashpragmaonce@gmail.com)
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
        ///
        /// This should not change the predictor's state (or modify member variables).
        virtual double Predict() = 0;
        /// @brief Creates a new copy of the object and returns the pointer.
        ///
        /// WARNING: Possible memory leak if not managed well.
        /// The returned pointer can be immediately turned into
        /// a smart pointer (i.e., std::shared_ptr)
        /// by passing it to a smart pointer constructor.
        virtual TimeSpanPredictor * Copy() = 0;
    };
}
