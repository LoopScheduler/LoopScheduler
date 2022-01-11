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

#include "Loop.h"

#include <list>
#include <map>
#include <queue>
#include <stdexcept>
#include <thread>

#include "Group.h"
#include "Module.h"

namespace LoopScheduler
{
    class boolean // false by default
    {
        public: boolean(); boolean(bool); operator bool();
        private: bool value;
    };
    boolean::boolean() : value(false) {}
    boolean::boolean(bool value) : value(value) {}
    boolean::operator bool() { return value; }

    Loop::Loop(std::shared_ptr<Group> Architecture) : Architecture(Architecture), _IsRunning(false), ShouldStop(false)
    {
        if (!Architecture->SetLoop(this))
            throw std::logic_error(
                "There was a problem in the architecture. Each group/module can only be a member of 1 group and 1 loop."
            );
    }

    Loop::~Loop()
    {
        std::unique_lock<std::mutex> guard(Mutex);
        if (_IsRunning)
        {
            guard.unlock();
            Stop();
        }
        Architecture->SetLoop(nullptr);
    }

    void Loop::Run(int threads_count)
    {
        if (threads_count < 1)
            threads_count = std::thread::hardware_concurrency();

        std::unique_lock<std::mutex> guard(Mutex);
        if (_IsRunning)
        {
            guard.unlock();
            throw std::logic_error("Cannot start running the loop twice.");
        }
        _IsRunning = true;
        ShouldStop = false;
        guard.unlock();

        auto loop = [this]
        {
            std::unique_lock<std::mutex> guard(Mutex);
            guard.unlock();
            while (true)
            {
                if (Architecture->IsDone())
                {
                    guard.lock();
                    if (Architecture->IsDone())
                    {
                        if (ShouldStop)
                            return;
                        Architecture->StartNextIteration();
                    }
                    guard.unlock();
                }

                if (!Architecture->RunNext())
                    Architecture->WaitForAvailability();
            }
        };

        std::vector<std::thread> threads;
        for (int i = 1; i < threads_count; i++)
        {
            threads.push_back(
                std::thread(loop)
            );
        }

        loop();

        for (int i = 0; i < threads.size(); i++)
            threads[i].join();

        guard.lock();
        _IsRunning = false;
        guard.unlock();
        ConditionVariable.notify_all();
    }

    void Loop::Stop()
    {
        std::unique_lock<std::mutex> guard(Mutex);
        if (_IsRunning)
            ShouldStop = true;
    }

    void Loop::StopAndWait()
    {
        std::unique_lock<std::mutex> guard(Mutex);
        if (_IsRunning)
        {
            ShouldStop = true;
            ConditionVariable.wait(guard, [this] { return !_IsRunning; });
        }
    }

    bool Loop::IsRunning()
    {
        std::unique_lock<std::mutex> guard(Mutex);
        return _IsRunning;
    }

    Group * Loop::GetArchitecture()
    {
        return Architecture.get();
    }

    std::weak_ptr<Group> Loop::GetArchitectureWeakPtr()
    {
        return std::weak_ptr<Group>(Architecture);
    }
}
