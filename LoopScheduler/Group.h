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

#include <memory>
#include <shared_mutex>
#include <variant>
#include <vector>

namespace LoopScheduler
{
    /// @brief Represents a group of runnable objects or (optionally) other groups scheduled in a certain way.
    ///
    /// Group members must only be specified on construction.
    /// Derived classes have to call IntroduceMembers in the constructor.
    /// Members list change should not be allowed.
    class Group
    {
        friend Loop;
    public:
        Group();
        virtual ~Group();
        /// @brief Thread-safe method to run the next thing.
        /// @return Whether a something was run.
        /// @param MaxEstimatedExecutionTime In seconds. No max estimated execution time if 0 (default).
        virtual bool RunNext(double MaxEstimatedExecutionTime = 0) = 0;
        /// @brief Checks whether something is available to run via RunNext.
        /// @param MaxEstimatedExecutionTime In seconds. No max estimated execution time if 0 (default).
        virtual bool IsRunAvailable(double MaxEstimatedExecutionTime = 0) = 0;
        /// @brief Waits until something is available to run via RunNext or nothing is available to run anymore.
        ///        May give false positive (return when there is nothing to run).
        ///        Should not block (keep waiting) when there's nothing left to run and IsDone returns true.
        /// @param MaxEstimatedExecutionTime In seconds. No max estimated execution time if 0 (default).
        /// @param MaxWaitingTime Maximum time to wait in seconds. No max time if 0 (default).
        virtual void WaitForRunAvailability(double MaxEstimatedExecutionTime = 0, double MaxWaitingTime = 0) = 0;
        /// @brief Checks whether something is available to run via RunNext or IsDone returns true.
        /// @param MaxEstimatedExecutionTime In seconds. No max estimated execution time if 0 (default).
        virtual bool IsAvailable(double MaxEstimatedExecutionTime = 0) = 0;
        /// @brief Waits until something is available to run via RunNext or IsDone returns true.
        ///        May give false positive (return when there is nothing to run).
        /// @param MaxEstimatedExecutionTime In seconds. No max estimated execution time if 0 (default).
        /// @param MaxWaitingTime Maximum time to wait in seconds. No max time if 0 (default).
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
        /// @brief Returns the higher predicted execution time of the whole group in seconds.
        ///
        /// Thread-safe
        virtual double PredictHigherExecutionTime() = 0;
        /// @brief Returns the lower predicted execution time of the whole group in seconds.
        ///
        /// Thread-safe
        virtual double PredictLowerExecutionTime() = 0;
        /// @brief Returns the group's parent group.
        Group * GetParent();
        /// @brief Returns the group's group members.
        std::vector<std::weak_ptr<Group>> GetMemberGroups();
        Loop * GetLoop();
    protected:
        /// @brief Has to be called once in the derived class's constructor.
        ///
        /// Members list change should not be allowed.
        /// Throws an exception if there's a problem.
        /// Call before initializing or revert and rethrow.
        void IntroduceMembers(std::vector<std::shared_ptr<Group>>);
        /// @return Whether it was successful. Should revert the changes if not successful.
        ///
        /// Handles setting the loop for members of other types than Group.
        virtual bool UpdateLoop(Loop*) = 0;
    private:
        /// @brief Accessed by Loop.
        ///
        /// Sets loop recursively.
        ///
        /// @return Whether it was successful.
        bool SetLoop(Loop * LoopPtr);
        std::shared_mutex SharedMutex;
        /// @brief Cannot have 2 parents, only be able to set when parent is destructed.
        ///        Could check for loops using this,
        ///        but with constructor only Members initialization there shouldn't be a loop.
        ///
        /// Note: With each group only having 1 parent,
        /// it's reliable to predict remaining execution times using the child groups.
        /// Also having multiple children of the same group is possible.
        Group * Parent;
        Loop * LoopPtr;
        std::vector<std::shared_ptr<Group>> MemberGroups;
        std::vector<std::weak_ptr<Group>> WeakMemberGroups;
    };
}
