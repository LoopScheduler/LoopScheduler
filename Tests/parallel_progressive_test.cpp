// clang++ ../LoopScheduler/*.cpp parallel_progressive_test.cpp -o Build/parallel_progressive_test --std=c++17 -pthread && ./Build/parallel_progressive_test
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
public:
    virtual void OnRun() override;
protected:
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
public:
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
    int work_amount_step;
    int total_work_amount;
    int test_repeats;
    int test_module_repeats;
    std::cout << "Enter the number of threads/modules: ";
    std::cin >> count;
    std::cout << "Enter the starting work amount for threads/modules on each iteration: ";
    std::cin >> work_amount;
    std::cout << "Enter the step for work amount changes for threads/modules on each iteration: ";
    std::cin >> work_amount_step;
    std::cout << "Enter the total work amount for a single module to calculate the number of iterations on each test: ";
    std::cin >> total_work_amount;
    std::cout << "Enter the number of test repeats, work amount will be updated on each repeat: ";
    std::cin >> test_repeats;
    std::cout << "Enter the number of repeats for the test module used to estimate the work amount time: ";
    std::cin >> test_module_repeats;

    if (count < 1)
    {
        std::cout << "Threads/modules count can't be 0 or less.\n";
        return 0;
    }

    std::cout << "\nwork_amount,"
              << "iterations_count,"
              << "avg_work_amount_time,"
              << "loopscheduler_time,threads_time,"
              << "efficiency,"
              << "loopscheduler_iterations_per_second,threads_iterations_per_second\n";

    for (int repeat_number = 0; repeat_number < test_repeats; repeat_number++)
    {
        int iterations_count = total_work_amount / work_amount;

        // Test LoopScheduler

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

        // Test threads

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

        // Estimate work amount time

        auto test_module = WorkingModule(work_amount, test_module_repeats);
        double test_module_time_sum = 0;
        for (int i = 0; i < test_module_repeats; i++)
        {
            start = std::chrono::steady_clock::now();
            test_module.OnRun();
            stop = std::chrono::steady_clock::now();
            std::chrono::duration<double> duration = stop - start;
            test_module_time_sum += duration.count();
        }
        double test_module_time_avg = test_module_time_sum / test_module_repeats;

        std::cout << work_amount << ',' // work_amount
                  << iterations_count << ',' // iterations_count
                  << test_module_time_avg << ',' // avg_work_amount_time
                  << loop_scheduler_duration.count() << ',' // loopscheduler_time
                  << threads_duration.count() << ',' // threads_time
                  << threads_duration.count() / loop_scheduler_duration.count() << ',' // efficiency
                  << iterations_count / loop_scheduler_duration.count() << ',' // loopscheduler_iterations_per_second
                  << iterations_count / threads_duration.count() << '\n'; // threads_iterations_per_second

        work_amount += work_amount_step;
    }

    return 0;
}
