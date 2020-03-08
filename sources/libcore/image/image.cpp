#include "image.h"

#include <cage-core/math.h>
#include <cage-core/color.h>

#include <utility>

namespace cage
{
	uint32 formatBytes(ImageFormatEnum format)
	{
		switch (format)
		{
		case ImageFormatEnum::U8: return sizeof(uint8);
		case ImageFormatEnum::U16: return sizeof(uint16);
		case ImageFormatEnum::Rgbe: return sizeof(uint32);
		case ImageFormatEnum::Float: return sizeof(float);
		case ImageFormatEnum::FloatLinear: return sizeof(float);
		default: CAGE_THROW_CRITICAL(Exception, "invalid image format");
		}
	}

	void Image::empty(uint32 w, uint32 h, uint32 c, ImageFormatEnum f)
	{
		CAGE_ASSERT(f != ImageFormatEnum::Default);
		CAGE_ASSERT(c > 0);
		CAGE_ASSERT(c == 3 || f != ImageFormatEnum::Rgbe);
		ImageImpl *impl = (ImageImpl*)this;
		impl->width = w;
		impl->height = h;
		impl->channels = c;
		impl->format = f;
		impl->mem.allocate(w * h * c * formatBytes(f));
		impl->mem.zero();
	}

	void Image::loadBuffer(const MemoryBuffer &buffer, uint32 width, uint32 height, uint32 channels, ImageFormatEnum format)
	{
		loadMemory(buffer.data(), buffer.size(), width, height, channels, format);
	}

	void Image::loadMemory(const void *buffer, uintPtr size, uint32 width, uint32 height, uint32 channels, ImageFormatEnum format)
	{
		ImageImpl *impl = (ImageImpl*)this;
		CAGE_ASSERT(size >= width * height * channels * formatBytes(format));
		empty(width, height, channels, format);
		detail::memcpy(impl->mem.data(), buffer, impl->mem.size());
	}

	uint32 Image::width() const
	{
		const ImageImpl *impl = (const ImageImpl*)this;
		return impl->width;
	}

	uint32 Image::height() const
	{
		const ImageImpl *impl = (const ImageImpl*)this;
		return impl->height;
	}

	uint32 Image::channels() const
	{
		const ImageImpl *impl = (const ImageImpl*)this;
		return impl->channels;
	}

	ImageFormatEnum Image::format() const
	{
		const ImageImpl *impl = (const ImageImpl*)this;
		return impl->format;
	}

	float Image::value(uint32 x, uint32 y, uint32 c) const
	{
		const ImageImpl *impl = (const ImageImpl*)this;
		CAGE_ASSERT(impl->channels == 3 || impl->format != ImageFormatEnum::Rgbe);
		CAGE_ASSERT(x < impl->width && y < impl->height && c < impl->channels);
		uint32 offset = (y * impl->width + x) * impl->channels;
		switch (impl->format)
		{
		case ImageFormatEnum::U8:
			return ((uint8 *)impl->mem.data())[offset + c] / 255.f;
		case ImageFormatEnum::U16:
			return ((uint16 *)impl->mem.data())[offset + c] / 65535.f;
		case ImageFormatEnum::Rgbe:
			return colorRgbeToRgb(((uint32 *)impl->mem.data())[offset])[c].value;
		case ImageFormatEnum::Float:
		case ImageFormatEnum::FloatLinear:
			return ((float *)impl->mem.data())[offset + c];
		default:
			CAGE_THROW_CRITICAL(Exception, "invalid image format");
		}
	}

	void Image::value(uint32 x, uint32 y, uint32 c, float v)
	{
		ImageImpl *impl = (ImageImpl*)this;
		CAGE_ASSERT(impl->channels == 3 || impl->format != ImageFormatEnum::Rgbe);
		CAGE_ASSERT(x < impl->width && y < impl->height && c < impl->channels);
		uint32 offset = (y * impl->width + x) * impl->channels;
		v = clamp(v, 0.f, 1.f);
		switch (impl->format)
		{
		case ImageFormatEnum::U8:
			((uint8 *)impl->mem.data())[offset + c] = numeric_cast<uint8>(v * 255.f);
			break;
		case ImageFormatEnum::U16:
			((uint16 *)impl->mem.data())[offset + c] = numeric_cast<uint16>(v * 65535.f);
			break;
		case ImageFormatEnum::Rgbe:
		{
			uint32 &p = ((uint32 *)impl->mem.data())[offset];
			vec3 s = colorRgbeToRgb(p);
			s[c] = v;
			p = colorRgbToRgbe(s);
		} break;
		case ImageFormatEnum::Float:
		case ImageFormatEnum::FloatLinear:
			((float *)impl->mem.data())[offset + c] = v;
			break;
		default:
			CAGE_THROW_CRITICAL(Exception, "invalid image format");
		}
	}

