#pragma once

#include "LoopScheduler.dec.h"

#include <memory>

namespace LoopScheduler
{
    class Group
    {
        friend Loop;
        friend Module;
    public:
        virtual ~Group();
    protected:
        virtual std::weak_ptr<Module> GetNextModule() = 0; // TODO: Remove if not used
        virtual bool RunNextModule(double MaxEstimatedExecutionTime = 0) = 0;
        virtual bool IsDone() = 0;
    };
}
