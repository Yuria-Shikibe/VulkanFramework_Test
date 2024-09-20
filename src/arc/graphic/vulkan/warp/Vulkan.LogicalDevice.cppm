module;

#include <vulkan/vulkan.h>

export module Core.Vulkan.LogicalDevice;

export import ext.handle_wrapper;
import Core.Vulkan.Validation;
import Core.Vulkan.PhysicalDevice;
import std;

export namespace Core::Vulkan{

	namespace RequiredFeatures{
		constexpr VkPhysicalDeviceVulkan13Features PhysicalDeviceVulkan13Features{[]{
			VkPhysicalDeviceVulkan13Features features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

			features.synchronization2 = true;
			features.dynamicRendering = true;

			return features;
		}()};

		constexpr VkPhysicalDeviceFeatures RequiredFeatures{[]{
			VkPhysicalDeviceFeatures features{};

			features.samplerAnisotropy = true;
			features.independentBlend = true;

			return features;
		}()};

		constexpr VkPhysicalDeviceDescriptorBufferFeaturesEXT DescriptorBufferFeatures{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
			.pNext = nullptr,
			.descriptorBuffer = true,
			.descriptorBufferCaptureReplay = false,
			.descriptorBufferImageLayoutIgnored = false,
			.descriptorBufferPushDescriptors = false
		};


		constexpr VkPhysicalDeviceDescriptorIndexingFeatures RequiredDescriptorIndexingFeatures{[]{
			VkPhysicalDeviceDescriptorIndexingFeatures features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};

			features.descriptorBindingSampledImageUpdateAfterBind = true;
			features.descriptorBindingUpdateUnusedWhilePending = true;
			features.descriptorBindingUniformBufferUpdateAfterBind = true;

			return features;
		}()};

