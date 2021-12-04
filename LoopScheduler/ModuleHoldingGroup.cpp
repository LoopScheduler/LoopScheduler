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
