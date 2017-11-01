#define CAGE_EXPORT
#include <cage-core/core.h>
#include <cage-core/log.h>
#include <cage-core/math.h>
#include "../system.h"
#undef min
#undef max

#include <limits>

namespace cage
{
	namespace detail
	{
		namespace
		{
			CAGE_THREAD_LOCAL_STORAGE bool isLocalBreakpointEnabled = true;
			CAGE_THREAD_LOCAL_STORAGE bool isLocalAssertDeadly = true;
			bool isGlobalBreakpointEnabled = true;
			bool isGlobalAssertDeadly = true;
		}

		void terminate()
		{
			debugOutput("invoking terminate");
			debugBreakpoint();
			std::terminate();
		}

		void debugOutput(const char *value)
		{
#ifdef CAGE_SYSTEM_WINDOWS
			if (IsDebuggerPresent())
			{
				OutputDebugString(value);
				OutputDebugString("\n");
				return;
			}
#endif
		}

		void debugBreakpoint()
		{
			if (!isLocalBreakpointEnabled || !isGlobalBreakpointEnabled)
				return;
#ifdef CAGE_SYSTEM_WINDOWS
			if (IsDebuggerPresent())
				__debugbreak();
#else
			//__builtin_trap();
#endif
		}

		overrideBreakpoint::overrideBreakpoint(bool enable) : original(isLocalBreakpointEnabled)
		{
			isLocalBreakpointEnabled = enable;
		}

		overrideBreakpoint::~overrideBreakpoint()
		{
			isLocalBreakpointEnabled = original;
		}

		overrideAssert::overrideAssert(bool deadly) : original(isLocalAssertDeadly)
		{
			isLocalAssertDeadly = deadly;
		}

		overrideAssert::~overrideAssert()
		{
			isLocalAssertDeadly = original;
		}

		void setGlobalBreakpointOverride(bool enable)
		{
			isGlobalBreakpointEnabled = enable;
		}

		void setGlobalAssertOverride(bool enable)
		{
			isGlobalAssertDeadly = enable;
		}
	}

	namespace privat
	{
		namespace
		{
			void assertOutputLine(const char *msg, bool continuous = true)
			{
				if (continuous)
					CAGE_LOG_CONTINUE(severityEnum::Critical, "assert", string(msg));
				else
					CAGE_LOG(severityEnum::Critical, "assert", string(msg));
			}
		}

		assertClass::assertClass(bool exp, const char *expt, const char *file, const char *line, const char *function) : valid(exp)
		{
			if (!valid)
			{
				char buffer[2048];
				buffer[0] = 0;
				detail::strcat(buffer, "assert '");
				detail::strcat(buffer, expt);
				detail::strcat(buffer, "' failed");
				assertOutputLine(buffer, false);
				buffer[0] = 0;
				detail::strcat(buffer, file);
				detail::strcat(buffer, "(");
				detail::strcat(buffer, line);
				detail::strcat(buffer, ") - ");
				detail::strcat(buffer, function);
				assertOutputLine(buffer);
			}
		}

		void assertClass::operator () () const
		{
			if (valid)
				return;

			if (detail::isLocalAssertDeadly && detail::isGlobalAssertDeadly)
				detail::terminate();
			else
				CAGE_THROW_CRITICAL(exception, "assert failure");
		}

		assertClass &assertClass::variable(const char *name, const pointer &var)
		{
			return variable(name, var.asVoid);
		}

#define GCHL_GENERATE(TYPE) \
		assertClass &assertClass::variable (const char *name, TYPE var)\
		{\
			if (!valid)\
			{\
				char buffer [50];\
				privat::sprint1(buffer, var);\
				format(name, buffer);\
			}\
			return *this;\
		}
		CAGE_EVAL_SMALL(CAGE_EXPAND_ARGS(GCHL_GENERATE, sint8, sint16, sint32, sint64, uint8, uint16, uint32, uint64, bool, float, double, void*));
#undef GCHL_GENERATE

#define GCHL_GENERATE(TYPE) \
		assertClass &assertClass::variable (const char *name, TYPE var)\
		{\
			if (!valid)\
			{\
				format(name, string(var).c_str()); \
			}\
			return *this;\
		}
		CAGE_EVAL_SMALL(CAGE_EXPAND_ARGS(GCHL_GENERATE, const real&, const rads&, const vec2&, const vec3&, const vec4&, const mat3&, const mat4&));
#undef GCHL_GENERATE

