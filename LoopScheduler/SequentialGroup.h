#pragma once

#include "LoopScheduler.dec.h"
#include "Group.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <variant>
#include <vector>

namespace LoopScheduler
{
    class SequentialGroup final : public Group
    {
    public:
        SequentialGroup(std::vector<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>>);
    protected:
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) override;
        virtual void WaitForNextEvent(double MaxEstimatedExecutionTime = 0, double MaxWaitingTime = 0) override;
        virtual bool IsDone() override;
        virtual void StartNextIteration() override;
        virtual double PredictHigherRemainingExecutionTime() override;
        virtual double PredictLowerRemainingExecutionTime() override;
    private:
        std::shared_mutex MembersSharedMutex;

        std::vector<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>> Members;
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
    template<> double SequentialGroup::PredictRemainingExecutionTimeNoLock<true>();
    template<> double SequentialGroup::PredictRemainingExecutionTimeNoLock<false>();
}
