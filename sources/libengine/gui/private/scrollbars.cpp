#include "../private.h"

namespace cage
{
	namespace
	{
		struct ScrollbarsImpl : public WidgetItem
		{
			GuiScrollbarsComponent &data;

			struct Scrollbar
			{
				vec2 position;
				vec2 size;
				real dotSize;
				real &value;
				Scrollbar(real &value) : position(vec2::Nan()), size(vec2::Nan()), dotSize(real::Nan()), value(value)
				{}
			} scrollbars[2];

			real wheelFactor;

			ScrollbarsImpl(HierarchyItem *hierarchy) : WidgetItem(hierarchy), data(GUI_REF_COMPONENT(Scrollbars)), scrollbars{ data.scroll[0], data.scroll[1] }
			{
				ensureItemHasLayout(hierarchy);
			}

			virtual void initialize() override
			{
				CAGE_ASSERT(!hierarchy->text, "scrollbars may not have text");
				CAGE_ASSERT(!hierarchy->Image, "scrollbars may not have image");
			}

			virtual void findRequestedSize() override
			{
				hierarchy->firstChild->findRequestedSize();
				hierarchy->requestedSize = hierarchy->firstChild->requestedSize;
			}

			virtual void findFinalPosition(const FinalPosition &update) override
			{
				wheelFactor = 70 / (hierarchy->requestedSize[1] - update.renderSize[1]);
				FinalPosition u(update);
				u.renderSize = hierarchy->requestedSize;
				real scw = skin->defaults.scrollbars.scrollbarSize + skin->defaults.scrollbars.contentPadding;
				for (uint32 a = 0; a < 2; a++)
				{
					bool show = data.overflow[a] == OverflowModeEnum::Always;
					if (data.overflow[a] == OverflowModeEnum::Auto && update.renderSize[a] + 1e-7 < hierarchy->requestedSize[a])
						show = true;
					if (show)
					{ // the content is larger than the available area
						Scrollbar &s = scrollbars[a];
						s.size[a] = update.renderSize[a];
						s.size[1 - a] = skin->defaults.scrollbars.scrollbarSize;
						s.position[a] = update.renderPos[a];
						s.position[1 - a] = update.renderPos[1 - a] + update.renderSize[1 - a] - s.size[1 - a];
						u.renderPos[a] -= (hierarchy->requestedSize[a] - update.renderSize[a] + scw) * scrollbars[a].value;
						u.clipSize[1 - a] -= scw;
						real minSize = min(s.size[0], s.size[1]);
						s.dotSize = max(minSize, sqr(update.renderSize[a]) / hierarchy->requestedSize[a]);
					}
					else
					{ // the content is smaller than the available area
						u.renderPos[a] += (update.renderSize[a] - hierarchy->requestedSize[a]) * data.alignment[a];
					}
				}
				u.clipSize = max(u.clipSize, 0);
				hierarchy->firstChild->findFinalPosition(u);
			}

			virtual void emit() const override
			{
				hierarchy->childrenEmit();
				for (uint32 a = 0; a < 2; a++)
				{
					const Scrollbar &s = scrollbars[a];
					if (s.position.valid())
					{
						emitElement(a == 0 ? GuiElementTypeEnum::ScrollbarHorizontalPanel : GuiElementTypeEnum::ScrollbarVerticalPanel, 0, s.position, s.size);
						vec2 ds;
						ds[a] = s.dotSize;
						ds[1 - a] = s.size[1 - a];
						vec2 dp = s.position;
						dp[a] += (s.size[a] - ds[a]) * s.value;
						emitElement(a == 0 ? GuiElementTypeEnum::ScrollbarHorizontalDot : GuiElementTypeEnum::ScrollbarVerticalDot, mode(s.position, s.size, 1 << (30 + a)), dp, ds);
					}
				}
			}

			virtual void generateEventReceivers() override
			{
				for (uint32 a = 0; a < 2; a++)
				{
					const Scrollbar &s = scrollbars[a];
					if (s.position.valid())
					{
						EventReceiver e;
						e.widget = this;
						e.pos = s.position;
						e.size = s.size;
						if (clip(e.pos, e.size, hierarchy->clipPos, hierarchy->clipSize))
							hierarchy->impl->mouseEventReceivers.push_back(e);
					}
				}

				{ // event receiver for wheel
					EventReceiver e;
					e.widget = this;
					e.pos = hierarchy->renderPos;
					e.size = hierarchy->renderSize;
					e.mask = 1 << 31;
					if (clip(e.pos, e.size, hierarchy->clipPos, hierarchy->clipSize))
						hierarchy->impl->mouseEventReceivers.push_back(e);
				}
			}

			bool handleMouse(MouseButtonsFlags buttons, ModifiersFlags modifiers, vec2 point, bool move)
			{
				if (buttons != MouseButtonsFlags::Left)
					return true;
				if (modifiers != ModifiersFlags::None)
					return true;
				for (uint32 a = 0; a < 2; a++)
				{
					const Scrollbar &s = scrollbars[a];
					if (s.position.valid())
					{
						if (!move && pointInside(s.position, s.size, point))
							makeFocused(1 << (30 + a));
						if (hasFocus(1 << (30 + a)))
						{
							s.value = (point[a] - s.position[a] - s.dotSize * 0.5) / (s.size[a] - s.dotSize);
							s.value = clamp(s.value, 0, 1);
							return true;
						}
					}
				}
				return true;
			}

			virtual bool mousePress(MouseButtonsFlags buttons, ModifiersFlags modifiers, vec2 point) override
			{
				makeFocused();
				return handleMouse(buttons, modifiers, point, false);
			}

			virtual bool mouseMove(MouseButtonsFlags buttons, ModifiersFlags modifiers, vec2 point) override
			{
				return handleMouse(buttons, modifiers, point, true);
			}

			virtual bool mouseWheel(sint8 wheel, ModifiersFlags modifiers, vec2 point) override
			{
				if (modifiers != ModifiersFlags::None)
					return false;
				const Scrollbar &s = scrollbars[1];
				if (s.position.valid())
				{
					s.value -= wheel * wheelFactor;
					s.value = clamp(s.value, 0, 1);
					return true;
				}
				return false;
			}
		};
	}

	void scrollbarsCreate(HierarchyItem *item)
	{
		CAGE_ASSERT(!item->item);
		item->item = item->impl->itemsMemory.createObject<ScrollbarsImpl>(item);
	}
}
