// clang++ condition_variable_wait.cpp -o Build/condition_variable_wait && ./Build/condition_variable_wait

#include <condition_variable>
#include <iostream>
#include <mutex>

int main()
{
    std::mutex m;
    std::condition_variable cv;
    std::unique_lock<std::mutex> l(m);
    cv.wait(l);
    std::cout << "1\n";
    int c = 0;
    cv.wait(l, [&c] { if (c == 0) return false; else return true; });
    std::cout << "1\n";
    return 0;
}
