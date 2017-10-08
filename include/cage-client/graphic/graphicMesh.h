namespace cage
{
	class CAGE_API meshClass
	{
	public:
		uint32 getId() const;
		void bind() const;

		void setFlags(meshFlags flags);
		void setPrimitiveType(uint32 type);
		void setBoundingBox(const aabb &box);
		void setTextures(const uint32 *textureNames);
		void setBuffers(uint32 verticesCount, uint32 vertexSize, void *vertexData, uint32 indicesCount, const uint32 *indexData, uint32 materialSize, void *materialData);
		void setAttribute(uint32 index, uint32 size, uint32 type, uint32 stride, void *data);

		meshFlags getFlags() const;
		aabb getBoundingBox() const;
		uint32 textureName(uint32 texIdx) const;

		void dispatch() const;
		void dispatch(uint32 instances) const;
	};

	CAGE_API holder<meshClass> newMesh(windowClass *context);
}
