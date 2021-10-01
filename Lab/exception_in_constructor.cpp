// clang++ exception_in_constructor.cpp -o Build/exception_in_constructor && ./Build/exception_in_constructor

#include <exception>
#include <iostream>

class Test
{
public:
    Test()
    {
        throw std::exception();
    }
    ~Test()
    {
        // Not called if an exception has been thrown in the constructor.
        std::cout << "Destructor called.\n";
    }
};

int main()
{
    try
    {
        Test t;
    }
    catch (const std::exception& e) {}
    return 0;
}
