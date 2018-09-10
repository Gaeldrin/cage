namespace cage
{
	CAGE_API void checkGlError();

#ifdef CAGE_DEBUG
#define CAGE_CHECK_GL_ERROR_DEBUG() { try { checkGlError(); } catch (const ::cage::graphicsException &) { CAGE_LOG(severityEnum::Error, "exception", "opengl error cought in file '" __FILE__ "' at line " CAGE_STRINGIZE(__LINE__) ); } }
#else
#define CAGE_CHECK_GL_ERROR_DEBUG()
#endif

	struct CAGE_API graphicsException : public codeException
	{
		graphicsException(GCHL_EXCEPTION_GENERATE_CTOR_PARAMS, uint32 code) noexcept;
		virtual graphicsException &log();
	};

	namespace detail
	{
		CAGE_API void purgeGlShaderCache();
	}
}
