#include "Loop.h"

#include <thread>
#include <vector>

#include "Group.h"

namespace LoopScheduler
{
    Loop::Loop(std::shared_ptr<Group> Architecture) : Architecture(Architecture), _IsRunning(false), ShouldStop(false)
    {
    }

    Loop::~Loop()
    {
        std::unique_lock<std::mutex> guard(Mutex);
        if (_IsRunning)
        {
            guard.unlock();
            Stop();
        }
    }

    void Loop::Start(int threads_count)
    {
        if (threads_count < 1)
            threads_count = std::thread::hardware_concurrency();

        std::unique_lock<std::mutex> guard(Mutex);
        if (_IsRunning)
        {
            guard.unlock();
            throw std::logic_error("Cannot start twice.");
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
