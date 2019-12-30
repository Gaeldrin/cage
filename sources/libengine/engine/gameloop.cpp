#include <atomic>
#include <exception>

#include <cage-core/core.h>
#include <cage-core/math.h>
#include <cage-core/camera.h>
#include <cage-core/config.h>
#include <cage-core/entities.h>
#include <cage-core/concurrent.h>
#include <cage-core/files.h> // getExecutableName
#include <cage-core/assetStructs.h>
#include <cage-core/assetManager.h>
#include <cage-core/hashString.h>
#include <cage-core/collisionMesh.h> // for sizeof in defineScheme
#include <cage-core/textPack.h> // for sizeof in defineScheme
#include <cage-core/memoryBuffer.h> // for sizeof in defineScheme
#include <cage-core/threadPool.h>
#include <cage-core/variableSmoothingBuffer.h>
#include <cage-core/swapBufferGuard.h>

#define CAGE_EXPORT
#include <cage-core/core/macro/api.h>
#include <cage-engine/core.h>
#include <cage-engine/graphics.h>
#include <cage-engine/window.h>
#include <cage-engine/sound.h>
#include <cage-engine/gui.h>
#include <cage-engine/engine.h>
#include <cage-engine/engineProfiling.h>

#include "engine.h"

//#define CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS

namespace cage
{
	namespace
	{
		ConfigBool confAutoAssetListen("cage/assets/listen", false);
		ConfigUint32 confOptickFrameMode("cage/profiling/frameMode", 1);
		ConfigBool confSimpleShaders("cage/graphics/simpleShaders", false);

		struct graphicsUploadThreadClass
		{
#ifdef CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
			static const uint32 threadIndex = 2;
#else
			static const uint32 threadIndex = EngineGraphicsDispatchThread::threadIndex;
#endif // CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
		};

		struct scopedSemaphores
		{
			scopedSemaphores(Holder<Semaphore> &lock, Holder<Semaphore> &unlock) : sem(unlock.get())
			{
				lock->lock();
			}

			~scopedSemaphores()
			{
				sem->unlock();
			}

		private:
			Semaphore *sem;
		};

		struct scopedTimer
		{
			VariableSmoothingBuffer<uint64, 60> &vsb;
			uint64 st;

			scopedTimer(VariableSmoothingBuffer<uint64, 60> &vsb) : vsb(vsb), st(getApplicationTime())
			{}

			~scopedTimer()
			{
				uint64 et = getApplicationTime();
				vsb.add(et - st);
			}
		};

		struct engineDataStruct
		{
			VariableSmoothingBuffer<uint64, 60> profilingBufferControl;
			VariableSmoothingBuffer<uint64, 60> profilingBufferSound;
			VariableSmoothingBuffer<uint64, 60> profilingBufferGraphicsPrepare;
			VariableSmoothingBuffer<uint64, 60> profilingBufferGraphicsDispatch;
			VariableSmoothingBuffer<uint64, 60> profilingBufferFrameTime;
			VariableSmoothingBuffer<uint64, 60> profilingBufferDrawCalls;
			VariableSmoothingBuffer<uint64, 60> profilingBufferDrawPrimitives;
			VariableSmoothingBuffer<uint64, 60> profilingBufferEntities;

			Holder<AssetManager> assets;
			Holder<Window> window;
#ifdef CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
			Holder<Window> windowUpload;
#endif // CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
			Holder<SoundContext> sound;
			Holder<Speaker> speaker;
			Holder<MixingBus> masterBus;
			Holder<MixingBus> musicBus;
			Holder<MixingBus> effectsBus;
			Holder<MixingBus> guiBus;
			Holder<Gui> gui;
			Holder<EntityManager> entities;

			Holder<Mutex> assetsSoundMutex;
			Holder<Mutex> assetsGraphicsMutex;
			Holder<Semaphore> graphicsSemaphore1;
			Holder<Semaphore> graphicsSemaphore2;
			Holder<Barrier> threadsStateBarier;
			Holder<Thread> graphicsDispatchThreadHolder;
			Holder<Thread> graphicsPrepareThreadHolder;
#ifdef CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
			Holder<Thread> graphicsUploadThreadHolder;
#endif // CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
			Holder<Thread> soundThreadHolder;
			Holder<ThreadPool> emitThreadsHolder;

