#pragma once

#include "LoopScheduler.dec.h"
#include "Group.h"

#include <map>
#include <vector>

#include "ParallelGroupMember.h"

namespace LoopScheduler
{
    class ParallelGroup final : public Group
    {
    public:
        ParallelGroup(std::vector<ParallelGroupMember>);
    protected:
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) override;
        virtual double PredictRemainingHigherExecutionTime() override;
        virtual double PredictRemainingLowerExecutionTime() override;
        virtual void WaitForNextEvent(double MaxEstimatedExecutionTime = 0) override;
        virtual bool IsDone() override;
        virtual void StartNextIteration() override;
    private:
        /// @brief 0 by default
        class integer
        {
            public: integer(); integer(int); operator int();
            private: int value;
        };

        std::vector<ParallelGroupMember> Members;
        std::vector<ParallelGroupMember> MembersThatCanRunMore;
        int CurrentMemberIndex;
        int CurrentExtraMemberIndex;
        std::map<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>, integer> RunCounts;
    };
}
