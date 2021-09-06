#include "private.h"

namespace cage
{
	EventReceiver::EventReceiver() : widget(nullptr), mask(1)
	{}

	bool EventReceiver::pointInside(Vec2 point, uint32 maskRequest) const
	{
		if ((mask & maskRequest) == 0)
			return false;
		return cage::pointInside(pos, size, point);
	}

	namespace
	{
		void findWidgets(HierarchyItem *item, uint32 name, std::vector<WidgetItem *> &result)
		{
			if (item->ent && item->ent->name() == name)
			{
				WidgetItem *w = dynamic_cast<WidgetItem *>(+item->item);
				if (w)
					result.push_back(w);
			}
			for (auto &it : item->children)
				findWidgets(+it, name, result);
		}

		std::vector<WidgetItem *> focused(GuiImpl *impl)
		{
			std::vector<WidgetItem*> result;
			if (impl->focusName)
			{
				result.reserve(3);
				findWidgets(+impl->root, impl->focusName, result);
			}
			return result;
		}

		template<class A, bool (WidgetItem::*F)(A, ModifiersFlags, Vec2)>
		bool passMouseEvent(GuiImpl *impl, A a, ModifiersFlags m, const Vec2i &point)
		{
			Vec2 pt;
			if (!impl->eventPoint(point, pt))
				return false;
			{ // first, pass the event to focused widget
				bool res = false;
				for (auto f : focused(impl))
				{
					if (f->widgetState.disabled || (f->*F)(a, m, pt))
						res = true;
				}
				if (res)
					return true;
			}
			// if nothing has focus, pass the event to anything under the cursor
			for (const auto &it : impl->mouseEventReceivers)
			{
				if (it.pointInside(pt))
				{
					if (it.widget->widgetState.disabled || (it.widget->*F)(a, m, pt))
						return true;
				}
			}
			return false;
		}

		template<bool (WidgetItem::*F)(uint32, ModifiersFlags)>
		bool passKeyEvent(GuiImpl *impl, uint32 key, ModifiersFlags m)
		{
			Vec2 dummy;
			if (!impl->eventPoint(impl->inputMouse, dummy))
				return false;
			bool res = false;
			for (auto f : focused(impl))
			{
				if (f->widgetState.disabled || (f->*F)(key, m))
					res = true;
			}
			return res;
		}
	}

	bool GuiImpl::eventPoint(const Vec2i &ptIn, Vec2 &ptOut)
	{
		inputMouse = ptIn;
		if (!eventsEnabled)
			return false;
		if (eventCoordinatesTransformer)
		{
			bool ret = eventCoordinatesTransformer(ptIn, ptOut);
			if (ret)
				outputMouse = ptOut / pointsScale;
			return ret;
		}
		outputMouse = ptOut = Vec2(ptIn[0], ptIn[1]) / pointsScale;
		return true;
	}

	bool GuiManager::windowResize(const Vec2i &res)
	{
		GuiImpl *impl = (GuiImpl*)this;
		impl->inputResolution = res;
		return false;
	}

	bool GuiManager::mousePress(MouseButtonsFlags buttons, ModifiersFlags modifiers, const Vec2i &point)
	{
		GuiImpl *impl = (GuiImpl*)this;
		impl->focusName = 0;
		return passMouseEvent<MouseButtonsFlags, &WidgetItem::mousePress>(impl, buttons, modifiers, point);
	}

	bool GuiManager::mouseDouble(MouseButtonsFlags buttons, ModifiersFlags modifiers, const Vec2i &point)
	{
		GuiImpl *impl = (GuiImpl*)this;
		return passMouseEvent<MouseButtonsFlags, &WidgetItem::mouseDouble>(impl, buttons, modifiers, point);
	}

	bool GuiManager::mouseRelease(MouseButtonsFlags buttons, ModifiersFlags modifiers, const Vec2i &point)
	{
		GuiImpl *impl = (GuiImpl*)this;
		return passMouseEvent<MouseButtonsFlags, &WidgetItem::mouseRelease>(impl, buttons, modifiers, point);
	}

	bool GuiManager::mouseMove(MouseButtonsFlags buttons, ModifiersFlags modifiers, const Vec2i &point)
	{
		GuiImpl *impl = (GuiImpl*)this;
		return passMouseEvent<MouseButtonsFlags, &WidgetItem::mouseMove>(impl, buttons, modifiers, point);
	}

	bool GuiManager::mouseWheel(sint32 wheel, ModifiersFlags modifiers, const Vec2i &point)
	{
		GuiImpl *impl = (GuiImpl*)this;
		Vec2 pt;
		if (!impl->eventPoint(point, pt))
			return false;
		for (const auto &it : impl->mouseEventReceivers)
		{
			if (it.pointInside(pt, 1 | (1 << 31))) // also accept wheel events
			{
				if (it.widget->widgetState.disabled || it.widget->mouseWheel(wheel, modifiers, pt))
					return true;
			}
		}
		return false;
	}

	bool GuiManager::keyPress(uint32 key, ModifiersFlags modifiers)
	{
		GuiImpl *impl = (GuiImpl*)this;
		return passKeyEvent<&WidgetItem::keyPress>(impl, key, modifiers);
	}

	bool GuiManager::keyRepeat(uint32 key, ModifiersFlags modifiers)
	{
		GuiImpl *impl = (GuiImpl*)this;
		return passKeyEvent<&WidgetItem::keyRepeat>(impl, key, modifiers);
	}

	bool GuiManager::keyRelease(uint32 key, ModifiersFlags modifiers)
	{
		GuiImpl *impl = (GuiImpl*)this;
		return passKeyEvent<&WidgetItem::keyRelease>(impl, key, modifiers);
	}

	bool GuiManager::keyChar(uint32 key)
	{
		GuiImpl *impl = (GuiImpl*)this;
		Vec2 dummy;
		if (!impl->eventPoint(impl->inputMouse, dummy))
			return false;
		bool res = false;
		for (auto f : focused(impl))
		{
			if (f->widgetState.disabled || f->keyChar(key))
				res = true;
		}
		return res;
	}

	void HierarchyItem::fireWidgetEvent() const
	{
		if (ent->has<GuiEventComponent>())
		{
			GuiEventComponent &v = ent->value<GuiEventComponent>();
			if (v.event)
			{
				if (v.event(ent->name()))
					return;
			}
		}
		impl->widgetEvent.dispatch(ent->name());
	}
}
