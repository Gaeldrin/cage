#ifndef guard_scopeLock_h_sdgfrdsfhg54s5sd54hg544h444
#define guard_scopeLock_h_sdgfrdsfhg54s5sd54hg544h444

namespace cage
{
	template<class T>
	struct ScopeLock
	{
		explicit ScopeLock(Holder<T> &ptr, bool tryLock) : ScopeLock(ptr.get(), tryLock)
		{}

		explicit ScopeLock(Holder<T> &ptr) : ScopeLock(ptr.get())
		{}

		explicit ScopeLock(T *ptr, bool tryLock) : ptr(ptr)
		{
			if (tryLock)
			{
				if (!ptr->tryLock())
					this->ptr = nullptr;
			}
			else
				ptr->lock();
		}

		explicit ScopeLock(T *ptr) : ptr(ptr)
		{
			ptr->lock();
		}

		// non copyable
		ScopeLock(const ScopeLock &) = delete;
		ScopeLock &operator = (const ScopeLock &) = delete;

		// move constructible
		ScopeLock(ScopeLock &&other) noexcept : ptr(other.ptr)
		{
			other.ptr = nullptr;
		}

		// not move assignable (releasing the previous lock would not be atomic)
		ScopeLock &operator = (ScopeLock &&) = delete;

		~ScopeLock()
		{
			clear();
		}

		void clear()
		{
			if (ptr)
			{
				ptr->unlock();
				ptr = nullptr;
			}
		}

		explicit operator bool() const noexcept
		{
			return !!ptr;
		}

	private:
		T *ptr;

		friend class ConditionalVariableBase;
	};
}

#endif // guard_scopeLock_h_sdgfrdsfhg54s5sd54hg544h444
