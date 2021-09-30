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
        /// @param RunSharesAfterFirstRun The number of runs for each additional iteration after the first run.
        ///                               0 Means it can only run once per loop iteration.
        ///
        /// This can be set to more than 1 for those members that need to run more often than others after the first run.
        ParallelGroupMember(
            std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>> Member,
            int RunSharesAfterFirstRun = 0
        );
        std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>> Member;
        int RunSharesAfterFirstRun;
    };
}
