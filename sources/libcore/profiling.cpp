#ifdef CAGE_PROFILING_ENABLED

#include <cage-core/profiling.h>
#include <cage-core/concurrent.h>
#include <cage-core/concurrentQueue.h>
#include <cage-core/networkWebsocket.h>
#include <cage-core/process.h>
#include <cage-core/config.h>
#include <cage-core/files.h>
#include <cage-core/math.h>
#include <cage-core/string.h>
#include <cage-core/stdHash.h>

#include <atomic>
#include <vector>
#include <string>
#include <unordered_map>

cage::PointerRange<const cage::uint8> profiling_htm();

namespace cage
{
	namespace
	{
#ifdef CAGE_SYSTEM_WINDOWS
		constexpr String DefaultBrowser = "\"C:/Program Files/Mozilla Firefox/firefox.exe\" --new-window";
#else
		constexpr String DefaultBrowser = "firefox";
#endif // CAGE_SYSTEM_WINDOWS

		constexpr uint64 ThreadNameSpecifier = (uint64)-2;

		ConfigBool confEnabled("cage/profiling/enabled", false);
		const ConfigBool confAutoStartClient("cage/profiling/autoStartClient", true);
		const ConfigString confBrowser("cage/profiling/browser", DefaultBrowser);

		uint64 timestamp() noexcept
		{
			try
			{
				// ensures that all timestamps are unique
				static std::atomic<uint64> atom = 0;
				uint64 newv = applicationTime();
				uint64 oldv = atom.load();
				do
				{
					if (oldv >= newv)
						newv = oldv + 1;
				} while (!atom.compare_exchange_weak(oldv, newv));
				return newv;
			}
			catch (...)
			{
				return m;
			}
		}

		struct QueueItem
		{
			String name;
			StringLiteral category;
			uint64 startTime = 0;
			uint64 endTime = 0;
			uint64 threadId = 0;
			bool framing = false;
		};

		using QueueType = ConcurrentQueue<QueueItem>;

		QueueType &queue()
		{
			static QueueType *q = new QueueType(); // intentional memory leak
			return *q;
		}

		struct Dispatcher
		{
			struct Runner
			{
				std::unordered_map<uint64, String> threadNames;
				Holder<WebsocketServer> server;
				Holder<WebsocketConnection> connection;
				Holder<Process> client;

				void eraseQueue()
				{
					QueueItem qi;
					while (queue().tryPop(qi))
					{
						if (qi.startTime == ThreadNameSpecifier && qi.endTime == ThreadNameSpecifier)
							threadNames[qi.threadId] = qi.name;
					}
				}

				void updateConnected()
				{
					struct NamesMap
					{
						std::unordered_map<String, uint32> data;
						uint32 next = 0;

						uint32 index(const String &name)
						{
							const auto it = data.find(name);
							if (it != data.end()) [[likely]]
								return it->second;
							return data[name] = next++;
						}

						uint32 index(StringLiteral name)
						{
							return index(String(name));
						}

						std::string mapping() const
						{
							std::vector<const String *> v;
							v.resize(data.size());
							for (const auto &it : data)
								v[it.second] = &it.first;

							std::string str;
							str.reserve(v.size() * 100);
							for (const auto &n : v)
								str += (Stringizer() + "\"" + *n + "\",\n ").value.c_str();
							return str + "\"\"";
						}

						NamesMap()
						{
							data.reserve(200);
						}
					} names;

					struct ThreadData
					{
						std::string events;

						ThreadData()
						{
							events.reserve(50000);
						}
					};
					std::unordered_map<uint64, ThreadData> data;

					QueueItem qi;
					while (queue().tryPop(qi))
					{
						if (qi.startTime == ThreadNameSpecifier && qi.endTime == ThreadNameSpecifier)
							threadNames[qi.threadId] = qi.name;
						else
						{
							const String s = Stringizer() + "[" + names.index(qi.name) + "," + names.index(qi.category) + "," + qi.startTime + "," + (qi.endTime - qi.startTime) + (qi.framing ? ",1" : "") + "], ";
							data[qi.threadId].events += s.c_str();
						}
					}

					std::string str = "{\"names\":[";
					str += names.mapping();
					str += "],\n\"threads\":[\n";
					for (const auto &thr : data)
					{
						str += "{\"name\":\"";
						str += threadNames[thr.first].c_str();
						str += "\",\n\"events\":[";
						str += thr.second.events;
						str += "[]\n]},\n";
					}
					str += "{}\n]}";

					connection->write(str);
				}

