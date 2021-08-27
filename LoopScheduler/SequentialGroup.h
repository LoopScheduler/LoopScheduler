#pragma once

#include "LoopScheduler.dec.h"
#include "Group.h"

#include <vector>

namespace LoopScheduler
{
    class SequentialGroup final : public Group
    {
    public:
        SequentialGroup(std::vector<GroupMember>);
    protected:
        virtual std::weak_ptr<Module> GetNextModule() override;
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) override;
        virtual bool IsDone() override;
        virtual void WaitForNextModule() override;
        virtual void StartNextIteration() override;
    private:
        std::vector<GroupMember> Members;
    };
}
