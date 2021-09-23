#pragma once

#include "LoopScheduler.dec.h"

#include <memory>
#include <shared_mutex>
#include <variant>
#include <vector>

namespace LoopScheduler
{
    /// @brief Represents a group of modules or other groups scheduled in a certain way.
    ///
    /// Group members must only be specified on construction.
    /// Derived classes have to call IntroduceMembers in the constructor.
    /// Members list change should not be allowed.
    class Group
    {
    public:
        Group();
        virtual ~Group();
        /// @brief Thread-safe method to run the next module.
        /// @return Whether a module was run.
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) = 0;
        /// @brief Checks whether a next module is available to run or IsDone returns true.
        virtual bool IsAvailable(double MaxEstimatedExecutionTime = 0) = 0;
        /// @brief Waits until a next module is available to run or IsDone returns true.
        ///        May give false positive (return when there is no module to run).
        /// @param MaxWaitingTime Maximum time to wait in seconds.
        virtual void WaitForAvailability(double MaxEstimatedExecutionTime = 0, double MaxWaitingTime = 0) = 0;
        /// @brief Thread-safe method to check whether the group is ready to finish the iteration.
        virtual bool IsDone() = 0;
        /// @brief Thread-safe method to start a new iteration.
        virtual void StartNextIteration() = 0;
        /// @brief Returns the higher predicted remaining execution time in seconds.
        ///
        /// Only zero when nothing is being executed in the group, otherwise, always non-zero positive.
        virtual double PredictHigherRemainingExecutionTime() = 0;
        /// @brief Returns the lower predicted remaining execution time in seconds.
        ///
        /// Only zero when nothing is being executed in the group, otherwise, always non-zero positive.
        virtual double PredictLowerRemainingExecutionTime() = 0;
        /// @brief Returns the group's parent group.
        Group * GetParent();
        /// @brief Returns the group's members.
        std::vector<std::variant<std::weak_ptr<Group>, std::weak_ptr<Module>>> GetMembers();
    protected:
        /// @brief Has to be called once in the derived class's constructor after initializing the members.
        ///
        /// Members list change should not be allowed.
        void IntroduceMembers(std::vector<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>>);
    private:
        std::shared_mutex SharedMutex;
        /// @brief Cannot have 2 parents, only be able to set when parent is destructed.
        ///        Could check for loops using this,
        ///        but with constructor only Members initialization there shouldn't be a loop.
        ///
        /// Note: With each group only having 1 parent,
        /// it's reliable to predict remaining execution times using the child groups.
        /// Also having multiple children of the same group is possible.
        Group * Parent;
        std::vector<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>> Members;
        std::vector<std::variant<std::weak_ptr<Group>, std::weak_ptr<Module>>> WeakMembers;
    };
}
