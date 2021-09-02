#pragma once

#include "LoopScheduler.dec.h"
#include "Group.h"

#include <vector>

namespace LoopScheduler
{
    class ParallelGroup final : public Group
    {
    public:
        ParallelGroup(std::vector<ParallelGroupMember>);
    protected:
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) override;
        virtual void WaitForNextEvent() override;
        virtual bool IsDone() override;
        virtual void StartNextIteration() override;
    private:
        std::vector<ParallelGroupMember> Members;
    };
}
