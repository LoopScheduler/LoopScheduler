// clang++ ../LoopScheduler/*.cpp parallel_test.cpp -o Build/parallel_test --std=c++17 -pthread && ./Build/parallel_test

#include <chrono>
#include <iostream>
#include <thread>

#include "../LoopScheduler/LoopScheduler.h"

class WorkingModule : public LoopScheduler::Module
{
public:
    WorkingModule(double WorkTime, double IterationsCountLimit);
protected:
    virtual void OnRun() override;
private:
    double WorkTime;
    int IterationsCount;
    int IterationsCountLimit;
};

WorkingModule::WorkingModule(double WorkTime, double IterationsCountLimit)
    : WorkTime(WorkTime), IterationsCount(0), IterationsCountLimit(IterationsCountLimit)
{}

void WorkingModule::OnRun()
{
    IterationsCount++;
    if (IterationsCount > IterationsCountLimit)
        GetLoop()->Stop();
    else
    {
        auto start = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        while (((std::chrono::duration<double>)(now - start)).count() < WorkTime)
        {
            now = std::chrono::steady_clock::now();
        }
    }
}

int main()
{
    int count;
    double time;
    int iterations_count;
    std::cout << "Enter the number of threads/modules: ";
    std::cin >> count;
    std::cout << "Enter the time in seconds for threads/modules to work on each iteration: ";
    std::cin >> time;
    std::cout << "Enter the number of iterations: ";
    std::cin >> iterations_count;
    std::vector<LoopScheduler::ParallelGroupMember> members;
    for (int i = 0; i < count; i++)
    {
        members.push_back(
            LoopScheduler::ParallelGroupMember(
                std::shared_ptr<LoopScheduler::Module>(
                    new WorkingModule(time, iterations_count)
                )
            )
        );
    }
    LoopScheduler::Loop loop(std::shared_ptr<LoopScheduler::Group>(new LoopScheduler::ParallelGroup(members)));

    auto start = std::chrono::steady_clock::now();
    loop.Run();
    auto stop = std::chrono::steady_clock::now();

    std::chrono::duration<double> loop_scheduler_duration = stop - start;

    std::cout << "LoopScheduler's time: " << loop_scheduler_duration.count() << '\n';

    start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int i = 0; i < count; i++)
    {
        threads.push_back(
            std::thread([time, iterations_count] {
                double WorkTime = time;
                int IterationsCount = 0;
                int IterationsCountLimit = iterations_count;
                while (true)
                {
                    IterationsCount++;
                    if (IterationsCount > IterationsCountLimit)
                        return;
                    else
                    {
                        auto start = std::chrono::steady_clock::now();
                        auto now = std::chrono::steady_clock::now();
                        while (((std::chrono::duration<double>)(now - start)).count() < WorkTime)
                        {
                            now = std::chrono::steady_clock::now();
                        }
                    }
                }
            })
        );
    }
    for (auto& thread : threads)
    {
        thread.join();
    }
    stop = std::chrono::steady_clock::now();

    std::chrono::duration<double> threads_duration = stop - start;

    std::cout << "Threads' time: " << threads_duration.count() << '\n';
    std::cout << "Efficiency: " << threads_duration.count() / loop_scheduler_duration.count() << '\n';

    return 0;
}
