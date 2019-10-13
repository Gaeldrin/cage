namespace cage
{
	// delegates

	template<class T>
	struct delegate;

	template<class R, class... Ts>
	struct delegate<R(Ts...)>
	{
	private:
		R(*fnc)(void *, Ts...) = nullptr;
		void *inst = nullptr;

	public:
		delegate()
		{}

		template<R(*F)(Ts...)>
		delegate &bind()
		{
			fnc = +[](void *inst, Ts... vs) {
				(void)inst;
				return F(templates::forward<Ts>(vs)...);
			};
			return *this;
		}

		template<class D, R(*F)(D, Ts...)>
		delegate &bind(D d)
		{
			CAGE_ASSERT(sizeof(d) <= sizeof(inst));
			union U
			{
				void *p;
				D d;
			};
			fnc = +[](void *inst, Ts... vs) {
				U u;
				u.p = inst;
				return F(u.d , templates::forward<Ts>(vs)...);
			};
			U u;
			u.d = d;
			inst = u.p;
			return *this;
		}

		template<class C, R(C::*F)(Ts...)>
		delegate &bind(C *i)
		{
			fnc = +[](void *inst, Ts... vs) {
				return (((C*)inst)->*F)(templates::forward<Ts>(vs)...);
			};
			inst = i;
			return *this;
		}

		template<class C, R(C::*F)(Ts...) const>
		delegate &bind(const C *i)
		{
			fnc = +[](void *inst, Ts... vs) {
				return (((const C*)inst)->*F)(templates::forward<Ts>(vs)...);
			};
			inst = const_cast<C*>(i);
			return *this;
		}

		explicit operator bool() const noexcept
		{
			return !!fnc;
		}

		void clear() noexcept
		{
			inst = nullptr;
			fnc = nullptr;
		}

		R operator ()(Ts... vs) const
		{
			CAGE_ASSERT(!!*this, inst, fnc);
			return fnc(inst, templates::forward<Ts>(vs)...);
		}
	};

	// events

	template<class>
	struct eventListener;
	template<class>
	struct eventDispatcher;

	namespace privat
	{
		struct CAGE_API eventLinker : private immovable
		{
			eventLinker();
			//eventLinker(eventLinker &other);
			//eventLinker(eventLinker &&other) = delete;
			//eventLinker &operator = (eventLinker &other) = delete;
			//eventLinker &operator = (eventLinker &&other) = delete;
			~eventLinker();
			void attach(eventLinker *d, sint32 order);
			void detach();
			eventLinker *p, *n;
		private:
			void unlink();
			bool valid() const;
			sint32 order;
		};
	}

	template<class... Ts>
	struct eventListener<bool(Ts...)> : protected privat::eventLinker, private delegate<bool(Ts...)>
	{
		void attach(eventDispatcher<bool(Ts...)> &dispatcher, sint32 order = 0)
		{
			privat::eventLinker::attach(&dispatcher, order);
		}

		using privat::eventLinker::detach;
		using delegate<bool(Ts...)>::bind;
		using delegate<bool(Ts...)>::clear;

	protected:
		bool invoke(Ts... vs) const
		{
			auto &d = (delegate<bool(Ts...)>&)*this;
			if (d)
				return d(templates::forward<Ts>(vs)...);
			return false;
		}

		friend struct eventDispatcher<bool(Ts...)>;
	};

	template<class... Ts>
	struct eventListener<void(Ts...)> : protected eventListener<bool(Ts...)>, private delegate<void(Ts...)>
	{
		eventListener()
		{
			eventListener<bool(Ts...)>::template bind<eventListener, &eventListener::invoke>(this);
		}

		using eventListener<bool(Ts...)>::attach;
		using eventListener<bool(Ts...)>::detach;
		using delegate<void(Ts...)>::bind;
		using delegate<void(Ts...)>::clear;

	private:
		bool invoke(Ts... vs) const
		{
			auto &d = (delegate<void(Ts...)>&)*this;
			if (d)
				d(templates::forward<Ts>(vs)...);
			return false;
		}
	};
	
	template<class... Ts>
	struct eventDispatcher<bool(Ts...)> : protected eventListener<bool(Ts...)>
	{
		void attach(eventListener<bool(Ts...)> &listener, sint32 order = 0)
		{
			listener.attach(*this, order);
		}

		void attach(eventListener<void(Ts...)> &listener, sint32 order = 0)
		{
			listener.attach(*this, order);
		}

		using eventListener<bool(Ts...)>::detach;

		bool dispatch(Ts... vs) const
		{
			CAGE_ASSERT(!this->p);
			const privat::eventLinker *l = this->n;
			while (l)
			{
				if (static_cast<const eventListener<bool(Ts...)>*>(l)->invoke(templates::forward<Ts>(vs)...))
					return true;
				l = l->n;
			}
			return false;
		}

	private:
		friend struct eventListener<bool(Ts...)>;
	};
}
