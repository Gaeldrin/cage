#include <cage-core/core.h>
#include <cage-core/math.h>
#include <cage-core/geometry.h>
#include <cage-core/assetStructs.h>
#include <cage-core/serialization.h>
#define CAGE_EXPORT
#include <cage-core/core/macro/api.h>
#include <cage-engine/core.h>
#include <cage-engine/graphics.h>
#include <cage-engine/assetStructs.h>

namespace cage
{
	namespace
	{
		void processLoad(const AssetContext *context, void *schemePointer)
		{
			if (!context->assetHolder)
			{
				context->assetHolder = newRenderObject().cast<void>();
				static_cast<renderObject*>(context->assetHolder.get())->setDebugName(context->textName);
			}
			renderObject *obj = static_cast<renderObject*>(context->assetHolder.get());
			context->returnData = obj;

			Deserializer des(context->originalData, numeric_cast<uintPtr>(context->originalSize));
			renderObjectHeader h;
			des >> h;
			obj->color = h.color;
			obj->opacity = h.opacity;
			obj->texAnimSpeed = h.texAnimSpeed;
			obj->texAnimOffset = h.texAnimOffset;
			obj->skelAnimName = h.skelAnimName;
			obj->skelAnimSpeed = h.skelAnimSpeed;
			obj->skelAnimOffset = h.skelAnimOffset;
			obj->worldSize = h.worldSize;
			obj->pixelsSize = h.pixelsSize;

			float *thresholds = (float*)des.advance(h.lodsCount * sizeof(uint32));
			uint32 *indices = (uint32*)des.advance((h.lodsCount + 1) * sizeof(uint32));
			uint32 *names = (uint32*)des.advance(h.meshesCount * sizeof(uint32));
			obj->setLods(h.lodsCount, h.meshesCount, thresholds, indices, names);
		}
	}

	AssetScheme genAssetSchemeRenderObject(uint32 threadIndex)
	{
		AssetScheme s;
		s.threadIndex = threadIndex;
		s.load.bind<&processLoad>();
		return s;
	}
}
