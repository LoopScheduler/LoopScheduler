#include "ParallelGroupMember.h"

namespace LoopScheduler
{
    ParallelGroupMember::ParallelGroupMember(
            std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>> Member,
            bool CanRunMoreThanOncePreCycle
        ) : Member(Member),
            CanRunMoreThanOncePreCycle(CanRunMoreThanOncePreCycle)
    {}
}
