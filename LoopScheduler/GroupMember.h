#pragma once

#include "LoopScheduler.dec.h"

#include <memory>
#include <variant>

namespace LoopScheduler
{
    struct GroupMember final
    {
    public:
        GroupMember(bool CanRunMoreThanOncePreCycle, std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>> Member);
        bool CanRunMoreThanOncePreCycle;
        std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>> Member;
    };
}
