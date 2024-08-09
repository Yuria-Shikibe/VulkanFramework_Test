module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.Buffer;
import Core.Vulkan.Dependency;
import std;

export namespace Core::Vulkan{
	std::uint32_t findMemoryType(const std::uint32_t typeFilter, VkPhysicalDevice physicalDevice,
							 VkMemoryPropertyFlags properties){
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for(std::uint32_t i = 0; i < memProperties.memoryTypeCount; i++){
			if(typeFilter & (1u << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties){

				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	class BasicBuffer : public Wrapper<VkBuffer>{
	protected:
		Dependency<VkDevice> device{};

		Dependency<VkDeviceMemory> memory{};

		VkDeviceSize capacity{};

	public:
		[[nodiscard]] BasicBuffer() = default;

		[[nodiscard]] explicit BasicBuffer(VkDevice device)
			: device{device}{}

		[[nodiscard]] explicit BasicBuffer(VkDevice device, VkBuffer handler, std::size_t capacity = 0)
			: Wrapper{handler}, device{device}, capacity{capacity}{}

		[[nodiscard]] constexpr VkDevice getDevice() const noexcept{ return device; }

		[[nodiscard]] constexpr VkBuffer getHandler() const noexcept{ return handler; }

		[[nodiscard]] constexpr VkDeviceMemory getMemory() const noexcept{ return memory; }

		[[nodiscard]] constexpr std::size_t size() const noexcept{ return capacity; }

		~BasicBuffer(){
			if(device){
				vkDestroyBuffer(device, handler,nullptr);
			}
		}

		BasicBuffer(const BasicBuffer& other) = delete;

		BasicBuffer(BasicBuffer&& other) noexcept = default;

		BasicBuffer& operator=(const BasicBuffer& other) = delete;

		BasicBuffer& operator=(BasicBuffer&& other) noexcept{
			if(this == &other) return *this;
			if(device)vkDestroyBuffer(device, handler,nullptr);
			Wrapper<VkBuffer>::operator =(std::move(other));
			device = std::move(other.device);
			memory = std::move(other.memory);
			capacity = other.capacity;
			return *this;
		}

		template <std::ranges::contiguous_range T>
			requires (std::ranges::sized_range<T>)
		void loadData(const T& range) const{
			const std::size_t dataSize = std::ranges::size(range) * sizeof(std::ranges::range_value_t<T>);

			if(dataSize > capacity){
				throw std::runtime_error("Insufficient buffer capacity!");
			}

			const auto pdata = map();
			std::memcpy(pdata, std::ranges::data(range), dataSize);
			unmap();
		}

		template <typename T>
			requires (std::is_trivially_copyable_v<T>)
		void loadData(const T& data) const{
			static constexpr std::size_t dataSize = sizeof(T);

			if(dataSize > capacity){
				throw std::runtime_error("Insufficient buffer capacity!");
			}

			const auto pdata = map();
			std::memcpy(pdata, &data, dataSize);
			unmap();
		}

		template <typename T, typename... Args>
		void emplace(Args&&... args) const{
			static constexpr std::size_t dataSize = sizeof(T);

			if(dataSize > capacity){
				throw std::runtime_error("Insufficient buffer capacity!");
			}

			const auto pdata = map();
			new(pdata) T(std::forward<Args>(args)...);
			unmap();
		}

		template <typename T>
			requires (std::is_trivially_copyable_v<T>)
		void loadData(const T* data) const{
			this->loadData(*data);
		}

		[[nodiscard]] void* map() const{
			void* data{};
			vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &data);
			return data;
		}

		void unmap() const{
			vkUnmapMemory(device, memory);
		}
	};
}