# LoopScheduler

A customizable and extensible framework for game loop architecture.

# Structure

- **[./LoopScheduler](https://github.com/LoopScheduler/LoopScheduler/tree/main/LoopScheduler):** LoopScheduler source files
- **[./Lab](https://github.com/LoopScheduler/LoopScheduler/tree/main/Lab):** Code to test C++ features
- **[./Tests](https://github.com/LoopScheduler/LoopScheduler/tree/main/Tests):** Test and evaluation code
- **[./Tests/Results](https://github.com/LoopScheduler/LoopScheduler/tree/main/Tests/Results):** Test and evaluation results
- **[./Tests/combined_test_inputs](https://github.com/LoopScheduler/LoopScheduler/tree/main/Tests/combined_test_inputs):** Some test inputs for combined_test

# Introduction

LoopScheduler is a simple framework to create desired loop architectures for games.
The main classes in this project are:

  - Loop
  - Group

The main concepts are described here, more details are available in the code documentation.

## Loop

A Loop object contains a Group as its root architecture.
This Group is used to schedule the next task.
The Loop object, runs loops in different threads and runs tasks from the root Group.

## Group

Group is an abstract class.
A Group object, may contain other Groups or objects that can run tasks.
This class has the responsibility of scheduling its own members/tasks, while its Group members schedule their own members/tasks.
Simply, a Group decides what to run next.
Different types of Groups can be implemented.
There are 2 simple Groups implemented in this project:

  - ParallelGroup
  - SequentialGroup

Both of these groups can have Group members and members of Module type.

### Module

The main methods in this class are:

  - GetRunningToken(): Used by Groups to run the module.
  - OnRun(): A virtual method that has to be implemented by the derived class.
  - HandleException(...): A virtual method to handle exceptions that is optional to implement.
  - Idle(...): Used by the module itself to idle and let the other tasks run in the meanwhile.

Each Module object has 2 TimeSpanPredictor objects to predict its higher and lower timespans.
The default predictors can be replaced with other predictors using the Module's constructor.
Also, whether a Module can run in parallel (to itself) can be set using the constructor.
For example:

```
class MyModule : LoopScheduler::Module
{
    MyModule() : LoopScheduler::Module(
                    CAN_MY_MODULE_RUN_IN_PARALLEL,
                    MyHigherTimeSpanPredictor(),
                    MyLowerTimeSpanPredictor()
                )
    {...}

    virtual void OnRun() override {...}
}
```

### ParallelGroup

Runs its members in parallel.
Some specified members can run more than once per iteration while some tasks take longer to finish.

### SequentialGroup

Runs its members sequentially.
A member cannot start its tasks until the previous member finishes its jobs.
A single Group member is allowed to run its own members in parallel.

# Build

CMake can be used to build the tests.
There are also clang++ commands at the start of the executable source files
inside [./Tests](https://github.com/LoopScheduler/LoopScheduler/tree/main/Tests)
and [./Lab](https://github.com/LoopScheduler/LoopScheduler/tree/main/Lab) directories.

To build using CMake on Linux:

  1. Install cmake, make and a C++ compiler using the package manager.
  2. Choose/create a build folder and navigate to there in terminal.
  3. Enter `cmake <project-path> && make` (replace `<project-path>` with the project's root directory where CMakeLists.txt exists).
     The test executables will be in the Tests folder where the commands were executed.

To build using CMake on Windows:

  1. Download and install CMake from: https://cmake.org/download/
  2. Generate the project for an IDE and use the supported IDE to build.

# Get started

Here are some examples to get an idea of how to used LoopScheduler.

## Include

To use LoopScheduler, include: [./LoopScheduler/LoopScheduler.h](https://github.com/LoopScheduler/LoopScheduler/blob/main/LoopScheduler/LoopScheduler.h)

## Implementing a Module

```
class MyModule : LoopScheduler::Module
{
    virtual void OnRun() override
    {
        // Idle for about 0.1s
        Idle(0.1);
        // Do anything
    }
}
```

## Setting up a Loop with a ParallelGroup

```
void SetupAndRun()
{
    // Prepare a ParallelGroup

    // Prepare its members:
    std::vector<LoopScheduler::ParallelGroupMember> parallel_members;
    // Create the first member:
    auto member1 = LoopScheduler::ParallelGroupMember(
        std::make_shared<MyModule>()
    );
    // Add to the vector:
    parallel_members.push_back(member1);
    // Prepare the second member, as one that can more than once per iteration (with 1 run share per extra iteration):
    auto member2 = LoopScheduler::ParallelGroupMember(
        std::make_shared<MyModule>(), 1
    );
    parallel_members.push_back(member2);
    // Create the ParallelGroup:
    std::shared_ptr<LoopScheduler::ParallelGroup> parallel_group(new LoopScheduler::ParallelGroup(parallel_members));
    // Alternatively:
    auto parallel_group_alt = std::make_shared<LoopScheduler::ParallelGroup>(parallel_members);

    // Prepare the loop
    LoopScheduler::Loop loop(parallel_group);

    // Run the loop (blocks until stopped)
    loop.Run();
}
```

# Test

The 2 evaluate executables are used to evaluate the performance.
To test the behavior, use combined_test to run one of the 2 pre-defined tests or create and run a custom test.
combined_test offers 3 options initially:

  1. Test 1: A pre-defined test used as an example of how LoopScheduler works.
  2. Test 2: Tests whether adding 1 module to 2 groups throws an exception.
  3. Custom test: Allows to configure and run a custom defined loop.
     [./Tests/combined_test_inputs](https://github.com/LoopScheduler/LoopScheduler/tree/main/Tests/combined_test_inputs) contains some examples.

The test results are manually verified except the pre-defined test2.
Some test inputs are available in [./Tests/combined_test_inputs](https://github.com/LoopScheduler/LoopScheduler/tree/main/Tests/combined_test_inputs).
To try them, copy the lines under "Input:" and paste them into the terminal after running combined_test.
