// clang++ ../LoopScheduler/*.cpp combined_test.cpp -o Build/combined_test --std=c++20 -pthread && ./Build/combined_test

#include "../LoopScheduler/LoopScheduler.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

class Report
{
public:
    int ReportStart(std::string Name);
    void ReportStop(int);
    std::string GetReport();
private:
    class RunInfo
    {
    public:
        RunInfo(
            std::thread::id ThreadId,
            std::string Name,
            std::chrono::steady_clock::time_point Start,
            std::chrono::steady_clock::time_point Stop
        );
        std::thread::id ThreadId;
        std::string Name;
        std::chrono::steady_clock::time_point Start;
        std::chrono::steady_clock::time_point Stop;
    };
    std::mutex Mutex;
    std::vector<RunInfo> Runs;
};

int Report::ReportStart(std::string Name)
{
    Mutex.lock();
    int result = Runs.size();
    Runs.push_back(RunInfo(std::this_thread::get_id(), Name, std::chrono::steady_clock::now(), std::chrono::steady_clock::now()));
    Mutex.unlock();
    return result;
}
void Report::ReportStop(int i)
{
    Mutex.lock();
    Runs[i].Stop = std::chrono::steady_clock::now();
    Mutex.unlock();
}
std::string Report::GetReport()
{
    Mutex.lock();
    std::string result = "";
    std::chrono::steady_clock::time_point start;
    std::map<std::thread::id, int> thread_numbers;
    int thread_counter = 0;
    if (Runs.size() != 0)
        start = Runs[0].Start;
    for (auto& run_info : Runs)
    {
        if (!thread_numbers.contains(run_info.ThreadId))
            thread_numbers[run_info.ThreadId] = thread_counter++;
        result += std::to_string(thread_numbers[run_info.ThreadId]) + ": ";
        result += run_info.Name + ", ";
        result += std::to_string(((std::chrono::duration<double>)(run_info.Start - start)).count()) + "-";
        result += std::to_string(((std::chrono::duration<double>)(run_info.Stop - start)).count()) + '\n';
    }
    Mutex.unlock();
    return result;
}

Report::RunInfo::RunInfo(
        std::thread::id ThreadId,
        std::string Name,
        std::chrono::steady_clock::time_point Start,
        std::chrono::steady_clock::time_point Stop
    ) : ThreadId(ThreadId), Name(Name), Start(Start), Stop(Stop)
{}

class IdlingTimerModule : public LoopScheduler::Module
{
public:
    IdlingTimerModule(double TimeMin, double TimeMax, double IdlingTime, Report& ReportRef, std::string Name);
protected:
    virtual void OnRun() override;
private:
    std::random_device random_device;
    std::default_random_engine random_engine;
    std::uniform_real_distribution<double> random_distribution;
    LoopScheduler::BiasedEMATimeSpanPredictor Predictor;
    double IdlingTimeSlice;
    Report& ReportRef;
    std::string Name;
};

IdlingTimerModule::IdlingTimerModule(double TimeMin, double TimeMax, double IdlingTimeSlice, Report& ReportRef, std::string Name)
    : random_device(),
      random_engine(random_device()),
      random_distribution(TimeMin, TimeMax),
      IdlingTimeSlice(IdlingTimeSlice),
      ReportRef(ReportRef),
      Name(Name),
      Predictor(IdlingTimeSlice, 0.05, 0.5)
{
}
void IdlingTimerModule::OnRun()
{
    int report_id = ReportRef.ReportStart(Name);
    double Time = random_distribution(random_engine);
    auto start = std::chrono::steady_clock::now();
    Idle(Predictor.Predict() - IdlingTimeSlice);
    double elapsed = ((std::chrono::duration<double>)(std::chrono::steady_clock::now() - start)).count();
    while (elapsed < Time)
    {
        Idle(IdlingTimeSlice);
        elapsed = ((std::chrono::duration<double>)(std::chrono::steady_clock::now() - start)).count();
    }
    Predictor.ReportObservation(elapsed);
    ReportRef.ReportStop(report_id);
}

class StoppingModule : public LoopScheduler::Module
{
public:
    StoppingModule(int RunCountsLimit);
protected:
    virtual void OnRun() override;
private:
    int RunCounts;
    int RunCountsLimit;
};

