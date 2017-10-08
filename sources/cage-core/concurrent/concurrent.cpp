#define CAGE_EXPORT
#include <cage-core/core.h>
#include <cage-core/concurrent.h>
#include "../system.h"

#include <thread>

namespace cage
{
	uint32 processorsCount()
	{
		return std::thread::hardware_concurrency();
	}

	uint64 threadId()
	{
#ifdef CAGE_SYSTEM_WINDOWS
		return numeric_cast<uint64>(GetCurrentThreadId());
#else
		return numeric_cast<uint64>(pthread_self());
#endif
	}

	uint64 processId()
	{
#ifdef CAGE_SYSTEM_WINDOWS
		return numeric_cast<uint64>(GetCurrentProcessId());
#else
		return numeric_cast<uint64>(getpid());
#endif
	}

	void threadSleep(uint64 micros)
	{
#ifdef CAGE_SYSTEM_WINDOWS
		micros /= 1000;
		if (micros == 0)
			micros = 1;
		Sleep((DWORD)micros);
#else
		usleep(micros);
#endif
	}
}
