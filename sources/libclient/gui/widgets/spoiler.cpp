#include <cage-core/core.h>
#include <cage-core/math.h>
#include <cage-core/memory.h>
#include <cage-core/entities.h>

#define CAGE_EXPORT
#include <cage-core/core/macro/api.h>
#include <cage-client/core.h>
#include <cage-client/gui.h>
#include <cage-client/graphics.h>
#include <cage-client/window.h>
#include "../private.h"

namespace cage
{
	namespace
	{
		struct spoilerImpl : public widgetItemStruct
		{
			spoilerComponent &data;

			spoilerImpl(hierarchyItemStruct *hierarchy) : widgetItemStruct(hierarchy), data(GUI_REF_COMPONENT(spoiler))
			{
				ensureItemHasLayout(hierarchy);
			}

			virtual void initialize() override
			{
				if (data.collapsed)
					hierarchy->detachChildren();
				if (hierarchy->text)
					hierarchy->text->text.apply(skin->defaults.spoiler.textFormat, hierarchy->impl);
			}

			virtual void findRequestedSize() override
			{
				if (hierarchy->firstChild)
				{
					hierarchy->firstChild->findRequestedSize();
					hierarchy->requestedSize = hierarchy->firstChild->requestedSize;
				}
				else
					hierarchy->requestedSize = vec2();
				offsetSize(hierarchy->requestedSize, skin->defaults.spoiler.contentPadding);
				hierarchy->requestedSize[1] += skin->defaults.spoiler.captionHeight;
				vec2 cs = hierarchy->text ? hierarchy->text->findRequestedSize() : vec2();
				offsetSize(cs, skin->defaults.spoiler.captionPadding);
				hierarchy->requestedSize[0] = max(hierarchy->requestedSize[0], cs[0] + skin->defaults.spoiler.captionHeight);
				offsetSize(hierarchy->requestedSize, skin->layouts[(uint32)elementTypeEnum::SpoilerBase].border);
				offsetSize(hierarchy->requestedSize, skin->defaults.spoiler.baseMargin);
			}

			virtual void findFinalPosition(const finalPositionStruct &update) override
			{
				if (!hierarchy->firstChild)
					return;
				finalPositionStruct u(update);
				u.renderPos[1] += skin->defaults.spoiler.captionHeight;
				u.renderSize[1] -= skin->defaults.spoiler.captionHeight;
				offset(u.renderPos, u.renderSize, -skin->layouts[(uint32)elementTypeEnum::SpoilerBase].border);
				offset(u.renderPos, u.renderSize, -skin->defaults.spoiler.baseMargin - skin->defaults.spoiler.contentPadding);
				hierarchy->firstChild->findFinalPosition(u);
			}

			virtual void emit() const override
			{
				vec2 p = hierarchy->renderPos;
				vec2 s = hierarchy->renderSize;
				offset(p, s, -skin->defaults.spoiler.baseMargin);
				emitElement(elementTypeEnum::SpoilerBase, mode(false), p, s);
				s = vec2(s[0], skin->defaults.spoiler.captionHeight);
				emitElement(elementTypeEnum::SpoilerCaption, mode(p, s), p, s);
				offset(p, s, -skin->layouts[(uint32)elementTypeEnum::SpoilerCaption].border - skin->defaults.spoiler.captionPadding);
				vec2 is = vec2(s[1], s[1]);
				vec2 ip = vec2(p[0] + s[0] - is[0], p[1]);
				emitElement(data.collapsed ? elementTypeEnum::SpoilerIconCollapsed : elementTypeEnum::SpoilerIconShown, mode(false, 0), ip, is);
				if (hierarchy->text)
				{
					s[0] -= s[1];
					s[0] = max(0, s[0]);
					hierarchy->text->emit(p, s);
				}
				hierarchy->childrenEmit();
			}

			void collapse(hierarchyItemStruct *item)
			{
				spoilerImpl *b = dynamic_cast<spoilerImpl*>(item->item);
				if (!b)
					return;
				b->data.collapsed = true;
			}

			virtual bool mousePress(mouseButtonsFlags buttons, modifiersFlags modifiers, vec2 point) override
			{
				hierarchy->impl->focusName = 0;
				if (buttons != mouseButtonsFlags::Left)
					return true;
				if (modifiers != modifiersFlags::None)
					return true;
				vec2 p = hierarchy->renderPos;
				vec2 s = vec2(hierarchy->renderSize[0], skin->defaults.spoiler.captionHeight);
				offset(p, s, -skin->defaults.spoiler.baseMargin * vec4(1, 1, 1, 0));
				if (pointInside(p, s, point))
				{
					data.collapsed = !data.collapsed;
					if (data.collapsesSiblings)
					{
						hierarchyItemStruct *i = hierarchy->prevSibling;
						while (i)
						{
							collapse(i);
							i = i->prevSibling;
						}
						i = hierarchy->nextSibling;
						while (i)
						{
							collapse(i);
							i = i->nextSibling;
						}
					}
				}
				return true;
			}
		};
	}

	void spoilerCreate(hierarchyItemStruct *item)
	{
		CAGE_ASSERT_RUNTIME(!item->item);
		item->item = item->impl->itemsMemory.createObject<spoilerImpl>(item);
	}
}
