#ifndef guard_graphics_h_d776d3a243c7464db721291294b5b1ef_
#define guard_graphics_h_d776d3a243c7464db721291294b5b1ef_

#include "core.h"

namespace cage
{
	CAGE_ENGINE_API void checkGlError();

#ifdef CAGE_DEBUG
#define CAGE_CHECK_GL_ERROR_DEBUG() { try { checkGlError(); } catch (const ::cage::GraphicsError &) { CAGE_LOG(::cage::SeverityEnum::Error, "exception", ::cage::stringizer() + "opengl error caught in file '" + __FILE__ + "' at line " + __LINE__); } }
#else
#define CAGE_CHECK_GL_ERROR_DEBUG() {}
#endif

	struct CAGE_ENGINE_API GraphicsError : public SystemError
	{
		using SystemError::SystemError;
	};

	struct CAGE_ENGINE_API GraphicsDebugScope : private Immovable
	{
		GraphicsDebugScope(const char *name);
		~GraphicsDebugScope();
	};

	namespace detail
	{
		CAGE_ENGINE_API void purgeGlShaderCache();
	}
}

#endif // guard_graphics_h_d776d3a243c7464db721291294b5b1ef_
