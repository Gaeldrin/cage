#include <vector>

#include "main.h"

#include <cage-core/memoryBuffer.h>

void testCopyAndMove()
{
	CAGE_TESTCASE("copy and move semantics");

	{
		CAGE_TESTCASE("vector of strings");
		std::vector<string> vec;
		vec.emplace_back("hi");
		vec.push_back("hello");
	}

	{
		CAGE_TESTCASE("vector of memory buffers");
		std::vector<MemoryBuffer> vec;
		vec.emplace_back(MemoryBuffer());
		vec.push_back(MemoryBuffer());
	}
}
