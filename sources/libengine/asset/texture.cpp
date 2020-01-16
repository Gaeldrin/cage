#include <cage-core/core.h>
#include <cage-core/math.h>
#include <cage-core/geometry.h>
#include <cage-core/config.h>
#include <cage-core/memory.h>
#include <cage-core/serialization.h>
#include <cage-core/assetStructs.h>
#define CAGE_EXPORT
#include <cage-core/core/macro/api.h>
#include <cage-engine/core.h>
#include <cage-engine/graphics.h>
#include <cage-engine/opengl.h>
#include <cage-engine/assetStructs.h>

namespace cage
{
	namespace
	{
		ConfigUint32 textureDownscale("cage/graphics/textureDownscale", 1);

		void processLoad(const AssetContext *context, void *schemePointer)
		{
			Deserializer des(context->originalData());
			TextureHeader data;
			des >> data;

			Holder<Texture> tex = newTexture(data.target);
			tex->setDebugName(context->textName);

			uint32 bytesSize = data.dimX * data.dimY * data.dimZ * data.bpp;
			char *values = (char*)des.advance(bytesSize);

			if (data.target == GL_TEXTURE_3D || data.target == GL_TEXTURE_2D_ARRAY)
				tex->image3d(data.dimX, data.dimY, data.dimZ, data.internalFormat, data.copyFormat, data.copyType, values);
			else if (data.target == GL_TEXTURE_CUBE_MAP)
				tex->imageCube(data.dimX, data.dimY, data.internalFormat, data.copyFormat, data.copyType, values, data.stride);
			else
				tex->image2d(data.dimX, data.dimY, data.internalFormat, data.copyFormat, data.copyType, values);
			tex->filters(data.filterMin, data.filterMag, data.filterAniso);
			tex->wraps(data.wrapX, data.wrapY, data.wrapZ);
			if (any(data.flags & TextureFlags::GenerateMipmaps))
				tex->generateMipmaps();

			tex->animationDuration = data.animationDuration;
			tex->animationLoop = any(data.flags & TextureFlags::AnimationLoop);

			context->assetHolder = templates::move(tex).cast<void>();
		}
	}

	AssetScheme genAssetSchemeTexture(uint32 threadIndex, Window *memoryContext)
	{
		AssetScheme s;
		s.threadIndex = threadIndex;
		s.schemePointer = memoryContext;
		s.load.bind<&processLoad>();
		return s;
	}
}
