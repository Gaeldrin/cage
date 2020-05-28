#include <cage-core/macros.h>
#include "polyhedron.h"

namespace cage
{
	template<class T>
	T attrAt(const PolyhedronAttribute<T> &attribute, const ivec3 &ids, const vec3 &weights)
	{
		return attribute.at(ids[0]) * weights[0] + attribute.at(ids[1]) * weights[1] + attribute.at(ids[2]) * weights[2];
	}

	template<>
	ivec4 attrAt(const PolyhedronAttribute<ivec4> &attribute, const ivec3 &ids, const vec3 &weights)
	{
		CAGE_THROW_ERROR(Exception, "cannot interpolate integer attributes");
	}

#define GCHL_GENERATE(TYPE, SINGULAR, PLURAL) \
	PointerRange<const TYPE> Polyhedron::PLURAL() const \
	{ \
		const PolyhedronImpl *impl = (const PolyhedronImpl *)this; \
		return impl->PLURAL; \
	} \
	PointerRange<TYPE> Polyhedron::PLURAL() \
	{ \
		PolyhedronImpl *impl = (PolyhedronImpl *)this; \
		return impl->PLURAL; \
	} \
	void Polyhedron::PLURAL(const PointerRange<const TYPE> &values) \
	{ \
		PolyhedronImpl *impl = (PolyhedronImpl *)this; \
		impl->PLURAL.resize(values.size()); \
		detail::memcpy(impl->PLURAL.data(), values.data(), values.size() * sizeof(TYPE)); \
	} \
	TYPE Polyhedron::SINGULAR(uint32 idx) const \
	{ \
		const PolyhedronImpl *impl = (const PolyhedronImpl *)this; \
		return impl->PLURAL[idx]; \
	} \
	void Polyhedron::SINGULAR(uint32 idx, const TYPE &value) \
	{ \
		PolyhedronImpl *impl = (PolyhedronImpl *)this; \
		impl->PLURAL[idx] = value; \
	} \
	TYPE Polyhedron::CAGE_JOIN(SINGULAR, At)(const ivec3 &ids, const vec3 &weights) const \
	{ \
		const PolyhedronImpl *impl = (const PolyhedronImpl *)this; \
		return attrAt<TYPE>(impl->PLURAL, ids, weights); \
	}

	GCHL_GENERATE(vec3, position, positions);
	GCHL_GENERATE(vec3, normal, normals);
	GCHL_GENERATE(vec3, tangent, tangents);
	GCHL_GENERATE(vec3, bitangent, bitangents);
	GCHL_GENERATE(vec2, uv, uvs);
	GCHL_GENERATE(vec3, uv3, uvs3);
	GCHL_GENERATE(ivec4, boneIndices, boneIndices);
	GCHL_GENERATE(vec4, boneWeights, boneWeights);
#undef GCHL_GENERATE
}
