#include "main.h"

#include <cage-core/containerSerialization.h>
#include <cage-core/memoryBuffer.h>
#include <cage-core/stdHash.h>
#include <cage-core/flatSet.h>
#include <vector>
#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>

void testContainerSerialization()
{
	CAGE_TESTCASE("container serialization");

	{
		CAGE_TESTCASE("vector");
		std::vector<String> cont;
		cont.push_back("hello");
		cont.push_back("world");
		MemoryBuffer buf;
		Serializer ser(buf);
		ser << cont;
		cont.clear();
		Deserializer des(buf);
		des >> cont;
		CAGE_TEST(cont.size() == 2);
		CAGE_TEST(cont[0] == "hello");
		CAGE_TEST(cont[1] == "world");
		CAGE_TEST(buf.size() == 8 + 4 + 5 + 4 + 5); // make sure that the behavior is consistent for both 32 and 64 bit applications
	}

	{
		CAGE_TESTCASE("list");
		std::list<String> cont;
		cont.push_back("hello");
		cont.push_back("world");
		MemoryBuffer buf;
		Serializer ser(buf);
		ser << cont;
		cont.clear();
		Deserializer des(buf);
		des >> cont;
		CAGE_TEST(cont.size() == 2);
		CAGE_TEST(cont.front() == "hello");
		CAGE_TEST(cont.back() == "world");
	}

	{
		CAGE_TESTCASE("map");
		std::map<String, String> cont;
		cont["abc"] = "def";
		cont["ghi"] = "jkl";
		MemoryBuffer buf;
		Serializer ser(buf);
		ser << cont;
		cont.clear();
		Deserializer des(buf);
		des >> cont;
		CAGE_TEST(cont.size() == 2);
		CAGE_TEST(cont["abc"] == "def");
		CAGE_TEST(cont["ghi"] == "jkl");
	}

	{
		CAGE_TESTCASE("unordered_map");
		std::unordered_map<String, String> cont;
		cont["abc"] = "def";
		cont["ghi"] = "jkl";
		MemoryBuffer buf;
		Serializer ser(buf);
		ser << cont;
		cont.clear();
		Deserializer des(buf);
		des >> cont;
		CAGE_TEST(cont.size() == 2);
		CAGE_TEST(cont["abc"] == "def");
		CAGE_TEST(cont["ghi"] == "jkl");
	}

	{
		CAGE_TESTCASE("set");
		std::set<String> cont;
		cont.insert("hello");
		cont.insert("world");
		MemoryBuffer buf;
		Serializer ser(buf);
		ser << cont;
		cont.clear();
		Deserializer des(buf);
		des >> cont;
		CAGE_TEST(cont.size() == 2);
		CAGE_TEST(cont.count("hello") == 1);
		CAGE_TEST(cont.count("world") == 1);
	}

	{
		CAGE_TESTCASE("unordered_set");
		std::unordered_set<String> cont;
		cont.insert("hello");
		cont.insert("world");
		MemoryBuffer buf;
		Serializer ser(buf);
		ser << cont;
		cont.clear();
		Deserializer des(buf);
		des >> cont;
		CAGE_TEST(cont.size() == 2);
		CAGE_TEST(cont.count("hello") == 1);
		CAGE_TEST(cont.count("world") == 1);
	}

	{
		CAGE_TESTCASE("vector of vectors");
		std::vector<std::vector<String>> cont;
		cont.resize(2);
		cont[0].push_back("a");
		cont[0].push_back("b");
		cont[0].push_back("c");
		cont[1].push_back("d");
		cont[1].push_back("e");
		MemoryBuffer buf;
		Serializer ser(buf);
		ser << cont;
		cont.clear();
		Deserializer des(buf);
		des >> cont;
		CAGE_TEST(cont.size() == 2);
		CAGE_TEST(cont[0].size() == 3);
		CAGE_TEST(cont[1].size() == 2);
		CAGE_TEST(cont[0][0] == "a");
		CAGE_TEST(cont[0][1] == "b");
		CAGE_TEST(cont[0][2] == "c");
		CAGE_TEST(cont[1][0] == "d");
		CAGE_TEST(cont[1][1] == "e");
	}

	{
		CAGE_TESTCASE("map of vectors");
		std::map<String, std::vector<String>> cont;
		cont["1"].push_back("a");
		cont["1"].push_back("b");
		cont["1"].push_back("c");
		cont["2"].push_back("d");
		cont["2"].push_back("e");
		MemoryBuffer buf;
		Serializer ser(buf);
		ser << cont;
		cont.clear();
		Deserializer des(buf);
		des >> cont;
		CAGE_TEST(cont.size() == 2);
		CAGE_TEST(cont["1"].size() == 3);
		CAGE_TEST(cont["2"].size() == 2);
		CAGE_TEST(cont["1"][0] == "a");
		CAGE_TEST(cont["1"][1] == "b");
		CAGE_TEST(cont["1"][2] == "c");
		CAGE_TEST(cont["2"][0] == "d");
		CAGE_TEST(cont["2"][1] == "e");
	}

	{
		CAGE_TESTCASE("r-value deserializer for vector");
		std::vector<String> cont;
		cont.push_back("hello");
		cont.push_back("world");
		MemoryBuffer buf;
		Serializer ser(buf);
		ser << cont;
		cont.clear();
		Deserializer des(buf);
		des.copy() >> cont;
		CAGE_TEST(cont.size() == 2);
		CAGE_TEST(cont[0] == "hello");
		CAGE_TEST(cont[1] == "world");
	}

	{
		CAGE_TESTCASE("pointer range");
		std::vector<String> cont;
		cont.push_back("hello");
		cont.push_back("world");
		PointerRange<const String> range = cont;
		MemoryBuffer buf;
		Serializer ser(buf);
		ser << range;
		cont.clear();
		Deserializer des(buf);
		des >> cont;
		CAGE_TEST(cont.size() == 2);
		CAGE_TEST(cont[0] == "hello");
		CAGE_TEST(cont[1] == "world");
		CAGE_TEST(buf.size() == 8 + 4 + 5 + 4 + 5);
	}

	{
		CAGE_TESTCASE("flat set");
		FlatSet<uint32> cont;
		cont.insert(42);
		cont.insert(13);
		cont.insert(20);
		MemoryBuffer buf;
		Serializer ser(buf);
		ser << cont;
		cont.clear();
		Deserializer des(buf);
		des >> cont;
		CAGE_TEST(cont.size() == 3);
		CAGE_TEST(cont.data()[0] == 13);
		CAGE_TEST(cont.data()[1] == 20);
		CAGE_TEST(cont.data()[2] == 42);
	}
}
