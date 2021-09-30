// clang++ ../LoopScheduler/*.cpp parallel_test.cpp -o Build/parallel_test --std=c++17 -pthread && ./Build/parallel_test

#include <chrono>
#include <iostream>
#include <thread>

#include "../LoopScheduler/LoopScheduler.h"

class WorkingModule : public LoopScheduler::Module
{
public:
    /// @param WorkAmount An amount of work
    WorkingModule(int WorkAmount, double IterationsCountLimit);
protected:
    virtual void OnRun() override;
private:
    int WorkAmount;
    int IterationsCount;
    int IterationsCountLimit;
};

WorkingModule::WorkingModule(int WorkAmount, double IterationsCountLimit)
    : WorkAmount(WorkAmount), IterationsCount(0), IterationsCountLimit(IterationsCountLimit)
{}

void WorkingModule::OnRun()
{
    IterationsCount++;
    if (IterationsCount > IterationsCountLimit)
        GetLoop()->Stop();
    else
    {
        for (int i = 0; i < WorkAmount; i++)
        {
            for (int i = 0; i < 100; i++); // Work unit
        }
    }
}

int main()
{
    int count;
    int work_amount;
    int iterations_count;
    std::cout << "Enter the number of threads/modules: ";
    std::cin >> count;
    std::cout << "Enter the work amount for threads/modules on each iteration (a large number like 10000): ";
    std::cin >> work_amount;
    std::cout << "Enter the number of iterations: ";
    std::cin >> iterations_count;
    std::vector<LoopScheduler::ParallelGroupMember> members;
    for (int i = 0; i < count; i++)
    {
        members.push_back(
            LoopScheduler::ParallelGroupMember(
                std::shared_ptr<LoopScheduler::Module>(
                    new WorkingModule(work_amount, iterations_count)
                )
            )
        );
    }
    LoopScheduler::Loop loop(std::shared_ptr<LoopScheduler::Group>(new LoopScheduler::ParallelGroup(members)));

    auto start = std::chrono::steady_clock::now();
    loop.Run();
    auto stop = std::chrono::steady_clock::now();

    std::chrono::duration<double> loop_scheduler_duration = stop - start;

    std::cout << "LoopScheduler: Total time: " << loop_scheduler_duration.count() << '\n';
    std::cout << "               Approximate iterations per second: "
              << iterations_count / loop_scheduler_duration.count() << "\n\n";

    start = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int i = 0; i < count; i++)
    {
        threads.push_back(
            std::thread([work_amount, iterations_count] {
                int WorkAmount = work_amount;
                int IterationsCount = 0;
                int IterationsCountLimit = iterations_count;
                while (true)
                {
                    IterationsCount++;
                    if (IterationsCount > IterationsCountLimit)
                        return;
                    else
                    {
                        for (int i = 0; i < WorkAmount; i++)
                        {
                            for (int i = 0; i < 100; i++); // Work unit
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

    std::cout << "Threads: Total time: " << threads_duration.count() << '\n';
    std::cout << "         Approximate iterations per second: "
              << iterations_count / threads_duration.count() << "\n\n";
    std::cout << "Efficiency: " << threads_duration.count() / loop_scheduler_duration.count() << '\n';

    return 0;
}
