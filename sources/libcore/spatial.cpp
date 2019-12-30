#include <vector>
#include <algorithm>
#include <atomic>
#include <array>

#define CAGE_EXPORT
#include <cage-core/core.h>
#include <cage-core/math.h>
#include <cage-core/geometry.h>
#include <cage-core/memory.h>
#include <cage-core/spatial.h>
#include <cage-core/ctl/unordered_map.h>

#include <xsimd/xsimd.hpp>

namespace cage
{
	namespace
	{
		union alignas(16) fastPoint
		{
			xsimd::batch<float, 4> v4;
			struct
			{
				vec3 v3;
				union
				{
					real f;
					sint32 i;
				};
			} s;

			fastPoint() : v4{ 0,0,0,0 }
			{}
		};

		struct fastBox
		{
			fastPoint low;
			fastPoint high;

			// initialize to negative box
			fastBox()
			{
				low.v4 = { real::Infinity().value, real::Infinity().value, real::Infinity().value, 0 };
				high.v4 = { -real::Infinity().value, -real::Infinity().value, -real::Infinity().value, 0 };
			}

			explicit fastBox(const aabb &b)
			{
				low.s.v3 = b.a;
				high.s.v3 = b.b;
			}

			fastBox operator + (const fastBox &other) const
			{
				fastBox r;
				r.low.v4 = xsimd::min(low.v4, other.low.v4);
				r.high.v4 = xsimd::max(high.v4, other.high.v4);
				return r;
			}

			fastBox &operator += (const fastBox &other)
			{
				return *this = *this + other;
			}

			real surface() const
			{
				if (empty())
					return 0;
				return aabb(*this).surface();
			}

			bool empty() const
			{
				return low.v4[0] == real::Infinity().value;
			}

			explicit operator aabb () const
			{
				return aabb(low.s.v3, high.s.v3);
			}
		};

		bool intersects(const fastBox &a, const fastBox &b)
		{
			CAGE_ASSERT(uintPtr(&a) % alignof(fastBox) == 0);
			CAGE_ASSERT(uintPtr(&b) % alignof(fastBox) == 0);
			if (a.empty() || b.empty())
				return false;
			static const xsimd::batch<float, 4> mask = { 1,1,1,0 };
			if (xsimd::any(a.high.v4 * mask < b.low.v4 * mask))
				return false;
			if (xsimd::any(a.low.v4 * mask > b.high.v4 * mask))
				return false;
			return true;
		}

		struct itemBase
		{
			fastBox box;
			vec3 center;
			const uint32 name;
			
			virtual aabb getBox() const = 0;
			virtual bool intersects(const line &other) = 0;
			virtual bool intersects(const triangle &other) = 0;
			virtual bool intersects(const plane &other) = 0;
			virtual bool intersects(const sphere &other) = 0;
			virtual bool intersects(const aabb &other) = 0;

			itemBase(uint32 name) : name(name)
			{}

			void update()
			{
				aabb b = getBox();
				box = fastBox(b);
				center = b.center();
			}
		};

		template<class T>
		struct itemShape : public itemBase, public T
		{
			itemShape(uint32 name, const T &other) : itemBase(name), T(other)
			{
				update();
			}

			virtual aabb getBox() const { return aabb(*(T*)(this)); }
			virtual bool intersects(const line &other) { return cage::intersects(*this, other); };
			virtual bool intersects(const triangle &other) { return cage::intersects(*this, other); };
			virtual bool intersects(const plane &other) { return cage::intersects(*this, other); };
			virtual bool intersects(const sphere &other) { return cage::intersects(*this, other); };
			virtual bool intersects(const aabb &other) { return cage::intersects(*this, other); };
		};

		union itemUnion
		{
			itemShape<line> a;
			itemShape<triangle> b;
			itemShape<plane> c;
			itemShape<sphere> d;
			itemShape<aabb> e;
		};

		struct nodeStruct
		{
			fastBox box;
			
			nodeStruct(const fastBox &box, sint32 a, sint32 b) : box(box)
			{
				this->a() = a;
				this->b() = b;
			}

			sint32 &a() { return box.low.s.i; } // negative -> inner node, -a = index of left child; non-negative -> leaf node, a = offset into itemNames array
			sint32 &b() { return box.high.s.i; } // negative -> inner node, -b = index of right child; non-negative -> leaf node, b = items count
			sint32 a() const { return box.low.s.i; }
			sint32 b() const { return box.high.s.i; }
		};

