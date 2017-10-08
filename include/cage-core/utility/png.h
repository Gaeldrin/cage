#ifndef guard_png_h_681DF37FA76B4FA48C656E96AF90EE69
#define guard_png_h_681DF37FA76B4FA48C656E96AF90EE69

namespace cage
{
	class CAGE_API pngImageClass
	{
	public:
		void empty(uint32 w, uint32 h, uint32 c = 4, uint32 bpc = 1);
		void encodeBuffer(memoryBuffer &buffer);
		void encodeFile(const string &filename);

		void decodeBuffer(const memoryBuffer &buffer);
		void decodeBuffer(const memoryBuffer &buffer, uint32 channels, uint32 bpc = 1);
		void decodeMemory(const void *buffer, uintPtr size);
		void decodeMemory(const void *buffer, uintPtr size, uint32 channels, uint32 bpc = 1);
		void decodeFile(const string &filename);
		void decodeFile(const string &filename, uint32 channels, uint32 bpc = 1);

		uint32 width() const;
		uint32 height() const;
		uint32 channels() const;
		uint32 bytesPerChannel() const;

		void *bufferData();
		uintPtr bufferSize() const;

		float value(uint32 x, uint32 y, uint32 c) const;
		void value(uint32 x, uint32 y, uint32 c, float v);

		void verticalFlip();
	};

	CAGE_API holder<pngImageClass> newPngImage();
}

#endif // guard_png_h_681DF37FA76B4FA48C656E96AF90EE69