			std::atomic<uint32> engineStarted;
			std::atomic<bool> stopping;
			uint64 currentControlTime;
			uint64 currentSoundTime;
			uint32 assetSyncAttempts;
			uint32 assetShaderTier;

			engineDataStruct(const EngineCreateConfig &config);

			~engineDataStruct();

			//////////////////////////////////////
			// graphics PREPARE
			//////////////////////////////////////

			void graphicsPrepareInitializeStage()
			{}

			void graphicsPrepareStep()
			{
				{
					OPTICK_EVENT("assets");
					assets->processCustomThread(graphicsPrepareThread().threadIndex);
				}
				scopedSemaphores lockGraphics(graphicsSemaphore1, graphicsSemaphore2);
				ScopeLock<Mutex> lockAssets(assetsGraphicsMutex);
				scopedTimer timing(profilingBufferGraphicsPrepare);
				{
					OPTICK_EVENT("callback");
					graphicsPrepareThread().prepare.dispatch();
				}
				{
					OPTICK_EVENT("tick");
					graphicsPrepareTick(getApplicationTime());
				}
			}

			void graphicsPrepareGameloopStage()
			{
				while (!stopping)
				{
					if (confOptickFrameMode == EngineGraphicsPrepareThread::threadIndex)
					{
						OPTICK_FRAME("engine graphics prepare");
						graphicsPrepareStep();
					}
					else
					{
						OPTICK_EVENT("engine graphics prepare");
						graphicsPrepareStep();
					}
				}
			}

			void graphicsPrepareStopStage()
			{
				graphicsSemaphore2->unlock();
			}

			void graphicsPrepareFinalizeStage()
			{
				while (assets->countTotal() > 0)
				{
					while (assets->processCustomThread(graphicsPrepareThread().threadIndex));
					threadSleep(5000);
				}
			}

			//////////////////////////////////////
			// graphics DISPATCH
			//////////////////////////////////////

			void graphicsDispatchInitializeStage()
			{
				window->makeCurrent();
				gui->graphicsInitialize();
				graphicsDispatchInitialize();
			}

			void graphicsDispatchStep()
			{
				scopedTimer timing(profilingBufferFrameTime);
				{
					scopedSemaphores lockGraphics(graphicsSemaphore2, graphicsSemaphore1);
					scopedTimer timing(profilingBufferGraphicsDispatch);
					{
						OPTICK_EVENT("render callback");
						graphicsDispatchThread().render.dispatch();
					}
					{
						OPTICK_EVENT("tick");
						uint32 drawCalls = 0, drawPrimitives = 0;
						graphicsDispatchTick(drawCalls, drawPrimitives);
						profilingBufferDrawCalls.add(drawCalls);
						profilingBufferDrawPrimitives.add(drawPrimitives);
						OPTICK_TAG("drawCalls", drawCalls);
						OPTICK_TAG("drawPrimitives", drawPrimitives);
					}
				}
				if (graphicsPrepareThread().stereoMode == StereoModeEnum::Mono)
				{
					OPTICK_EVENT("gui render");
					gui->graphicsRender();
					CAGE_CHECK_GL_ERROR_DEBUG();
				}
				{
					OPTICK_EVENT("assets");
					assets->processCustomThread(EngineGraphicsDispatchThread::threadIndex);
				}
				{
					OPTICK_EVENT("swap callback");
					graphicsDispatchThread().swap.dispatch();
				}
				{
					OPTICK_EVENT("swap");
					graphicsDispatchSwap();
				}
			}

			void graphicsDispatchGameloopStage()
			{
				while (!stopping)
				{
					if (confOptickFrameMode == EngineGraphicsDispatchThread::threadIndex)
					{
						OPTICK_FRAME("engine graphics dispatch");
						graphicsDispatchStep();
					}
					else
					{
						OPTICK_EVENT("engine graphics dispatch");
						graphicsDispatchStep();
					}
				}
			}

			void graphicsDispatchStopStage()
			{
				graphicsSemaphore1->unlock();
			}

			void graphicsDispatchFinalizeStage()
			{
				gui->graphicsFinalize();
				graphicsDispatchFinalize();
				while (assets->countTotal() > 0)
				{
					while (assets->processCustomThread(graphicsDispatchThread().threadIndex));
					threadSleep(5000);
				}
			}

			//////////////////////////////////////
			// SOUND
			//////////////////////////////////////

			void soundInitializeStage()
			{
				//gui->soundInitialize(sound.get());
			}

