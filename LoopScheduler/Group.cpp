// Copyright (c) 2021 Majidzadeh (hashpragmaonce@gmail.com)
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Group.h"

#include <list>
#include <map>
#include <mutex>
#include <stdexcept>
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

    void Group::IntroduceMembers(std::vector<std::shared_ptr<Group>> MemberGroups)
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
