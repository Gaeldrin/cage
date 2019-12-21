#include "main.h"
#include <cage-core/pointerRangeHolder.h>

namespace
{
	uint32 counter = 0;

	struct testStruct
	{
		testStruct()
		{
			counter++;
		}

		testStruct(testStruct &&)
		{
			counter++;
		}

		~testStruct()
		{
			counter--;
		}

		testStruct(const testStruct &) = delete;
		testStruct &operator = (const testStruct &) = delete;
		testStruct &operator = (testStruct &&) = default;

		void fnc()
		{}
	};

	holder<pointerRange<uint32>> makeRangeInts()
	{
		std::vector<uint32> numbers;
		numbers.push_back(5);
		numbers.push_back(42);
		numbers.push_back(13);
		return pointerRangeHolder<uint32>(templates::move(numbers));
	}

	holder<pointerRange<testStruct>> makeRangeTests()
	{
		std::vector<testStruct> tests;
		CAGE_TEST(counter == 0);
		tests.emplace_back();
		tests.emplace_back();
		tests.emplace_back();
		tests.emplace_back();
		CAGE_TEST(counter == 4);
		return pointerRangeHolder<testStruct>(templates::move(tests));
	}

	void functionTakingMutableRange(pointerRange<uint32> r)
	{
		// do nothing
	}

	void functionTakingConstRange(pointerRange<const uint32> r)
	{
		// do nothing
	}
}

void testPointerRange()
{
	CAGE_TESTCASE("pointer range");

	{
		CAGE_TESTCASE("basic iteration");
		std::vector<uint32> numbers;
		numbers.push_back(5);
		numbers.push_back(42);
		numbers.push_back(13);
		pointerRange<const uint32> range = numbers;
		uint32 sum = 0;
		for (auto it : range)
			sum += it;
		CAGE_TEST(sum == 5 + 42 + 13);
		CAGE_TEST(!range.empty());
		CAGE_TEST(pointerRange<const uint32>().empty());
	}

	{
		CAGE_TESTCASE("holder with pointer range <uint32>");
		auto range = makeRangeInts();
		uint32 sum = 0;
		for (auto it : range)
			sum += it;
		CAGE_TEST(sum == 5 + 42 + 13);
	}

	{
		CAGE_TESTCASE("holder with pointer range <testStruct>");
		{
			CAGE_TEST(counter == 0);
			auto range = makeRangeTests();
			CAGE_TEST(counter == 4);
			for (auto &it : range)
				it.fnc();
			CAGE_TEST(counter == 4);
		}
		CAGE_TEST(counter == 0);
	}

	{
		CAGE_TESTCASE("direct dereference of a holder with pointer range");
		uint32 sum = 0;
		// do not dereference the holder! it would deallocate before iterating
		// instead iterate over the holder directly
		for (auto it : makeRangeInts())
			sum += it;
		CAGE_TEST(sum == 5 + 42 + 13);
	}

	{
		CAGE_TESTCASE("empty ranges");
		functionTakingConstRange({});
		functionTakingMutableRange({});
		pointerRange<uint32> empty1 = {};
		pointerRange<const uint32> empty2 = {};
		holder<pointerRange<uint32>> empty3 = {};
		holder<pointerRange<const uint32>> empty4 = {};
	}

	{
		CAGE_TESTCASE("implicit const casts");
		holder<pointerRange<uint32>> range = makeRangeInts();
		pointerRange<uint32> rng1 = range;
		pointerRange<const uint32> rng2 = rng1;
		//pointerRange<uint32> rng3 = rng2; // must not compile - it would drop the const
		functionTakingConstRange(rng1);
		functionTakingMutableRange(rng1);
		functionTakingConstRange(rng2);
		//functionTakingMutableRange(rng2); // must not compile - it would drop the const
		functionTakingConstRange(range);
		functionTakingMutableRange(range);
		functionTakingConstRange(makeRangeInts());
		functionTakingMutableRange(makeRangeInts());
		holder<pointerRange<const uint32>> crange = templates::move(range);
		CAGE_TEST(crange->size() == makeRangeInts()->size());
	}

	{
		CAGE_TESTCASE("operator []");
		holder<pointerRange<uint32>> range = makeRangeInts();
		CAGE_TEST(range[0] == 5);
		CAGE_TEST(range[1] == 42);
		CAGE_TEST(range[2] == 13);
		CAGE_TEST_ASSERTED(range[3]);
		pointerRange<uint32> rng1 = range;
		CAGE_TEST(rng1[0] == 5);
		CAGE_TEST(rng1[1] == 42);
		CAGE_TEST(rng1[2] == 13);
		CAGE_TEST_ASSERTED(rng1[3]);
	}
}
