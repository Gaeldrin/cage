#ifndef guard_variableSmoothingBuffer_h_A47863F30E264545882F83741CC64A3F
#define guard_variableSmoothingBuffer_h_A47863F30E264545882F83741CC64A3F

#include "math.h"

namespace cage
{
	template<class T, uint32 N = 16>
	struct VariableSmoothingBuffer
	{
		void seed(const T &value)
		{
			for (uint32 i = 0; i < N; i++)
				buffer[i] = value;
			sum = value * N;
		}

		void add(const T &value)
		{
			index = (index + 1) % N;
			sum += value - buffer[index];
			buffer[index] = value;
		}

		T smooth() const
		{
			return sum / N;
		}

		T current() const
		{
			return buffer[index];
		}

		T oldest() const
		{
			return buffer[(index + 1) % N];
		}

		T max() const
		{
			T res = buffer[0];
			for (uint32 i = 1; i < N; i++)
				res = cage::max(res, buffer[i]);
			return res;
		}

		T min() const
		{
			T res = buffer[0];
			for (uint32 i = 1; i < N; i++)
				res = cage::min(res, buffer[i]);
			return res;
		}

	private:
		uint32 index = 0;
		T buffer[N] = {};
		T sum = T();
	};

	namespace privat
	{
		CAGE_CORE_API Quat averageQuaternions(PointerRange<const Quat> quaternions);
	}

	template<uint32 N>
	struct VariableSmoothingBuffer<Quat, N>
	{
		void seed(const Quat &value)
		{
			for (uint32 i = 0; i < N; i++)
				buffer[i] = value;
			avg = value;
		}

		void add(const Quat &value)
		{
			index = (index + 1) % N;
			buffer[index] = value;
			dirty = true;
		}

		Quat smooth() const
		{
			if (dirty)
			{
				avg = privat::averageQuaternions(buffer);
				dirty = false;
			}
			return avg;
		}

		Quat current() const
		{
			return buffer[index];
		}

		Quat oldest() const
		{
			return buffer[(index + 1) % N];
		}

	private:
		uint32 index = 0;
		Quat buffer[N];
		mutable Quat avg;
		mutable bool dirty = false;
	};
}

#endif // guard_variableSmoothingBuffer_h_A47863F30E264545882F83741CC64A3F
