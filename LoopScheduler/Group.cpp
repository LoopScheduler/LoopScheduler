#include "Group.h"

#include <exception>
#include <list>
#include <map>
#include <mutex>
#include <utility>

#include "Module.h"

namespace LoopScheduler
{
    Group::Group() : Parent(nullptr), LoopPtr(nullptr) {}

    Group::~Group()
    {
        for (auto& member : MemberGroups)
        {
            std::unique_lock<std::shared_mutex> lock(member->SharedMutex);
            if (member->Parent == this)
                member->Parent = nullptr;
        }
    }

    Group * Group::GetParent()
    {
        std::shared_lock<std::shared_mutex> lock(SharedMutex);
        return Parent;
    }

    std::vector<std::weak_ptr<Group>> Group::GetMemberGroups()
    {
        return WeakMemberGroups;
    }

    Loop * Group::GetLoop()
    {
        std::shared_lock<std::shared_mutex> lock(SharedMutex);
        return LoopPtr;
    }

    void Group::IntroduceMemberGroups(std::vector<std::shared_ptr<Group>> MemberGroups)
    {
        if (this->MemberGroups.size() != 0)
            throw std::logic_error("Cannot introduce members more than once.");

        bool failed = false;
        std::list<std::shared_ptr<Group>> visited_groups;
        for (auto& member : MemberGroups)
        {
            std::unique_lock<std::shared_mutex> lock(member->SharedMutex);
            if (member->Parent != nullptr && member->Parent != this)
            {
                failed = true;
                break;
            }
            visited_groups.push_back(member);
            member->Parent = this;
            WeakMemberGroups.push_back(std::weak_ptr<Group>(member));
        }
        if (failed)
        {
            // Revert
            for (auto& g : visited_groups)
            {
                std::unique_lock<std::shared_mutex> lock(g->SharedMutex);
                g->Parent = nullptr;
            }
            WeakMemberGroups.clear();
            throw std::logic_error("A group cannot be a member of more than 1 groups.");
        }
        this->MemberGroups = std::move(MemberGroups);
    }

    bool Group::SetLoop(Loop * LoopPtr)
    {
        std::unique_lock<std::shared_mutex> lock(SharedMutex);
        if (this->LoopPtr != nullptr && LoopPtr != nullptr)
            return false;
        auto temp = this->LoopPtr;
        this->LoopPtr = LoopPtr;
        for (int i = 0; i < MemberGroups.size(); i++)
        {
            if (!MemberGroups[i]->SetLoop(LoopPtr))
            {
                for (int j = 0; j < i; j++)
                    MemberGroups[i]->SetLoop(nullptr); // Revert
                this->LoopPtr = temp; // Revert
                return false;
            }
        }
        if (!UpdateLoop(LoopPtr))
        {
            this->LoopPtr = temp; // Revert
            return false;
        }
        return true;
    }
}
