#include "Group.h"

#include <exception>
#include <list>
#include <map>
#include <mutex>
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
                std::unique_lock<std::shared_mutex> lock(g->SharedMutex);
                g->Parent = nullptr;
            }
            else
            {
                auto& m = std::get<std::shared_ptr<Module>>(member);
                std::unique_lock<std::shared_mutex> lock(m->SharedMutex);
                m->Parent = nullptr;
            }
        }
    }

    Group * Group::GetParent()
    {
        std::shared_lock<std::shared_mutex> lock(SharedMutex);
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

        bool failed = false;
        std::list<std::shared_ptr<Group>> visited_groups;
        std::list<std::shared_ptr<Module>> visited_modules;
        for (auto& member : Members)
        {
            if (std::holds_alternative<std::shared_ptr<Group>>(member))
            {
                auto& g = std::get<std::shared_ptr<Group>>(member);
                std::unique_lock<std::shared_mutex> lock(g->SharedMutex);
                if (g->Parent != nullptr && g->Parent != this)
                {
                    failed = true;
                    break;
                }
                visited_groups.push_back(g);
                g->Parent = this;
                WeakMembers.push_back(std::weak_ptr<Group>(g));
            }
            else
            {
                auto& m = std::get<std::shared_ptr<Module>>(member);
                std::unique_lock<std::shared_mutex> lock(m->SharedMutex);
                if (m->Parent != nullptr && m->Parent != this)
                {
                    failed = true;
                    break;
                }
                visited_modules.push_back(m);
                m->Parent = this;
                WeakMembers.push_back(std::weak_ptr<Module>(m));
            }
        }
        if (failed)
        {
            // Revert
            for (auto& g : visited_groups)
            {
                std::unique_lock<std::shared_mutex> lock(g->SharedMutex);
                g->Parent = nullptr;
            }
            for (auto& m : visited_modules)
            {
                std::unique_lock<std::shared_mutex> lock(m->SharedMutex);
                m->Parent = nullptr;
            }
            WeakMembers.clear();
            throw std::logic_error("A group or a module cannot be a member of more than 1 groups.");
        }
        this->Members = std::move(Members);
    }
}
