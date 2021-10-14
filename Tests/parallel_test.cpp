// clang++ ../LoopScheduler/*.cpp parallel_test.cpp -o Build/parallel_test --std=c++17 -pthread && ./Build/parallel_test
// Evaluates running only parallel modules.

#include "../LoopScheduler/LoopScheduler.h"

#include <chrono>
#include <iostream>
#include <thread>

class WorkingModule : public LoopScheduler::Module
{
public:
    /// @param WorkAmount An amount of work
    WorkingModule(int WorkAmount, int IterationsCountLimit);
protected:
    virtual void OnRun() override;
    int WorkAmount;
    int IterationsCount;
    int IterationsCountLimit;
};

WorkingModule::WorkingModule(int WorkAmount, int IterationsCountLimit)
    : WorkAmount(WorkAmount), IterationsCount(0), IterationsCountLimit(IterationsCountLimit)
{}

void WorkingModule::OnRun()
{
    IterationsCount++;
    for (int i = 0; i < WorkAmount; i++)
    {
        for (int i = 0; i < 100; i++); // Work unit
    }
    if (IterationsCount >= IterationsCountLimit) // Dummy
        return;
}

class StopperWorkingModule : public WorkingModule
{
public:
    StopperWorkingModule(int WorkAmount, int IterationsCountLimit);
protected:
    virtual void OnRun() override;
};

StopperWorkingModule::StopperWorkingModule(int WorkAmount, int IterationsCountLimit)
    : WorkingModule(WorkAmount, IterationsCountLimit)
{}

void StopperWorkingModule::OnRun()
{
    IterationsCount++;
    for (int i = 0; i < WorkAmount; i++)
    {
        for (int i = 0; i < 100; i++); // Work unit
    }
    if (IterationsCount >= IterationsCountLimit) // Will stop after this iteration
        GetLoop()->Stop();
}

int main()
{
    int count;
    int work_amount;
    int iterations_count;
    int test_repeats;
    std::cout << "Enter the number of threads/modules: ";
    std::cin >> count;
    std::cout << "Enter the work amount for threads/modules on each iteration (a large number like 10000): ";
    std::cin >> work_amount;
    std::cout << "Enter the number of iterations: ";
    std::cin >> iterations_count;
    std::cout << "Enter the number of test repeats: ";
    std::cin >> test_repeats;

    if (count < 1)
    {
        std::cout << "Threads/modules count can't be 0 or less.\n";
        return 0;
    }

    for (int repeat_number = 0; repeat_number < test_repeats; repeat_number++)
    {
        std::cout << "\nTest " << repeat_number << ":\n\n";

        std::vector<LoopScheduler::ParallelGroupMember> members;
        members.push_back(
            LoopScheduler::ParallelGroupMember(
                std::shared_ptr<LoopScheduler::Module>(
                    new StopperWorkingModule(work_amount, iterations_count)
                )
            )
        );
        for (int i = 1; i < count; i++)
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
        loop.Run(count < std::thread::hardware_concurrency() ? count : std::thread::hardware_concurrency());
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
                        for (int i = 0; i < WorkAmount; i++)
                        {
                            for (int i = 0; i < 100; i++); // Work unit
                        }
                        if (IterationsCount >= IterationsCountLimit)
                            return;
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
    }

    return 0;
}
