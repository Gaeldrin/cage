#include <cage-core/core.h>
#include <cage-core/math.h>
#include <cage-core/memory.h>
#include <cage-core/entities.h>
#include <cage-core/utility/hashString.h>

#define CAGE_EXPORT
#include <cage-core/core/macro/api.h>
#include <cage-client/core.h>
#include <cage-client/gui.h>
#include <cage-client/graphic.h>
#include "../private.h"

namespace cage
{
	namespace
	{
		uint32 font;
		textFormatComponent text;
		imageFormatComponent image;

		struct skinInitializerStruct
		{
			skinInitializerStruct()
			{
				font = hashString("cage/font/roboto.ttf?12");
				text.color = vec3(1, 1, 1);
				text.fontName = font;
				text.align = textAlignEnum::Left;
				text.lineSpacing = 0;
				image.animationOffset = 0;
				image.animationSpeed = 1;
				image.mode = imageModeEnum::Stretch;
			}
		} skinInitializer;
	}

	skinElementLayoutStruct::textureUvStruct::textureUvStruct()
	{
		detail::memset(this, 0, sizeof(*this));
	}

	skinElementLayoutStruct::skinElementLayoutStruct() : border(2,2,2,2)
	{}

	skinWidgetDefaultsStruct::skinWidgetDefaultsStruct()
	{}

	skinConfigStruct::skinConfigStruct() : textureName(hashString("cage/texture/gui.psd"))
	{
		{ // automatically prepare all elements
			for (uint32 y = 0; y < (uint32)elementTypeEnum::TotalElements; y++)
			{
				skinElementLayoutStruct &el = layouts[y];
				for (uint32 x = 0; x < 4; x++)
				{
					skinElementLayoutStruct::textureUvOiStruct &oi = el.textureUv.data[x];
					vec4 &o = oi.outer;
					vec4 &i = oi.inner;
					// horizontal
					o[0] = 0.1 * x + 0.02 * (x + 1) + (y % 2 == 1 ? 0.5 : 0);
					o[2] = o[0] + 0.1;
					i[0] = o[0] + 0.025;
					i[2] = o[0] + 0.075;
					// vertical
					uint32 yy = y / 2;
					o[1] = (0.1 * yy + 0.02 * (yy + 1)) / 8;
					o[3] = o[1] + 0.1 / 8;
					i[1] = o[1] + 0.025 / 8;
					i[3] = o[1] + 0.075 / 8;
				}
			}
		}
		{ // manual overrides for some elements
			{ // things without border
				for (auto t : { elementTypeEnum::RadioBoxChecked, elementTypeEnum::RadioBoxUnchecked, elementTypeEnum::SliderHorizontalDot, elementTypeEnum::SliderVerticalDot, elementTypeEnum::WindowResizer })
				{
					auto &a = layouts[(uint32)t].textureUv;
					for (uint32 i = 0; i < 4; i++)
					{
						auto &b = a.data[i];
						b.inner = b.outer;
					}
				}
			}
			{ // things without hover
				for (auto t : { elementTypeEnum::GroupCell, elementTypeEnum::GroupPanel, elementTypeEnum::GroupSpoilerCollapsed, elementTypeEnum::GroupSpoilerShown,
					elementTypeEnum::WindowBaseModal, elementTypeEnum::WindowBaseNormal,
					elementTypeEnum::ComboBoxList, elementTypeEnum::ListBoxList,
					elementTypeEnum::ProgressBar,
					elementTypeEnum::ToolTip,
					})
				{
					auto &a = layouts[(uint32)t].textureUv;
					a.hover = a.normal;
				}
			}
			{ // things without focus
				for (auto t : { elementTypeEnum::GroupCell, elementTypeEnum::GroupPanel, elementTypeEnum::GroupSpoilerCollapsed, elementTypeEnum::GroupSpoilerShown, elementTypeEnum::GroupCaption,
					elementTypeEnum::WindowBaseModal, elementTypeEnum::WindowBaseNormal, elementTypeEnum::WindowCaption, elementTypeEnum::WindowResizer,
					elementTypeEnum::InputButtonNormalDecrement, elementTypeEnum::InputButtonNormalIncrement, elementTypeEnum::InputButtonPressedDecrement, elementTypeEnum::InputButtonPressedIncrement,
					elementTypeEnum::ComboBoxList, elementTypeEnum::ComboBoxItem, elementTypeEnum::ComboBoxSelectedItem, elementTypeEnum::ListBoxItem, elementTypeEnum::ListBoxCheckedItem,
					elementTypeEnum::ProgressBar, elementTypeEnum::SliderHorizontalDot, elementTypeEnum::SliderVerticalDot,
					elementTypeEnum::ScrollbarHorizontalDot, elementTypeEnum::ScrollbarHorizontalPanel, elementTypeEnum::ScrollbarVerticalDot, elementTypeEnum::ScrollbarVerticalPanel,
					elementTypeEnum::ToolTip,
					})
				{
					auto &a = layouts[(uint32)t].textureUv;
					a.focus = a.normal;
				}
			}
			{ // things without disabled
				for (auto t : { elementTypeEnum::WindowBaseModal, elementTypeEnum::WindowBaseNormal, elementTypeEnum::WindowCaption, elementTypeEnum::WindowResizer,
					elementTypeEnum::WindowButtonClose, elementTypeEnum::WindowButtonMaximize, elementTypeEnum::WindowButtonMinimize, elementTypeEnum::WindowButtonRestore,
					elementTypeEnum::ScrollbarHorizontalDot, elementTypeEnum::ScrollbarHorizontalPanel, elementTypeEnum::ScrollbarVerticalDot, elementTypeEnum::ScrollbarVerticalPanel,
					elementTypeEnum::ToolTip,
					})
				{
					auto &a = layouts[(uint32)t].textureUv;
					a.disab = a.normal;
				}
			}
			// todo color picker, graphs, task bar
		}
	}

