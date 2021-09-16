// clang++ increment_guard.cpp -o Build/increment_guard && ./Build/increment_guard

#include <iostream>

class IncrementGuard
{
public:
    IncrementGuard(int& num) : num(num)
    {
        num++;
    }
    ~IncrementGuard()
    {
        num--;
    }
private:
    int& num;
};

int main()
{
    int a = 0;
    {
        IncrementGuard g(a);
        std::cout << a << '\n';
    }
    std::cout << a << '\n';
    return 0;
}