			void soundTiming(uint64 timeDelay)
			{
				if (timeDelay > soundThread().timePerTick * 2)
				{
					uint64 skip = timeDelay / soundThread().timePerTick + 1;
					CAGE_LOG(SeverityEnum::Warning, "engine", stringizer() + "skipping " + skip + " sound ticks");
					currentSoundTime += skip * soundThread().timePerTick;
				}
				else
				{
					if (timeDelay < soundThread().timePerTick)
					{
						OPTICK_EVENT("sleep");
						threadSleep(soundThread().timePerTick - timeDelay);
					}
					currentSoundTime += soundThread().timePerTick;
				}
			}

			void soundStep()
			{
				{
					ScopeLock<Mutex> lockAssets(assetsSoundMutex);
					scopedTimer timing(profilingBufferSound);
					{
						OPTICK_EVENT("sound callback");
						soundThread().sound.dispatch();
					}
					{
						OPTICK_EVENT("tick");
						soundTick(currentSoundTime);
					}
				}
				{
					OPTICK_EVENT("assets");
					assets->processCustomThread(EngineSoundThread::threadIndex);
				}
				uint64 newTime = getApplicationTime();
				soundTiming(newTime > currentSoundTime ? newTime - currentSoundTime : 0);
			}

			void soundGameloopStage()
			{
				while (!stopping)
				{
					if (confOptickFrameMode == EngineSoundThread::threadIndex)
					{
						OPTICK_FRAME("engine sound");
						soundStep();
					}
					else
					{
						OPTICK_EVENT("engine sound");
						soundStep();
					}
				}
			}

			void soundStopStage()
			{
				// nothing
			}

			void soundFinalizeStage()
			{
				soundFinalize();
				//gui->soundFinalize();
				while (assets->countTotal() > 0)
				{
					while (assets->processCustomThread(soundThread().threadIndex));
					threadSleep(5000);
				}
			}

			//////////////////////////////////////
			// CONTROL
			//////////////////////////////////////

			void controlAssets()
			{
				OPTICK_EVENT("assets");
				{
					assetSyncAttempts++;
					OPTICK_TAG("assetSyncAttempts", assetSyncAttempts);
					ScopeLock<Mutex> lockGraphics(assetsGraphicsMutex, assetSyncAttempts < 20);
					if (lockGraphics)
					{
						ScopeLock<Mutex> lockSound(assetsSoundMutex, assetSyncAttempts < 10);
						if (lockSound)
						{
							{
								OPTICK_EVENT("callback");
								controlThread().assets.dispatch();
							}
							{
								OPTICK_EVENT("control");
								while (assets->processControlThread());
							}
							assetSyncAttempts = 0;
						}
					}
				}
				{
					OPTICK_EVENT("assets");
					while (assets->processCustomThread(EngineControlThread::threadIndex));
				}
			}

			void updateHistoryComponents()
			{
				OPTICK_EVENT("update history");
				for (Entity *e : TransformComponent::component->entities())
				{
					CAGE_COMPONENT_ENGINE(Transform, ts, e);
					TransformComponent &hs = e->value<TransformComponent>(TransformComponent::componentHistory);
					hs = ts;
				}
			}

			void emitThreadsEntry(uint32 index, uint32)
			{
				switch (index)
				{
				case 0:
				{
					OPTICK_EVENT("sound emit");
					soundEmit(currentSoundTime);
				} break;
				case 1:
				{
					OPTICK_EVENT("gui emit");
					gui->controlUpdateDone();
				} break;
				case 2:
				{
					OPTICK_EVENT("graphics emit");
					graphicsPrepareEmit(currentControlTime);
				} break;
				default:
					CAGE_THROW_CRITICAL(Exception, "invalid engine emit thread index");
				}
			}

			void controlTiming(uint64 timeDelay)
			{
				if (timeDelay > controlThread().timePerTick * 2)
				{
					uint64 skip = timeDelay / controlThread().timePerTick + 1;
					CAGE_LOG(SeverityEnum::Warning, "engine", stringizer() + "skipping " + skip + " control update ticks");
					currentControlTime += skip * controlThread().timePerTick;
				}
				else
				{
					if (timeDelay < controlThread().timePerTick)
					{
						OPTICK_EVENT("sleep");
						threadSleep(controlThread().timePerTick - timeDelay);
					}
					currentControlTime += controlThread().timePerTick;
				}
			}

