#ifndef guard_guiManager_xf1g9ret213sdr45zh
#define guard_guiManager_xf1g9ret213sdr45zh

#include <cage-engine/inputs.h>

namespace cage
{
	class EntityManager;
	class RenderQueue;
	class ProvisionalGraphics;
	struct GuiSkinConfig;

	class CAGE_ENGINE_API GuiManager : private Immovable
	{
	public:
		void outputResolution(const Vec2i &resolution); // pixels
		Vec2i outputResolution() const;
		void outputRetina(Real retina); // pixels per point (1D)
		Real outputRetina() const;
		void zoom(Real zoom); // pixels per point (1D)
		Real zoom() const;
		void focus(uint32 widget);
		uint32 focus() const;

		void prepare(); // prepare the gui for handling events
		Holder<RenderQueue> finish(); // finish handling events, generate rendering commands, and release resources
		void cleanUp();

		bool handleInput(const GenericInput &);
		void invalidateInputs(); // skip all remaining inputs until next prepare
		Vec2i inputResolution() const;
		Delegate<bool(const Vec2i&, Vec2&)> eventCoordinatesTransformer; // called from prepare or handleInput, it should return false to signal that the point is outside the gui, otherwise the point should be converted from window coordinate system to the gui output resolution coordinate system
		EventDispatcher<bool(const GenericInput &)> widgetEvent; // called from prepare or handleInput

		GuiSkinConfig &skin(uint32 index = 0);
		const GuiSkinConfig &skin(uint32 index = 0) const;

		EntityManager *entities();
	};

	struct CAGE_ENGINE_API GuiManagerCreateConfig
	{
		AssetManager *assetMgr = nullptr;
		ProvisionalGraphics *provisionalGraphics = nullptr;
		uint32 skinsCount = 1;
	};

	CAGE_ENGINE_API Holder<GuiManager> newGuiManager(const GuiManagerCreateConfig &config);
}

#endif // guard_guiManager_xf1g9ret213sdr45zh
