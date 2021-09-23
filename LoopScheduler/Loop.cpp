#include "Loop.h"

#include <list>
#include <map>
#include <queue>
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
        std::map<std::weak_ptr<Group>, boolean> visited;
        std::queue<std::weak_ptr<Group>> queue;
        queue.push(std::weak_ptr<Group>(Architecture));
        while (!queue.empty())
        {
            auto next = queue.front().lock();
            queue.pop();
            for (auto& member : next->GetMembers())
            {
                if (std::holds_alternative<std::weak_ptr<Group>>(member))
                {
                    auto& g = std::get<std::weak_ptr<Group>>(member);
                    if (!visited[g])
                    {
                        queue.push(g);
                        visited[g] = true;
                    }
                }
                else
                {
                    Modules.push_back(std::get<std::weak_ptr<Module>>(member).lock());
                }
            }
        }
        bool failed = false;
        std::list<std::shared_ptr<Module>> visited_modules;
        for (auto& m : Modules)
        {
            std::unique_lock<std::shared_mutex> lock(m->SharedMutex);
            if (m->Loop != nullptr && m->Loop != this)
            {
                failed = true;
                break;
            }
            m->Loop = this;
        }
        if (failed)
        {
            // Revert
            for (auto& m : visited_modules)
            {
                std::unique_lock<std::shared_mutex> lock(m->SharedMutex);
                m->Loop = nullptr;
            }
            Architecture = nullptr;
            Modules.clear();
            throw std::logic_error("A module cannot be in more than 1 loop.");
        }
    }

    Loop::~Loop()
    {
        std::unique_lock<std::mutex> guard(Mutex);
        if (_IsRunning)
        {
            guard.unlock();
            Stop();
        }
        for (auto& m : Modules)
        {
            std::unique_lock<std::shared_mutex> lock(m->SharedMutex);
            m->Loop = nullptr;
        }
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
        else
        {
            _IsRunning = true;
            guard.unlock();
        }

        std::vector<std::thread> threads;
        for (int i = 0; i < threads_count; i++)
        {
            threads.push_back(
                std::thread([i, this]
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

                        if (!Architecture->RunNextModule())
                            Architecture->WaitForAvailability();
                    }
                })
            );
        }

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
        {
            ShouldStop = true;
            ConditionVariable.wait(guard, [this] { return !_IsRunning; });
            guard.unlock();
        }
    }

    bool Loop::IsRunning()
    {
        std::unique_lock<std::mutex> guard(Mutex);
        return _IsRunning;
    }
}