			void controlStep()
			{
				updateHistoryComponents();
				{
					OPTICK_EVENT("gui update");
					gui->setOutputResolution(window->resolution());
					gui->controlUpdateStart();
				}
				{
					OPTICK_EVENT("window events");
					window->processEvents();
				}
				{
					scopedTimer timing(profilingBufferControl);
					OPTICK_EVENT("application update");
					controlThread().update.dispatch();
				}
				{
					OPTICK_EVENT("emit");
					OPTICK_TAG("entitiesCount", entities->group()->count());
					profilingBufferEntities.add(entities->group()->count());
					emitThreadsHolder->run();
				}
				controlAssets();
				uint64 newTime = getApplicationTime();
				controlTiming(newTime > currentControlTime ? newTime - currentControlTime : 0);
			}

			void controlGameloopStage()
			{
				while (!stopping)
				{
					if (confOptickFrameMode == EngineControlThread::threadIndex)
					{
						OPTICK_FRAME("engine control");
						controlStep();
					}
					else
					{
						OPTICK_EVENT("engine control");
						controlStep();
					}
				}
			}

			//////////////////////////////////////
			// ENTRY methods
			//////////////////////////////////////

#define GCHL_GENERATE_CATCH(NAME, STAGE) \
			catch (const cage::Exception &e) { CAGE_LOG(SeverityEnum::Error, "engine", stringizer() + "caught cage::exception in " CAGE_STRINGIZE(STAGE) " in " CAGE_STRINGIZE(NAME) ": " + e.message); engineStop(); } \
			catch (const std::exception &e) { CAGE_LOG(SeverityEnum::Error, "engine", stringizer() + "caught std::exception in " CAGE_STRINGIZE(STAGE) " in " CAGE_STRINGIZE(NAME) ": " + e.what()); engineStop(); } \
			catch (...) { CAGE_LOG(SeverityEnum::Error, "engine", "caught unknown exception in " CAGE_STRINGIZE(STAGE) " in " CAGE_STRINGIZE(NAME)); engineStop(); }
#define GCHL_GENERATE_ENTRY(NAME) \
			void CAGE_JOIN(NAME, Entry)() \
			{ \
				try { CAGE_JOIN(NAME, InitializeStage)(); } \
				GCHL_GENERATE_CATCH(NAME, initialization (engine)) \
				{ ScopeLock<Barrier> l(threadsStateBarier); } \
				{ ScopeLock<Barrier> l(threadsStateBarier); } \
				try { CAGE_JOIN(NAME, Thread)().initialize.dispatch(); } \
				GCHL_GENERATE_CATCH(NAME, initialization (application)) \
				{ ScopeLock<Barrier> l(threadsStateBarier); } \
				try { CAGE_JOIN(NAME, GameloopStage)(); } \
				GCHL_GENERATE_CATCH(NAME, gameloop) \
				CAGE_JOIN(NAME, StopStage)(); \
				{ ScopeLock<Barrier> l(threadsStateBarier); } \
				try { CAGE_JOIN(NAME, Thread)().finalize.dispatch(); } \
				GCHL_GENERATE_CATCH(NAME, finalization (application)) \
				try { CAGE_JOIN(NAME, FinalizeStage)(); } \
				GCHL_GENERATE_CATCH(NAME, finalization (engine)) \
			}
			CAGE_EVAL_SMALL(CAGE_EXPAND_ARGS(GCHL_GENERATE_ENTRY, graphicsPrepare, graphicsDispatch, sound));
#undef GCHL_GENERATE_ENTRY

#ifdef CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
			void graphicsUploadEntry()
			{
				windowUpload->makeCurrent();
				while (!stopping)
				{
					while (assets->processCustomThread(graphicsUploadThreadClass::threadIndex));
					threadSleep(5000);
				}
				while (assets->countTotal() > 0)
				{
					while (assets->processCustomThread(graphicsUploadThreadClass::threadIndex));
					threadSleep(5000);
				}
				windowUpload->makeNotCurrent();
			}
#endif // CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS

