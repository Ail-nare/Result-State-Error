#include "rse.hpp"
#include <iostream>
using namespace std::string_literals;

Rse<int> fn(int i) {
    if (i % 3 == 0)
        return "string"s;
    return i;
}

Rse<int&, int> fn2(int& i) {
    if (i % 3 == 0)
        return RSE::Error(i);
    return i;
}


Rse<int, std::exception> fn3(int i) {
    if (i % 3 == 0)
        return RSE::Error<std::runtime_error>(":D");
    return i;
}

int main() {

    // sizeof(fn3(i)) == 8
    std::cout << "sizeof(fn(i)) == " << sizeof(fn(0)) << std::endl;
    for (int i = 0; i < 10; i++) {
        // auto, auto&& and const auto& all works
        auto [value, error] = fn(i);

        if (error) {
            std::cerr << "Error: " << *error << std::endl;
        } else {
            std::cout << value << std::endl;
        }
    }

    // sizeof(fn3(i)) == 16
    std::cout << std::endl << "sizeof(fn2(i)) == " << sizeof(fn2(std::declval<int&>())) << std::endl;
    for (int i = 0; i < 10; i++) {
        // auto, auto&& and const auto& all works
        auto [value, error] = fn2(i);

        if (error) {
            std::cerr << "Error: " << *error << std::endl;
        } else {
            std::cout << &i << ":" << &value << ":" << value << std::endl;
        }
    }

    // sizeof(fn3(i)) == 8
    std::cout << std::endl << "sizeof(fn3(i)) == " << sizeof(fn3(0)) << std::endl;
    for (int i = 0; i < 10; i++) {
        // auto, auto&& and const auto& all works
        auto [value, error] = fn3(i);

        if (error) {
            std::cerr << "Error: " << error->what() << std::endl;
        } else {
            std::cout << value << std::endl;
        }
    }
}
