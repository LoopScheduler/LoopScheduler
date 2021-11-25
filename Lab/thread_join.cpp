// clang++ thread_join.cpp -o Build/thread_join -pthread && ./Build/thread_join

#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

int main()
{
    std::thread thread([] { });
    std::cout << "Started the thread.\n";
    std::cout << "Sleeping for 1 second...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "thread.joinable(): " << thread.joinable() << '\n';
    std::cout << "Joining...\n";
    thread.join();
    std::cout << "thread.joinable(): " << thread.joinable() << '\n';
    try
    {
        std::cout << "Joining again...\n";
        thread.join();
    }
    catch (const std::exception& e)
    {
        std::cout << "Exception: " << e.what() << '\n';
    }
}
