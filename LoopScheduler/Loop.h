#pragma once

#include "LoopScheduler.dec.h"

#include <condition_variable>
#include <memory>
#include <mutex>

namespace LoopScheduler
{
    /// @brief Runs a multi-threaded loop using an architecture defined by Group objects.
    class Loop final
    {
    public:
        Loop(std::shared_ptr<Group> Architecture);
        ~Loop();

        /// @brief Thread-safe method to start the loop. An exception will be thrown if called more than once without a Stop() in between.
        /// @param threads_count The number of threads to allocate. The default is the number of logical CPU cores.
        void Start(int threads_count = 0);
        /// @brief Thread-safe method to stop the loop. Won't do anything if the loop isn't running.
        void Stop();
        /// @brief Thread-safe method to check whether the loop is running.
        bool IsRunning();
    private:
        std::shared_ptr<Group> Architecture;
        std::mutex Mutex;
        std::condition_variable ConditionVariable;
        /// @brief Only set in Start()
        bool _IsRunning;
        bool ShouldStop;
    };
}
