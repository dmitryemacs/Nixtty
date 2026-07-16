#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry r;
        return r;
    }
    void add(const char* name, std::function<void()> fn) {
        tests.push_back({name, std::move(fn)});
    }
    const std::vector<TestCase>& all() const { return tests; }
private:
    std::vector<TestCase> tests;
};

struct TestRegistrar {
    TestRegistrar(const char* name, std::function<void()> fn) {
        TestRegistry::instance().add(name, std::move(fn));
    }
};

#define TEST(name) \
    static void test_##name(); \
    static TestRegistrar reg_##name(#name, test_##name); \
    static void test_##name()

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s:%d: ASSERT_TRUE(%s)\n", __FILE__, __LINE__, #expr); \
        std::abort(); \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a != _b) { \
        fprintf(stderr, "  FAIL: %s:%d: ASSERT_EQ(%s, %s) — got %d vs %d\n", \
            __FILE__, __LINE__, #a, #b, (int)_a, (int)_b); \
        std::abort(); \
    } \
} while(0)

#define ASSERT_STREQ(a, b) do { \
    if (std::string(a) != std::string(b)) { \
        fprintf(stderr, "  FAIL: %s:%d: ASSERT_STREQ(\"%s\", \"%s\")\n", \
            __FILE__, __LINE__, a, b); \
        std::abort(); \
    } \
} while(0)

#define ASSERT_FLOAT_EQ(a, b) do { \
    float _a = (a); float _b = (b); \
    if (_a != _b) { \
        fprintf(stderr, "  FAIL: %s:%d: ASSERT_FLOAT_EQ(%f, %f)\n", \
            __FILE__, __LINE__, (double)_a, (double)_b); \
        std::abort(); \
    } \
} while(0)