	void Image::value(uint32 x, uint32 y, uint32 c, const real &v) { value(x, y, c, v.value); }

	real Image::get1(uint32 x, uint32 y) const
	{
		CAGE_ASSERT(channels() == 1);
		return value(x, y, 0);
	}

	vec2 Image::get2(uint32 x, uint32 y) const
	{
		ImageImpl *impl = (ImageImpl*)this;
		CAGE_ASSERT(impl->channels == 2);
		CAGE_ASSERT(x < impl->width && y < impl->height);
		uint32 offset = (y * impl->width + x) * 2;
		switch (impl->format)
		{
		case ImageFormatEnum::U8:
		{
			uint8 *p = ((uint8 *)impl->mem.data()) + offset;
			return vec2(p[0], p[1]) / 255;
		}
		case ImageFormatEnum::U16:
		{
			uint16 *p = ((uint16 *)impl->mem.data()) + offset;
			return vec2(p[0], p[1]) / 65535;
		}
		case ImageFormatEnum::Float:
		case ImageFormatEnum::FloatLinear:
		{
			float *p = ((float *)impl->mem.data()) + offset;
			return vec2(p[0], p[1]);
		}
		default:
			CAGE_THROW_CRITICAL(Exception, "invalid image format");
		}
	}

	vec3 Image::get3(uint32 x, uint32 y) const
	{
		ImageImpl *impl = (ImageImpl*)this;
		CAGE_ASSERT(impl->channels == 3);
		CAGE_ASSERT(x < impl->width && y < impl->height);
		uint32 offset = (y * impl->width + x) * 3;
		switch (impl->format)
		{
		case ImageFormatEnum::U8:
		{
			uint8 *p = ((uint8 *)impl->mem.data()) + offset;
			return vec3(p[0], p[1], p[2]) / 255;
		}
		case ImageFormatEnum::U16:
		{
			uint16 *p = ((uint16 *)impl->mem.data()) + offset;
			return vec3(p[0], p[1], p[2]) / 65535;
		}
		case ImageFormatEnum::Rgbe:
		{
			uint32 p = ((uint32 *)impl->mem.data())[offset];
			return colorRgbeToRgb(p);
		}
		case ImageFormatEnum::Float:
		case ImageFormatEnum::FloatLinear:
		{
			float *p = ((float *)impl->mem.data()) + offset;
			return vec3(p[0], p[1], p[2]);
		}
		default:
			CAGE_THROW_CRITICAL(Exception, "invalid image format");
		}
	}

	vec4 Image::get4(uint32 x, uint32 y) const
	{
		ImageImpl *impl = (ImageImpl*)this;
		CAGE_ASSERT(impl->channels == 4);
		CAGE_ASSERT(x < impl->width && y < impl->height);
		uint32 offset = (y * impl->width + x) * 4;
		switch (impl->format)
		{
		case ImageFormatEnum::U8:
		{
			uint8 *p = ((uint8 *)impl->mem.data()) + offset;
			return vec4(p[0], p[1], p[2], p[3]) / 255;
		}
		case ImageFormatEnum::U16:
		{
			uint16 *p = ((uint16 *)impl->mem.data()) + offset;
			return vec4(p[0], p[1], p[2], p[3]) / 65535;
		}
		case ImageFormatEnum::Float:
		case ImageFormatEnum::FloatLinear:
		{
			float *p = ((float *)impl->mem.data()) + offset;
			return vec4(p[0], p[1], p[2], p[3]);
		}
		default:
			CAGE_THROW_CRITICAL(Exception, "invalid image format");
		}
	}

	void Image::get(uint32 x, uint32 y, real &value) const { value = get1(x, y); }
	void Image::get(uint32 x, uint32 y, vec2 &value) const { value = get2(x, y); }
	void Image::get(uint32 x, uint32 y, vec3 &value) const { value = get3(x, y); }
	void Image::get(uint32 x, uint32 y, vec4 &value) const { value = get4(x, y); }

	void Image::set(uint32 x, uint32 y, const real &v)
	{
		CAGE_ASSERT(channels() == 1);
		value(x, y, 0, v.value);
	}

	void Image::set(uint32 x, uint32 y, const vec2 &v)
	{
		ImageImpl *impl = (ImageImpl*)this;
		CAGE_ASSERT(impl->channels == 2);
		CAGE_ASSERT(x < impl->width && y < impl->height);
		uint32 offset = (y * impl->width + x) * 2;
		switch (impl->format)
		{
		case ImageFormatEnum::U8:
		{
			uint8 *p = ((uint8 *)impl->mem.data()) + offset;
			vec2 vv = v * 255;
			for (int i = 0; i < 2; i++)
				p[i] = numeric_cast<uint8>(vv[i]);
		} break;
		case ImageFormatEnum::U16:
		{
			uint16 *p = ((uint16 *)impl->mem.data()) + offset;
			vec2 vv = v * 65535;
			for (int i = 0; i < 2; i++)
				p[i] = numeric_cast<uint16>(vv[i]);
		} break;
		case ImageFormatEnum::Float:
		case ImageFormatEnum::FloatLinear:
		{
			float *p = ((float *)impl->mem.data()) + offset;
			*(vec2 *)p = v;
		} break;
		default:
			CAGE_THROW_CRITICAL(Exception, "invalid image format");
		}
	}

