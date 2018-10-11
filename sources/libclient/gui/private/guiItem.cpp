#include <cage-core/core.h>
#include <cage-core/math.h>
#include <cage-core/memory.h>
#include <cage-core/entities.h>
#include <cage-core/config.h>
#include <cage-core/color.h>

#define CAGE_EXPORT
#include <cage-core/core/macro/api.h>
#include <cage-client/core.h>
#include <cage-client/gui.h>
#include <cage-client/graphics.h>
#include <cage-client/window.h>
#include "../private.h"

namespace cage
{
	configBool renderDebugConfig("cage-client.gui.renderDebug", false);

	namespace detail
	{
		uint32 hash(uint32 key);
	}

	void finalPositionStruct::clip(vec2 p, vec2 s)
	{
		CAGE_ASSERT_RUNTIME(p.valid());
		CAGE_ASSERT_RUNTIME(s.valid() && s[0] >= 0 && s[1] >= 0, p, s);
		CAGE_ASSERT_RUNTIME(clipPos.valid());
		CAGE_ASSERT_RUNTIME(clipSize.valid() && clipSize[0] >= 0 && clipSize[1] >= 0, clipPos, clipSize);
		vec2 e = min(clipPos + clipSize, p + s);
		clipPos = max(clipPos, p);
		clipSize = max(e - clipPos, vec2());
	}

	finalPositionStruct::finalPositionStruct() : renderPos(vec2::Nan), renderSize(vec2::Nan), clipPos(vec2::Nan), clipSize(vec2::Nan)
	{}

	guiItemStruct::guiItemStruct(guiImpl *impl, entityClass *entity) :
		requestedSize(vec2::Nan), renderPos(vec2::Nan), renderSize(vec2::Nan), clipPos(vec2::Nan), clipSize(vec2::Nan),
		impl(impl), entity(entity),
		parent(nullptr), prevSibling(nullptr), nextSibling(nullptr), firstChild(nullptr), lastChild(nullptr),
		widget(nullptr), layout(nullptr), text(nullptr), image(nullptr),
		order(0), subsidedItem(false)
	{}

	void guiItemStruct::initialize()
	{
		if (widget)
			widget->initialize();
		if (layout)
			layout->initialize();
		if (text)
			text->initialize();
		if (image)
			image->initialize();
	}

	void guiItemStruct::findRequestedSize()
	{
		if (widget)
			widget->findRequestedSize();
		else if (layout)
			layout->findRequestedSize();
		else if (text)
			requestedSize = text->findRequestedSize();
		else if (image)
			requestedSize = image->findRequestedSize();
		CAGE_ASSERT_RUNTIME(requestedSize.valid());
	}

	void guiItemStruct::findFinalPosition(const finalPositionStruct &update)
	{
		CAGE_ASSERT_RUNTIME(requestedSize.valid());
		CAGE_ASSERT_RUNTIME(update.renderPos.valid());
		CAGE_ASSERT_RUNTIME(update.renderSize.valid());
		CAGE_ASSERT_RUNTIME(update.clipPos.valid());
		CAGE_ASSERT_RUNTIME(update.clipSize.valid());

		renderPos = update.renderPos;
		renderSize = update.renderSize;

		finalPositionStruct u(update);
		u.clip(u.renderPos, u.renderSize);
		clipPos = u.clipPos;
		clipSize = u.clipSize;

		if (widget)
			widget->findFinalPosition(u);
		else if (layout)
			layout->findFinalPosition(u);
		else
		{
			uint32 name = entity ? entity->name() : 0;
			CAGE_ASSERT_RUNTIME(layout, "trying to layout an entity without layouting specified", name);
		}

		CAGE_ASSERT_RUNTIME(renderPos.valid());
		CAGE_ASSERT_RUNTIME(renderSize.valid());
		CAGE_ASSERT_RUNTIME(clipPos.valid());
		CAGE_ASSERT_RUNTIME(clipSize.valid());
		for (uint32 a = 0; a < 2; a++)
		{
			CAGE_ASSERT_RUNTIME(clipPos[a] >= u.clipPos[a], clipPos, u.clipPos);
			CAGE_ASSERT_RUNTIME(clipPos[a] + clipSize[a] <= u.clipPos[a] + u.clipSize[a], clipPos, clipSize, u.clipPos, u.clipSize);
		}
	}