		class spatialDataImpl : public SpatialData
		{
		public:
			MemoryArenaGrowing<MemoryAllocatorPolicyPool<templates::PoolAllocatorAtomSize<itemUnion>::result>, MemoryConcurrentPolicyNone> itemsPool;
			MemoryArena itemsArena;
			cage::unordered_map<uint32, Holder<itemBase>> itemsTable;
			std::atomic<bool> dirty;
			std::vector<nodeStruct, MemoryArenaStd<nodeStruct>> nodes;
			std::vector<itemBase*> indices;
			typedef std::vector<itemBase*>::iterator indicesIterator;
			static const uint32 binsCount = 10;
			std::array<fastBox, binsCount> leftBinBoxes;
			std::array<fastBox, binsCount> rightBinBoxes;
			std::array<uint32, binsCount> leftBinCounts;

			spatialDataImpl(const SpatialDataCreateConfig &config) : itemsPool(config.maxItems * sizeof(itemUnion)), itemsArena(&itemsPool), dirty(false), nodes(detail::systemArena())
			{
				CAGE_ASSERT((uintPtr(this) % alignof(fastBox)) == 0, uintPtr(this) % alignof(fastBox), alignof(fastBox));
				CAGE_ASSERT((uintPtr(leftBinBoxes.data()) % alignof(fastBox)) == 0);
				CAGE_ASSERT((uintPtr(rightBinBoxes.data()) % alignof(fastBox)) == 0);
			}

			~spatialDataImpl()
			{
				clear();
			}

			void rebuild(uint32 nodeIndex, uint32 nodeDepth, real parentSah)
			{
				nodeStruct &node = nodes[nodeIndex];
				CAGE_ASSERT(node.a() >= 0 && node.b() >= 0); // is leaf now
				if (node.b() < 16)
					return; // leaf node: too few primitives
				uint32 bestAxis = m;
				uint32 bestSplit = m;
				uint32 bestItemsCount = 0;
				real bestSah = real::Infinity();
				fastBox bestBoxLeft;
				fastBox bestBoxRight;
				for (uint32 axis = 0; axis < 3; axis++)
				{
					if (node.box.high.v4[axis] - node.box.low.v4[axis] < 1e-7)
						continue; // the box is flat (along this axis)
					for (fastBox &b : leftBinBoxes)
						b = fastBox();
					for (uint32 &c : leftBinCounts)
						c = 0;
					real binSizeInv = binsCount / (node.box.high.v4[axis] - node.box.low.v4[axis]);
					real planeOffset = node.box.low.v4[axis];
					for (sint32 i = node.a(), et = node.a() + node.b(); i != et; i++)
					{
						itemBase *item = indices[i];
						uint32 binIndex = numeric_cast<uint32>((item->center[axis] - planeOffset) * binSizeInv);
						CAGE_ASSERT(binIndex <= binsCount);
						binIndex = min(binIndex, binsCount - 1);
						leftBinBoxes[binIndex] += item->box;
						leftBinCounts[binIndex]++;
					}
					// right to left sweep
					rightBinBoxes[binsCount - 1] = leftBinBoxes[binsCount - 1];
					for (uint32 i = binsCount - 1; i > 0; i--)
						rightBinBoxes[i - 1] = rightBinBoxes[i] + leftBinBoxes[i - 1];
					// left to right sweep
					for (uint32 i = 1; i < binsCount; i++)
					{
						leftBinBoxes[i] += leftBinBoxes[i - 1];
						leftBinCounts[i] += leftBinCounts[i - 1];
					}
					CAGE_ASSERT(leftBinCounts[binsCount - 1] == node.b());
					// compute sah
					for (uint32 i = 0; i < binsCount - 1; i++)
					{
						real sahL = leftBinBoxes[i].surface() * leftBinCounts[i];
						real sahR = rightBinBoxes[i + 1].surface() * (node.b() - leftBinCounts[i]);
						real sah = sahL + sahR;
						if (sah < bestSah)
						{
							bestAxis = axis;
							bestSplit = i;
							bestSah = sah;
							bestItemsCount = leftBinCounts[bestSplit];
							bestBoxLeft = leftBinBoxes[i];
							bestBoxRight = rightBinBoxes[i + 1];
						}
					}
				}
				CAGE_ASSERT(bestSah.valid());
				if (bestSah >= parentSah)
					return; // leaf node: split would make no improvement
				if (bestItemsCount == 0)
					return; // leaf node: split cannot separate any objects (they are probably all at one position)
				CAGE_ASSERT(bestAxis < 3);
				CAGE_ASSERT(bestSplit + 1 < binsCount); // splits count is one less than bins count
				CAGE_ASSERT(bestItemsCount < numeric_cast<uint32>(node.b()));
				{
					real binSizeInv = binsCount / (node.box.high.v4[bestAxis] - node.box.low.v4[bestAxis]);
					real planeOffset = node.box.low.v4[bestAxis];
					std::partition(indices.begin() + node.a(), indices.begin() + (node.a() + node.b()), [&](itemBase *item) {
						uint32 binIndex = numeric_cast<uint32>((item->center[bestAxis] - planeOffset) * binSizeInv);
						return binIndex < bestSplit + 1;
					});
				}
				sint32 leftNodeIndex = numeric_cast<sint32>(nodes.size());
				nodes.emplace_back(bestBoxLeft, node.a(), bestItemsCount);
				rebuild(leftNodeIndex, nodeDepth + 1, bestSah);
				sint32 rightNodeIndex = numeric_cast<sint32>(nodes.size());
				nodes.emplace_back(bestBoxRight, node.a() + bestItemsCount, node.b() - bestItemsCount);
				rebuild(rightNodeIndex, nodeDepth + 1, bestSah);
				node.a() = -leftNodeIndex;
				node.b() = -rightNodeIndex;
			}