		assertClass &assertClass::variable(const char *name, const string &var)
		{
			if (!valid)
			{
				format(name, var.c_str());
			}
			return *this;
		}

		assertClass &assertClass::variable(const char *name, const char *var)
		{
			if (!valid)
			{
				char buffer[2048];
				privat::sprint1(buffer, var);
				format(name, buffer);
			}
			return *this;
		}

		void assertClass::format(const char *name, const char *var) const
		{
			char buffer[2048];
			buffer[0] = 0;
			detail::strcat(buffer, " > ");
			detail::strcat(buffer, name);
			detail::strcat(buffer, ": ");
			detail::strcat(buffer, var);
			assertOutputLine(buffer);
		}
	}

	namespace
	{
		CAGE_ASSERT_COMPILE(sizeof(uint8) == 1, assert_size_uint8);
		CAGE_ASSERT_COMPILE(sizeof(uint16) == 2, assert_size_uint16);
		CAGE_ASSERT_COMPILE(sizeof(uint32) == 4, assert_size_uint32);
		CAGE_ASSERT_COMPILE(sizeof(uint64) == 8, assert_size_uint64);
		CAGE_ASSERT_COMPILE(sizeof(uintPtr) == sizeof(void*), assert_size_uintPtr);
		CAGE_ASSERT_COMPILE(sizeof(sint8) == 1, assert_size_sint8);
		CAGE_ASSERT_COMPILE(sizeof(sint16) == 2, assert_size_sint16);
		CAGE_ASSERT_COMPILE(sizeof(sint32) == 4, assert_size_sint32);
		CAGE_ASSERT_COMPILE(sizeof(sint64) == 8, assert_size_sint64);
		CAGE_ASSERT_COMPILE(sizeof(sintPtr) == sizeof(void*), assert_size_sintPtr);

		CAGE_ASSERT_COMPILE(detail::numeric_limits<char>::min() == std::numeric_limits<char>::min(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<char>::max() == std::numeric_limits<char>::max(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<short>::min() == std::numeric_limits<short>::min(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<short>::max() == std::numeric_limits<short>::max(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<int>::min() == std::numeric_limits<int>::min(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<int>::max() == std::numeric_limits<int>::max(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<long>::min() == std::numeric_limits<long>::min(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<long>::max() == std::numeric_limits<long>::max(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<long long>::min() == std::numeric_limits<long long>::min(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<long long>::max() == std::numeric_limits<long long>::max(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<unsigned char>::min() == std::numeric_limits<unsigned char>::min(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<unsigned char>::max() == std::numeric_limits<unsigned char>::max(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<unsigned short>::min() == std::numeric_limits<unsigned short>::min(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<unsigned short>::max() == std::numeric_limits<unsigned short>::max(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<unsigned int>::min() == std::numeric_limits<unsigned int>::min(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<unsigned int>::max() == std::numeric_limits<unsigned int>::max(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<unsigned long>::min() == std::numeric_limits<unsigned long>::min(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<unsigned long>::max() == std::numeric_limits<unsigned long>::max(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<unsigned long long>::min() == std::numeric_limits<unsigned long long>::min(), assert_numeric_limits);
		CAGE_ASSERT_COMPILE(detail::numeric_limits<unsigned long long>::max() == std::numeric_limits<unsigned long long>::max(), assert_numeric_limits);
	}
}
