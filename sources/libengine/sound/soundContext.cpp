#include <cage-core/core.h>
#include <cage-core/math.h>
#include <cage-core/memory.h>
#include "private.h"

#define CAGE_EXPORT
#include <cage-core/core/macro/api.h>
#include <cage-engine/core.h>
#include <cage-engine/sound.h>

namespace cage
{
	namespace
	{
		bool rtprioWarningCallbackOnce()
		{
			CAGE_LOG(SeverityEnum::Warning, "sound", "failed to set thread priority for audio");
			return true;
		}

		void rtprioWarningCallback()
		{
			static bool fired = rtprioWarningCallbackOnce();
		}

		class soundContextImpl : public SoundContext
		{
		public:
			string name;
			SoundIo *soundio;
			memoryArenaGrowing<memoryAllocatorPolicyPool<sizeof(templates::allocatorSizeSet<void*>)>, memoryConcurrentPolicyNone> linksMemory;

			soundContextImpl(const SoundContextCreateConfig &config, const string &name) : name(name.replace(":", "_")), soundio(nullptr), linksMemory(config.linksMemory)
			{
				CAGE_LOG(SeverityEnum::Info, "sound", stringizer() + "creating sound context, name: '" + name + "'");
				soundio = soundio_create();
				if (!soundio)
					CAGE_THROW_ERROR(SoundError, "error create soundio", 0);
				soundio->app_name = this->name.c_str();
				soundio->emit_rtprio_warning = &rtprioWarningCallback;
				checkSoundIoError(soundio_connect(soundio));
				soundio_flush_events(soundio);
			}

			~soundContextImpl()
			{
				soundio_disconnect(soundio);
				soundio_destroy(soundio);
			}
		};
	}

	namespace soundPrivat
	{
		SoundIo *soundioFromContext(SoundContext *context)
		{
			soundContextImpl *impl = (soundContextImpl*)context;
			return impl->soundio;
		}

		MemoryArena linksArenaFromContext(SoundContext *context)
		{
			soundContextImpl *impl = (soundContextImpl*)context;
			return MemoryArena(&impl->linksMemory);
		}
	}

	string SoundContext::getContextName() const
	{
		soundContextImpl *impl = (soundContextImpl*)this;
		return impl->name;
	}

	SoundContextCreateConfig::SoundContextCreateConfig() : linksMemory(1024 * 1024)
	{}

	Holder<SoundContext> newSoundContext(const SoundContextCreateConfig &config, const string &name)
	{
		return detail::systemArena().createImpl<SoundContext, soundContextImpl>(config, name);
	}
}