	void guiItemStruct::checkExplicitPosition(vec2 &pos, vec2 &size) const
	{
		if (!subsidedItem && GUI_HAS_COMPONENT(position, entity))
		{
			GUI_GET_COMPONENT(position, p, entity);
			size = impl->eval<2>(p.size, size);
			pos = impl->eval<2>(p.position, pos) - p.anchor * size;
		}
		CAGE_ASSERT_RUNTIME(size.valid(), "this item must have explicit size", entity ? entity->name() : 0);
	}

	void guiItemStruct::checkExplicitPosition(vec2 &size) const
	{
		vec2 pos = vec2::Nan;
		checkExplicitPosition(pos, size);
		CAGE_ASSERT_RUNTIME(!pos.valid(), "this item may not have explicit position", entity ? entity->name() : 0);
		CAGE_ASSERT_RUNTIME(size.valid(), "this item must have explicit size", entity ? entity->name() : 0);
	}

	void guiItemStruct::moveToWindow(bool horizontal, bool vertical)
	{
		bool enabled[2] = { horizontal, vertical };
		for (uint32 i = 0; i < 2; i++)
		{
			if (!enabled[i])
				continue;
			real offset = 0;
			if (renderPos[i] + renderSize[i] > impl->outputSize[i])
				offset = (impl->outputSize[i] - renderSize[i]) - renderPos[i];
			else if (renderPos[i] < 0)
				offset = -renderPos[i];
			renderPos[i] += offset;
		}
	}

	void guiItemStruct::detachChildren()
	{
		while (firstChild)
			firstChild->detachParent();
	}

	void guiItemStruct::detachParent()
	{
		CAGE_ASSERT_RUNTIME(parent);
		if (prevSibling)
			prevSibling->nextSibling = nextSibling;
		if (nextSibling)
			nextSibling->prevSibling = prevSibling;
		if (parent->firstChild == this)
			parent->firstChild = nextSibling;
		if (parent->lastChild == this)
			parent->lastChild = prevSibling;
		parent = nextSibling = prevSibling = nullptr;
	}

	void guiItemStruct::attachParent(guiItemStruct *newParent)
	{
		if (parent)
			detachParent();
		parent = newParent;
		if (!parent->firstChild)
		{
			parent->firstChild = parent->lastChild = this;
			return;
		}
		prevSibling = parent->lastChild;
		prevSibling->nextSibling = this;
		parent->lastChild = this;
	}

	void guiItemStruct::childrenEmit() const
	{
		bool renderDebugLocal = renderDebugConfig;
		guiItemStruct *a = firstChild;
		while (a)
		{
			if (a->widget)
				a->widget->emit();
			else
				a->childrenEmit();
			if (renderDebugLocal)
				a->emitDebug();
			a = a->nextSibling;
		}
	}

	void guiItemStruct::emitDebug() const
	{
		real h = real(detail::hash(entity ? entity->name() : 0)) / real(detail::numeric_limits<uint32>().max());
		emitDebug(renderPos, renderSize, vec4(convertHsvToRgb(vec3(h, 1, 1)), 1));
	}

	void guiItemStruct::emitDebug(vec2 pos, vec2 size, vec4 color) const
	{
		auto *e = impl->emitControl;
		auto *t = e->memory.createObject<renderableDebugStruct>();
		t->data.position = impl->pointsToNdc(pos, size);
		t->data.color = color;
		e->last->next = t;
		e->last = t;
	}

	void offsetPosition(vec2 &position, const vec4 &offset)
	{
		CAGE_ASSERT_RUNTIME(position.valid() && offset.valid(), position, offset);
		position -= vec2(offset);
	}

	void offsetSize(vec2 &size, const vec4 &offset)
	{
		CAGE_ASSERT_RUNTIME(size.valid() && offset.valid(), size, offset);
		size += vec2(offset) + vec2(offset[2], offset[3]);
		size = max(size, vec2());
	}

	void offset(vec2 &position, vec2 &size, const vec4 &offset)
	{
		offsetPosition(position, offset);
		offsetSize(size, offset);
	}

	bool pointInside(vec2 pos, vec2 size, vec2 point)
	{
		CAGE_ASSERT_RUNTIME(pos.valid() && size.valid() && point.valid(), pos, size, point);
		if (point[0] < pos[0] || point[1] < pos[1])
			return false;
		pos += size;
		if (point[0] > pos[0] || point[1] > pos[1])
			return false;
		return true;
	}
}