	skinWidgetDefaultsStruct::labelStruct::labelStruct() : textFormat(text), imageFormat(image), margin(1, 2, 1, 2)
	{}

	skinWidgetDefaultsStruct::buttonStruct::buttonStruct() : textFormat(text), imageFormat(image), normalPadding(1, 1, 2, 2), pressedPadding(2, 2, 1, 1), margin(1, 1, 1, 1), size(150, 32)
	{
		textFormat.align = textAlignEnum::Center;
	}

	skinWidgetDefaultsStruct::inputBoxStruct::inputBoxStruct() : textFormat(text), invalidValueTextColor(1, 0, 0), basePadding(1, 1, 1, 1), buttonsPadding(1, 1, 1, 1), margin(1, 1, 1, 1), size(300, 32), buttonsOffset(), buttonsSpacing(), buttonsMode(inputButtonsPlacementModeEnum::InsideRight)
	{}

	skinWidgetDefaultsStruct::textAreaStruct::textAreaStruct() : textFormat(text), padding(3, 3, 3, 3), margin(1, 1, 1, 1), size(450, 200)
	{}

	skinWidgetDefaultsStruct::checkBoxStruct::checkBoxStruct() : textFormat(text), margin(1, 1, 1, 1), size(28, 28), labelOffset(3)
	{}

	skinWidgetDefaultsStruct::comboBoxStruct::comboBoxStruct() : textFormat(text), basePadding(1, 1, 1, 1), baseMargin(1, 1, 1, 1), listPadding(0, 0, 0, 0), itemPadding(1, 1, 1, 1), size(250, 32), listOffset(-5), itemSpacing(0)
	{}

	skinWidgetDefaultsStruct::listBoxStruct::listBoxStruct() : textFormat(text), basePadding(0, 0, 0, 0), baseMargin(1, 1, 1, 1), itemPadding(1, 1, 1, 1), size(250, 32), itemSpacing(0)
	{}

	skinWidgetDefaultsStruct::progressBarStruct::progressBarStruct() : textFormat(text), backgroundImageFormat(image), fillingImageFormat(image), baseMargin(1, 1, 1, 1), textPadding(1, 1, 1, 1), fillingPadding(1, 1, 1, 1), size(200, 28)
	{
		fillingImage.animationStart = 0;
		fillingImage.textureName = hashString("todo");
		fillingImage.textureUvOffset = vec2();
		fillingImage.textureUvSize = vec2(1, 1);
	}

	skinWidgetDefaultsStruct::sliderBarStruct::sliderBarStruct() : textFormat(text)
	{
		horizontal.padding = vec4(1, 1, 1, 1);
		horizontal.margin = vec4(1, 1, 1, 1);
		horizontal.size = vec2(200, 28);
		vertical.padding = vec4(1, 1, 1, 1);
		vertical.margin = vec4(1, 1, 1, 1);
		vertical.size = vec2(28, 200);
	}

	skinWidgetDefaultsStruct::colorPickerStruct::colorPickerStruct() : collapsedSize(40, 32), fullSize(250, 180)
	{}

	skinWidgetDefaultsStruct::graphCanvasStruct::graphCanvasStruct() : size(200, 120)
	{}

	skinWidgetDefaultsStruct::scrollableBaseStruct::scrollableBaseStruct() : textFormat(text), contentPadding(2, 2, 2, 2), baseMargin(0, 0, 0, 0), scrollbarsSizes(16, 16), captionHeight(28)
	{}

	skinWidgetDefaultsStruct::groupBoxStruct::groupBoxStruct() :  imageFormat(image)
	{
		text.align = textAlignEnum::Center;
	}

	skinWidgetDefaultsStruct::windowStruct::windowStruct() : imageFormat(image), buttonsSpacing(0)
	{
		textFormat.align = textAlignEnum::Center;
	}

	skinWidgetDefaultsStruct::taskBarStruct::taskBarStruct() : textFormat(text), imageFormat(image), size(400, 32)
	{}

	skinWidgetDefaultsStruct::tooltipStruct::tooltipStruct() : textFormat(text)
	{}
}
