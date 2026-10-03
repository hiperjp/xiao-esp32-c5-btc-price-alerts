#pragma once
#include <cmath>
#include <cstdio>
#include <string>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                     \
  do {                                                                  \
    ++g_checks;                                                         \
    if (!(cond)) {                                                      \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_failures;                                                     \
    }                                                                   \
  } while (0)

#define CHECK_STR(actual, expected)                                     \
  do {                                                                  \
    ++g_checks;                                                         \
    std::string a_ = (actual), e_ = (expected);                         \
    if (a_ != e_) {                                                     \
      std::printf("  FAIL %s:%d:\n    got:      \"%s\"\n    expected: \"%s\"\n", \
                  __FILE__, __LINE__, a_.c_str(), e_.c_str());          \
      ++g_failures;                                                     \
    }                                                                   \
  } while (0)

#define CHECK_NEAR(actual, expected)                                    \
  do {                                                                  \
    ++g_checks;                                                         \
    double a_ = (actual), e_ = (expected);                              \
    if (std::fabs(a_ - e_) > 1e-6) {                                    \
      std::printf("  FAIL %s:%d: %.8f != %.8f\n", __FILE__, __LINE__, a_, e_); \
      ++g_failures;                                                     \
    }                                                                   \
  } while (0)

#define RUN(test_fn)                    \
  do {                                  \
    std::printf("- %s\n", #test_fn);    \
    test_fn();                          \
  } while (0)

static int testSummary() {
  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
