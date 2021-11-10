// clang++ ../LoopScheduler/*.cpp sequential_progressive_test.cpp -o Build/sequential_progressive_test --std=c++17 -pthread && ./Build/sequential_progressive_test
// Evaluates running only single-threaded modules.

#include "../LoopScheduler/LoopScheduler.h"

#include <chrono>
#include <iostream>
#include <thread>

class WorkingModule : public LoopScheduler::Module
{
public:
    /// @param WorkAmount An amount of work
    WorkingModule(int WorkAmount, int IterationsCountLimit);
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
    int loop_threads_count;
    int work_amount;
    int work_amount_step;
    int iterations_count;
    int test_repeats;
    int test_module_repeats;
    std::cout << "Enter the Loop's threads count: ";
    std::cin >> loop_threads_count;
    std::cout << "Enter the number of modules: ";
    std::cin >> count;
    std::cout << "Enter the starting work amount for modules on each iteration (a large number like 10000): ";
    std::cin >> work_amount;
    std::cout << "Enter the step for work amount changes for modules on each iteration: ";
    std::cin >> work_amount_step;
    std::cout << "Enter the number of iterations: ";
    std::cin >> iterations_count;
    std::cout << "Enter the number of test repeats, work amount will be updated on each repeat: ";
    std::cin >> test_repeats;
    std::cout << "Enter the number of repeats for the test module used to estimate the work amount time: ";
    std::cin >> test_module_repeats;

    if (count < 1)
    {
        std::cout << "Modules count can't be 0 or less.\n";
        return 0;
    }

    std::cout << "\nwork_amount,avg_work_amount_time,efficiency,loopscheduler_iterations_per_second,simple_loop_iterations_per_second\n";

    for (int repeat_number = 0; repeat_number < test_repeats; repeat_number++)
    {
        // Test LoopScheduler

        std::vector<LoopScheduler::SequentialGroupMember> members;
        members.push_back(
            std::shared_ptr<LoopScheduler::Module>(
                new StopperWorkingModule(work_amount, iterations_count)
            )
        );
        for (int i = 1; i < count; i++)
        {
            members.push_back(
                std::shared_ptr<LoopScheduler::Module>(
                    new WorkingModule(work_amount, iterations_count)
                )
            );
        }
        LoopScheduler::Loop loop(std::shared_ptr<LoopScheduler::Group>(new LoopScheduler::SequentialGroup(members)));

        auto start = std::chrono::steady_clock::now();
        loop.Run(loop_threads_count);
        auto stop = std::chrono::steady_clock::now();

        std::chrono::duration<double> loop_scheduler_duration = stop - start;

        // Test simple loop

        std::vector<std::shared_ptr<WorkingModule>> modules;
        for (int i = 0; i < count; i++)
        {
            modules.push_back(
                std::shared_ptr<WorkingModule>(
                    new WorkingModule(work_amount, iterations_count)
                )
            );
        }

        start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations_count; i++)
        {
            for (int j = 0; j < count; j++)
            {
                modules[j]->OnRun();
            }
        }
        stop = std::chrono::steady_clock::now();

        std::chrono::duration<double> simple_loop_duration = stop - start;

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
                  << test_module_time_avg << ',' // avg_work_amount_time
                  << simple_loop_duration.count() / loop_scheduler_duration.count() << ',' // efficiency
                  << iterations_count / loop_scheduler_duration.count() << ',' // loopscheduler_iterations_per_second
                  << iterations_count / simple_loop_duration.count() << '\n'; // simple_loop_iterations_per_second

        work_amount += work_amount_step;
    }

    return 0;
}
