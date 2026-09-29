#include <exception>
#include <iostream>

#include "mini_test.hpp"

int main() {
    int failed = 0;
    for (const auto& c : minitest::registry()) {
        try {
            c.fn();
            std::cout << "[ OK ] " << c.name << "\n";
        } catch (const minitest::Failure& f) {
            ++failed;
            std::cout << "[FAIL] " << c.name << "\n" << f.message << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "[FAIL] " << c.name << "\n  unexpected exception: " << e.what() << "\n";
        }
    }
    const auto total = minitest::registry().size();
    std::cout << "\n" << (total - failed) << "/" << total << " tests passed\n";
    return failed == 0 ? 0 : 1;
}