				void updateConnecting()
				{
					if (server)
					{
						connection = server->accept();
						if (connection)
						{
							CAGE_LOG(SeverityEnum::Info, "profiling", Stringizer() + "profiling client connected: " + connection->address() + ":" + connection->port());
							server.clear();
						}
					}
					else
					{
						server = newWebsocketServer(randomRange(10000u, 65000u));
						CAGE_LOG(SeverityEnum::Info, "profiling", Stringizer() + "profiling server listens on port " + server->port());

						if (confAutoStartClient)
						{
							try
							{
								writeFile("profiling.htm")->write(profiling_htm().cast<const char>());
								const String baseUrl = pathWorkingDir() + "/profiling.htm";
								const String url = Stringizer() + "file://" + baseUrl + "?port=" + server->port();
								ProcessCreateConfig cfg(Stringizer() + (String)confBrowser + " " + url);
								cfg.discardStdErr = cfg.discardStdIn = cfg.discardStdOut = true;
								client = newProcess(cfg);
							}
							catch (const cage::Exception &)
							{
								CAGE_LOG(SeverityEnum::Warning, "profiling", "failed to automatically launch profiling client");
							}
						}
					}
					eraseQueue();
				}

				void updateDisabled()
				{
					server.clear();
					connection.clear();
					client.clear();
					eraseQueue();
				}

				void run()
				{
					while (!queue().stopped())
					{
						const bool enabled = confEnabled;
						try
						{
							if (enabled)
							{
								if (connection)
									updateConnected();
								else
									updateConnecting();
								threadSleep(50000);
							}
							else
							{
								updateDisabled();
								threadSleep(200000);
							}
						}
						catch (...)
						{
							if (enabled)
							{
								confEnabled = false;
								CAGE_LOG(SeverityEnum::Warning, "profiling", "disabling profiling due to error");
							}
						}
					}
				}
			};

			void threadEntry()
			{
				try
				{
					Runner runner;
					runner.run();
				}
				catch (...)
				{
					// nothing
				}
			}

			~Dispatcher()
			{
				queue().terminate();
				thread.clear();
			}

			Holder<Thread> thread = newThread(Delegate<void()>().bind<Dispatcher, &Dispatcher::threadEntry>(this), "profiling dispatcher");
		} dispatcher;
	}

	void profilingThreadName() noexcept
	{
		try
		{
			QueueItem qi;
			qi.name = currentThreadName();
			qi.threadId = currentThreadId();
			qi.startTime = qi.endTime = ThreadNameSpecifier;
			queue().push(qi);
		}
		catch (...)
		{
			// nothing
		}
	}

	ProfilingEvent profilingEventBegin(const String &name, StringLiteral category, bool framing) noexcept
	{
		ProfilingEvent ev;
		ev.name = name;
		ev.category = category;
		ev.startTime = timestamp();
		ev.framing = framing;
		return ev;
	}

	void profilingEventEnd(ProfilingEvent &ev) noexcept
	{
		if (ev.startTime == m)
			return;
		try
		{
			if (!confEnabled)
				return;
			QueueItem qi;
			qi.name = ev.name;
			qi.category = ev.category;
			qi.startTime = ev.startTime;
			qi.endTime = timestamp();
			qi.threadId = currentThreadId();
			qi.framing = ev.framing;
			queue().push(qi);
		}
		catch (...)
		{
			// nothing
		}
		ev.startTime = m;
	}

	ProfilingScope::ProfilingScope() noexcept
	{}

	ProfilingScope::ProfilingScope(const String &name, StringLiteral category, bool framing) noexcept
	{
		event = profilingEventBegin(name, category, framing);
	}

	ProfilingScope::ProfilingScope(ProfilingScope &&other) noexcept
	{
		*this = std::move(other);
	}

	ProfilingScope &ProfilingScope::operator = (ProfilingScope &&other) noexcept
	{
		profilingEventEnd(event);
		event = {};
		std::swap(event, other.event);
		return *this;
	}

	ProfilingScope::~ProfilingScope() noexcept
	{
		profilingEventEnd(event);
	}
}

#endif