		constexpr VkPhysicalDeviceBufferDeviceAddressFeaturesEXT PhysicalDeviceBufferDeviceAddressFeatures{[]{
			VkPhysicalDeviceBufferDeviceAddressFeaturesEXT features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_ADDRESS_FEATURES_EXT};

			features.bufferDeviceAddress = true;

			return features;
		}()};


		template <typename ...T>
		struct ExtChain{
			using Chain = std::tuple<T...>;
			Chain chain{};

			template <typename ...Args>
			[[nodiscard]] explicit ExtChain(Args&& ...args) : chain{args...}{
				create();
			}

		private:
			template <typename T1, typename T2>
			static void set(T1& t1, T2& t2){
				t1.pNext = &t2;
			}

			template <typename T1, typename T2>
			static void check(T1& t1, T2& t2){
				// std::println("[Vulkan] EXT {}.pNext -> {}", typeid(T1).name(), typeid(T2).name());

				if(t1.pNext != &t2){
					throw std::runtime_error("Chain Failed");
				}
			}

			void create(){
				[&]<std::size_t ...I>(std::index_sequence<I...>){
					((std::get<I>(chain).pNext = nullptr), ...);
				}(std::make_index_sequence<sizeof...(T)>{});

				[&]<std::size_t ...I>(std::index_sequence<I...>){
					(this->set(std::get<I>(chain), std::get<I + 1>(chain)), ...);
				}(std::make_index_sequence<sizeof...(T) - 1>{});

				[&]<std::size_t ...I>(std::index_sequence<I...>){
					(this->check(std::get<I>(chain), std::get<I + 1>(chain)), ...);
				}(std::make_index_sequence<sizeof...(T) - 1>{});
			}

		public:
			[[nodiscard]] void* getFirst() const{
				return const_cast<std::tuple_element_t<0, Chain>*>(&std::get<0>(chain));
			}
		};


		template <typename ...Args>
		ExtChain(Args&& ...) -> ExtChain<std::decay_t<Args> ...>;

		const ExtChain extChain{
			PhysicalDeviceVulkan13Features,
			RequiredDescriptorIndexingFeatures,
			PhysicalDeviceBufferDeviceAddressFeatures,
			DescriptorBufferFeatures,
		};
	}


	class LogicalDevice : public ext::wrapper<VkDevice>{
		std::vector<VkQueue> graphicQueues{};
		std::vector<VkQueue> computeQueues{};
		VkQueue presentQueue{};

	public:

		[[nodiscard]] LogicalDevice() = default;

		~LogicalDevice(){
			vkDestroyDevice(handle, nullptr);
		}

		[[nodiscard]] VkQueue graphicQueue(const std::uint32_t index) const{
			return graphicQueues[std::min(static_cast<const std::uint32_t>(graphicQueues.size() - 1), index)];
		}

		[[nodiscard]] VkQueue computeQueue(const std::uint32_t index) const{
			return computeQueues[std::min(static_cast<const std::uint32_t>(computeQueues.size() - 1), index)];
		}

		[[nodiscard]] VkQueue getPrimaryGraphicsQueue() const noexcept{ return graphicQueues.front(); }

		[[nodiscard]] VkQueue getPresentQueue() const noexcept{ return presentQueue; }

		[[nodiscard]] VkQueue getPrimaryComputeQueue() const noexcept{ return computeQueues.front(); }


		LogicalDevice(const LogicalDevice& other) = delete;

		LogicalDevice(LogicalDevice&& other) noexcept = default;

		LogicalDevice& operator=(const LogicalDevice& other) = delete;

		LogicalDevice& operator=(LogicalDevice&& other) noexcept = default;


		LogicalDevice(VkPhysicalDevice physicalDevice, const QueueFamilyIndices& indices){
			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
			std::vector<std::vector<float>> queueCreatePriorityInfos{};


			const std::unordered_set uniqueQueueFamilies{indices.graphic, indices.compute, indices.present};

			constexpr float queuePriority = 1.0f;
			for(const auto [index, count] : uniqueQueueFamilies){
				auto& info = queueCreateInfos.emplace_back();
				auto& priority = queueCreatePriorityInfos.emplace_back();

				priority = std::views::iota(0u, count) | std::views::reverse | std::views::transform([count](auto i) -> float{
					return static_cast<float>(i + 1) / static_cast<float>(count);
				}) | std::ranges::to<std::vector>();


				info = VkDeviceQueueCreateInfo{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = index,
					.queueCount = count,
					.pQueuePriorities = priority.data()
				};
			}

			VkPhysicalDeviceFeatures2 deviceFeatures2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
			deviceFeatures2.pNext = RequiredFeatures::extChain.getFirst();//const_cast<decltype(RequiredFeatures::extChain)::First*>(&RequiredFeatures::extChain.first);
			deviceFeatures2.features = RequiredFeatures::RequiredFeatures;


			VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
			createInfo.pNext = &deviceFeatures2;

			createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
			createInfo.pQueueCreateInfos = queueCreateInfos.data();

			createInfo.enabledExtensionCount = static_cast<std::uint32_t>(DeviceExtensions.size());
			createInfo.ppEnabledExtensionNames = DeviceExtensions.data();

			if constexpr(EnableValidationLayers){
				createInfo.enabledLayerCount = static_cast<std::uint32_t>(UsedValidationLayers.size());
				createInfo.ppEnabledLayerNames = UsedValidationLayers.data();
			} else{
				createInfo.enabledLayerCount = 0;
			}

			if(auto rst = vkCreateDevice(physicalDevice, &createInfo, nullptr, &handle); rst != VK_SUCCESS){
				std::println(std::cerr, "Failed To Create Device: {}", static_cast<int>(rst));
				throw std::runtime_error("Failed to create logical device!");
			}else{
				std::println("[Vulkan] Device Created: {:#0X}", reinterpret_cast<std::uintptr_t>(handle));
			}

			indices.graphic.createQueues(handle, graphicQueues);
			indices.compute.createQueues(handle, computeQueues);
			vkGetDeviceQueue(handle, indices.present.index, 0, &presentQueue);
		}
	};
}
