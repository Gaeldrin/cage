#include <cage-core/hashString.h>
#include <cage-core/memoryBuffer.h>

#include <cstring>

namespace cage
{
	namespace detail
	{
		uint32 hashBuffer(const char *buffer, uintPtr size)
		{
			uint32 hash = GCHL_HASH_OFFSET;
			while (size--)
			{
				hash ^= *buffer++;
				hash *= GCHL_HASH_PRIME;
			}
			return hash;
		}

		uint32 hashBuffer(const MemoryBuffer &buffer)
		{
			return hashBuffer(buffer.data(), buffer.size());
		}

		uint32 HashString(const char *str)
		{
			return hashBuffer(str, std::strlen(str));
		}
	}
}
