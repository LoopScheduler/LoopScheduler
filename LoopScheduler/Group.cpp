#include "Group.h"

#include <map>
#include <utility>

#include "Module.h"

namespace LoopScheduler
{
    Group::Group() : Parent(nullptr) {}

    Group::~Group()
    {
        for (auto& member : Members)
        {
            if (std::holds_alternative<std::shared_ptr<Group>>(member))
            {
                auto& g = std::get<std::shared_ptr<Group>>(member);
                g->Parent = nullptr;
            }
            else
            {
                auto& m = std::get<std::shared_ptr<Module>>(member);
                m->Parent = nullptr;
            }
        }
    }

    Group * Group::GetParent()
    {
        return Parent;
    }

    std::vector<std::variant<std::weak_ptr<Group>, std::weak_ptr<Module>>> Group::GetMembers()
    {
        return WeakMembers;
    }

    void Group::IntroduceMembers(std::vector<std::variant<std::shared_ptr<Group>, std::shared_ptr<Module>>> Members)
    {
        if (this->Members.size() != 0)
            throw std::logic_error("Cannot introduce members more than once.");

        for (auto& member : Members)
        {
            if (std::holds_alternative<std::shared_ptr<Group>>(member))
            {
                auto& g = std::get<std::shared_ptr<Group>>(member);
                if (g->Parent != nullptr)
                    throw std::logic_error("A group cannot be a member of more than 1 groups.");
            }
            else
            {
                auto& m = std::get<std::shared_ptr<Module>>(member);
                if (m->Parent != nullptr)
                    throw std::logic_error("A module cannot be a member of more than 1 groups.");
            }
        }
        for (auto& member : Members)
        {
            if (std::holds_alternative<std::shared_ptr<Group>>(member))
            {
                auto& g = std::get<std::shared_ptr<Group>>(member);
                g->Parent = this;
                WeakMembers.push_back(std::weak_ptr<Group>(g));
            }
            else
            {
                auto& m = std::get<std::shared_ptr<Module>>(member);
                m->Parent = this;
                WeakMembers.push_back(std::weak_ptr<Module>(m));
            }
        }
        this->Members = std::move(Members);
    }
}
