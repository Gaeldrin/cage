#ifndef guard_guiComponents_sdf1gh45hk485aws
#define guard_guiComponents_sdf1gh45hk485aws

#include "core.h"

namespace cage
{
	struct MemoryBuffer;

	struct CAGE_ENGINE_API GuiParentComponent
	{
		uint32 parent = 0;
		sint32 order = 0;
	};

	struct CAGE_ENGINE_API GuiImageComponent
	{
		uint64 animationStart = m; // -1 will be replaced by current time
		Vec2 textureUvOffset;
		Vec2 textureUvSize = Vec2(1);
		uint32 textureName = 0;
	};

	enum class ImageModeEnum : uint32
	{
		Stretch,
		// todo
	};

	struct CAGE_ENGINE_API GuiImageFormatComponent
	{
		Real animationSpeed = 1;
		Real animationOffset;
		ImageModeEnum mode = ImageModeEnum::Stretch;
	};

	struct CAGE_ENGINE_API GuiTextComponent
	{
		String value; // list of parameters separated by '|' when formatted, otherwise the string as is
		uint32 assetName = 0;
		uint32 textName = 0;
	};

	struct CAGE_ENGINE_API GuiTextFormatComponent
	{
		Vec3 color = Vec3::Nan();
		uint32 font = 0;
		Real size = Real::Nan();
		Real lineSpacing = Real::Nan();
		TextAlignEnum align = (TextAlignEnum)m;
	};

	struct CAGE_ENGINE_API GuiSelectionComponent
	{
		uint32 start = m; // utf32 characters (not bytes)
		uint32 length = 0; // utf32 characters (not bytes)
	};

	struct CAGE_ENGINE_API GuiTooltipComponent
	{
		String value; // list of parameters separated by '|' when formatted, otherwise the string as is
		uint32 assetName = 0;
		uint32 textName = 0;
	};

	struct CAGE_ENGINE_API GuiWidgetStateComponent
	{
		uint32 skinIndex = m; // -1 = inherit
		bool disabled = false;
	};

	struct CAGE_ENGINE_API GuiSelectedItemComponent
	{};

	enum class OverflowModeEnum : uint32
	{
		Auto, // show scrollbar when needed only
		Always, // always show scrollbar
		Never, // never show scrollbar
		// overflowing content is hidden irrespective of this setting
	};

	struct CAGE_ENGINE_API GuiScrollbarsComponent
	{
		Vec2 alignment; // 0.5 is center
		Vec2 scroll;
		OverflowModeEnum overflow[2] = { OverflowModeEnum::Auto, OverflowModeEnum::Auto };
	};

	struct CAGE_ENGINE_API GuiExplicitSizeComponent
	{
		Vec2 size = Vec2::Nan();
	};

	struct CAGE_ENGINE_API GuiEventComponent
	{
		Delegate<bool(uint32)> event;
	};

	struct CAGE_ENGINE_API GuiLayoutLineComponent
	{
		bool vertical = false; // false -> items are next to each other, true -> items are above/under each other
	};

	struct CAGE_ENGINE_API GuiLayoutTableComponent
	{
		uint32 sections = 2; // set sections to 0 to make it square-ish
		bool grid = false; // false -> each column and row sizes are independent; true -> all columns and rows have same sizes
		bool vertical = true; // false -> fills entire row; true -> fills entire column
	};

	// must have exactly two children
	struct CAGE_ENGINE_API GuiLayoutSplitterComponent
	{
		bool vertical = false; // false -> left sub-area, right sub-area; true -> top, bottom
		bool inverse = false; // false -> first item is fixed size, second item fills the remaining space; true -> second item is fixed size, first item fills the remaining space
	};

	struct CAGE_ENGINE_API GuiLabelComponent
	{
		// GuiTextComponent defines foreground of the widget
		// GuiImageComponent defines background of the widget
	};

	struct CAGE_ENGINE_API GuiButtonComponent
	{
		// GuiTextComponent defines foreground
		// GuiImageComponent defines background
	};

