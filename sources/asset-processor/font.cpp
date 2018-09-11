#include <vector>

#include "processor.h"

#include <cage-core/utility/hashString.h>
#include <cage-core/utility/png.h>
#include <cage-core/utility/memoryBuffer.h>
#include <cage-core/utility/serialization.h>
#include "utility/binPacking.h"

#include <msdfgen.h>
#include <msdfgen-ext.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#define CALL(FNC, ...) { int err = FNC(__VA_ARGS__); if (err) { CAGE_LOG(severityEnum::Note, "exception", translateErrorCode(err)); CAGE_THROW_ERROR(codeException, "FreeType " CAGE_STRINGIZE(FNC) " error", err); } }

namespace
{
	const cage::string translateErrorCode(int code)
	{
		switch (code)
#undef __FTERRORS_H__
#define FT_ERRORDEF(E,V,S)  case V: return S;
#define FT_ERROR_START_LIST     {
#define FT_ERROR_END_LIST       default: CAGE_THROW_ERROR(exception, "unknown freetype error code"); };
#include FT_ERRORS_H
	}

	FT_Library library;
	FT_Face face;

	struct glyphStruct
	{
		fontHeaderStruct::glyphDataStruct data;
		holder<pngImageClass> png;
		uint32 pngX, pngY;
		glyphStruct() : pngX(0), pngY(0)
		{}
	};

	fontHeaderStruct data;

	std::vector<glyphStruct> glyphs;
	std::vector<real> kerning;
	std::vector<uint32> charsetChars;
	std::vector<uint32> charsetGlyphs;
	holder<pngImageClass> texels;

	vec3 to(const msdfgen::FloatRGB &rgb)
	{
		return vec3(rgb.r, rgb.g, rgb.b);
	}

	msdfgen::Vector2 from(const vec2 &v)
	{
		return msdfgen::Vector2(v[0].value, v[1].value);
	}

	real fontScale;
	real maxOffTop, maxOffBottom;
	static const uint32 border = 6;

	void glyphImage(glyphStruct &g, msdfgen::Shape &shape)
	{
		shape.normalize();
		msdfgen::edgeColoringSimple(shape, 3.0);
		if (!shape.validate())
			CAGE_THROW_ERROR(exception, "shape validation failed");
		double l = real::PositiveInfinity.value, b = real::PositiveInfinity.value, r = real::NegativeInfinity.value, t = real::NegativeInfinity.value;
		shape.bounds(l, b, r, t);

		// generate glyph image
		g.png = newPngImage();
		g.png->empty(numeric_cast<uint32>(r - l) + border * 2, numeric_cast<uint32>(t - b) + border * 2, 3);
		msdfgen::Bitmap<msdfgen::FloatRGB> msdf(g.png->width(), g.png->height());
		msdfgen::generateMSDF(msdf, shape, 4.0, 1.0, from(-vec2(l, b) + border));
		for (uint32 y = 0; y < g.png->height(); y++)
			for (uint32 x = 0; x < g.png->width(); x++)
				for (uint32 c = 0; c < 3; c++)
					g.png->value(x, y, c, to(msdf(x, y))[c].value);

		// glyph size compensation for the border
		vec2 ps = vec2(g.png->width(), g.png->height()) - 2 * border;
		vec2 br = border * g.data.size / ps;
		g.data.size += 2 * br;
		g.data.bearing -= br;
	}