			void initialize(const EngineCreateConfig &config)
			{
				CAGE_ASSERT(engineStarted == 0);
				engineStarted = 1;

				CAGE_LOG(SeverityEnum::Info, "engine", "initializing engine");

				{ // create assets manager
					AssetManagerCreateConfig c;
					if (config.assets)
						c = *config.assets;
					c.schemeMaxCount = max(c.schemeMaxCount, 30u);
					c.threadMaxCount = max(c.threadMaxCount, 5u);
					assets = newAssetManager(c);
				}

				{ // create graphics
					window = newWindow();
					window->makeNotCurrent();
#ifdef CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
					windowUpload = newWindow(window.get());
					windowUpload->makeNotCurrent();
					windowUpload->modeSetHidden();
#endif // CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
				}

				{ // create sound
					string name = pathExtractFilename(detail::getExecutableFullPathNoExe());
					sound = newSoundContext(config.soundContext ? *config.soundContext : SoundContextCreateConfig(), name);
					speaker = newSpeakerOutput(sound.get(), config.speaker ? *config.speaker : SpeakerCreateConfig(), name);
					masterBus = newMixingBus(sound.get());
					musicBus = newMixingBus(sound.get());
					effectsBus = newMixingBus(sound.get());
					guiBus = newMixingBus(sound.get());
					speaker->setInput(masterBus.get());
					masterBus->addInput(musicBus.get());
					masterBus->addInput(effectsBus.get());
					masterBus->addInput(guiBus.get());
				}

				{ // create gui
					GuiCreateConfig c;
					if (config.gui)
						c = *config.gui;
					c.assetMgr = assets.get();
					gui = newGuiManager(c);
					gui->handleWindowEvents(window.get());
					gui->setOutputSoundBus(guiBus.get());
				}

				{ // create entities
					entities = newEntityManager(config.entities ? *config.entities : EntityManagerCreateConfig());
				}

				{ // create sync objects
					threadsStateBarier = newBarrier(4);
					assetsSoundMutex = newMutex();
					assetsGraphicsMutex = newMutex();
					graphicsSemaphore1 = newSemaphore(1, 1);
					graphicsSemaphore2 = newSemaphore(0, 1);
				}

				{ // create threads
					graphicsDispatchThreadHolder = newThread(Delegate<void()>().bind<engineDataStruct, &engineDataStruct::graphicsDispatchEntry>(this), "engine graphics dispatch");
					graphicsPrepareThreadHolder = newThread(Delegate<void()>().bind<engineDataStruct, &engineDataStruct::graphicsPrepareEntry>(this), "engine graphics prepare");
#ifdef CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
					graphicsUploadThreadHolder = newThread(Delegate<void()>().bind<engineDataStruct, &engineDataStruct::graphicsUploadEntry>(this), "engine graphics upload");
#endif // CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
					soundThreadHolder = newThread(Delegate<void()>().bind<engineDataStruct, &engineDataStruct::soundEntry>(this), "engine sound");
					emitThreadsHolder = newThreadPool("engine emit ", 3);
					emitThreadsHolder->function.bind<engineDataStruct, &engineDataStruct::emitThreadsEntry>(this);
				}

				{ // initialize asset schemes
					// core assets
					assets->defineScheme<void>(assetSchemeIndexPack, genAssetSchemePack(EngineControlThread::threadIndex));
					assets->defineScheme<MemoryBuffer>(assetSchemeIndexRaw, genAssetSchemeRaw(EngineControlThread::threadIndex));
					assets->defineScheme<TextPack>(assetSchemeIndexTextPack, genAssetSchemeTextPack(EngineControlThread::threadIndex));
					assets->defineScheme<CollisionMesh>(assetSchemeIndexCollisionMesh, genAssetSchemeCollisionMesh(EngineControlThread::threadIndex));
					// client assets
					assets->defineScheme<ShaderProgram>(assetSchemeIndexShaderProgram, genAssetSchemeShaderProgram(graphicsUploadThreadClass::threadIndex, window.get()));
					assets->defineScheme<Texture>(assetSchemeIndexRenderTexture, genAssetSchemeRenderTexture(graphicsUploadThreadClass::threadIndex, window.get()));
					assets->defineScheme<Mesh>(assetSchemeIndexMesh, genAssetSchemeRenderMesh(EngineGraphicsDispatchThread::threadIndex, window.get()));
					assets->defineScheme<SkeletonRig>(assetSchemeIndexSkeletonRig, genAssetSchemeSkeletonRig(EngineGraphicsPrepareThread::threadIndex));
					assets->defineScheme<SkeletalAnimation>(assetSchemeIndexSkeletalAnimation, genAssetSchemeSkeletalAnimation(EngineGraphicsPrepareThread::threadIndex));
					assets->defineScheme<RenderObject>(assetSchemeIndexRenderObject, genAssetSchemeRenderObject(EngineGraphicsPrepareThread::threadIndex));
					assets->defineScheme<Font>(assetSchemeIndexFontFace, genAssetSchemeFontFace(graphicsUploadThreadClass::threadIndex, window.get()));
					assets->defineScheme<SoundSource>(assetSchemeIndexSoundSource, genAssetSchemeSoundSource(EngineSoundThread::threadIndex, sound.get()));
					// cage pack
					assets->add(HashString("cage/cage.pack"));
					assetShaderTier = confSimpleShaders ? HashString("cage/shader/engine/low.pack") : HashString("cage/shader/engine/high.pack");
					assets->add(assetShaderTier);
				}

				{ // initialize assets change listening
					if (confAutoAssetListen)
					{
						CAGE_LOG(SeverityEnum::Info, "assets", "enabling assets updates listening");
						detail::OverrideBreakpoint OverrideBreakpoint;
						detail::OverrideException overrideException;
						try
						{
							assets->listen("localhost", 65042);
						}
						catch (const Exception &)
						{
							CAGE_LOG(SeverityEnum::Warning, "assets", "assets updates failed to connect to the database");
						}
					}
				}

				{ // initialize entity components
					EntityManager *entityMgr = entities.get();
					TransformComponent::component = entityMgr->defineComponent(TransformComponent(), true);
					TransformComponent::componentHistory = entityMgr->defineComponent(TransformComponent(), false);
					RenderComponent::component = entityMgr->defineComponent(RenderComponent(), true);
					TextureAnimationComponent::component = entityMgr->defineComponent(TextureAnimationComponent(), false);
					SkeletalAnimationComponent::component = entityMgr->defineComponent(SkeletalAnimationComponent(), false);
					LightComponent::component = entityMgr->defineComponent(LightComponent(), true);
					ShadowmapComponent::component = entityMgr->defineComponent(ShadowmapComponent(), false);
					RenderTextComponent::component = entityMgr->defineComponent(RenderTextComponent(), true);
					CameraComponent::component = entityMgr->defineComponent(CameraComponent(), true);
					SoundComponent::component = entityMgr->defineComponent(SoundComponent(), true);
					ListenerComponent::component = entityMgr->defineComponent(ListenerComponent(), true);
				}

				{ ScopeLock<Barrier> l(threadsStateBarier); }
				CAGE_LOG(SeverityEnum::Info, "engine", "engine initialized");

				CAGE_ASSERT(engineStarted == 1);
				engineStarted = 2;
			}

