#pragma once

#include "LoopScheduler.dec.h"
#include "Group.h"

#include <chrono>
#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <shared_mutex>
#include <tuple>
#include <vector>

#include "ParallelGroupMember.h"

namespace LoopScheduler
{
    /// @brief A group that runs the sub-groups and modules in parallel, but in order.
    ///
    /// Some members can be run more than once.
    /// This can be specified in ParallelGroupMember when constructing and object of this class.
    /// When IsDone() returns true, members might be still running.
    /// That means when this group is in the root,
    /// sometimes the last modules of an iteration might be running
    /// at the same time as the first modules from the next iteration.
    class ParallelGroup final : public Group
    {
    public:
        ParallelGroup(std::vector<ParallelGroupMember>);
    protected:
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) override;
        virtual void WaitForNextEvent(double MaxEstimatedExecutionTime = 0, double MaxWaitingTime = 0) override;
        virtual bool IsDone() override;
        virtual void StartNextIteration() override;
        virtual double PredictHigherRemainingExecutionTime() override;
        virtual double PredictLowerRemainingExecutionTime() override;
    private:
        /// @brief A shared mutex for class members.
        std::shared_mutex MembersSharedMutex;

        std::vector<ParallelGroupMember> Members;
        std::vector<std::shared_ptr<Group>> GroupMembers;
        std::list<int> MainQueue;
        std::list<int> SecondaryQueue;
        /// @brief Used in WaitForNextEvent to notify itself.
        ///
        /// Each thread should wait for a member and finally notify and remove itself from the map.
        /// New threads will be created only if they are terminated before (not contained in the map).
        std::map<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>,
                 std::unique_ptr<std::thread>> WaitingAndNotifyingThreads;

        class integer // 0 by default
        {
            public: integer(); integer(int); operator int(); int value;
        };

        class ModuleRunCountAndPredictedStopTimes
        {
        public:
            integer RunCount;
            std::chrono::steady_clock::time_point StartTime;
            double HigherPredictedTimeSpan;
            double LowerPredictedTimeSpan;
        };

        std::map<std::shared_ptr<Module>, ModuleRunCountAndPredictedStopTimes> ModulesRunCountsAndPredictedStopTimes;
        std::map<std::shared_ptr<Group>, integer> GroupsRunCounts;

        std::mutex NextEventConditionMutex;
        std::condition_variable NextEventConditionVariable;

        inline bool RunModule(std::shared_ptr<Module>&, std::unique_lock<std::shared_mutex>&);
        inline bool RunGroup(std::shared_ptr<Group>&, std::unique_lock<std::shared_mutex>&);

        inline void StartNextIterationForThisGroup();
    };
}
