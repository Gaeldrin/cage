#ifndef guard_entitiesVisitor_h_m1nb54v6sre8t
#define guard_entitiesVisitor_h_m1nb54v6sre8t

#include "entities.h"

#include <tuple>
#include <algorithm>

namespace cage
{
	namespace privat
	{
		template<class>
		struct DecomposeLambda;
		template<class R, class T, class... Args>
		struct DecomposeLambda<R (T::*)(Args...) const>
		{
			using Return = R;
			using Class = T;
			using Params = std::tuple<Args...>;
		};

		template<class Lambda>
		struct LambdaParamsPack
		{
			using Params = typename DecomposeLambda<decltype(&Lambda::operator())>::Params;
		};

		template<std::size_t Off, class Seq>
		struct offset_sequence;
		template<std::size_t Off, std::size_t... I>
		struct offset_sequence<Off, std::index_sequence<I...>>
		{
			using type = std::index_sequence<Off + I...>;
		};
		template<std::size_t Off, class Seq>
		using offset_sequence_t = typename offset_sequence<Off, Seq>::type;

		template<class Types, std::size_t I>
		CAGE_FORCE_INLINE void fillComponentsArray(EntityManager *ents, EntityComponent *components[])
		{
			using T = std::tuple_element_t<I, Types>;
			using D = std::decay_t<T>;
			static_assert(std::is_same_v<T, D &> || std::is_same_v<T, const D &>);
			components[I] = ents->component<D>();
			if (!components[I])
				CAGE_THROW_CRITICAL(Exception, "undefined entities component for type");
		}
		template<class Types, std::size_t... I>
		CAGE_FORCE_INLINE void fillComponentsArray(EntityManager *ents, EntityComponent *components[], std::index_sequence<I...>)
		{
			((fillComponentsArray<Types, I>(ents, components)), ...);
		}

		template<bool UseEnt, class Visitor, class Types, std::size_t... I>
		CAGE_FORCE_INLINE void invokeVisitor(const Visitor &visitor, EntityComponent *components[], Entity *e, std::index_sequence<I...>)
		{
			if constexpr (UseEnt)
				visitor(e, ((e->value<std::decay_t<std::tuple_element_t<I, Types>>>(components[I])))...);
			else
				visitor(((e->value<std::decay_t<std::tuple_element_t<I, Types>>>(components[I])))...);
		}
	}

	template<class Visitor>
	void entitiesVisitor(EntityManager *ents, const Visitor &visitor)
	{
		using Types = typename privat::LambdaParamsPack<Visitor>::Params;
		constexpr uint32 typesCount = std::tuple_size_v<Types>;
		static_assert(typesCount > 0);
		constexpr bool useEnt = std::is_same_v<std::tuple_element_t<0, Types>, Entity *>;

		if constexpr (useEnt && typesCount == 1)
		{
			for (Entity *e : ents->entities())
				visitor(e);
		}
		else
		{
			constexpr std::size_t offset = useEnt ? 1 : 0;
			using Sequence = privat::offset_sequence_t<offset, std::make_index_sequence<typesCount - offset>>;

			EntityComponent *components[typesCount] = {};
			privat::fillComponentsArray<Types>(ents, components, Sequence());

			EntityComponent *cmps[typesCount - offset] = {};
			for (uint32 i = 0; i < typesCount - offset; i++)
				cmps[i] = components[i + offset];
			std::sort(std::begin(cmps), std::end(cmps), [](EntityComponent *a, EntityComponent *b) { return a->count() < b->count(); });

			PointerRange<EntityComponent *> conds = { std::begin(cmps) + 1, std::end(cmps) };

			for (Entity *e : cmps[0]->entities())
			{
				bool ok = true;
				for (EntityComponent *c : conds)
					ok = ok && e->has(c);
				if (!ok)
					continue;
				privat::invokeVisitor<useEnt, Visitor, Types>(visitor, components, e, Sequence());
			}
		}
	}
}

#endif // guard_entitiesVisitor_h_m1nb54v6sre8t
