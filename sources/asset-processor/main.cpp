#include "processor.h"
#include <cage-core/logger.h>
#include <cage-core/hashString.h>

#include <exception>
#include <map>
#include <cstdio> // fgets, ferror
#include <cstring> // strlen

// passed names
string inputDirectory; // c:/asset
string inputName; // path/file?specifier;identifier
string outputDirectory; // c:/data
string outputName; // 123456789
string assetPath;
string schemePath;
uint32 schemeIndex;

// derived names
string inputFileName; // c:/asset/path/file
string outputFileName; // c:/data/123456789
string inputFile; // path/file
string inputSpec; // specifier
string inputIdentifier; // identifier

const char *logComponentName;

AssetHeader initializeAssetHeader()
{
	AssetHeader h = initializeAssetHeader(inputName, numeric_cast<uint16>(schemeIndex));
	string intr = properties("alias");
	if (!intr.empty())
	{
		intr = pathJoin(pathExtractPath(inputName), intr);
		writeLine(stringizer() + "alias = " + intr);
		h.aliasName = HashString(intr);
	}
	return h;
}

namespace
{
	std::map<string, string> props;

	string readLine()
	{
		char buf[string::MaxLength];
		if (std::fgets(buf, string::MaxLength, stdin) == nullptr)
			CAGE_THROW_ERROR(SystemError, "fgets", std::ferror(stdin));
		return string(buf, numeric_cast<uint32>(std::strlen(buf))).trim();
	}

	void derivedProperties()
	{
		inputFile = inputName;
		if (inputFile.find(';') != m)
		{
			inputIdentifier = inputFile.split(";");
			std::swap(inputIdentifier, inputFile);
		}
		if (inputFile.find('?') != m)
		{
			inputSpec = inputFile.split("?");
			std::swap(inputSpec, inputFile);
		}

		inputFileName = pathJoin(inputDirectory, inputFile);
		outputFileName = pathJoin(outputDirectory, outputName);
	}

	void loadProperties()
	{
		inputDirectory = readLine();
		inputName = readLine();
		outputDirectory = readLine();
		outputName = readLine();
		assetPath = readLine();
		schemePath = readLine();
		schemeIndex = readLine().toUint32();

		derivedProperties();

		while (true)
		{
			string value = readLine();
			if (value == "cage-end")
				break;
			if (value.find('=') == m)
			{
				CAGE_LOG(SeverityEnum::Note, "exception", stringizer() + "line: " + value);
				CAGE_THROW_ERROR(Exception, "missing '=' in property line");
			}
			string name = value.split("=");
			props[name] = value;
		}
	}

	void initializeSecondaryLog(const string &path)
	{
		static Holder<LoggerOutputFile> *secondaryLogFile = new Holder<LoggerOutputFile>(); // intentional leak
		static Holder<Logger> *secondaryLog = new Holder<Logger>(); // intentional leak - this will allow to log to the very end of the application
		*secondaryLogFile = newLoggerOutputFile(path, false);
		*secondaryLog = newLogger();
		(*secondaryLog)->output.bind<LoggerOutputFile, &LoggerOutputFile::output>(secondaryLogFile->get());
		(*secondaryLog)->format.bind<&logFormatFileShort>();
	}
}

void writeLine(const string &other)
{
	CAGE_LOG(SeverityEnum::Info, "asset-processor", stringizer() + "writing: '" + other + "'");
	{
		string b = other;
		if (b.split("=").trim() == "ref")
			CAGE_LOG(SeverityEnum::Note, "asset-processor", stringizer() + "reference hash: '" + (uint32)HashString(b.trim().c_str()) + "'");
	}
	if (fprintf(stdout, "%s\n", other.c_str()) < 0)
		CAGE_THROW_ERROR(SystemError, "fprintf", ferror(stdout));
}

string properties(const string &name)
{
	auto it = props.find(name);
	if (it != props.end())
		return it->second;
	else
	{
		CAGE_LOG(SeverityEnum::Note, "exception", stringizer() + "property name: '" + name + "'");
		CAGE_THROW_ERROR(Exception, "property not found");
	}
}

int main(int argc, const char *args[])
{
	try
	{
		if (argc == 3 && string(args[1]) == "analyze")
		{
			logComponentName = "analyze";
			inputDirectory = pathExtractPath(args[2]);
			inputName = pathExtractFilename(args[2]);
			derivedProperties();
			initializeSecondaryLog(pathJoin(configGetString("cage-asset-processor/analyzeLog/path", "analyze-log"), pathReplaceInvalidCharacters(inputName) + ".log"));
			return processAnalyze();
		}

		if (argc != 2)
			CAGE_THROW_ERROR(Exception, "missing asset type parameter");

		loadProperties();
		initializeSecondaryLog(pathJoin(configGetString("cage-asset-processor/processLog/path", "process-log"), pathReplaceInvalidCharacters(inputName) + ".log"));

#define GCHL_GENERATE(N) CAGE_LOG(SeverityEnum::Info, "asset-processor", stringizer() + "input " #N ": '" + N + "'");
		GCHL_GENERATE(inputDirectory);
		GCHL_GENERATE(inputName);
		GCHL_GENERATE(outputDirectory);
		GCHL_GENERATE(outputName);
		GCHL_GENERATE(assetPath);
		GCHL_GENERATE(schemePath);
		GCHL_GENERATE(schemeIndex);
		GCHL_GENERATE(inputFileName);
		GCHL_GENERATE(outputFileName);
		GCHL_GENERATE(inputFile);
		GCHL_GENERATE(inputSpec);
		GCHL_GENERATE(inputIdentifier);
#undef GCHL_GENERATE

		for (const auto &it : props)
			CAGE_LOG(SeverityEnum::Info, "asset-processor", stringizer() + "property '" + it.first + "': '" + it.second + "'");

		Delegate<void()> func;
		string component = string(args[1]);
		if (component == "texture")
			func.bind<&processTexture>();
		else if (component == "shader")
			func.bind<&processShader>();
		else if (component == "pack")
			func.bind<&processPack>();
		else if (component == "object")
			func.bind<&processObject>();
		else if (component == "animation")
			func.bind<&processAnimation>();
		else if (component == "mesh")
			func.bind<&processMesh>();
		else if (component == "skeleton")
			func.bind<&processSkeleton>();
		else if (component == "font")
			func.bind<&processFont>();
		else if (component == "textpack")
			func.bind<&processTextpack>();
		else if (component == "sound")
			func.bind<&processSound>();
		else if (component == "collider")
			func.bind<&processCollider>();
		else if (component == "raw")
			func.bind<&processRaw>();
		else
			CAGE_THROW_ERROR(Exception, "invalid asset type parameter");

		logComponentName = args[1];
		writeLine("cage-begin");
		func();
		writeLine("cage-end");
		return 0;
	}
	catch (const cage::Exception &)
	{
	}
	catch (const std::exception &e)
	{
		CAGE_LOG(SeverityEnum::Error, "exception", stringizer() + "std exception: " + e.what());
	}
	catch (...)
	{
		CAGE_LOG(SeverityEnum::Error, "exception", "unknown exception");
	}
	writeLine("cage-error");
	return 1;
}
