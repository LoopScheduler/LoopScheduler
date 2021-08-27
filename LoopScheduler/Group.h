#pragma once

#include "LoopScheduler.dec.h"

#include <memory>

namespace LoopScheduler
{
    /// @brief Represents a group of modules or other groups scheduled in a certain way.
    class Group
    {
        friend Loop;
        friend Module;
    public:
        virtual ~Group();
    protected:
        /// @brief Thread-safe method to get the next module scheduled to run.
        virtual std::weak_ptr<Module> GetNextModule() = 0; // TODO: Remove if not used?
        /// @brief Thread-safe method to run the next module.
        /// @return Whether a module was run.
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) = 0;
        /// @brief Thread-safe method to check whether the group is ready to finish the iteration.
        virtual bool IsDone() = 0;
        /// @brief Waits until a next module is available to run.
        virtual void WaitForNextModule() = 0;
        /// @brief Thread-safe method to start a new iteration.
        virtual void StartNextIteration() = 0;
    };
}
