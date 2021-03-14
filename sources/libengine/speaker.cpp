#include <cage-core/string.h>
#include <cage-core/files.h>
#include <cage-engine/speaker.h>

#include <cubeb/cubeb.h>
#ifdef CAGE_SYSTEM_WINDOWS
#include <Objbase.h>
#endif // CAGE_SYSTEM_WINDOWS

//#define GCHL_CUBEB_LOGGING
#ifdef GCHL_CUBEB_LOGGING
#include <cstdarg>
#endif

namespace cage
{
	void cageCheckCubebError(int code)
	{
		switch (code)
		{
		case CUBEB_OK:
			return;
		case CUBEB_ERROR:
			CAGE_THROW_ERROR(Exception, "generic sound error");
			break;
		case CUBEB_ERROR_INVALID_FORMAT:
			CAGE_THROW_ERROR(Exception, "invalid sound format");
			break;
		case CUBEB_ERROR_INVALID_PARAMETER:
			CAGE_THROW_ERROR(Exception, "invalid sound parameter");
			break;
		case CUBEB_ERROR_NOT_SUPPORTED:
			CAGE_THROW_ERROR(Exception, "sound not supported error");
			break;
		case CUBEB_ERROR_DEVICE_UNAVAILABLE:
			CAGE_THROW_ERROR(Exception, "sound device unavailable");
			break;
		default:
			CAGE_THROW_CRITICAL(SystemError, "unknown sound error", code);
		}
	}

	namespace
	{
#ifdef GCHL_CUBEB_LOGGING
		void soundLogCallback(const char *fmt, ...)
		{
			char buffer[512];
			va_list args;
			va_start(args, fmt);
			vsnprintf(buffer, 500, fmt, args);
			va_end(args);
			CAGE_LOG(SeverityEnum::Info, "cubeb", buffer);
		}
#endif

		class CageCubebInitializer
		{
		public:
			cubeb *c = nullptr;

			CageCubebInitializer()
			{
#ifdef GCHL_CUBEB_LOGGING
				cageCheckCubebError(cubeb_set_log_callback(CUBEB_LOG_VERBOSE, &soundLogCallback));
#endif
#ifdef CAGE_SYSTEM_WINDOWS
				CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif // CAGE_SYSTEM_WINDOWS
				const string name = replace(pathExtractFilename(detail::getExecutableFullPathNoExe()), ":", "_");
				CAGE_LOG(SeverityEnum::Info, "cubeb", stringizer() + "creating cubeb context, name: '" + name + "'");
				cageCheckCubebError(cubeb_init(&c, name.c_str(), nullptr));
				CAGE_ASSERT(c);
				CAGE_LOG(SeverityEnum::Info, "cubeb", stringizer() + "using cubeb backend: '" + cubeb_get_backend_id(c) + "'");
			}

			~CageCubebInitializer()
			{
				CAGE_LOG(SeverityEnum::Info, "cubeb", stringizer() + "destroying cubeb context");
				cubeb_destroy(c);
#ifdef CAGE_SYSTEM_WINDOWS
				CoUninitialize();
#endif // CAGE_SYSTEM_WINDOWS
			}
		};
	}

	cubeb *cageCubebInitializeFunc()
	{
		static CageCubebInitializer *m = new CageCubebInitializer(); // intentional leak
		if (!m || !m->c)
			CAGE_THROW_ERROR(Exception, "cubeb initialization had failed");
		return m->c;
	}

	namespace
	{
		long dataCallbackFree(cubeb_stream *stream, void *user_ptr, void const *input_buffer, void *output_buffer, long nframes);
		void stateCallbackFree(cubeb_stream *stream, void *user_ptr, cubeb_state state);

		struct DevicesCollection : Immovable, public cubeb_device_collection
		{
			DevicesCollection()
			{
				cageCheckCubebError(cubeb_enumerate_devices(context, CUBEB_DEVICE_TYPE_OUTPUT, this));
			}

			~DevicesCollection()
			{
				cubeb_device_collection_destroy(context, this);
			}

		private:
			cubeb *context = cageCubebInitializeFunc();
		};

		class SpeakerImpl : public Speaker
		{
		public:
			const Delegate<void(const SpeakerCallbackData &)> callback;
			cubeb_stream *stream = nullptr;
			uint32 channels = 0;
			uint32 sampleRate = 0;
			uint32 latency = 0;
			bool started = false;

