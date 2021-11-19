// clang++ ../LoopScheduler/*.cpp sequential_progressive_test.cpp -o Build/sequential_progressive_test --std=c++17 -pthread && ./Build/sequential_progressive_test
// Evaluates running only single-threaded modules.

#include "../LoopScheduler/LoopScheduler.h"

#include <chrono>
#include <iostream>
#include <thread>

void WorkUnit()
{
    for (int i = 0; i < 100; i++);
}

void Work(int WorkAmount)
{
    for (int i = 0; i < WorkAmount; i++)
        WorkUnit();
}

class WorkingModule : public LoopScheduler::Module
{
public:
    WorkingModule(int WorkAmount);
protected:
    virtual void OnRun() override;
    int WorkAmount;
};

WorkingModule::WorkingModule(int WorkAmount)
    : WorkAmount(WorkAmount)
{}

void WorkingModule::OnRun()
{
    Work(WorkAmount);
}

class StopperWorkingModule : public LoopScheduler::Module
{
public:
    StopperWorkingModule(int WorkAmount, int IterationsCountLimit);
protected:
    virtual void OnRun() override;
    int WorkAmount;
    int IterationsCount;
    int IterationsCountLimit;
};

StopperWorkingModule::StopperWorkingModule(int WorkAmount, int IterationsCountLimit)
    : WorkAmount(WorkAmount), IterationsCount(0), IterationsCountLimit(IterationsCountLimit)
{}

void StopperWorkingModule::OnRun()
{
    IterationsCount++;
    Work(WorkAmount);
    if (IterationsCount >= IterationsCountLimit) // Will stop after this iteration
        GetLoop()->Stop();
}

int main()
{
    int count;
    int loop_threads_count;
    int work_amount;
    int work_amount_step;
    int total_work_amount;
    int test_repeats;
    int test_module_repeats;
    std::cout << "Enter the Loop's threads count: ";
    std::cin >> loop_threads_count;
    std::cout << "Enter the number of modules: ";
    std::cin >> count;
    std::cout << "Enter the starting work amount for modules on each iteration: ";
    std::cin >> work_amount;
    std::cout << "Enter the step for work amount changes for modules on each iteration: ";
    std::cin >> work_amount_step;
    std::cout << "Enter the total work amount for a single module to calculate the number of iterations on each test: ";
    std::cin >> total_work_amount;
    std::cout << "Enter the number of test repeats, work amount will be updated on each repeat: ";
    std::cin >> test_repeats;
    std::cout << "Enter the number of repeats for the test module used to estimate the work amount time: ";
    std::cin >> test_module_repeats;

    if (count < 1)
    {
        std::cout << "Modules count can't be 0 or less.\n";
        return 0;
    }

    std::cout << "\nwork_amount,"
              << "iterations_count,"
              << "avg_work_amount_time,"
              << "loopscheduler_time,simple_loop_time,"
              << "efficiency,"
              << "loopscheduler_iterations_per_second,simple_loop_iterations_per_second\n";

    for (int repeat_number = 0; repeat_number < test_repeats; repeat_number++)
    {
        int iterations_count = total_work_amount / work_amount;

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
                    new WorkingModule(work_amount)
                )
            );
        }
        LoopScheduler::Loop loop(std::shared_ptr<LoopScheduler::Group>(new LoopScheduler::SequentialGroup(members)));

        auto start = std::chrono::steady_clock::now();
        loop.Run(loop_threads_count);
        auto stop = std::chrono::steady_clock::now();

        std::chrono::duration<double> loop_scheduler_duration = stop - start;

        // Test simple loop

        start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations_count; i++)
        {
            for (int j = 0; j < count; j++)
            {
                Work(work_amount);
            }
        }
        stop = std::chrono::steady_clock::now();

        std::chrono::duration<double> simple_loop_duration = stop - start;

        // Estimate work amount time

        double test_module_time_sum = 0;
        for (int i = 0; i < test_module_repeats; i++)
        {
            start = std::chrono::steady_clock::now();
            Work(work_amount);
            stop = std::chrono::steady_clock::now();
            std::chrono::duration<double> duration = stop - start;
            test_module_time_sum += duration.count();
        }
        double test_module_time_avg = test_module_time_sum / test_module_repeats;

        std::cout << work_amount << ',' // work_amount
                  << iterations_count << ',' // iterations_count
                  << test_module_time_avg << ',' // avg_work_amount_time
                  << loop_scheduler_duration.count() << ',' // loopscheduler_time
                  << simple_loop_duration.count() << ',' // simple_loop_time
                  << simple_loop_duration.count() / loop_scheduler_duration.count() << ',' // efficiency
                  << iterations_count / loop_scheduler_duration.count() << ',' // loopscheduler_iterations_per_second
                  << iterations_count / simple_loop_duration.count() << '\n'; // simple_loop_iterations_per_second

        work_amount += work_amount_step;
    }

    return 0;
}