	void loadGlyphs()
	{
		CAGE_LOG(severityEnum::Info, logComponentName, "load glyphs");
		data.glyphCount = numeric_cast<uint32>(face->num_glyphs);
		CAGE_LOG(severityEnum::Info, logComponentName, string() + "font has " + data.glyphCount + " glyphs");
		glyphs.reserve(data.glyphCount + 10);
		glyphs.resize(data.glyphCount);
		uint32 maxPngW = 0, maxPngH = 0;
		for (uint32 glyphIndex = 0; glyphIndex < data.glyphCount; glyphIndex++)
		{
			glyphStruct &g = glyphs[glyphIndex];
			CALL(FT_Load_Glyph, face, glyphIndex, FT_LOAD_DEFAULT);

			// load glyph metrics
			const FT_Glyph_Metrics &glm = face->glyph->metrics;
			g.data.size[0] = glm.width / 64.0;
			g.data.size[1] = glm.height / 64.0;
			g.data.bearing[0] = glm.horiBearingX / 64.0;
			g.data.bearing[1] = glm.horiBearingY / 64.0;
			g.data.advance = glm.horiAdvance / 64.0;
			g.data.size *= fontScale;
			g.data.bearing *= fontScale;
			g.data.advance *= fontScale;

			// load glyph shape
			msdfgen::Shape shape;
			msdfgen::loadGlyphSlot(shape, face->glyph);
			if (!shape.contours.empty())
				glyphImage(g, shape);

			// update global data
			data.glyphMaxSize = max(data.glyphMaxSize, g.data.size);
			maxOffTop = max(maxOffTop, g.data.size[1] - g.data.bearing[1]);
			maxOffBottom = max(maxOffBottom, g.data.bearing[1]);
			if (g.png)
			{
				maxPngW = max(maxPngW, g.png->width());
				maxPngH = max(maxPngH, g.png->height());
			}
		}
		data.firstLineOffset = maxOffTop;
		data.lineHeight = ((face->height * fontScale / 64.0) + (maxOffTop + maxOffBottom)) / 2;
		CAGE_LOG(severityEnum::Note, logComponentName, string() + "units per EM: " + face->units_per_EM);
		CAGE_LOG(severityEnum::Note, logComponentName, string() + "(unused) line height (top + bottom): " + (maxOffTop + maxOffBottom));
		CAGE_LOG(severityEnum::Note, logComponentName, string() + "(unused) line height (from font): " + (face->height * fontScale / 64.0));
		CAGE_LOG(severityEnum::Note, logComponentName, string() + "line height: " + data.lineHeight);
		CAGE_LOG(severityEnum::Note, logComponentName, string() + "first line offset: " + data.firstLineOffset);
		CAGE_LOG(severityEnum::Note, logComponentName, string() + "max glyph size: " + data.glyphMaxSize);
		CAGE_LOG(severityEnum::Note, logComponentName, string() + "max glyph image size: " + maxPngW + "*" + maxPngH);
	}

	void loadCharset()
	{
		CAGE_LOG(severityEnum::Info, logComponentName, "load charset");
		charsetChars.reserve(1000);
		charsetGlyphs.reserve(1000);
		FT_ULong charcode;
		FT_UInt glyphIndex;
		charcode = FT_Get_First_Char(face, &glyphIndex);
		while (glyphIndex)
		{
			charsetChars.push_back(numeric_cast<uint32>(charcode));
			charsetGlyphs.push_back(numeric_cast<uint32>(glyphIndex));
			charcode = FT_Get_Next_Char(face, charcode, &glyphIndex);
		}
		data.charCount = numeric_cast<uint32>(charsetChars.size());
		CAGE_LOG(severityEnum::Info, logComponentName, string() + "font has " + data.charCount + " characters");
	}

	void loadKerning()
	{
		CAGE_LOG(severityEnum::Info, logComponentName, "load kerning");
		if (FT_HAS_KERNING(face))
		{
			kerning.resize(data.glyphCount * data.glyphCount);
			for (uint32 L = 0; L < data.glyphCount; L++)
			{
				for (uint32 R = 0; R < data.glyphCount; R++)
				{
					FT_Vector k;
					CALL(FT_Get_Kerning, face, L, R, FT_KERNING_DEFAULT, &k);
					kerning[L * data.glyphCount + R] = fontScale * k.x / 64.0;
				}
			}
			data.flags |= fontFlags::Kerning;
		}
		else
			CAGE_LOG(severityEnum::Info, logComponentName, "font has no kerning");
	}

