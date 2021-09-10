#pragma once

#include "LoopScheduler.dec.h"

#include <memory>
#include <variant>

namespace LoopScheduler
{
    /// @brief Represents a group member of either another group or a module.
    ///        Used by ParallelGroup.
    struct ParallelGroupMember final
    {
    public:
        /// @param CanRunMoreThanOncePreCycle Whether the module can run more than once in a single loop iteration.
        /// @param CanRunInParallel Whether the module can run in another thread while it's already running.
        ParallelGroupMember(
            std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>> Member,
            bool CanRunMoreThanOncePreCycle = false
        );
        std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>> Member;
        bool CanRunMoreThanOncePreCycle;
    };
}
