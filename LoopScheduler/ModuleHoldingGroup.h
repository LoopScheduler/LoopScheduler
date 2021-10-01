#pragma once

#include "LoopScheduler.dec.h"
#include "Group.h"

namespace LoopScheduler
{
    /// @brief Holds modules as members too.
    ///
    /// Module members must only be specified on construction.
    /// Derived classes have to call IntroduceMembers in the constructor.
    /// Members list change should not be allowed.
    class ModuleHoldingGroup : public Group
    {
    public:
        virtual ~ModuleHoldingGroup();
    protected:
        /// @brief Has to be called once in the derived class's constructor.
        ///
        /// Members list change should not be allowed.
        /// Throws an exception if there's a problem.
        /// Call before initializing or revert and rethrow.
        void IntroduceMembers(std::vector<std::shared_ptr<Group>>, std::vector<std::shared_ptr<Module>>);
    private:
        std::vector<std::shared_ptr<Module>> MemberModules;
    };
}
