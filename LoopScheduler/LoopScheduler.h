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

/// @file LoopScheduler.h
/// @brief Includes all LoopScheduler classes.
///        This is the header to include to use LoopScheduler.

#ifndef LOOPSCHEDULER_USE_SMART_CV_WAITER
    #define LOOPSCHEDULER_USE_SMART_CV_WAITER 1
#endif

#pragma once

#include "Loop.h"
#include "Group.h"
#include "ModuleHoldingGroup.h"
#include "SequentialGroup.h"
#include "ParallelGroup.h"
#include "ParallelGroupMember.h"
#include "Module.h"
#include "TimeSpanPredictor.h"
#include "BiasedEMATimeSpanPredictor.h"
#include "SmartCVWaiter.h"
