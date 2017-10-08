#ifndef guard_scheme_h_f17e7ce9_c9c5_49b3_b59d_c42929085c79_
#define guard_scheme_h_f17e7ce9_c9c5_49b3_b59d_c42929085c79_

struct schemeFieldStruct
{
	string name;
	string type;
	string min;
	string max;
	string defaul;
	string values;

	bool valid() const;
	bool applyToAssetField(string &val, const string &assetName) const;
	inline bool operator < (const schemeFieldStruct &other) const
	{
		return stringCompareFast(name, other.name);
	}
};

struct schemeStruct
{
	string name;
	string processor;
	uint32 schemeIndex;
	holderSet<schemeFieldStruct> schemeFields;

	void parse(iniClass *ini);
	void load(fileClass *file);
	void save(fileClass *file);
	bool applyOnAsset(struct assetStruct &ass);
	inline bool operator < (const schemeStruct &other) const
	{
		return stringCompareFast(name, other.name);
	}
};

#endif // guard_scheme_h_f17e7ce9_c9c5_49b3_b59d_c42929085c79_