	void Image::set(uint32 x, uint32 y, const vec3 &v)
	{
		ImageImpl *impl = (ImageImpl*)this;
		CAGE_ASSERT(impl->channels == 3);
		CAGE_ASSERT(x < impl->width && y < impl->height);
		uint32 offset = (y * impl->width + x) * 3;
		switch (impl->format)
		{
		case ImageFormatEnum::U8:
		{
			uint8 *p = ((uint8 *)impl->mem.data()) + offset;
			vec3 vv = v * 255;
			for (int i = 0; i < 3; i++)
				p[i] = numeric_cast<uint8>(vv[i]);
		} break;
		case ImageFormatEnum::U16:
		{
			uint16 *p = ((uint16 *)impl->mem.data()) + offset;
			vec3 vv = v * 65535;
			for (int i = 0; i < 3; i++)
				p[i] = numeric_cast<uint16>(vv[i]);
		} break;
		case ImageFormatEnum::Rgbe:
		{
			uint32 &p = ((uint32 *)impl->mem.data())[offset];
			p = colorRgbToRgbe(v);
		} break;
		case ImageFormatEnum::Float:
		case ImageFormatEnum::FloatLinear:
		{
			float *p = ((float *)impl->mem.data()) + offset;
			*(vec3 *)p = v;
		} break;
		default:
			CAGE_THROW_CRITICAL(Exception, "invalid image format");
		}
	}

	void Image::set(uint32 x, uint32 y, const vec4 &v)
	{
		ImageImpl *impl = (ImageImpl*)this;
		CAGE_ASSERT(impl->channels == 4);
		CAGE_ASSERT(x < impl->width && y < impl->height);
		uint32 offset = (y * impl->width + x) * 4;
		switch (impl->format)
		{
		case ImageFormatEnum::U8:
		{
			uint8 *p = ((uint8 *)impl->mem.data()) + offset;
			vec4 vv = v * 255;
			for (int i = 0; i < 4; i++)
				p[i] = numeric_cast<uint8>(vv[i]);
		} break;
		case ImageFormatEnum::U16:
		{
			uint16 *p = ((uint16 *)impl->mem.data()) + offset;
			vec4 vv = v * 65535;
			for (int i = 0; i < 4; i++)
				p[i] = numeric_cast<uint16>(vv[i]);
		} break;
		case ImageFormatEnum::Float:
		case ImageFormatEnum::FloatLinear:
		{
			float *p = ((float *)impl->mem.data()) + offset;
			*(vec4 *)p = v;
		} break;
		default:
			CAGE_THROW_CRITICAL(Exception, "invalid image format");
		}
	}

	PointerRange<const uint8> Image::rawViewU8n() const
	{
		ImageImpl *impl = (ImageImpl*)this;
		CAGE_ASSERT(impl->format == ImageFormatEnum::U8);
		return { (uint8*)impl->mem.data(), (uint8*)(impl->mem.data() + impl->mem.size()) };
	}

	PointerRange<const float> Image::rawViewFloat() const
	{
		ImageImpl *impl = (ImageImpl*)this;
		CAGE_ASSERT(impl->format == ImageFormatEnum::Float || impl->format == ImageFormatEnum::FloatLinear);
		return { (float*)impl->mem.data(), (float*)(impl->mem.data() + impl->mem.size()) };
	}

	void Image::verticalFlip()
	{
		ImageImpl *impl = (ImageImpl*)this;
		uint32 lineSize = formatBytes(impl->format) * impl->channels * impl->width;
		uint32 swapsCount = impl->height / 2;
		MemoryBuffer tmp;
		tmp.allocate(lineSize);
		for (uint32 i = 0; i < swapsCount; i++)
		{
			detail::memcpy(tmp.data(), impl->mem.data() + i * lineSize, lineSize);
			detail::memcpy(impl->mem.data() + i * lineSize, impl->mem.data() + (impl->height - i - 1) * lineSize, lineSize);
			detail::memcpy(impl->mem.data() + (impl->height - i - 1) * lineSize, tmp.data(), lineSize);
		}
	}

	void Image::premultiplyAlpha(const ImageOperationsConfig &config)
	{
		CAGE_THROW_CRITICAL(NotImplemented, "image premultiplyAlpha");
	}

