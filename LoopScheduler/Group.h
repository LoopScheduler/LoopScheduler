#pragma once

#include "LoopScheduler.dec.h"

#include <memory>

namespace LoopScheduler
{
    /// @brief Represents a group of modules or other groups scheduled in a certain way.
    class Group
    {
    public:
        virtual ~Group();
        /// @brief Thread-safe method to run the next module.
        /// @return Whether a module was run.
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) = 0;
        /// @brief Returns the higher predicted remaining execution time in seconds.
        ///
        /// Only zero when nothing is being executed in the group, otherwise, always non-zero positive.
        virtual double PredictRemainingHigherExecutionTime() = 0;
        /// @brief Returns the lower predicted remaining execution time in seconds.
        ///
        /// Only zero when nothing is being executed in the group, otherwise, always non-zero positive.
        virtual double PredictRemainingLowerExecutionTime() = 0;
        /// @brief Waits until a next module is available to run or IsDone returns true.
        ///        May give false positive (return when there is no module to run).
        virtual void WaitForNextEvent(double MaxEstimatedExecutionTime = 0) = 0;
        /// @brief Thread-safe method to check whether the group is ready to finish the iteration.
        virtual bool IsDone() = 0;
        /// @brief Thread-safe method to start a new iteration.
        virtual void StartNextIteration() = 0;
    protected:
        void CheckMemberGroupForLoops(Group*); // TODO: Implement loop detection
    private:
        std::weak_ptr<Group> Parent;
    };
}
