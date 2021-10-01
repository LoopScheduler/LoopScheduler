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
    class SequentialGroup final : public ModuleHoldingGroup
    {
    public:
        SequentialGroup(std::vector<SequentialGroupMember>);
    protected:
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) override;
        virtual bool IsAvailable(double MaxEstimatedExecutionTime = 0) override;
        virtual void WaitForAvailability(double MaxEstimatedExecutionTime = 0, double MaxWaitingTime = 0) override;
        virtual bool IsDone() override;
        virtual void StartNextIteration() override;
        virtual double PredictHigherRemainingExecutionTime() override;
        virtual double PredictLowerRemainingExecutionTime() override;
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
        template <bool Higher>
        inline double PredictRemainingExecutionTimeNoLock();
    };
}
