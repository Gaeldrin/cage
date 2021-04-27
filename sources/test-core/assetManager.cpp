#include "main.h"
#include <cage-core/files.h>
#include <cage-core/memoryBuffer.h>
#include <cage-core/serialization.h>
#include <cage-core/concurrent.h>
#include <cage-core/assetManager.h>
#include <cage-core/assetHeader.h>
#include <cage-core/assetContext.h>
#include <cage-core/math.h>

#include <atomic>

namespace
{
	const string AssetsPath = pathJoin(pathWorkingDir(), "testdir/assetManager/assets");

	constexpr uint32 AssetSchemeIndexCounter = 13;

	struct AssetCounter : private Immovable
	{
		static std::atomic<sint32> counter;

		AssetCounter()
		{
			CAGE_ASSERT(counter >= 0);
			counter++;
		}

		~AssetCounter()
		{
			counter--;
			CAGE_ASSERT(counter >= 0);
		}
	};

	std::atomic<sint32> AssetCounter::counter{0};

	void processAssetCounterLoad(AssetContext *context)
	{
		if (context->realName >= 5000)
			CAGE_THROW_ERROR(Exception, "intentionally failed asset processing");
		Holder<AssetCounter> h = systemArena().createHolder<AssetCounter>();
		context->assetHolder = templates::move(h).cast<void>();
	}

	AssetScheme genAssetSchemeCounter()
	{
		AssetScheme s;
		s.load.bind<&processAssetCounterLoad>();
		return s;
	}

	void makeAssetPack(uint32 name, PointerRange<const uint32> deps)
	{
		AssetHeader hdr = initializeAssetHeader(stringizer() + name, AssetSchemeIndexPack);
		hdr.dependenciesCount = numeric_cast<uint16>(deps.size());
		Holder<File> f = writeFile(pathJoin(AssetsPath, stringizer() + name));
		f->write(bufferView(hdr));
		f->write(bufferCast<const char, const uint32>(deps));
		f->close();
	}

	void makeAssetRaw(uint32 name, PointerRange<const char> contents)
	{
		AssetHeader hdr = initializeAssetHeader(stringizer() + name, AssetSchemeIndexRaw);
		hdr.originalSize = contents.size();
		Holder<File> f = writeFile(pathJoin(AssetsPath, stringizer() + name));
		f->write(bufferView(hdr));
		f->write(contents);
		f->close();
	}

	void makeAssetCounter(uint32 name, PointerRange<const uint32> deps)
	{
		AssetHeader hdr = initializeAssetHeader(stringizer() + name, AssetSchemeIndexCounter);
		hdr.dependenciesCount = numeric_cast<uint16>(deps.size());
		Holder<File> f = writeFile(pathJoin(AssetsPath, stringizer() + name));
		f->write(bufferView(hdr));
		f->write(bufferCast(deps));
		f->close();
	}

	void makeAssetCounter(uint32 name)
	{
		makeAssetCounter(name, {});
	}

	Holder<AssetManager> instantiate()
	{
		CAGE_TEST(AssetCounter::counter == 0);
		pathRemove(AssetsPath);
		pathCreateDirectories(AssetsPath);
		AssetManagerCreateConfig cfg;
		cfg.assetsFolderName = AssetsPath;
		Holder<AssetManager> man = newAssetManager(cfg);
		man->defineScheme<AssetSchemeIndexPack, AssetPack>(genAssetSchemePack());
		man->defineScheme<AssetSchemeIndexRaw, MemoryBuffer>(genAssetSchemeRaw());
		man->defineScheme<AssetSchemeIndexCounter, AssetCounter>(genAssetSchemeCounter());
		return man;
	}

	void waitProcessing(Holder<AssetManager> &man)
	{
		while (man->processing())
			threadYield();
	}

