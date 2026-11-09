#include "thread_pool.hpp"

#include <chrono>
#include <iostream>
#include <string>

int main() {
    try {
        cppapplab::concurrency::ThreadPool pool(4U, 16U);
        auto first = pool.submit([] { return std::string("CppAppLab W6"); });
        auto second = pool.submit([](int left, int right) { return left + right; }, 20, 22);
        std::cout << first.get() << '\n' << second.get() << '\n';
        pool.shutdown();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
