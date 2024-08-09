module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.DeviceMemory;
import Core.Vulkan.Dependency;
import Core.Vulkan.Buffer;
import std;

export namespace Core::Vulkan{
	class DeviceMemory;

	class SegmentBuffer : public BasicBuffer{
		friend DeviceMemory;

		DeviceMemory* source{};

		~SegmentBuffer() = default;
	};

	/**
	 * @brief Used for small memory segments
	 */
	class DeviceMemory : public Wrapper<VkDeviceMemory>{
		Dependency<VkDevice> device{};

		using MemoryOffset = std::uint32_t;

		enum class NodeState : MemoryOffset{
			begin, merged, end
		};

		MemoryOffset capacity{};

		mutable std::mutex mutex_bufferToSrc{};
		mutable std::mutex mutex_heap{};

		struct AllocNode{
			NodeState state{};

			[[nodiscard]] constexpr bool isBegin() const noexcept{return state == NodeState::begin;}
			[[nodiscard]] constexpr bool isEnd() const noexcept{return state == NodeState::end;}
			[[nodiscard]] constexpr bool merged() const noexcept{return state == NodeState::merged;}
		};

		std::map<MemoryOffset, AllocNode> validSegments{};
		using iterator = decltype(validSegments)::iterator;
		using value_type = decltype(validSegments)::value_type;
		std::unordered_map<VkBuffer, iterator> beginMap{};

		std::size_t rangeSize(const decltype(validSegments)::iterator& begin, const decltype(validSegments)::iterator& end){
			return end->first - begin->first;
		}
	public:

		void deallocate(const SegmentBuffer& buffer){
			if(buffer.getMemory() != handler){
				throw std::runtime_error("Deallocate At Wrong Source");
			}

			//TODO find check? should never happens

			iterator src{};
			{
				std::lock_guard _{mutex_bufferToSrc};

				const auto srcItr = beginMap.find(buffer.get());
				src = srcItr->second;
				beginMap.erase(srcItr);
			}

			{
				std::lock_guard _{mutex_heap};

				const auto itr_begin = src;
				const auto itr_end = ++src;

				release(itr_begin, itr_end);
			}
		}

		void init(){
			validSegments.emplace(0, AllocNode{NodeState::begin});
			validSegments.emplace(capacity, AllocNode{NodeState::end});
		}

		void allocate(MemoryOffset size, SegmentBuffer& buffer){
			auto emptyBegin{validSegments.begin()};
			auto emptyEnd{validSegments.begin()};

			for (auto itr = validSegments.begin(); itr != validSegments.end(); ++itr){
				if(itr->second.merged())continue;

				if(itr->second.isBegin())emptyBegin = itr;

				if(itr->second.isEnd()){
					emptyEnd = itr;

					auto validSize = rangeSize(emptyBegin, emptyEnd);
					if(validSize >= size){
						insert(emptyBegin, size, buffer);
						break;
					}
				}
			}
		}

	private:
		void insert(const iterator& begin, MemoryOffset size, SegmentBuffer& buffer){
			auto [pair, rst] = beginMap.try_emplace(buffer, begin);
			if(!rst){
				throw std::runtime_error("Memory allocation twice on a same buffer!");
			}

			buffer.source = this;
			buffer.capacity = size;
			buffer.memory = handler;

			validSegments.emplace(begin->first, NodeState::end);
			validSegments.emplace(begin->first + size, NodeState::begin);
		}

		void capture(const iterator& begin, const MemoryOffset size){
			{// capture begin
				begin->second.state = NodeState::merged;
			}

			{// capture end
				const auto next = std::next(begin);

				if(rangeSize(begin, next) == size){
					next->second.state = NodeState::merged; //Merge Captured Segments
				}else{ //Mark Free Segment Begin
					validSegments.emplace(begin->first + size, NodeState::begin);
				}
			}
		}

		void release(const iterator& begin, const iterator& end){
			if(begin->second.isEnd())validSegments.erase(begin); //merge
			else begin->second.state = NodeState::begin;

			if(end->second.isBegin())validSegments.erase(end); //merge
			else end->second.state = NodeState::end;
		}

	};


}
