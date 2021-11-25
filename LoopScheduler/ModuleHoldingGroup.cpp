#include "ModuleHoldingGroup.h"

#include <stdexcept>
#include <utility>

#include "Module.h"

namespace LoopScheduler
{
    ModuleHoldingGroup::~ModuleHoldingGroup()
    {
        for (int i = 0; i < MemberModules.size(); i++)
        {
            if (MemberModules[i]->GetParent() == this)
                MemberModules[i]->SetParent(nullptr);
        }
    }

    void ModuleHoldingGroup::IntroduceMembers(
            std::vector<std::shared_ptr<Group>> MemberGroups,
            std::vector<std::shared_ptr<Module>> MemberModules
        )
    {
        if (this->MemberModules.size() != 0)
            throw std::logic_error("Cannot introduce members more than once.");

        for (int i = 0; i < MemberModules.size(); i++)
        {
            if (!MemberModules[i]->SetParent(this))
            {
                for (int j = 0; j < i; j++)
                    MemberModules[j]->SetParent(nullptr); // Revert
                throw std::logic_error("A module cannot be a member of more than 1 groups.");
            }
        }

        try
        {
            Group::IntroduceMembers(std::move(MemberGroups));
        }
        catch (const std::logic_error& e)
        {
            for (int i = 0; i < MemberModules.size(); i++)
                MemberModules[i]->SetParent(nullptr);
            throw e;
        }

        this->MemberModules = std::move(MemberModules);
    }
}