StoppingModule::StoppingModule(int RunCountsLimit)
    : RunCountsLimit(RunCountsLimit)
{}
void StoppingModule::OnRun()
{
    RunCounts++;
    if (RunCounts >= RunCountsLimit)
        GetLoop()->Stop();
}

class WorkingModule : public LoopScheduler::Module
{
public:
    WorkingModule(int WorkAmountMin, int WorkAmountMax, Report& ReportRef, std::string Name, bool CanRunInParallel = false);
protected:
    virtual void OnRun() override;
private:
    std::random_device random_device;
    std::default_random_engine random_engine;
    std::uniform_int_distribution<int> random_distribution;
    Report& ReportRef;
    std::string Name;
};

WorkingModule::WorkingModule(int WorkAmountMin, int WorkAmountMax, Report& ReportRef, std::string Name, bool CanRunInParallel)
    : random_device(),
      random_engine(random_device()),
      random_distribution(WorkAmountMin, WorkAmountMax),
      ReportRef(ReportRef),
      Name(Name),
      Module(CanRunInParallel)
{
}
void WorkingModule::OnRun()
{
    int report_id = ReportRef.ReportStart(Name);
    int WorkAmount = random_distribution(random_engine);
    for (int i = 0; i < WorkAmount; i++)
    {
        for (int i = 0; i < 100; i++); // Work unit
    }
    ReportRef.ReportStop(report_id);
}

void test1()
{
    Report report;

    std::vector<LoopScheduler::ParallelGroupMember> parallel_members;
    parallel_members.push_back(LoopScheduler::ParallelGroupMember(std::make_shared<IdlingTimerModule>(0.01, 0.015, 0.005, report, "Idler")));
    parallel_members.push_back(LoopScheduler::ParallelGroupMember(
        std::make_shared<WorkingModule>(100000, 150000, report, "Worker1"), 1
    ));
    parallel_members.push_back(LoopScheduler::ParallelGroupMember(
        std::make_shared<WorkingModule>(50000, 100000, report, "Worker2"), 1
    ));
    parallel_members.push_back(LoopScheduler::ParallelGroupMember(
        std::make_shared<WorkingModule>(10000, 20000, report, "Worker3"), 1
    ));
    parallel_members.push_back(LoopScheduler::ParallelGroupMember(
        std::make_shared<WorkingModule>(10000, 20000, report, "Worker4"), 1
    ));
    parallel_members.push_back(LoopScheduler::ParallelGroupMember(
        std::make_shared<WorkingModule>(10000, 20000, report, "Worker5"), 1
    ));
    parallel_members.push_back(LoopScheduler::ParallelGroupMember(std::make_shared<StoppingModule>(100)));

    std::shared_ptr<LoopScheduler::ParallelGroup> parallel_group(new LoopScheduler::ParallelGroup(parallel_members));

    std::vector<LoopScheduler::SequentialGroupMember> sequential_members;
    sequential_members.push_back(parallel_group);
    sequential_members.push_back(std::make_shared<WorkingModule>(100000, 150000, report, "Single-threaded worker"));

    std::shared_ptr<LoopScheduler::SequentialGroup> sequential_group(new LoopScheduler::SequentialGroup(sequential_members));

    LoopScheduler::Loop loop(sequential_group);
    loop.Run(4);

    std::cout << report.GetReport();
}

