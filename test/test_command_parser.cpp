#include "test.h"
#include "command_parser.h"

static void test_set_low_and_high() {
  Command c = parseCommand("/low 90000");
  CHECK(c.type == CommandType::SetLow);
  CHECK(c.valid);
  CHECK_NEAR(c.value, 90000);

  c = parseCommand("/high 130000.5");
  CHECK(c.type == CommandType::SetHigh);
  CHECK(c.valid);
  CHECK_NEAR(c.value, 130000.5);
}

static void test_off_disables() {
  Command c = parseCommand("/low off");
  CHECK(c.type == CommandType::SetLow);
  CHECK(c.valid);
  CHECK_NEAR(c.value, 0);

  c = parseCommand("/high OFF");
  CHECK(c.type == CommandType::SetHigh);
  CHECK(c.valid);
  CHECK_NEAR(c.value, 0);
}

static void test_case_spaces_and_bot_suffix() {
  for (const char* s : {"/LOW 90000", "  /low   90000  ", "/low@hjpBtcAlertsBot 90000", "/Low\t90000"}) {
    Command c = parseCommand(s);
    CHECK(c.type == CommandType::SetLow);
    CHECK(c.valid);
    CHECK_NEAR(c.value, 90000);
  }
}

static void test_invalid_threshold_arguments() {
  for (const char* s : {"/low", "/low abc", "/low -5", "/low 0", "/low 90,000",
                        "/low 90000 100", "/low 1e5", "/high", "/high 1,5"}) {
    Command c = parseCommand(s);
    CHECK(c.type == CommandType::SetLow || c.type == CommandType::SetHigh);
    if (c.valid) std::printf("  accepted: \"%s\"\n", s);
    CHECK(!c.valid);
  }
}

static void test_dot_thousands_separator_parses_as_decimal() {
  // Accepted as 90 on purpose; the confirmation message makes the mistake visible.
  Command c = parseCommand("/low 90.000");
  CHECK(c.valid);
  CHECK_NEAR(c.value, 90);
}

static void test_status_and_help() {
  CHECK(parseCommand("/status").type == CommandType::Status);
  CHECK(parseCommand("/Status@hjpBtcAlertsBot").type == CommandType::Status);
  CHECK(parseCommand("/help").type == CommandType::Help);
  CHECK(parseCommand("/start").type == CommandType::Help);
}

static void test_unknown() {
  for (const char* s : {"hola", "", "   ", "/foo", "low 90000", "/", "/lowx 90000"}) {
    CHECK(parseCommand(s).type == CommandType::Unknown);
  }
}

int main() {
  RUN(test_set_low_and_high);
  RUN(test_off_disables);
  RUN(test_case_spaces_and_bot_suffix);
  RUN(test_invalid_threshold_arguments);
  RUN(test_dot_thousands_separator_parses_as_decimal);
  RUN(test_status_and_help);
  RUN(test_unknown);
  return testSummary();
}
