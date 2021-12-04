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
#include "ModuleHoldingGroup.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <variant>
#include <vector>

namespace LoopScheduler
{
    typedef std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>> SequentialGroupMember;
    /// @brief A group that runs the subgroups and modules only once per iteration, in order and without overlapping.
    ///
    /// Each stage is done when the subgroup or module of that stage is done.
    /// When a stage is done, the next stage starts ignoring whether the next stage's module can run.
    /// The module or subgroup of a stage won't run in parallel with other stages.
    /// A stage is defined as a member of a vector using the constructor.
    class SequentialGroup : public ModuleHoldingGroup
    {
    public:
        SequentialGroup(std::vector<SequentialGroupMember>);
        virtual bool RunNext(double MaxEstimatedExecutionTime = 0) override;
        virtual bool IsRunAvailable(double MaxEstimatedExecutionTime = 0) override;
        virtual void WaitForRunAvailability(double MaxEstimatedExecutionTime = 0, double MaxWaitingTime = 0) override;
        virtual bool IsAvailable(double MaxEstimatedExecutionTime = 0) override;
        virtual void WaitForAvailability(double MaxEstimatedExecutionTime = 0, double MaxWaitingTime = 0) override;
        virtual bool IsDone() override;
        virtual void StartNextIteration() override;
        virtual double PredictHigherRemainingExecutionTime() override;
        virtual double PredictLowerRemainingExecutionTime() override;
    protected:
        virtual bool UpdateLoop(Loop*) override;
    private:
        /// @brief A shared mutex for class members.
        std::shared_mutex MembersSharedMutex;

        std::vector<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>> Members;
        std::vector<std::shared_ptr<Group>> GroupMembers;
        /// @brief Can only be in range [-1, Members.size() - 1] (Only { -1 } if Members.size() = 0)
        int CurrentMemberIndex;
        int CurrentMemberRunsCount;
        int RunningThreadsCount;

        std::chrono::steady_clock::time_point LastModuleStartTime;
        double LastModuleHigherPredictedTimeSpan;
        double LastModuleLowerPredictedTimeSpan;

        std::mutex NextEventConditionMutex;
        std::condition_variable NextEventConditionVariable;

        /// @brief ShouldRunNextModuleFromCurrentMemberIndex
        ///        && ShouldTryRunNextGroupFromCurrentMemberIndex
        ///        && ShouldIncrementCurrentMemberIndex
        ///        = false
        ///
        /// NO MUTEX LOCK
        inline bool ShouldRunNextModuleFromCurrentMemberIndex(double MaxEstimatedExecutionTime);
        /// @brief ShouldRunNextModuleFromCurrentMemberIndex
        ///        && ShouldTryRunNextGroupFromCurrentMemberIndex
        ///        && ShouldIncrementCurrentMemberIndex
        ///        = false
        ///
        /// NO MUTEX LOCK
        ///
        /// @param InputMaxEstimatedExecutionTime The given MaxEstimatedExecutionTime.
        /// @param OutputMaxEstimatedExecutionTime The value that should be used instead. This is set only when the return value is true.
        inline bool ShouldTryRunNextGroupFromCurrentMemberIndex(double InputMaxEstimatedExecutionTime, double& OutputMaxEstimatedExecutionTime);
        /// @brief ShouldRunNextModuleFromCurrentMemberIndex
        ///        && ShouldTryRunNextGroupFromCurrentMemberIndex
        ///        && ShouldIncrementCurrentMemberIndex
        ///        = false
        ///
        /// NO MUTEX LOCK
        inline bool ShouldIncrementCurrentMemberIndex();
        /// NO MUTEX LOCK
        inline bool IsRunAvailableNoLock(double MaxEstimatedExecutionTime);
        /// LOCKS MUTEX
        inline void WaitForAvailabilityCommon(double MaxEstimatedExecutionTime, double MaxWaitingTime);
        /// NO MUTEX LOCK
        template <bool Higher>
        inline double PredictRemainingExecutionTimeNoLock();
    };
}
