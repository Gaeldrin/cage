#include <set>

#include <cage-core/core.h>
#include <cage-core/logger.h>
#include <cage-core/config.h>

using namespace cage;

#include "config.h"
#include "processor.h"

bool consoleLogFilter(const cage::detail::loggerInfo &info)
{
	return info.severity >= SeverityEnum::Error || string(info.component) == "exception" || string(info.component) == "asset" || string(info.component) == "verdict";
}

int main(int argc, const char *args[])
{
	Holder<Logger> conLog = newLogger();
	conLog->filter.bind<&consoleLogFilter>();
	conLog->format.bind<&logFormatConsole>();
	conLog->output.bind<&logOutputStdOut>();

	configParseCmd(argc, args);

	CAGE_LOG(SeverityEnum::Info, "database", "start");
	start();

	if (configListening)
	{
		CAGE_LOG(SeverityEnum::Info, "database", "waiting for changes");
		try
		{
			listen();
		}
		catch (...)
		{
			CAGE_LOG(SeverityEnum::Info, "database", "stopped");
		}
	}

	CAGE_LOG(SeverityEnum::Info, "database", "end");

	return verdict() ? 0 : -1;
}