			bool similar(const fastBox &a, const fastBox &b)
			{
				return (length(a.low.s.v3 - b.low.s.v3) + length(a.high.s.v3 - b.high.s.v3)) < 1e-3;
			}

			void validate(uint32 nodeIndex)
			{
				nodeStruct &node = nodes[nodeIndex];
				CAGE_ASSERT((node.a() < 0) == (node.b() < 0));
				if (node.a() < 0)
				{ // inner node
					nodeStruct &l = nodes[-node.a()];
					nodeStruct &r = nodes[-node.b()];
					validate(-node.a());
					validate(-node.b());
					CAGE_ASSERT(similar(node.box, l.box + r.box));
				}
				else
				{ // leaf node
					fastBox box;
					for (uint32 i = node.a(), e = node.a() + node.b(); i < e; i++)
						box += indices[i]->box;
					CAGE_ASSERT(similar(node.box, box));
				}
			}

			void rebuild()
			{
				dirty = true;
				nodes.clear();
				indices.clear();
				if (itemsTable.size() == 0)
				{
					dirty = false;
					return;
				}
				nodes.reserve(itemsTable.size());
				indices.reserve(itemsTable.size());
				fastBox worldBox;
				for (const auto &it : itemsTable)
				{
					indices.push_back(it.second.get());
					worldBox += it.second->box;
				}
				nodes.emplace_back(worldBox, 0, numeric_cast<sint32>(itemsTable.size()));
				rebuild(0, 0, real::Infinity());
				CAGE_ASSERT(uintPtr(nodes.data()) % alignof(nodeStruct) == 0, uintPtr(nodes.data()) % alignof(nodeStruct), alignof(nodeStruct), alignof(fastBox), sizeof(nodeStruct));
#ifdef CAGE_ASSERT_ENABLED
				validate(0);
#endif // CAGE_ASSERT_ENABLED
				dirty = false;
			}
		};

		class spatialQueryImpl : public SpatialQuery
		{
		public:
			const spatialDataImpl *const data;
			std::vector<uint32> resultNames;

			spatialQueryImpl(const spatialDataImpl *data) : data(data)
			{
				resultNames.reserve(100);
			}

			void clear()
			{
				CAGE_ASSERT(!data->dirty);
				resultNames.clear();
			}

			template<class T>
			struct intersectorStruct
			{
				const spatialDataImpl *data;
				std::vector<uint32> &resultNames;
				const T &other;
				const fastBox otherBox;

				intersectorStruct(const spatialDataImpl *data, std::vector<uint32> &resultNames, const T &other) : data(data), resultNames(resultNames), other(other), otherBox(aabb(other))
				{
					CAGE_ASSERT((uintPtr(this) % alignof(fastBox)) == 0, uintPtr(this) % alignof(fastBox), alignof(fastBox));
					CAGE_ASSERT((uintPtr(&otherBox) % alignof(fastBox)) == 0);
					intersection(0);
				}