void test2()
{
    Report report;
    std::vector<LoopScheduler::ParallelGroupMember> parallel_members;
    parallel_members.push_back(LoopScheduler::ParallelGroupMember(
        std::make_shared<WorkingModule>(100000, 150000, report, "Worker")
    ));
    try
    {
        std::shared_ptr<LoopScheduler::ParallelGroup> parallel_group(new LoopScheduler::ParallelGroup(parallel_members));
        std::cout << "Test 1-1 passed.\n";
    }
    catch (std::exception& e)
    {
        std::cout << "Test 1-1 failed. Exception message: ";
        std::cout << e.what() << '\n';
    }
    std::shared_ptr<LoopScheduler::ParallelGroup> parallel_group1;
    try
    {
        parallel_group1 = std::shared_ptr<LoopScheduler::ParallelGroup>(new LoopScheduler::ParallelGroup(parallel_members));
        std::cout << "Test 1-2 passed.\n";
    }
    catch (std::exception& e)
    {
        std::cout << "Test 1-2 failed. Exception message: ";
        std::cout << e.what() << '\n';
    }
    try
    {
        std::shared_ptr<LoopScheduler::ParallelGroup> parallel_group(new LoopScheduler::ParallelGroup(parallel_members));
        std::cout << "Test 1-3 faild.\n";
    }
    catch(std::logic_error& e)
    {
        if (e.what() == std::string("A module cannot be a member of more than 1 groups."))
            std::cout << "Test 1-3 passed.\n";
        else
        {
            std::cout << "Test 1-3 failed. Logic error message: ";
            std::cout << e.what() << '\n';
        }
    }
    catch (std::exception& e)
    {
        std::cout << "Test 1-3 failed. Exception message: ";
        std::cout << e.what() << '\n';
    }

    std::vector<LoopScheduler::SequentialGroupMember> sequential_members;
    sequential_members.push_back(
        std::make_shared<WorkingModule>(100000, 150000, report, "Worker")
    );
    try
    {
        std::shared_ptr<LoopScheduler::SequentialGroup> sequential_group(new LoopScheduler::SequentialGroup(sequential_members));
        std::cout << "Test 2-1 passed.\n";
    }
    catch (std::exception& e)
    {
        std::cout << "Test 2-1 failed. Exception message: ";
        std::cout << e.what() << '\n';
    }
    std::shared_ptr<LoopScheduler::SequentialGroup> sequential_group1;
    try
    {
        sequential_group1 = std::shared_ptr<LoopScheduler::SequentialGroup>(new LoopScheduler::SequentialGroup(sequential_members));
        std::cout << "Test 2-2 passed.\n";
    }
    catch (std::exception& e)
    {
        std::cout << "Test 2-2 failed. Exception message: ";
        std::cout << e.what() << '\n';
    }
    try
    {
        std::shared_ptr<LoopScheduler::SequentialGroup> sequential_group(new LoopScheduler::SequentialGroup(sequential_members));
        std::cout << "Test 2-3 faild.\n";
    }
    catch(std::logic_error& e)
    {
        if (e.what() == std::string("A module cannot be a member of more than 1 groups."))
            std::cout << "Test 2-3 passed.\n";
        else
        {
            std::cout << "Test 2-3 failed. Logic error message: ";
            std::cout << e.what() << '\n';
        }
    }
    catch (std::exception& e)
    {
        std::cout << "Test 2-3 failed. Exception message: ";
        std::cout << e.what() << '\n';
    }
}

