#include <utility>

#include "processor.h"

#include <cage-core/utility/memoryBuffer.h>

void processRaw()
{
	writeLine(string("use=") + inputFile);

	memoryBuffer data;
	{ // load file
		holder<fileClass> f = newFile(inputFile, fileMode(true, false));
		data.reallocate(f->size());
		f->read(data.data(), data.size());
	}

	assetHeaderStruct h = initializeAssetHeaderStruct();
	h.originalSize = numeric_cast<uint32>(data.size());

	CAGE_LOG(severityEnum::Info, logComponentName, string() + "original data size: " + data.size() + " bytes");
	if (data.size() >= properties("compress_threshold").toUint32())
	{
		memoryBuffer data2 = detail::compress(data);
		CAGE_LOG(severityEnum::Info, logComponentName, string() + "compressed data size: " + data2.size() + " bytes");
		if (data2.size() < data.size())
		{
			std::swap(data, data2);
			CAGE_LOG(severityEnum::Info, logComponentName, "using the compressed data");
			h.compressedSize = numeric_cast<uint32>(data2.size());
		}
		else
			CAGE_LOG(severityEnum::Info, logComponentName, "using the data without compression");
	}
	else
		CAGE_LOG(severityEnum::Info, logComponentName, "data are under compression threshold");

	holder<fileClass> f = newFile(outputFileName, fileMode(false, true));
	f->write(&h, sizeof(h));
	f->write(data.data(), data.size());
	f->close();
}
