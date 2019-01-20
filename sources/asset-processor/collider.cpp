#include "utility/assimp.h"
#include <cage-core/collider.h>
#include <cage-core/memoryBuffer.h>

void processCollider()
{
	holder<assimpContextClass> context = newAssimpContext(0, 0);
	const aiScene *scene = context->getScene();
	const aiMesh *am = scene->mMeshes[context->selectMesh()];

	switch (am->mPrimitiveTypes)
	{
	case aiPrimitiveType_TRIANGLE: break;
	default: CAGE_THROW_ERROR(exception, "collider works with triangles only");
	}

	holder<colliderClass> collider = newCollider();

	mat3 axesScale = axesScaleMatrix();
	for (uint32 i = 0; i < am->mNumFaces; i++)
	{
		triangle tri;
		for (uint32 j = 0; j < 3; j++)
		{
			uint32 idx = numeric_cast<uint32>(am->mFaces[i].mIndices[j]);
			tri[j] = axesScale * conv(am->mVertices[i]);
		}
		collider->addTriangle(tri);
	}
	CAGE_LOG(severityEnum::Info, logComponentName, string() + "total triangles: " + collider->trianglesCount());

	collider->rebuild();

	CAGE_LOG(severityEnum::Info, logComponentName, string() + "aabb: " + collider->box());

	memoryBuffer buff = collider->serialize();

	CAGE_LOG(severityEnum::Info, logComponentName, string() + "buffer size (before compression): " + buff.size());
	memoryBuffer comp = detail::compress(buff);
	CAGE_LOG(severityEnum::Info, logComponentName, string() + "buffer size (after compression): " + comp.size());

	assetHeaderStruct h = initializeAssetHeaderStruct();
	h.originalSize = numeric_cast<uint32>(buff.size());
	h.compressedSize = numeric_cast<uint32>(comp.size());
	holder<fileClass> f = newFile(outputFileName, fileMode(false, true));
	f->write(&h, sizeof(h));
	f->write(comp.data(), comp.size());
	f->close();
}