			void start()
			{
				OPTICK_THREAD("engine control");
				CAGE_ASSERT(engineStarted == 2);
				engineStarted = 3;

				try
				{
					controlThread().initialize.dispatch();
				}
				GCHL_GENERATE_CATCH(control, initialization (application))

				CAGE_LOG(SeverityEnum::Info, "engine", "starting engine");

				currentControlTime = currentSoundTime = getApplicationTime();

				{ ScopeLock<Barrier> l(threadsStateBarier); }
				{ ScopeLock<Barrier> l(threadsStateBarier); }

				try
				{
					controlGameloopStage();
				}
				GCHL_GENERATE_CATCH(control, gameloop)

				CAGE_LOG(SeverityEnum::Info, "engine", "engine stopped");

				try
				{
					controlThread().finalize.dispatch();
				}
				GCHL_GENERATE_CATCH(control, finalization (application))

				CAGE_ASSERT(engineStarted == 3);
				engineStarted = 4;
			}

			void finalize()
			{
				CAGE_ASSERT(engineStarted == 4);
				engineStarted = 5;

				CAGE_LOG(SeverityEnum::Info, "engine", "finalizing engine");
				{ ScopeLock<Barrier> l(threadsStateBarier); }

				{ // unload assets
					assets->remove(HashString("cage/cage.pack"));
					assets->remove(assetShaderTier);
					while (assets->countTotal() > 0)
					{
						try
						{
							controlThread().assets.dispatch();
						}
						GCHL_GENERATE_CATCH(control, finalization (unloading assets))
						while (assets->processCustomThread(controlThread().threadIndex) || assets->processControlThread());
						threadSleep(5000);
					}
				}

				{ // wait for threads to finish
					graphicsPrepareThreadHolder->wait();
					graphicsDispatchThreadHolder->wait();
#ifdef CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
					graphicsUploadThreadHolder->wait();
#endif // CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
					soundThreadHolder->wait();
				}

				{ // destroy entities
					entities.clear();
				}

				{ // destroy gui
					gui.clear();
				}

				{ // destroy sound
					effectsBus.clear();
					musicBus.clear();
					guiBus.clear();
					masterBus.clear();
					speaker.clear();
					sound.clear();
				}

				{ // destroy graphics
					if (window)
						window->makeCurrent();
					window.clear();
#ifdef CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
					if (windowUpload)
						windowUpload->makeCurrent();
					windowUpload.clear();
#endif // CAGE_USE_SEPARATE_THREAD_FOR_GPU_UPLOADS
				}

				{ // destroy assets
					assets.clear();
				}

				CAGE_LOG(SeverityEnum::Info, "engine", "engine finalized");

				CAGE_ASSERT(engineStarted == 5);
				engineStarted = 6;
			}
		};

