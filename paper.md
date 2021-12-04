---
title: 'LoopScheduler: A customizable and extensible framework for game loop architecture'
tags:
  - C++
  - game
  - loop
  - game engine
  - software architecture
authors:
  - name: Majidzadeh
    orcid: 0000-0002-4951-0250
    affiliation: 1
affiliations:
 - name: Independent Researcher
   index: 1
date: 22 October 2021
bibliography: paper.bib
---

# Summary

There have been game loop patterns beyond simple flow of input, update and
render loops. One issue with these simple loops is that they don't fully utilize
the processing power. One of the approaches to address this is to uncouple the
updates and rendering, and also separately run game updates that need more
frequent updates more frequently, versus those that need less [@valente_2005].
The goal of this work is to utilize CPU usage and make the loop architecture
customizable by breaking different parts of the loop to modules and running them
in a multi-threaded loop. A limited number of threads are used to run all the
tasks, because the order of execution in a game loop iteration is not important.
Using a limited number of threads has the benefit of reducing scheduling
overhead. Also, modules that run in a game loop are usually predictable,
therefore timespan predictors are used for scheduling.

# Statement of need

LoopScheduler is a general and flexible framework to apply a loop architecture,
that is, how modules get executed in a loop. The benefit of this framework is
that the scheduling of the tasks in the loop is not hard coded while using the
CPU efficiently. One of the problems that most games have is that modules run by
the CPU are bound to GPU tasks, this can introduce an input or feedback latency,
or audio problems on a system without a high-end GPU. To utilize the CPU more,
more tasks can be done in the spare time while the GPU task or any other time
consuming task is working, which improves the responsiveness. The other benefit
of using LoopScheduler is that it enables customizable game engines. This
framework also takes into acount the history of timespans to predict them and
schedule more efficiently. Some game loop architectures have been proposed to
utilize the CPU and GPU [@zamith_2009], [@zamith_2008], [@joselli_2010].
However, LoopScheduler is a simple and general framework to realize suitable
loop architectures, offering simplicity, customizability and extensibility. What
architecture to use depends on the game's needs and components.

# Approach

The primary classes in LoopScheduler are `Loop` and `Group`. A `Loop` contains
a `Group` that contains other groups or other items to run.

A `Loop` contains multiple threads that run in loop. A `Loop` object has a
`Group` as its architecture. This `Group` is used by each thread to run the next
scheduled item. The scheduling is done by this `Group` and its member `Group`
objects collectively.

A `Group` has the responsibility to schedule tasks and other member groups.
There are 2 types of `Group` implemented in the framework, `SequentialGroup` and
`ParallelGroup`. These 2 Groups can contain 2 types of members, other groups and
modules. A `Module` has a virtual run method so that it can run a task per
iteration. It can also idle at times, letting others to run. This is done using
an idle method that uses the root `Group`, which is the `Loop`'s architecture,
to run the next item. `SequentialGroup` runs groups and modules in sequence,
without overlaps between its members. `ParallelGroup` runs groups and modules in
parallel and runs some specified members more times in the spare time until the
next iteration can start.

Other types of groups can be implemented by the user for other purposes. For
example, a dynamic threads group that can run the threads throughout multiple
loop iterations, or other groups with other scheduling methods could be
implemented. For simplicity, only `SequentialGroup` and `ParallelGroup` are
designed and implemented.

![An example of 7 modules running in 2 groups.\label{fig:combined_example}](Tests/Results/combined_test/test1-example-figure.png)

In \autoref{fig:combined_example} the architecture (root) `Group` is a
`SequentialGroup` that contains a module shown in gray with an S letter and a
`ParallelGroup` containing an Idler module and 5 simple modules. Idler idles for
a random timespan between 10-15ms which can represent a module that waits for
the GPU to complete its current task. The 5 simple modules do work with
different ranges of work amounts. \autoref{fig:combined_example} is based on
real data of running a `Loop` with the described configuration. We can see that
`SequentialGroup` makes use of the spare time by running other modules that take
short enough time to run, from the member `ParallelGroup`. This is enabled by
predicting the timespans for modules based on history. The timespan predictor
itself is extensible. For example, general predictors that support different
processor core frequencies can be implemented and used. For simplicity, the
currently used predictor isn't designed for such processors.

# Evaluation

LoopScheduler is evaluated in 2 configurations, one having a `SequentialGroup`
and the other having a `ParallelGroup` as the architecture. The tests are done
on a computer with Intel® Core™ i5-8250U CPU with 4 cores and 8 threads, and
16 GB DDR4 2400MHz RAM, running Linux. The evaluation code is compiled using
Clang version 12.0.1 with no optimization. The `ParallelGroup` performance
running n modules is compared to n threads calling the same function with no
synchronization between them while running, and only the final joins. The
`SequentialGroup` performance is compared to a for loop calling the same
function. The efficiency is calculated by dividing the comparing code's
execution time by LoopScheduler's time.

![ParallelGroup in 4 threads with 4 modules vs 4 threads.\label{fig:parallel_evaluation_4}](Tests/Results/parallel_evaluation/fig/test-4-slow-no-title.png)

![ParallelGroup in 8 threads with 8 modules vs 8 threads.\label{fig:parallel_evaluation_8}](Tests/Results/parallel_evaluation/fig/test-8-slow-no-title.png)

![ParallelGroup with 80 modules vs 80 threads.\label{fig:parallel_evaluation_80}](Tests/Results/parallel_evaluation/fig/test-80-0-no-title.png)

![SequentialGroup in 4 threads vs simple loop, both running 2 modules per iteration.\label{fig:sequential_evaluation_4_2}](Tests/Results/sequential_evaluation/fig/test-4-2-no-title.png)

\autoref{fig:parallel_evaluation_80} shows that the times are competitive with
many modules running in parallel. The iteration rates are close to game
framerates in this particular configuration. In other cases, the efficiencies
are below 1, mostly more than 0.99 and close to 0.995, and mostly more than
0.995 in \autoref{fig:parallel_evaluation_8}.

# References