				void intersection(uint32 nodeIndex)
				{
					const nodeStruct &node = data->nodes[nodeIndex];
					if (!intersects(otherBox, node.box))
						return;
					if (!intersects(other, aabb(node.box)))
						return;
					if (node.a() < 0)
					{ // internode
						intersection(-node.a());
						intersection(-node.b());
					}
					else
					{ // leaf
						for (uint32 i = node.a(), e = node.a() + node.b(); i < e; i++)
						{
							itemBase *item = data->indices[i];
							if (item->intersects(other))
								resultNames.push_back(item->name);
						}
					}
				}
			};

			template<class T>
			void intersection(const T &other)
			{
				CAGE_ASSERT(!data->dirty);
				clear();
				if (data->nodes.empty())
					return;
				intersectorStruct<T> i(data, resultNames, other);
			}
		};
	}

	PointerRange<uint32> SpatialQuery::result() const
	{
		spatialQueryImpl *impl = (spatialQueryImpl*)this;
		return impl->resultNames;
	}

	void SpatialQuery::intersection(const vec3 &shape)
	{
		intersection(aabb(shape, shape));
	}

	void SpatialQuery::intersection(const line &shape)
	{
		spatialQueryImpl *impl = (spatialQueryImpl*)this;
		impl->intersection(shape);
	}

	void SpatialQuery::intersection(const triangle &shape)
	{
		spatialQueryImpl *impl = (spatialQueryImpl*)this;
		impl->intersection(shape);
	}

	void SpatialQuery::intersection(const plane &shape)
	{
		spatialQueryImpl *impl = (spatialQueryImpl*)this;
		impl->intersection(shape);
	}

	void SpatialQuery::intersection(const sphere &shape)
	{
		spatialQueryImpl *impl = (spatialQueryImpl*)this;
		impl->intersection(shape);
	}

	void SpatialQuery::intersection(const aabb &shape)
	{
		spatialQueryImpl *impl = (spatialQueryImpl*)this;
		impl->intersection(shape);
	}

	void SpatialData::update(uint32 name, const vec3 &other)
	{
		update(name, aabb(other, other));
	}

	void SpatialData::update(uint32 name, const line &other)
	{
		CAGE_ASSERT(other.valid());
		CAGE_ASSERT(other.isPoint() || other.isSegment());
		spatialDataImpl *impl = (spatialDataImpl*)this;
		remove(name);
		impl->itemsTable[name] = impl->itemsArena.createImpl<itemBase, itemShape<line>>(name, other);
	}

	void SpatialData::update(uint32 name, const triangle &other)
	{
		CAGE_ASSERT(other.valid());
		CAGE_ASSERT(other.area() < real::Infinity());
		spatialDataImpl *impl = (spatialDataImpl*)this;
		remove(name);
		impl->itemsTable[name] = impl->itemsArena.createImpl<itemBase, itemShape<triangle>>(name, other);
	}

	void SpatialData::update(uint32 name, const sphere &other)
	{
		CAGE_ASSERT(other.valid());
		CAGE_ASSERT(other.volume() < real::Infinity());
		spatialDataImpl *impl = (spatialDataImpl*)this;
		remove(name);
		impl->itemsTable[name] = impl->itemsArena.createImpl<itemBase, itemShape<sphere>>(name, other);
	}

	void SpatialData::update(uint32 name, const aabb &other)
	{
		CAGE_ASSERT(other.valid());
		CAGE_ASSERT(other.volume() < real::Infinity());
		spatialDataImpl *impl = (spatialDataImpl*)this;
		remove(name);
		impl->itemsTable[name] = impl->itemsArena.createImpl<itemBase, itemShape<aabb>>(name, other);
	}

	void SpatialData::remove(uint32 name)
	{
		spatialDataImpl *impl = (spatialDataImpl*)this;
		impl->dirty = true;
		impl->itemsTable.erase(name);
	}

	void SpatialData::clear()
	{
		spatialDataImpl *impl = (spatialDataImpl*)this;
		impl->dirty = true;
		impl->itemsTable.clear();
	}

	void SpatialData::rebuild()
	{
		spatialDataImpl *impl = (spatialDataImpl*)this;
		impl->rebuild();
	}

	SpatialDataCreateConfig::SpatialDataCreateConfig() : maxItems(1000 * 100)
	{}

	Holder<SpatialData> newSpatialData(const SpatialDataCreateConfig &config)
	{
		return detail::systemArena().createImpl<SpatialData, spatialDataImpl>(config);
	}

	Holder<SpatialQuery> newSpatialQuery(const SpatialData *data)
	{
		return detail::systemArena().createImpl<SpatialQuery, spatialQueryImpl>((spatialDataImpl*)data);
	}
}