	template<uint32 Length>
	void checkContents(Holder<AssetManager> &man, const uint32 name, const char (&content)[Length])
	{
		Holder<MemoryBuffer> a = man->get<AssetSchemeIndexRaw, MemoryBuffer>(10);
		CAGE_TEST(a);
		CAGE_TEST(a->size() == Length - 1);
		CAGE_TEST(detail::memcmp(a->data(), content, Length - 1) == 0);
	}

	void processAssetDummyLoad(AssetContext *context)
	{
		// nothing
	}

	AssetScheme genDummyScheme()
	{
		AssetScheme s;
		s.load.bind<&processAssetDummyLoad>();
		return s;
	}
}

void testAssetManager()
{
	CAGE_TESTCASE("asset manager");

	{
		CAGE_TESTCASE("basics");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(10);
		makeAssetCounter(20);
		makeAssetCounter(30);
		CAGE_TEST(AssetCounter::counter == 0);
		man->add(10);
		man->add(20);
		man->add(30);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 3);
		{
			Holder<AssetCounter> a = man->get<AssetSchemeIndexCounter, AssetCounter>(10);
			CAGE_TEST(a);
			CAGE_TEST(AssetCounter::counter == 3);
		}
		man->remove(10);
		man->remove(20);
		man->remove(30);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("type validation");
		Holder<AssetManager> man = instantiate();
		man->defineScheme<71, uint8>(genDummyScheme());
		man->defineScheme<72, uint16>(genDummyScheme());
		man->defineScheme<73, uint32>(genDummyScheme());
		man->defineScheme<74, uint64>(genDummyScheme());
		{ auto a = man->tryGet<71, uint8>(42); CAGE_TEST(!a); }
		{ auto a = man->tryGet<72, uint16>(42); CAGE_TEST(!a); }
		{ auto a = man->tryGet<73, uint32>(42); CAGE_TEST(!a); }
		{ auto a = man->tryGet<74, uint64>(42); CAGE_TEST(!a); }
		CAGE_TEST_ASSERTED((man->tryGet<71, uint64>(42)));
		CAGE_TEST_ASSERTED((man->tryGet<72, uint8>(42)));
		CAGE_TEST_ASSERTED((man->tryGet<73, uint16>(42)));
		CAGE_TEST_ASSERTED((man->tryGet<74, uint32>(42)));
	}

	{
		CAGE_TESTCASE("multiply added same asset");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(10);
		CAGE_TEST(AssetCounter::counter == 0);
		man->add(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		man->add(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		man->add(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		man->remove(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		man->remove(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		man->remove(10);
		man->unloadWait();
		CAGE_TEST(AssetCounter::counter == 0);
	}

	{
		CAGE_TESTCASE("dependencies 1");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(10);
		makeAssetCounter(20);
		makeAssetCounter(30);
		makeAssetPack(50, PointerRange<const uint32>({ (uint32)10, (uint32)20, (uint32)30 }));
		CAGE_TEST(AssetCounter::counter == 0);
		man->add(50);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 3);
		man->remove(50);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("dependencies 2");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(10);
		makeAssetCounter(20);
		makeAssetCounter(30);
		makeAssetCounter(50, PointerRange<const uint32>({ (uint32)10, (uint32)20, (uint32)30 }));
		CAGE_TEST(AssetCounter::counter == 0);
		man->add(50);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 4);
		man->remove(50);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("immediate remove");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(10);
		man->add(10);
		man->remove(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("repeated add and remove");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(10);
		for (uint32 i = 0; i < 10; i++)
		{
			man->add(10);
			man->remove(10);
			if ((i % 3) == 0)
				waitProcessing(man);
		}
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("asset content");
		Holder<AssetManager> man = instantiate();
		constexpr const char Content[] = "hello world";
		makeAssetRaw(10, Content);
		man->add(10);
		waitProcessing(man);
		checkContents(man, 10, Content);
		man->remove(10);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("reload asset");
		Holder<AssetManager> man = instantiate();
		constexpr const char Content1[] = "hello world";
		constexpr const char Content2[] = "lorem ipsum dolor sit amet";
		makeAssetRaw(10, Content1);
		man->add(10);
		waitProcessing(man);
		makeAssetRaw(10, Content2);
		checkContents(man, 10, Content1);
		man->reload(10);
		waitProcessing(man);
		checkContents(man, 10, Content2);
		man->remove(10);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("holding asset after remove");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(10);
		man->add(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		{
			Holder<AssetCounter> a = man->get<AssetSchemeIndexCounter, AssetCounter>(10);
			man->remove(10);
			waitProcessing(man);
			CAGE_TEST(AssetCounter::counter == 1);
		}
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("fabricated");
		Holder<AssetManager> man = instantiate();
		{
			Holder<AssetCounter> f = systemArena().createHolder<AssetCounter>();
			man->fabricate<AssetSchemeIndexCounter, AssetCounter>(10, templates::move(f));
		}
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		CAGE_TEST((man->get<AssetSchemeIndexCounter, AssetCounter>(10)));
		man->remove(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("fabricated overlay");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(10);
		man->add(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		AssetCounter *ptr = nullptr;
		{
			Holder<AssetCounter> f = systemArena().createHolder<AssetCounter>();
			ptr = f.get();
			man->fabricate<AssetSchemeIndexCounter, AssetCounter>(10, templates::move(f));
		}
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		CAGE_TEST((man->get<AssetSchemeIndexCounter, AssetCounter>(10)).get() == ptr);
		man->remove(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		CAGE_TEST((man->get<AssetSchemeIndexCounter, AssetCounter>(10)).get() == ptr);
		man->remove(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("alias");
		Holder<AssetManager> man = instantiate();
		{
			AssetHeader hdr = initializeAssetHeader(stringizer() + 10, AssetSchemeIndexCounter);
			hdr.aliasName = 20;
			Holder<File> f = writeFile(pathJoin(AssetsPath, stringizer() + 10));
			f->write(bufferView(hdr));
			f->close();
		}
		man->add(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		CAGE_TEST((man->get<AssetSchemeIndexCounter, AssetCounter>(10)));
		CAGE_TEST((man->get<AssetSchemeIndexCounter, AssetCounter>(20)));
		man->remove(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("non-existent asset");
		Holder<AssetManager> man = instantiate();
		Holder<AssetCounter> a = man->get<AssetSchemeIndexCounter, AssetCounter>(5);
		CAGE_TEST(!a);
	}

	{
		CAGE_TESTCASE("missing asset file");
		Holder<AssetManager> man = instantiate();
		CAGE_TEST(AssetCounter::counter == 0);
		CAGE_TEST(!pathIsFile(pathJoin(AssetsPath, "10")));
		man->add(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->remove(10);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("getting asset with scheme mismatching type");
		Holder<AssetManager> man = instantiate();
		CAGE_TEST_ASSERTED((man->get<AssetSchemeIndexRaw, AssetCounter>(5)));
	}

	{
		CAGE_TESTCASE("getting asset with wrong scheme and type");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(10);
		man->add(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 1);
		CAGE_TEST_THROWN((man->get<AssetSchemeIndexRaw, MemoryBuffer>(10)));
		man->remove(10);
		man->unloadWait();
		CAGE_TEST(AssetCounter::counter == 0);
	}

	{
		CAGE_TESTCASE("unknown scheme");
		Holder<AssetManager> man = instantiate();
		{
			constexpr uint32 name = 10;
			AssetHeader hdr = initializeAssetHeader(stringizer() + name, 42);
			Holder<File> f = writeFile(pathJoin(AssetsPath, stringizer() + name));
			f->write(bufferView(hdr));
			f->close();
		}
		man->add(10);
		waitProcessing(man);
		CAGE_TEST_THROWN((man->get<AssetSchemeIndexCounter, AssetCounter>(10)));
		man->remove(10);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("accessing missing asset file");
		Holder<AssetManager> man = instantiate();
		man->add(10);
		waitProcessing(man);
		CAGE_TEST_THROWN((man->get<AssetSchemeIndexCounter, AssetCounter>(10)));
		man->remove(10);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("corrupted header");
		Holder<AssetManager> man = instantiate();
		{
			constexpr uint32 name = 10;
			AssetHeader hdr = initializeAssetHeader(stringizer() + name, AssetSchemeIndexCounter);
			Holder<File> f = writeFile(pathJoin(AssetsPath, stringizer() + name));
			const auto view1 = bufferView(hdr);
			const auto view2 = PointerRange<const char>(view1.begin(), view1.begin() + randomRange(uintPtr(0), view1.size() - 1u));
			f->write(view2);
			f->close();
		}
		man->add(10);
		waitProcessing(man);
		CAGE_TEST_THROWN((man->get<AssetSchemeIndexCounter, AssetCounter>(10)));
		man->remove(10);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("fail decompression");
		Holder<AssetManager> man = instantiate();
		{
			constexpr const char Content[] = "lorem ipsum dolor sit amet";
			constexpr uint32 name = 10;
			AssetHeader hdr = initializeAssetHeader(stringizer() + name, AssetSchemeIndexRaw);
			hdr.compressedSize = sizeof(Content);
			hdr.originalSize = sizeof(Content) * 2;
			Holder<File> f = writeFile(pathJoin(AssetsPath, stringizer() + name));
			f->write(bufferView(hdr));
			f->write(Content);
			f->close();
		}
		{
			detail::setGlobalBreakpointOverride(false); // the decompression thread would stop on breakpoint otherwise
			man->add(10);
			waitProcessing(man);
			detail::setGlobalBreakpointOverride(true);
		}
		CAGE_TEST_THROWN((man->get<AssetSchemeIndexRaw, MemoryBuffer>(10)));
		man->remove(10);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("fail processing");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(5000);
		{
			detail::setGlobalBreakpointOverride(false); // the processing thread would stop on breakpoint otherwise
			man->add(5000);
			waitProcessing(man);
			detail::setGlobalBreakpointOverride(true);
		}
		CAGE_TEST_THROWN((man->get<AssetSchemeIndexCounter, AssetCounter>(5000)));
		man->remove(5000);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("fail processing with dependencies 1");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(10);
		makeAssetCounter(20);
		makeAssetCounter(5000, PointerRange<const uint32>({ (uint32)10, (uint32)20 }));
		{
			detail::setGlobalBreakpointOverride(false); // the processing thread would stop on breakpoint otherwise
			man->add(5000);
			waitProcessing(man);
			detail::setGlobalBreakpointOverride(true);
		}
		CAGE_TEST(AssetCounter::counter == 2);
		CAGE_TEST((man->get<AssetSchemeIndexCounter, AssetCounter>(10)));
		CAGE_TEST((man->get<AssetSchemeIndexCounter, AssetCounter>(20)));
		CAGE_TEST_THROWN((man->get<AssetSchemeIndexCounter, AssetCounter>(5000)));
		man->remove(5000);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->unloadWait();
	}

	{
		CAGE_TESTCASE("fail processing with dependencies 2");
		Holder<AssetManager> man = instantiate();
		makeAssetCounter(10, PointerRange<const uint32>({ (uint32)20, (uint32)5000 }));
		makeAssetCounter(20);
		makeAssetCounter(5000);
		{
			detail::setGlobalBreakpointOverride(false); // the processing thread would stop on breakpoint otherwise
			man->add(10);
			waitProcessing(man);
			detail::setGlobalBreakpointOverride(true);
		}
		CAGE_TEST(AssetCounter::counter == 2);
		CAGE_TEST((man->get<AssetSchemeIndexCounter, AssetCounter>(10)));
		CAGE_TEST((man->get<AssetSchemeIndexCounter, AssetCounter>(20)));
		CAGE_TEST_THROWN((man->get<AssetSchemeIndexCounter, AssetCounter>(5000)));
		man->remove(10);
		waitProcessing(man);
		CAGE_TEST(AssetCounter::counter == 0);
		man->unloadWait();
	}
}