	enum class InputTypeEnum : uint32
	{
		Text,
		Url,
		Email,
		Integer,
		Real,
		Password,
	};

	enum class InputStyleFlags : uint32
	{
		// input box and text area
		None = 0,
		ReadOnly = 1 << 0,
		SelectAllOnFocusGain = 1 << 1,
		GoToEndOnFocusGain = 1 << 2,
		ShowArrowButtons = 1 << 3,
		AlwaysRoundValueToStep = 1 << 4,
		//WriteTabs = 1 << 5, // tab key will write tab rather than skip to next widget
	};

	struct CAGE_ENGINE_API GuiInputComponent
	{
		String value; // utf8 encoded string (size is in bytes)
		union CAGE_ENGINE_API Union
		{
			Real f;
			sint32 i = 0;
			Union() {};
		} min, max, step;
		uint32 cursor = m; // utf32 characters (not bytes)
		InputTypeEnum type = InputTypeEnum::Text;
		InputStyleFlags style = InputStyleFlags::ShowArrowButtons;
		bool valid = false;
		// GuiTextComponent defines placeholder
		// GuiTextFormatComponent defines format
		// GuiSelectionComponent defines selected text
	};

	struct CAGE_ENGINE_API GuiTextAreaComponent
	{
		MemoryBuffer *buffer = nullptr; // utf8 encoded string
		uint32 cursor = m; // utf32 characters (not bytes)
		uint32 maxLength = 1024 * 1024; // bytes
		InputStyleFlags style = InputStyleFlags::None;
		// GuiSelectionComponent defines selected text
	};

	enum class CheckBoxStateEnum : uint32
	{
		Unchecked,
		Checked,
		Indeterminate,
	};

	struct CAGE_ENGINE_API GuiCheckBoxComponent
	{
		CheckBoxStateEnum state = CheckBoxStateEnum::Unchecked;
		// GuiTextComponent defines label shown next to the check box
	};

	struct CAGE_ENGINE_API GuiRadioBoxComponent
	{
		uint32 group = 0; // defines what other radio buttons are unchecked when this becomes checked
		CheckBoxStateEnum state = CheckBoxStateEnum::Unchecked;
		// GuiTextComponent defines label shown next to the radio box
	};

	struct CAGE_ENGINE_API GuiComboBoxComponent
	{
		uint32 selected = m; // -1 = nothing selected
		// GuiTextComponent defines placeholder
		// children with GuiTextComponent defines individual lines
		// GuiTextFormatComponent applies to all lines, may be overridden by individual childs
		// GuiSelectedItemComponent on childs defines which line is selected (the selected property is authoritative)
	};

	struct CAGE_ENGINE_API GuiListBoxComponent
	{
		// real scrollbar;
		// children with GuiTextComponent defines individual lines
		// GuiTextFormatComponent applies to all lines, may be overridden by individual childs
		// GuiSelectedItemComponent on childs defines which lines are selected
	};

	struct CAGE_ENGINE_API GuiProgressBarComponent
	{
		Real progress; // 0 .. 1
		bool showValue = false; // overrides the text with the value (may use internationalization for formatting)
		// GuiTextComponent defines text shown over the bar
	};

	struct CAGE_ENGINE_API GuiSliderBarComponent
	{
		Real value;
		Real min = 0, max = 1;
		bool vertical = false;
	};

	struct CAGE_ENGINE_API GuiColorPickerComponent
	{
		Vec3 color = Vec3(1, 0, 0);
		bool collapsible = false;
	};

	struct CAGE_ENGINE_API GuiPanelComponent
	{
		// GuiTextComponent defines caption
		// GuiImageComponent defines background
	};

	struct CAGE_ENGINE_API GuiSpoilerComponent
	{
		bool collapsesSiblings = true;
		bool collapsed = true;
		// GuiTextComponent defines caption
		// GuiImageComponent defines background
	};
}

#endif // guard_guiComponents_sdf1gh45hk485aws
