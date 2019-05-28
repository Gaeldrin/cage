#include <cstdlib>

#include "main.h"
#include <cage-core/files.h>

using namespace cage;

uint32 cageTestCaseStruct::counter = 0;

void testMacros();
void testEnums();
void testExceptions();
void testNumericTypes();
void testStrings();
void testDelegates();
void testHolder();
void testPointerRange();
void testEnumerate();
void testEvents();
void testMath();
void testMathGlm();
void testGeometry();
void testMemoryArenas();
void testMemoryPools();
void testMemoryPerformance();
void testMemoryBuffers();
void testSerialization();
void testConcurrent();
void testConcurrentQueue();
void testFiles();
void testArchives();
void testLineReader();
void testHashTable();
void testRandom();
void testIni();
void testConfig();
void testColor();
void testPng();
void testNoise();
void testSpatial();
void testCollisions();
void testSceneEntities();
void testSceneSerialize();
void testVariableInterpolatingBuffer();
void testVariableSmoothingBufferStruct();
void testCopyAndMove();
void testSwapBufferGuard();
void testTcp();
void testUdp();
void testUdpDiscovery();
void testProgram();
void testSystemInformation();
void generatePointsOnSphere();

int main()
{
	holder<loggerClass> log1 = newLogger();
	log1->format.bind<logFormatPolicyConsole>();
	log1->output.bind<logOutputPolicyStdOut>();

	//generatePointsOnSphere();
	testMacros();
	testEnums();
	testExceptions();
	testNumericTypes();
	testStrings();
	testDelegates();
	testHolder();
	testPointerRange();
	testEnumerate();
	testEvents();
	testMath();
	testMathGlm();
	testGeometry();
	testMemoryArenas();
	testMemoryPools();
	testMemoryPerformance();
	testMemoryBuffers();
	testSerialization();
	testConcurrent();
	testConcurrentQueue();
	testFiles();
	testArchives();
	testLineReader();
	testHashTable();
	testRandom();
	testIni();
	testConfig();
	testColor();
	testPng();
	testNoise();
	testSpatial();
	testCollisions();
	testSceneEntities();
	testSceneSerialize();
	testVariableInterpolatingBuffer();
	testVariableSmoothingBufferStruct();
	testCopyAndMove();
	testSwapBufferGuard();
	testTcp();
	testUdp();
	testUdpDiscovery();
	testProgram();
	testSystemInformation();
	pathRemove("testdir");

	{
		CAGE_TESTCASE("all tests done ok");
	}

	return 0;
}
