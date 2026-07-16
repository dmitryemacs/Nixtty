#include "test.h"
#include <cstdio>

int main() {
    auto& tests = TestRegistry::instance().all();
    int passed = 0;
    int failed = 0;
    int total = (int)tests.size();

    printf("Running %d tests...\n\n", total);

    for (auto& t : tests) {
        printf("  %-50s", t.name);
        fflush(stdout);
        t.fn();
        printf("OK\n");
        passed++;
    }

    printf("\n%d/%d tests passed\n", passed, total);
    return 0;
}