std::string prompt_name(std::map<std::string, std::shared_ptr<LoopScheduler::Group>>& groups)
{
    std::string name;
    while (true)
    {
        std::cin >> name;
        if (groups.contains(name))
            std::cout << "Another group already has this name. Try another name: ";
        else if (name == "stopper" || name == "worker" || name == "aworker" || name == "idler" || name == "done")
            std::cout << "This name is reserved. Try another name: ";
        else
            break;
    }
    return name;
}
std::variant<std::variant<std::shared_ptr<LoopScheduler::Group>, std::shared_ptr<LoopScheduler::Module>>, int> prompt_member(
        Report& report,
        std::map<std::string, std::shared_ptr<LoopScheduler::Group>>& groups
    )
{
    while (true)
    {
        std::string input;
        std::cout << "Enter one of the following words to add that type of module:\n";
        std::cout << "  stopper: StoppingModule\n";
        std::cout << "  worker: WorkingModule\n";
        std::cout << "  aworker: WorkingModule with CanRunInParallel=true\n";
        std::cout << "  idler: IdlingTimerModule\n";
        std::cout << "Or enter a group name to include that as a member, 'done' to stop: ";
        std::cin >> input;
        if (input == "done")
        {
            return 0;
        }
        if (input == "stopper")
        {
            int count;
            std::cout << "Enter the run count limit for the StoppingModule: ";
            std::cin >> count;
            return std::make_shared<StoppingModule>(count);
        }
        if (input == "worker" || input == "aworker")
        {
            int min_work;
            int max_work;
            std::string name;
            std::cout << "Enter a name for this module. This name will appear in the report: ";
            std::cin >> name;
            std::cout << "Enter the minimum work amount for the WorkingModule: ";
            std::cin >> min_work;
            std::cout << "Enter the maximum work amount for the WorkingModule: ";
            std::cin >> max_work;
            bool can_run_in_parallel = input[0] == 'a';
            return std::make_shared<WorkingModule>(min_work, max_work, report, name, can_run_in_parallel);
        }
        if (input == "idler")
        {
            double min_time;
            double max_time;
            double idling_time_slice;
            std::string name;
            std::cout << "Enter a name for this module. This name will appear in the report: ";
            std::cin >> name;
            std::cout << "Enter the minimum time in seconds for the IdlingTimerModule: ";
            std::cin >> min_time;
            std::cout << "Enter the maximum time in seconds for the IdlingTimerModule: ";
            std::cin >> max_time;
            std::cout << "This module tries to predict the time and idle for the lower predicted time.\n";
            std::cout << "After that, it idles at time slices and checks whether it's done waiting.\n";
            std::cout << "Enter the idling time slice in seconds for the IdlingTimerModule: ";
            std::cin >> idling_time_slice;
            return std::make_shared<IdlingTimerModule>(min_time, max_time, idling_time_slice, report, name);
        }
        if (groups.contains(input))
        {
            return groups[input];
        }
        std::cout << "Invalid input.\n";
    }
}
void test_custom()
{
    Report report;

    std::string input;
    std::map<std::string, std::shared_ptr<LoopScheduler::Group>> groups;
    while (true)
    {
        std::cout << "Enter 'parallel' to create a ParallelGroup, or 'sequential' to create a SequentialGroup, 'done' to stop: ";
        std::cin >> input;
        if (input == "parallel")
        {
            std::cout << "Enter a name for the new ParallelGroup: ";
            std::string name = prompt_name(groups);
            std::cout << "Adding members to " << name << ". Do not forget to add 1 StoppingModule to the loop.\n";
            std::vector<LoopScheduler::ParallelGroupMember> parallel_members;
            while (true)
            {
                auto member = prompt_member(report, groups);
                if (std::holds_alternative<int>(member))
                    break;
                int run_shares_after_first_run;
                std::cout << "Enter the extra run shares after the first run for this member (0: once per iteration): ";
                std::cin >> run_shares_after_first_run;
                parallel_members.push_back(LoopScheduler::ParallelGroupMember(std::get<0>(member), run_shares_after_first_run));
            }
            groups[name] = std::shared_ptr<LoopScheduler::ParallelGroup>(new LoopScheduler::ParallelGroup(parallel_members));
        }
        else if (input == "sequential")
        {
            std::cout << "Enter a name for the new SequentialGroup: ";
            std::string name = prompt_name(groups);
            std::cout << "Adding members to " << name << ". Do not forget to add 1 StoppingModule to the loop.\n";
            std::vector<LoopScheduler::SequentialGroupMember> sequential_members;
            while (true)
            {
                auto member = prompt_member(report, groups);
                if (std::holds_alternative<int>(member))
                    break;
                sequential_members.push_back(LoopScheduler::SequentialGroupMember(std::get<0>(member)));
            }
            groups[name] = std::shared_ptr<LoopScheduler::SequentialGroup>(new LoopScheduler::SequentialGroup(sequential_members));
        }
        else if (input == "done")
        {
            break;
        }
        else
        {
            std::cout << "Invalid input.\n";
        }
    }
    if (groups.size() == 0)
    {
        std::cout << "No groups were created.\n";
        return;
    }

    while (true)
    {
        std::cout << "Enter the name of the group to use as the architecture (root): ";
        std::cin >> input;
        if (groups.contains(input))
            break;
        std::cout << "A group with that name is not defined.\n";
    }
    LoopScheduler::Loop loop(groups[input]);
    std::cout << "Enter the number of threads for the loop: ";
    int threads_count;
    std::cin >> threads_count;
    std::cout << "\nRunning...\n";
    loop.Run(threads_count);

    std::cout << report.GetReport();
}

int main()
{
    std::cout << "1: Run test1. A test to showcase some features.\n";
    std::cout << "2: Run test2. Tests whether adding 1 module to 2 groups throws an exception.\n";
    std::cout << "c: Create and run a custom test.\n";
    std::cout << "Enter 1, 2, or c: ";
    std::string input;
    std::cin >> input;
    if (input == "1")
        test1();
    else if (input == "2")
        test2();
    else if (input == "c")
        test_custom();
    return 0;
}
