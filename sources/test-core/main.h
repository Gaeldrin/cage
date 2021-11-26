#include <cage-core/debug.h>

using namespace cage;

#define CAGE_TESTCASE(NAME) { CAGE_LOG(SeverityEnum::Info, "testcase", NAME); }
#define CAGE_TEST(COND,...) { if (!(COND)) { CAGE_LOG(SeverityEnum::Info, "test", #COND); CAGE_THROW_CRITICAL(Exception, "test failed"); } }
#define CAGE_TEST_THROWN(COND,...) { bool ok = false; { CAGE_LOG(SeverityEnum::Info, "exception", "awaiting exception"); ::cage::detail::OverrideBreakpoint OverrideBreakpoint; try { COND; } catch (...) { ok = true; } } if (!ok) { CAGE_LOG(SeverityEnum::Info, "exception", "caught no exception"); CAGE_THROW_CRITICAL(Exception, #COND); } else { CAGE_LOG(SeverityEnum::Info, "exception", "the exception was expected"); } }
#ifdef CAGE_ASSERT_ENABLED
#define CAGE_TEST_ASSERTED(COND,...) { ::cage::detail::OverrideAssert overrideAssert; CAGE_TEST_THROWN(COND); }
#else
#define CAGE_TEST_ASSERTED(COND,...) {}
#endif
