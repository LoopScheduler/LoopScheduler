#pragma once

#include "LoopScheduler.dec.h"
#include "Group.h"

#include <initializer_list>
#include <vector>

namespace LoopScheduler
{
    class ParallelGroup final : public Group
    {
    public:
        ParallelGroup(std::initializer_list<GroupMember>);
    protected:
        virtual std::weak_ptr<Module> GetNextModule() override;
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) override;
        virtual bool IsDone() override;
        virtual bool WaitUntilDone() override;
        virtual bool StartNextIteration() override;
    private:
        std::vector<GroupMember> Members;
    };
}
