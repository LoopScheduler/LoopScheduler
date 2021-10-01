// clang++ ../LoopScheduler/*.cpp combined_test.cpp -o Build/combined_test --std=c++20 -pthread && ./Build/combined_test

#include "../LoopScheduler/LoopScheduler.h"

#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

class Report
{
public:
    int ReportStart(std::thread::id ThreadId, std::string Name);
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

int Report::ReportStart(std::thread::id ThreadId, std::string Name)
{
    Mutex.lock();
    int result = Runs.size();
    Runs.push_back(RunInfo(ThreadId, Name, std::chrono::steady_clock::now(), std::chrono::steady_clock::now()));
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
    std::string result;
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
        result += std::to_string(((std::chrono::duration<double>)(run_info.Stop - start)).count());
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
    IdlingTimerModule(double TimeMin, double TimeMax, double IdlingTime);
protected:
    virtual void OnRun() override;
private:
    std::random_device random_device;
    std::default_random_engine random_engine;
    std::uniform_real_distribution<double> random_distribution;
    double IdlingTime;
};

IdlingTimerModule::IdlingTimerModule(double TimeMin, double TimeMax, double IdlingTime)
    : random_device(),
      random_engine(random_device()),
      random_distribution(TimeMin, TimeMax),
      IdlingTime(IdlingTime)
{
}
void IdlingTimerModule::OnRun()
{
    double Time = random_distribution(random_engine);
    auto start = std::chrono::steady_clock::now();
    while (((std::chrono::duration<double>)(std::chrono::steady_clock::now() - start)).count() < Time)
    {
        Idle(IdlingTime);
    }
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
    WorkingModule(int WorkAmountMin, int WorkAmountMax);
protected:
    virtual void OnRun() override;
private:
    std::random_device random_device;
    std::default_random_engine random_engine;
    std::uniform_int_distribution<int> random_distribution;
};

WorkingModule::WorkingModule(int WorkAmountMin, int WorkAmountMax)
    : random_device(),
      random_engine(random_device()),
      random_distribution(WorkAmountMin, WorkAmountMax)
{
}
void WorkingModule::OnRun()
{
    int WorkAmount = random_distribution(random_engine);
    for (int i = 0; i < WorkAmount; i++)
    {
        for (int i = 0; i < 100; i++); // Work unit
    }
}

void test1()
{
    //
}

int main()
{
    test1();
    return 0;
}
