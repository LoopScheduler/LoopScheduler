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

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace LoopScheduler
{
    /// @brief Performs std::condition_variable::wait_for in a smarter way,
    ///        by accounting for the historical wait_for timing error
    ///        to wait more predictably.
    class SmartCVWaiter
    {
    public:
        SmartCVWaiter(std::unique_ptr<TimeSpanPredictor> HigherErrorPredictor = nullptr);

        template <typename PredicateType>
        bool WaitFor(std::condition_variable cv, std::unique_lock<std::mutex>& cv_lock,
                std::chrono::duration<double> time, PredicateType predicate);
    private:
        std::unique_ptr<TimeSpanPredictor> HigherErrorPredictor;
        std::shared_mutex PredictorMutex;
    };

    template <typename PredicateType>
    bool SmartCVWaiter::WaitFor<PredicateType>(std::condition_variable cv, std::unique_lock<std::mutex>& cv_lock,
            std::chrono::duration<double> time, PredicateType predicate)
    {
        double error_prediction;
        {
            std::shared_lock<std::shared_mutex> shared_lock(PredictorMutex);
            error_prediction = HigherErrorPredictor->Predict();
        }
        if (error_prediction >= time.count())
            return false;
        std::chrono::duration<double> corrected_time(error_prediction > 0 ? time - std::chrono::duration<double>(error_prediction) : time);
        std::chrono::time_point<std::chrono::steady_clock> start;
        bool result = cv.wait_for(cv_lock, corrected_time, predicate);
        if (!result) // Only record when the predicate wasn't satisfied => pure time error
        {
            std::chrono::time_point<std::chrono::steady_clock> stop;
            std::chrono::duration<double> actual_time = stop - start;
            std::lock_guard<std::shared_mutex> lock(PredictorMutex);
            HigherErrorPredictor->ReportObservation((actual_time - time).count());
        }
        return result;
    }
}