	msdfgen::Shape cursorShape()
	{
		msdfgen::Shape shape;
		msdfgen::Contour &c = shape.addContour();
		float x = 0.1f / fontScale.value;
		float y0 = (-maxOffBottom / fontScale).value;
		float y1 = (maxOffTop / fontScale).value;
		msdfgen::Point2 points[4] = {
			{0, y0}, {0, y1}, {x, y1}, {x, y0}
		};
		c.addEdge(msdfgen::EdgeHolder(points[0], points[1]));
		c.addEdge(msdfgen::EdgeHolder(points[1], points[2]));
		c.addEdge(msdfgen::EdgeHolder(points[2], points[3]));
		c.addEdge(msdfgen::EdgeHolder(points[3], points[0]));
		return shape;
	}

	void addArtificialData()
	{
		CAGE_LOG(severityEnum::Info, logComponentName, "adding artificial data");

		bool foundReturn = false;
		for (uint32 i = 0; i < data.charCount; i++)
		{
			if (charsetChars[i] == '\n')
			{
				foundReturn = true;
				break;
			}
		}
		if (!foundReturn)
		{
			CAGE_LOG(severityEnum::Warning, logComponentName, string() + "artificially adding return");
			uint32 idx = 0;
			while (idx < charsetChars.size() && charsetChars[idx] < '\n')
				idx++;
			charsetChars.insert(charsetChars.begin() + idx, '\n');
			charsetGlyphs.insert(charsetGlyphs.begin() + idx, -1);
			data.charCount++;
		}

		{ // add cursor
			CAGE_LOG(severityEnum::Warning, logComponentName, string() + "artificially adding cursor glyph");
			data.glyphCount++;
			glyphs.resize(glyphs.size() + 1);
			glyphStruct &g = glyphs[glyphs.size() - 1];
			msdfgen::Shape shape = cursorShape();
			glyphImage(g, shape);
			g.data.advance = 0;
			g.data.bearing[0] = -0.1;
			g.data.bearing[1] = data.lineHeight * 3 / 5;
			g.data.size[0] = 0.2;
			g.data.size[1] = data.lineHeight;

			if (!kerning.empty())
			{ // compensate kerning
				std::vector<real> k;
				k.resize(data.glyphCount * data.glyphCount);
				uint32 dgcm1 = data.glyphCount - 1;
				for (uint32 i = 0; i < dgcm1; i++)
					detail::memcpy(&k[i * data.glyphCount], &kerning[i * dgcm1], dgcm1 * sizeof(real));
				std::swap(k, kerning);
			}
		}
	}

	void createAtlasCoordinates()
	{
		CAGE_LOG(severityEnum::Info, logComponentName, "create atlas coordinates");
		holder<binPackingClass> packer = newBinPacking();
		uint32 area = 0;
		uint32 mgs = 0;
		for (uint32 glyphIndex = 0; glyphIndex < data.glyphCount; glyphIndex++)
		{
			glyphStruct &g = glyphs[glyphIndex];
			if (!g.png)
				continue;
			area += g.png->width() * g.png->height();
			mgs = max(mgs, max(g.png->width(), g.png->height()));
			packer->add(glyphIndex, g.png->width(), g.png->height());
		}
		uint32 res = 64;
		while (res < mgs) res *= 2;
		while (res * res < area) res *= 2;
		while (true)
		{
			CAGE_LOG(severityEnum::Info, logComponentName, string() + "trying to pack into resolution " + res + "*" + res);
			if (packer->solve(res, res))
				break;
			res *= 2;
		}
		for (uint32 index = 0, e = packer->count(); index < e; index++)
		{
			uint32 glyphIndex = 0, x = 0, y = 0;
			packer->get(index, glyphIndex, x, y);
			CAGE_ASSERT_RUNTIME(glyphIndex < glyphs.size(), glyphIndex, glyphs.size());
			glyphStruct &g = glyphs[glyphIndex];
			CAGE_ASSERT_RUNTIME(x < res, "texture x coordinate out of range", x, res, index, glyphIndex);
			CAGE_ASSERT_RUNTIME(y < res, "texture y coordinate out of range", y, res, index, glyphIndex);
			g.pngX = x;
			g.pngY = y;
			vec2 to = vec2(g.pngX, g.pngY) / res;
			vec2 ts = vec2(g.png->width(), g.png->height()) / res;
			g.data.texUv = vec4(to, ts);
		}
		data.texWidth = res;
		data.texHeight = res;
		CAGE_LOG(severityEnum::Info, logComponentName, string() + "texture atlas resolution " + data.texWidth + "*" + data.texHeight);
	}

