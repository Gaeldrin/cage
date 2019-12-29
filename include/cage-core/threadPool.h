#ifndef guard_threadPool_h_85C3A6DCAB82493AB056948639D0AC0A
#define guard_threadPool_h_85C3A6DCAB82493AB056948639D0AC0A

namespace cage
{
	class CAGE_API ThreadPool : private Immovable
	{
	public:
		// thread index, threads count
		Delegate<void(uint32, uint32)> function;

		void run();
	};

	// threadsCount == 0 -> run in calling thread
	// threadsCount == m -> as many threads as there is processors
	CAGE_API Holder<ThreadPool> newThreadPool(const string &threadNames = "worker_", uint32 threadsCount = m);

	CAGE_API void threadPoolTasksSplit(uint32 threadIndex, uint32 threadsCount, uint32 tasksCount, uint32 &begin, uint32 &end);
}

#endif // guard_threadPool_h_85C3A6DCAB82493AB056948639D0AC0A
