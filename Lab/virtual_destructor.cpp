// clang++ virtual_destructor.cpp -o Build/virtual_destructor && ./Build/virtual_destructor

#include <iostream>
#include <string>

class Member final
{
public:
    Member(std::string name) { this->name = name; }
    ~Member() { std::cout << name << " is destructed.\n"; }
private:
    std::string name;
};

class A
{
public:
    A() : member("A's member") {}
    virtual ~A() = default; // Has to have a body or =default
    virtual void func() = 0;
private:
    Member member;
};

class B : public A
{
public:
    B() : member("B's member") {}
    virtual void func() {}
private:
    Member member;
};

int main()
{
    std::cout << "A* delete:\n";
    A * a = new B();
    delete a;
    std::cout << "\nB delete:\n";
    {
        B b;
    }
    return 0;
}
