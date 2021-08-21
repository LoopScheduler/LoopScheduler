#include "GroupMember.h"

namespace LoopScheduler
{
    GroupMember::GroupMember(
        bool CanRunMoreThanOncePreCycle,
        std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>> Member
        ) : CanRunMoreThanOncePreCycle(CanRunMoreThanOncePreCycle), Member(Member)
    {}
}
