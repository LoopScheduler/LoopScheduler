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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace LoopScheduler
{
    /// @brief Runs a multi-threaded loop using an architecture defined by Group objects.
    class Loop final
    {
    public:
        Loop(std::shared_ptr<Group> Architecture);
        ~Loop();

        /// @brief Thread-safe method to run the loop. An exception will be thrown if called more than once without a Stop() in between.
        ///
        /// One of the loop threads is the one which this method is called in, new threads are created for others.
        ///
        /// @param threads_count The number of threads to allocate. The default is the number of logical CPU cores.
        void Run(int threads_count = 0);
        /// @brief Thread-safe method to stop the loop. Won't do anything if the loop isn't running.
        void Stop();
        /// @brief Thread-safe method to stop the loop and wait for it. Won't do anything if the loop isn't running.
        ///
        /// Don't call this from a Module or anything run by the architecture inside the loop.
        void StopAndWait();
        /// @brief Thread-safe method to check whether the loop is running.
        bool IsRunning();
        /// @return Pointer to the architecture group. Has to be used immediately because it's a normal pointer.
        Group * GetArchitecture();
        /// @return A weak pointer to the architecture group to use later.
        std::weak_ptr<Group> GetArchitectureWeakPtr();
    private:
        std::shared_ptr<Group> Architecture;
        std::mutex Mutex;
        std::condition_variable ConditionVariable;
        /// @brief Only set in Run()
        bool _IsRunning;
        bool ShouldStop;
    };
}
