#include <cage-core/mesh.h>

#include <vector>

namespace cage
{
	template<class T>
	struct MeshAttribute : public std::vector<T>
	{};

	class MeshImpl : public Mesh
	{
	public:
		MeshAttribute<Vec3> positions;
		MeshAttribute<Vec3> normals;
		MeshAttribute<Vec3> tangents;
		MeshAttribute<Vec4i> boneIndices;
		MeshAttribute<Vec4> boneWeights;
		MeshAttribute<Vec2> uvs;
		MeshAttribute<Vec3> uvs3;

		std::vector<uint32> indices;

		MeshTypeEnum type = MeshTypeEnum::Triangles;

		void swap(MeshImpl &other);
	};
}

#define POLYHEDRON_ATTRIBUTES positions, normals, tangents, boneIndices, boneWeights, uvs, uvs3
