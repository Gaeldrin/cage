#include <cage-core/core.h>
#include <cage-core/math.h>
#include <cage-core/memory.h>

#define CAGE_EXPORT
#include <cage-core/core/macro/api.h>
#include <cage-client/core.h>
#include <cage-client/gui.h>
#include <cage-client/graphics.h>
#include <cage-client/window.h>
#include "private.h"

namespace cage
{
	void guiClass::graphicsInitialize()
	{
		guiImpl *impl = (guiImpl*)this;

		// preallocate skins element buffers
		for (auto &s : impl->skins)
		{
			s.elementsGpuBuffer = newUniformBuffer();
			s.elementsGpuBuffer->bind();
			s.elementsGpuBuffer->writeWhole(nullptr, sizeof(skinElementLayoutStruct::textureUvStruct) * (uint32)elementTypeEnum::TotalElements);
		}
	}

	void guiClass::graphicsFinalize()
	{
		guiImpl *impl = (guiImpl*)this;
		for (auto &it : impl->skins)
			it.elementsGpuBuffer.clear();
	}

	void guiClass::graphicsRender()
	{
		guiImpl *impl = (guiImpl*)this;
		impl->graphicsDispatch();
	}
}
