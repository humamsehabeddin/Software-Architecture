#pragma once
// A tiny, dependency-free test harness (no GoogleTest needed).

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace minitest {

struct Failure {
    std::string message;
};

struct Case {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

template <typename T>
std::string show(const T& v) {
    std::ostringstream os;
    os << v;
    return os.str();
}
inline std::string show(const std::string& s) { return "\"" + s + "\""; }
template <typename T>
std::string show(const std::vector<T>& v) {
    std::string out = "{";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) out += ", ";
        out += show(v[i]);
    }
    return out + "}";
}

template <typename A, typename B>
void checkEq(const A& a, const B& b, const char* ea, const char* eb, const char* file, int line) {
    if (!(a == b)) {
        std::ostringstream os;
        os << "  " << file << ":" << line << ": CHECK_EQ(" << ea << ", " << eb
           << ") failed\n    left:  " << show(a) << "\n    right: " << show(b);
        throw Failure{os.str()};
    }
}

}  // namespace minitest

#define TEST(name)                                                        \
    static void name();                                                   \
    static const minitest::Registrar registrar_##name(#name, &name);      \
    static void name()

#define CHECK(cond)                                                                        \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            throw minitest::Failure{std::string("  ") + __FILE__ + ":" +                  \
                                    std::to_string(__LINE__) + ": CHECK(" #cond ") failed"}; \
        }                                                                                  \
    } while (0)

#define CHECK_EQ(a, b) minitest::checkEq((a), (b), #a, #b, __FILE__, __LINE__)

#define CHECK_THROWS(expr, ExType)                                                          \
    do {                                                                                    \
        bool caught_ = false;                                                               \
        try {                                                                               \
            expr;                                                                           \
        } catch (const ExType&) {                                                           \
            caught_ = true;                                                                 \
        } catch (...) {                                                                     \
        }                                                                                   \
        if (!caught_) {                                                                     \
            throw minitest::Failure{std::string("  ") + __FILE__ + ":" +                    \
                                    std::to_string(__LINE__) + ": expected " #ExType};      \
        }                                                                                   \
    } while (0)
