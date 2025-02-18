#include "../private.h"

namespace cage
{
	namespace
	{
		struct ProgressBarImpl : public WidgetItem
		{
			ProgressBarImpl(HierarchyItem *hierarchy) : WidgetItem(hierarchy)
			{}

			virtual void initialize() override
			{

			}

			virtual void findRequestedSize() override
			{
				hierarchy->requestedSize = Vec2(); // todo this is a temporary hack
			}

			virtual void emit() override
			{

			}
		};
	}

	void ProgressBarCreate(HierarchyItem *item)
	{
		CAGE_ASSERT(!item->item);
		item->item = item->impl->memory->createHolder<ProgressBarImpl>(item).cast<BaseItem>();
	}
}
