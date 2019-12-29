namespace cage
{
	class CAGE_API frameBuffer : private Immovable
	{
#ifdef CAGE_DEBUG
		detail::StringBase<64> debugName;
#endif // CAGE_DEBUG

	public:
		void setDebugName(const string &name);

		uint32 getId() const;
		uint32 getTarget() const;
		void bind() const;

		void depthTexture(renderTexture *tex);
		void colorTexture(uint32 index, renderTexture *tex, uint32 mipmapLevel = 0);
		void activeAttachments(uint32 mask);
		void clear();
		void checkStatus();
	};

	CAGE_API Holder<frameBuffer> newFrameBufferDraw();
	CAGE_API Holder<frameBuffer> newFrameBufferRead();
}
