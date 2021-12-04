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