		Holder<engineDataStruct> engineData;

		engineDataStruct::engineDataStruct(const EngineCreateConfig &config) : engineStarted(0), stopping(false), currentControlTime(0), currentSoundTime(0), assetSyncAttempts(0), assetShaderTier(0)
		{
			CAGE_LOG(SeverityEnum::Info, "engine", "creating engine");
			graphicsDispatchCreate(config);
			graphicsPrepareCreate(config);
			soundCreate(config);
			CAGE_LOG(SeverityEnum::Info, "engine", "engine created");
		}

		engineDataStruct::~engineDataStruct()
		{
			CAGE_LOG(SeverityEnum::Info, "engine", "destroying engine");
			soundDestroy();
			graphicsPrepareDestroy();
			graphicsDispatchDestroy();
			CAGE_LOG(SeverityEnum::Info, "engine", "engine destroyed");
		}
	}

	void engineInitialize(const EngineCreateConfig &config)
	{
		CAGE_ASSERT(!engineData);
		engineData = detail::systemArena().createHolder<engineDataStruct>(config);
		engineData->initialize(config);
	}

	void engineStart()
	{
		CAGE_ASSERT(engineData);
		engineData->start();
	}

	void engineStop()
	{
		CAGE_ASSERT(engineData);
		if (!engineData->stopping.exchange(true))
		{
			CAGE_LOG(SeverityEnum::Info, "engine", "stopping engine");
		}
	}

	void engineFinalize()
	{
		CAGE_ASSERT(engineData);
		engineData->finalize();
		engineData.clear();
	}

	SoundContext *sound()
	{
		return engineData->sound.get();
	}

	AssetManager *assets()
	{
		return engineData->assets.get();
	}

	EntityManager *entities()
	{
		return engineData->entities.get();
	}

	Window *window()
	{
		return engineData->window.get();
	}

	Gui *gui()
	{
		return engineData->gui.get();
	}

	Speaker *speaker()
	{
		return engineData->speaker.get();
	}

	MixingBus *masterMixer()
	{
		return engineData->masterBus.get();
	}

	MixingBus *musicMixer()
	{
		return engineData->musicBus.get();
	}

	MixingBus *effectsMixer()
	{
		return engineData->effectsBus.get();
	}

	MixingBus *guiMixer()
	{
		return engineData->guiBus.get();
	}

	uint64 currentControlTime()
	{
		return engineData->currentControlTime;
	}

	uint64 engineProfilingValues(EngineProfilingStatsFlags flags, EngineProfilingModeEnum mode)
	{
		uint64 result = 0;
#define GCHL_GENERATE(NAME) \
		if ((flags & EngineProfilingStatsFlags::NAME) == EngineProfilingStatsFlags::NAME) \
		{ \
			auto &buffer = CAGE_JOIN(engineData->profilingBuffer, NAME); \
			switch (mode) \
			{ \
			case EngineProfilingModeEnum::Average: result += buffer.smooth(); break; \
			case EngineProfilingModeEnum::Maximum: result += buffer.max(); break; \
			case EngineProfilingModeEnum::Last: result += buffer.current(); break; \
			default: CAGE_THROW_CRITICAL(Exception, "invalid profiling mode enum"); \
			} \
		}
		CAGE_EVAL_SMALL(CAGE_EXPAND_ARGS(GCHL_GENERATE,
			Control,
			Sound,
			GraphicsPrepare,
			GraphicsDispatch,
			FrameTime,
			DrawCalls,
			DrawPrimitives,
			Entities
		));
#undef GCHL_GENERATE
		return result;
	}
}