	void Image::demultiplyAlpha(const ImageOperationsConfig &config)
	{
		CAGE_THROW_CRITICAL(NotImplemented, "image demultiplyAlpha");
	}

	void Image::gammaToLinear(const ImageOperationsConfig &config)
	{
		CAGE_THROW_CRITICAL(NotImplemented, "image gammaToLinear");
	}

	void Image::linearToGamma(const ImageOperationsConfig &config)
	{
		CAGE_THROW_CRITICAL(NotImplemented, "image linearToGamma");
	}

	void Image::resize(uint32 width, uint32 height, const ImageOperationsConfig &config)
	{
		CAGE_THROW_CRITICAL(NotImplemented, "image resize");
	}

	void Image::convert(uint32 channels)
	{
		ImageImpl *impl = (ImageImpl*)this;
		if (impl->channels == channels)
			return; // no op
		CAGE_THROW_CRITICAL(NotImplemented, "image convert");
	}

	void Image::convert(ImageFormatEnum format)
	{
		ImageImpl *impl = (ImageImpl*)this;
		if (impl->format == format)
			return; // no op
		Holder<Image> tmp = newImage();
		tmp->empty(impl->width, impl->height, impl->channels, format);
		imageBlit(this, tmp.get(), 0, 0, 0, 0, impl->width, impl->height);
		ImageImpl *t = (ImageImpl *)tmp.get();
		std::swap(impl->mem, t->mem);
		std::swap(impl->format, t->format);
	}

	Holder<Image> newImage()
	{
		return detail::systemArena().createImpl<Image, ImageImpl>();
	}

	namespace
	{
		bool overlaps(uint32 x1, uint32 y1, uint32 s)
		{
			if (x1 > y1)
				std::swap(x1, y1);
			uint32 x2 = x1 + s;
			uint32 y2 = y1 + s;
			return x1 < y2 && y1 < x2;
		}

		bool overlaps(uint32 x1, uint32 y1, uint32 x2, uint32 y2, uint32 w, uint32 h)
		{
			return overlaps(x1, x2, w) && overlaps(y1, y2, h);
		}
	}

	void imageBlit(const Image *sourceImg, Image *targetImg, uint32 sourceX, uint32 sourceY, uint32 targetX, uint32 targetY, uint32 width, uint32 height)
	{
		ImageImpl *s = (ImageImpl*)sourceImg;
		ImageImpl *t = (ImageImpl*)targetImg;
		CAGE_ASSERT(s->format != ImageFormatEnum::Default && s->channels > 0);
		CAGE_ASSERT(s != t || !overlaps(sourceX, sourceY, targetX, targetY, width, height));
		if (t->format == ImageFormatEnum::Default && targetX == 0 && targetY == 0)
			t->empty(width, height, s->channels, s->format);
		CAGE_ASSERT(s->channels == t->channels);
		CAGE_ASSERT(sourceX + width <= s->width);
		CAGE_ASSERT(sourceY + height <= s->height);
		CAGE_ASSERT(targetX + width <= t->width);
		CAGE_ASSERT(targetY + height <= t->height);
		if (s->format == t->format)
		{
			uint32 ps = formatBytes(s->format) * s->channels;
			uint32 sl = s->width * ps;
			uint32 tl = t->width * ps;
			char *ss = s->mem.data();
			char *tt = t->mem.data();
			for (uint32 y = 0; y < height; y++)
				detail::memcpy(tt + (targetY + y) * tl + targetX * ps, ss + (sourceY + y) * sl + sourceX * ps, width * ps);
		}
		else
		{
			uint32 cc = s->channels;
			switch (cc)
			{
			case 1:
			{
				for (uint32 y = 0; y < height; y++)
					for (uint32 x = 0; x < width; x++)
						t->set(targetX + x, targetY + y, s->get1(sourceX + x, sourceY + y));
			} break;
			case 2:
			{
				for (uint32 y = 0; y < height; y++)
					for (uint32 x = 0; x < width; x++)
						t->set(targetX + x, targetY + y, s->get2(sourceX + x, sourceY + y));
			} break;
			case 3:
			{
				for (uint32 y = 0; y < height; y++)
					for (uint32 x = 0; x < width; x++)
						t->set(targetX + x, targetY + y, s->get3(sourceX + x, sourceY + y));
			} break;
			case 4:
			{
				for (uint32 y = 0; y < height; y++)
					for (uint32 x = 0; x < width; x++)
						t->set(targetX + x, targetY + y, s->get4(sourceX + x, sourceY + y));
			} break;
			default:
			{
				for (uint32 y = 0; y < height; y++)
					for (uint32 x = 0; x < width; x++)
						for (uint32 c = 0; c < cc; c++)
							t->value(targetX + x, targetY + y, c, s->value(sourceX + x, sourceY + y, c));
			} break;
			}
		}
	}
}
