#pragma once

#include "LoopScheduler.dec.h"
#include "Group.h"

namespace LoopScheduler
{
    /// @brief Holds modules as members too.
    ///
    /// Module members must only be specified on construction.
    /// Derived classes have to call IntroduceMemberModules in the constructor.
    /// Members list change should not be allowed.
    class ModuleHoldingGroup : public Group
    {
    public:
        virtual ~ModuleHoldingGroup();
    protected:
        /// @brief Has to be called once in the derived class's constructor.
        ///
        /// Members list change should not be allowed.
        void IntroduceMemberModules(std::vector<std::shared_ptr<Module>>);
    private:
        std::vector<std::shared_ptr<Module>> MemberModules;
    };
}
