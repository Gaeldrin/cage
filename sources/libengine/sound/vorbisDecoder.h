#include "cage-core/core.h"

#include <vorbis/vorbisfile.h>

#include <vector>

namespace cage
{
	namespace soundPrivat
	{
		struct VorbisData
		{
			VorbisData();
			~VorbisData();
			void init(const void *buffer, uintPtr size);
			void clear();
			void read(float *output, uint32 index, uint32 frames);
			void decode(uint32 &channels, uint32 &frames, uint32 &sampleRate, float *output);

		private:
			std::vector<char> buffer;
			ov_callbacks callbacks;
			std::size_t currentPosition;
			OggVorbis_File ovf;

			static std::size_t read_func(void *ptr, std::size_t size, std::size_t nmemb, void *datasource);
			static int seek_func(void *datasource, ogg_int64_t offset, int whence);
			static long tell_func(void *datasource);
		};
	}
}
