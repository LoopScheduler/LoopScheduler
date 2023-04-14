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
#include "TimeSpanPredictor.h"

namespace LoopScheduler
{
    /// @brief A TimeSpanPredictor implementation that uses Exponential Moving Average
    ///        with 2 different weights for incrementing and decrementing.
    class BiasedEMATimeSpanPredictor final : public TimeSpanPredictor
    {
    public:
        BiasedEMATimeSpanPredictor(double InitialValue, double IncrementAlpha, double DecrementAlpha);
        virtual void Initialize(double TimeSpan) override;
        virtual void ReportObservation(double TimeSpan) override;
        virtual double Predict() override;

        static constexpr double DEFAULT_FAST_ALPHA = 0.2;
        static constexpr double DEFAULT_SLOW_ALPHA = 0.05;
    private:
        double IncrementAlpha;
        double DecrementAlpha;
        double TimeSpanBEMA;
    };
}
