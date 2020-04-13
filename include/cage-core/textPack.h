#ifndef guard_textPack_h_B436B597745C461DAA266CE6FBBE10D1
#define guard_textPack_h_B436B597745C461DAA266CE6FBBE10D1

#include "core.h"

namespace cage
{
	class CAGE_CORE_API TextPack : private Immovable
	{
	public:
		void set(uint32 name, const string &text);
		void erase(uint32 name);
		void clear();

		string get(uint32 name) const;
		string format(uint32 name, uint32 paramCount, const string *paramValues) const;

		static string format(const string &format, uint32 paramCount, const string *paramValues);
	};

	CAGE_CORE_API Holder<TextPack> newTextPack();

	CAGE_CORE_API AssetScheme genAssetSchemeTextPack();
	static constexpr uint32 AssetSchemeIndexTextPack = 2;
}

#endif // guard_textPack_h_B436B597745C461DAA266CE6FBBE10D1