	void createAtlasPixels()
	{
		CAGE_LOG(severityEnum::Info, logComponentName, "create atlas pixels");
		texels = newPngImage();
		texels->empty(data.texWidth, data.texHeight, 3);
		for (uint32 glyphIndex = 0; glyphIndex < data.glyphCount; glyphIndex++)
		{
			glyphStruct &g = glyphs[glyphIndex];
			if (!g.png)
				continue;
			pngBlit(g.png.get(), texels.get(), 0, 0, g.pngX, g.pngY, g.png->width(), g.png->height());
		}
		data.texSize = numeric_cast<uint32>(texels->bufferSize());
	}

	void exportData()
	{
		CAGE_LOG(severityEnum::Info, logComponentName, "export data");

		CAGE_ASSERT_RUNTIME(glyphs.size() == data.glyphCount, glyphs.size(), data.glyphCount);
		CAGE_ASSERT_RUNTIME(charsetChars.size() == data.charCount, charsetChars.size(), data.charCount);
		CAGE_ASSERT_RUNTIME(charsetChars.size() == charsetGlyphs.size(), charsetChars.size(), charsetGlyphs.size());
		CAGE_ASSERT_RUNTIME(kerning.size() == 0 || kerning.size() == data.glyphCount * data.glyphCount, kerning.size(), data.glyphCount * data.glyphCount, data.glyphCount);

		assetHeaderStruct h = initializeAssetHeaderStruct();
		h.originalSize = sizeof(data) + data.texSize +
			data.glyphCount * sizeof(fontHeaderStruct::glyphDataStruct) +
			sizeof(real) * numeric_cast<uint32>(kerning.size()) +
			sizeof(uint32) * numeric_cast<uint32>(charsetChars.size()) +
			sizeof(uint32) * numeric_cast<uint32>(charsetGlyphs.size());

		memoryBuffer buf;
		serializer sr(buf);

		sr << data;
		CAGE_ASSERT_RUNTIME(texels->bufferSize() == data.texSize);
		sr.write(texels->bufferData(), texels->bufferSize());
		for (uint32 glyphIndex = 0; glyphIndex < data.glyphCount; glyphIndex++)
			sr << glyphs[glyphIndex].data;
		if (kerning.size() > 0)
			sr.write(&kerning[0], sizeof(kerning[0]) * kerning.size());
		if (charsetChars.size() > 0)
		{
			sr.write(&charsetChars[0], sizeof(charsetChars[0]) * charsetChars.size());
			sr.write(&charsetGlyphs[0], sizeof(charsetGlyphs[0]) * charsetGlyphs.size());
		}

		CAGE_ASSERT_RUNTIME(h.originalSize == buf.size());
		CAGE_LOG(severityEnum::Info, logComponentName, string() + "buffer size (before compression): " + buf.size());
		buf = detail::compress(buf);
		CAGE_LOG(severityEnum::Info, logComponentName, string() + "buffer size (after compression): " + buf.size());
		h.compressedSize = buf.size();

		holder<fileClass> f = newFile(outputFileName, fileMode(false, true));
		f->write(&h, sizeof(h));
		f->writeBuffer(buf);
		f->close();
	}

