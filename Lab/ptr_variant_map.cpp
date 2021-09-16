// clang++ ptr_variant_map.cpp -o Build/ptr_variant_map --std=c++17 && ./Build/ptr_variant_map

#include <iostream>
#include <map>
#include <memory>
#include <variant>

class A { int a; };
class B { int b; };

class integer
{
    public: integer() : value(0) {}
    integer(int value) : value(value) {}
    operator int() { return value; }
    private: int value;
};

int main()
{
    std::map<std::variant<std::shared_ptr<A>, std::shared_ptr<B>>, integer> m;

    auto a1 = std::make_shared<A>();
    auto a2 = std::make_shared<A>();
    auto b1 = std::make_shared<B>();
    auto b2 = std::make_shared<B>();
    auto a1_other = a1;
    auto a2_other = a2;
    auto b1_other = b1;
    auto b2_other = b2;

    m[a1] = 1;
    m[a2] = 2;
    m[b1] = 3;
    m[b2] = 4;

    std::cout << "Values for original ptrs:\n";

    std::cout << m[a1] << ' ';
    std::cout << m[a2] << ' ';
    std::cout << m[b1] << ' ';
    std::cout << m[b2] << '\n';

    std::cout << "Values for other ptrs:\n";

    std::cout << m[a1_other] << ' ';
    std::cout << m[a2_other] << ' ';
    std::cout << m[b1_other] << ' ';
    std::cout << m[b2_other] << '\n';

    return 0;
}