			SpeakerImpl(const SpeakerCreateConfig &config, Delegate<void(const SpeakerCallbackData &)> callback) : callback(callback), channels(config.channels), sampleRate(config.sampleRate)
			{
				const string name = replace(config.name, ":", "_");
				CAGE_LOG(SeverityEnum::Info, "sound", stringizer() + "creating speaker, name: '" + name + "'");
				cubeb *context = cageCubebInitializeFunc();

				cubeb_devid devid = nullptr;
				if (config.deviceId.empty())
				{
					CAGE_LOG(SeverityEnum::Info, "sound", stringizer() + "requesting default device");
					if (!channels)
						cageCheckCubebError(cubeb_get_max_channel_count(context, &channels));
					if (!sampleRate)
						cageCheckCubebError(cubeb_get_preferred_sample_rate(context, &sampleRate));
					{
						cubeb_stream_params params = {};
						params.format = CUBEB_SAMPLE_FLOAT32NE;
						params.channels = channels;
						params.rate = sampleRate;
						cageCheckCubebError(cubeb_get_min_latency(context, &params, &latency));
					}
				}
				else
				{
					CAGE_LOG(SeverityEnum::Info, "sound", stringizer() + "requesting device id: '" + config.deviceId + "'");
					DevicesCollection collection;
					const cubeb_device_info *info = nullptr;
					for (uint32 index = 0; index < collection.count; index++)
					{
						const cubeb_device_info &d = collection.device[index];
						if (d.device_id == config.deviceId)
							info = &d;
					}
					if (!info)
						CAGE_THROW_ERROR(Exception, "invalid sound device id");
					if (info->state != CUBEB_DEVICE_STATE_ENABLED)
						CAGE_THROW_ERROR(Exception, "sound device is disabled or unplugged");
					devid = info->devid;
					if (!channels)
						channels = info->max_channels;
					if (!sampleRate)
						sampleRate = info->default_rate;
					latency = info->latency_lo;
				}

				CAGE_LOG(SeverityEnum::Info, "sound", stringizer() + "initializing sound stream with " + channels + " channels, " + sampleRate + " Hz sample rate and " + latency + " frames latency");

				{
					cubeb_stream_params params = {};
					params.format = CUBEB_SAMPLE_FLOAT32NE;
					params.channels = channels;
					params.rate = sampleRate;
					cageCheckCubebError(cubeb_stream_init(context, &stream, name.c_str(), nullptr, nullptr, devid, &params, latency, &dataCallbackFree, &stateCallbackFree, this));
				}
			}

			~SpeakerImpl()
			{
				if (stream)
				{
					cubeb_stream_stop(stream);
					cubeb_stream_destroy(stream);
				}
			}

			void start()
			{
				if (started)
					return;
				started = true;
				cageCheckCubebError(cubeb_stream_start(stream));
			}

			void stop()
			{
				if (!started)
					return;
				started = false;
				cageCheckCubebError(cubeb_stream_stop(stream));
			}

			void dataCallback(float *output_buffer, uint32 nframes)
			{
				SpeakerCallbackData data;
				data.buffer = { output_buffer, output_buffer + channels * nframes };
				data.channels = channels;
				data.frames = nframes;
				data.samplerate = sampleRate;
				callback(data);
			}

			void stateCallback(cubeb_state state)
			{
				SpeakerCallbackData data;
				if (state == CUBEB_STATE_STARTED)
				{
					data.channels = channels;
					data.samplerate = sampleRate;
				}
				callback(data);
			}
		};

		long dataCallbackFree(cubeb_stream *stream, void *user_ptr, void const *input_buffer, void *output_buffer, long nframes)
		{
			((SpeakerImpl *)user_ptr)->dataCallback((float *)output_buffer, numeric_cast<uint32>(nframes));
			return nframes;
		}

		void stateCallbackFree(cubeb_stream *stream, void *user_ptr, cubeb_state state)
		{
			((SpeakerImpl *)user_ptr)->stateCallback(state);
		}
	}

	uint32 Speaker::channels() const
	{
		const SpeakerImpl *impl = (const SpeakerImpl *)this;
		return impl->channels;
	}

	uint32 Speaker::sampleRate() const
	{
		const SpeakerImpl *impl = (const SpeakerImpl *)this;
		return impl->sampleRate;
	}

	uint32 Speaker::latency() const
	{
		const SpeakerImpl *impl = (const SpeakerImpl *)this;
		return impl->latency;
	}

	void Speaker::start()
	{
		SpeakerImpl *impl = (SpeakerImpl *)this;
		impl->start();
	}

	void Speaker::stop()
	{
		SpeakerImpl *impl = (SpeakerImpl *)this;
		impl->stop();
	}

	bool Speaker::running() const
	{
		const SpeakerImpl *impl = (const SpeakerImpl *)this;
		return impl->started;
	}

	Holder<Speaker> newSpeaker(const SpeakerCreateConfig &config, Delegate<void(const SpeakerCallbackData &)> callback)
	{
		return detail::systemArena().createImpl<Speaker, SpeakerImpl>(config, callback);
	}
}
