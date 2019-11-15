#include "main.h"
#include <cage-core/systemInformation.h>

void testSystemInformation()
{
	CAGE_TESTCASE("system information");

	CAGE_LOG(severityEnum::Info, "info", stringizer() + "system: " + systemName());
	CAGE_LOG(severityEnum::Info, "info", stringizer() + "user: " + userName());
	CAGE_LOG(severityEnum::Info, "info", stringizer() + "host: " + hostName());
	CAGE_LOG(severityEnum::Info, "info", stringizer() + "processors count: " + processorsCount());
	CAGE_LOG(severityEnum::Info, "info", stringizer() + "processor name: " + processorName());
	CAGE_LOG(severityEnum::Info, "info", stringizer() + "processor speed: " + (processorClockSpeed() / 1000 / 1000) + " MHz");
	CAGE_LOG(severityEnum::Info, "info", stringizer() + "memory capacity: " + (memoryCapacity() / 1024 / 1024) + " MB");
	CAGE_LOG(severityEnum::Info, "info", stringizer() + "memory available: " + (memoryAvailable() / 1024 / 1024) + " MB");
}
