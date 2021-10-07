// clang++ map_access.cpp -o Build/map_access && ./Build/map_access

#include <iostream>
#include <map>

class A
{
public:
    int a;
    A() : a(0) {}
};

int main()
{
    std::map<int, A> a_map;
    auto& temp = a_map[0];
    std::cout << a_map[0].a << '\n';
    temp.a = 2;
    std::cout << a_map[0].a << '\n';
    return 0;
}