	void printDebugData()
	{
		CAGE_LOG(severityEnum::Info, logComponentName, "print debug data");

		if (configGetBool("cage-asset-processor.font.preview"))
		{
			texels->verticalFlip();
			texels->encodeFile(pathJoin(configGetString("cage-asset-processor.font.path", "asset-preview"), pathMakeValid(inputName) + ".png"));
		}

		if (configGetBool("cage-asset-processor.font.glyphs"))
		{ // glyphs
			holder<fileClass> f = newFile(pathJoin(configGetString("cage-asset-processor.font.path", "asset-preview"), pathMakeValid(inputName) + ".glyphs.txt"), fileMode(false, true, true));
			f->writeLine(
				string("glyph").fill(10) +
				string("tex coord").fill(60) +
				string("size").fill(30) +
				string("bearing").fill(30) +
				string("advance")
			);
			for (uint32 glyphIndex = 0; glyphIndex < data.glyphCount; glyphIndex++)
			{
				glyphStruct &g = glyphs[glyphIndex];
				f->writeLine(
					string(glyphIndex).fill(10) +
					(string() + g.data.texUv).fill(60) +
					(string() + g.data.size).fill(30) +
					(string() + g.data.bearing).fill(30) +
					string(g.data.advance)
				);
			}
		}

		if (configGetBool("cage-asset-processor.font.characters"))
		{ // characters
			holder<fileClass> f = newFile(pathJoin(configGetString("cage-asset-processor.font.path", "asset-preview"), pathMakeValid(inputName) + ".characters.txt"), fileMode(false, true, true));
			f->writeLine(
				string("code").fill(10) +
				string("char").fill(5) +
				string("glyph")
			);
			for (uint32 charIndex = 0; charIndex < data.charCount; charIndex++)
			{
				uint32 c = charsetChars[charIndex];
				char C = c < 256 ? c : ' ';
				f->writeLine(
					string(c).fill(10) +
					string(&C, 1).fill(5) +
					string(charsetGlyphs[charIndex])
				);
			}
		}

		if (configGetBool("cage-asset-processor.font.kerning"))
		{ // kerning
			holder<fileClass> f = newFile(pathJoin(configGetString("cage-asset-processor.font.path", "asset-preview"), pathMakeValid(inputName) + ".kerning.txt"), fileMode(false, true, true));
			f->writeLine(
				string("g1").fill(5) +
				string("g2").fill(5) +
				string("kerning")
			);
			if (kerning.empty())
				f->writeLine("no data");
			else
			{
				uint32 m = data.glyphCount;
				CAGE_ASSERT_RUNTIME(kerning.size() == m * m, kerning.size(), m);
				for (uint32 x = 0; x < m; x++)
				{
					for (uint32 y = 0; y < m; y++)
					{
						real k = kerning[x * m + y];
						if (k == 0)
							continue;
						f->writeLine(
							string(x).fill(5) +
							string(y).fill(5) +
							string(k)
						);
					}
				}
			}
		}
	}

	void clearAll()
	{
		glyphs.clear();
		kerning.clear();
		charsetChars.clear();
		charsetGlyphs.clear();
		texels.clear();
	}
}

void processFont()
{
	writeLine(string("use=") + inputFile);
	if (!inputSpec.empty())
		CAGE_THROW_ERROR(exception, "input specification must be empty");
	CALL(FT_Init_FreeType, &library);
	CALL(FT_New_Face, library, inputFileName.c_str(), 0, &face);
	if (!FT_IS_SCALABLE(face))
		CAGE_THROW_ERROR(exception, "font is not scalable");
	uint32 resolution = properties("resolution").toUint32();
	CALL(FT_Select_Charmap, face, FT_ENCODING_UNICODE);
	CALL(FT_Set_Pixel_Sizes, face, resolution, resolution);
	fontScale = 1.0f / resolution;
	loadGlyphs();
	loadCharset();
	loadKerning();
	addArtificialData();
	createAtlasCoordinates();
	createAtlasPixels();
	exportData();
	printDebugData();
	clearAll();
	CALL(FT_Done_FreeType, library);
}

void analyzeFont()
{
	try
	{
		CALL(FT_Init_FreeType, &library);
		CALL(FT_New_Face, library, inputFileName.c_str(), 0, &face);
		CALL(FT_Select_Charmap, face, FT_ENCODING_UNICODE);
		writeLine("cage-begin");
		writeLine("scheme=font");
		writeLine(string() + "asset=" + inputFile);
		writeLine("cage-end");
	}
	catch (...)
	{
		// do nothing
	}
	CALL(FT_Done_FreeType, library);
}
