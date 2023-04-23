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
    class ParallelGroup : public ModuleHoldingGroup
    {
    public:
        /// @param ExtendIterationForAdditionalGroupRuns Whether a member group with RunSharesAfterFirstRun > 0
        ///                                              can start a new iteration and thus extend the time for
        ///                                              IsDone to be true again
        ///                                              for this group to be able to start a new iteration,
        ///                                              ONLY when MaxEstimatedExecutionTime is provided
        ///                                              through RunNext(...).
        ///                                              Use this parameter carefully, it could cause problems.
        ///                                              Setting this to true can be useful when this group is
        ///                                              a member of a SequentialGroup.
        ///                                              When false, a new iteration won't be started
        ///                                              on additional member group runs.
        /// @param HigherExecutionTimePredictor Predictor to predict the higher execution time of the whole group.
        ///                                     nullptr to use default.
        /// @param LowerExecutionTimePredictor Predictor to predict the lower execution time of the whole group.
        ///                                    nullptr to use default.
        ParallelGroup(
            std::vector<ParallelGroupMember> Members,
            bool ExtendIterationForAdditionalGroupRuns = false,
            std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor = nullptr,
            std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor = nullptr
        );
        virtual bool RunNext(double MaxEstimatedExecutionTime = 0) override;
        virtual bool IsRunAvailable(double MaxEstimatedExecutionTime = 0) override;
        virtual void WaitForRunAvailability(double MaxEstimatedExecutionTime = 0, double MaxWaitingTime = 0) override;
        virtual bool IsAvailable(double MaxEstimatedExecutionTime = 0) override;
        virtual void WaitForAvailability(double MaxEstimatedExecutionTime = 0, double MaxWaitingTime = 0) override;
        virtual bool IsDone() override;
        virtual void StartNextIteration() override;
        virtual double PredictHigherRemainingExecutionTime() override;
        virtual double PredictLowerRemainingExecutionTime() override;
        virtual double PredictHigherExecutionTime() override;
        virtual double PredictLowerExecutionTime() override;
    protected:
        virtual bool UpdateLoop(Loop*) override;
    private:
        /// @brief A shared mutex for class members.
        std::shared_mutex MembersSharedMutex;

        std::vector<ParallelGroupMember> Members;
        std::vector<std::shared_ptr<Group>> GroupMembers;
        std::list<int> MainQueue;
        std::list<int> SecondaryQueue;
        bool ExtendIterationForAdditionalGroupRuns;
        int RunningThreadsCount;
        int NotifyingCounter;
        int RunNextCount;

        /// Is set to true on measurement start,
        /// and set to false after the measurement.
        /// Measurement starts on the first RunNext(...) call after StartNextIteration() is called.
        bool MeasuringTimespan;
        /// Only set when MeasuringTimespan is false, MeasuringTimespan is set to true when this is set.
        std::chrono::steady_clock::time_point IterationStartTime;

        std::unique_ptr<TimeSpanPredictor> HigherExecutionTimePredictor;
        std::unique_ptr<TimeSpanPredictor> LowerExecutionTimePredictor;

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

        /// Must be locked BEFORE MembersSharedMutex lock
        /// when modifying members before NextEventConditionVariable.notify_all().
        std::mutex NextEventConditionMutex;
        std::condition_variable NextEventConditionVariable;

        inline bool RunModule(
            std::shared_ptr<Module>&,
            std::unique_lock<std::shared_mutex>&,
            std::list<int>&,
            std::list<int>&,
            std::list<int>::iterator&,
            int
        );
        inline bool RunGroup(std::shared_ptr<Group>&, std::unique_lock<std::shared_mutex>&, double MaxEstimatedExecutionTime);

        /// Should be placed in RunNext's start.
        /// NO MUTEX LOCK
        inline void TimespanMeasurementStart();
        /// Should be placed after each direct or indirect MainQueue.erase(...)
        /// and before unlocking the mutex.
        /// NO MUTEX LOCK
        inline void TimespanMeasurementStop();

        /// NO MUTEX LOCK
        inline bool IsRunAvailableNoLock(double MaxEstimatedExecutionTime);
        /// LOCKS MUTEX
        template <bool RunAvailability>
        inline void WaitForAvailabilityTemplate(double MaxEstimatedExecutionTime, double MaxWaitingTime);
        /// NO SUBGROUP CALL
        /// NO MUTEX LOCK
        inline void StartNextIterationForThisGroup();
    };
}
