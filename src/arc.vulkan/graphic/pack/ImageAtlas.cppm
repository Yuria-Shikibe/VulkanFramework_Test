module;

#include <vulkan/vulkan.h>

export module Graphic.ImageAtlas;

export import Core.Vulkan.Texture;
import Core.Vulkan.Context;
import ext.handle_wrapper;
import Core.Vulkan.Buffer.CommandBuffer;
import Core.Vulkan.Buffer.ExclusiveBuffer;
import Core.Vulkan.CommandPool;

import Graphic.ImageRegion;
import Graphic.Pixmap;

import Geom.Vector2D;
import Geom.Rect_Orthogonal;
import ext.Allocator2D;
import ext.Heterogeneous;
import std;

namespace Graphic{
	export constexpr Geom::USize2 DefTexturePageSize = Geom::norBaseVec2<std::uint32_t> * (1 << 13);

	using T = std::uint32_t;
	using Rect = Geom::Rect_Orthogonal<T>;
	using PointType = Geom::Vector2D<T>;
	using SizeType = Geom::Vector2D<T>;


	export struct SubpageData{
		ext::Allocator2D allocator2D;
		Core::Vulkan::Texture texture{};

		struct AllocatedViewRegion : ImageViewRegion{
			ext::dependency<SubpageData*> srcAllocator{};
			Geom::OrthoRectUInt region{};

			[[nodiscard]] AllocatedViewRegion() = default;

			using ImageViewRegion::ImageViewRegion;

			[[nodiscard]] const ImageViewRegion& asView() const noexcept{
				return static_cast<const ImageViewRegion&>(*this);
			}

			[[nodiscard]] AllocatedViewRegion(VkImageView imageView, const Geom::USize2& srcImageSize,
			                         const Geom::Rect_Orthogonal<std::uint32_t>& internal,
			                         SubpageData* srcAllocator)
				: ImageViewRegion{imageView, srcImageSize, internal},
				  srcAllocator{srcAllocator}, region{internal}{}

			void shrink(const std::uint32_t size){
				region.shrink(size);
				setUV(region);
			}

			~AllocatedViewRegion(){
				if(srcAllocator) srcAllocator->deallocate(region.getSrc());
			}

			AllocatedViewRegion(const AllocatedViewRegion& other) = delete;

			AllocatedViewRegion(AllocatedViewRegion&& other) noexcept
				: ImageViewRegion{std::move(other)},
				  srcAllocator{std::move(other.srcAllocator)},
				  region{std::move(other.region)}{}

			AllocatedViewRegion& operator=(const AllocatedViewRegion& other) = delete;

			AllocatedViewRegion& operator=(AllocatedViewRegion&& other) noexcept{
				if(this == &other) return *this;
				if(srcAllocator) srcAllocator->deallocate(region.getSrc());
				ImageViewRegion::operator =(std::move(other));
				srcAllocator = std::move(other.srcAllocator);
				region = std::move(other.region);
				return *this;
			}
		};

		auto allocate(const SizeType size){
			return allocator2D.allocate(size);
		}

		auto deallocate(const PointType point){
			return allocator2D.deallocate(point);
		}

		[[nodiscard]] explicit SubpageData(const Core::Vulkan::Context* context, const SizeType size) // NOLINT(*-pro-type-member-init)
			: allocator2D{size}, texture{context->physicalDevice, context->device}{
			texture.createEmpty(size, 1);
		}

		std::optional<AllocatedViewRegion> tryAllocate(VkCommandBuffer commandBuffer, VkImage image, const Rect& region, std::uint32_t margin){
			if(const auto rect = allocate(region.getSize().add(margin))){
				const Rect rst = rect->copy().setSize(region.getSize());
				texture.writeImage(commandBuffer, image,
					region.as<int>(),
					rst.as<int>());

				return AllocatedViewRegion{texture.getView(), texture.getSize(), rst, this};
			}

			return std::nullopt;
		}

		std::optional<AllocatedViewRegion> tryAllocate(VkCommandBuffer commandBuffer, VkBuffer buffer, const Rect& region, std::uint32_t margin){
			if(const auto rect = allocate(region.getSize().add(margin))){
				const Rect rst = rect->copy().setSize(region.getSize());
				texture.writeBuffer(commandBuffer, buffer, rst.as<int>());

				return AllocatedViewRegion{texture.getView(), texture.getSize(), rst, this};
			}

			return std::nullopt;
		}
	};

	export struct ImagePage{
		SizeType size{};

		std::string name{};
		std::deque<SubpageData> subpages{};
		ext::StringHashMap<SubpageData::AllocatedViewRegion> namedImageRegions{};

		std::uint32_t margin{4};

		std::shared_mutex writeMutex{};

		[[nodiscard]] ImagePage(const SizeType& size, const std::string_view name)
			: size{size},
			  name{name}{}

		SubpageData& createPage(const Core::Vulkan::Context* context){
			std::unique_lock lk{writeMutex};
			return subpages.emplace_back(SubpageData{context, size});
		}

		template <typename T>
			requires (std::same_as<T, VkImage> || std::same_as<T, VkBuffer>)
		[[nodiscard]] SubpageData::AllocatedViewRegion allocate(
			const Core::Vulkan::Context* context,
			VkCommandBuffer commandBuffer,
			T data, const Rect region){

			for(std::shared_lock lk{writeMutex}; auto& subpass : subpages){
				if(std::optional<SubpageData::AllocatedViewRegion> (rst) = subpass.tryAllocate(commandBuffer, data, region, margin)){
					return std::move(rst.value());
				}
			}

			SubpageData& newSubpage = createPage(context);
			std::optional<SubpageData::AllocatedViewRegion> rst = newSubpage.tryAllocate(commandBuffer, data, region, margin);

			if(!rst){
				throw std::invalid_argument("Invalid region size");
			}

			return std::move(rst.value());
		}

		SubpageData::AllocatedViewRegion* find(const std::string_view localName){
			if(const auto page = namedImageRegions.find(localName); page != namedImageRegions.end()){
				return &page->second;
			}

			return nullptr;
		}


		std::pair<SubpageData::AllocatedViewRegion&, bool> registerNamedRegion(std::string&& name, SubpageData::AllocatedViewRegion&& region){
			auto [itr, rst] = namedImageRegions.try_emplace(std::move(name), std::move(region));
			return {itr->second, rst};
		}

		decltype(auto) registerNamedRegion(const std::string_view name, SubpageData::AllocatedViewRegion&& region){
			return registerNamedRegion(std::string{name}, std::move(region));
		}

		decltype(auto) registerNamedRegion(const char* name, SubpageData::AllocatedViewRegion&& region){
			return registerNamedRegion(std::string{name}, std::move(region));
		}

		std::vector<Pixmap> exportImages(const Core::Vulkan::CommandPool& commandPool, VkQueue queue) const{
			std::vector<Pixmap> pixmaps{};
			pixmaps.reserve(subpages.size());

			for (const auto & subpage : subpages){
				pixmaps.push_back(subpage.texture.exportToPixmap(commandPool.obtainTransient(queue)));
			}

			return pixmaps;
		}
	};

	export using AllocatedImageViewRegion = SubpageData::AllocatedViewRegion;


	export struct ImageAtlas{
		const Core::Vulkan::Context* context{};
		Core::Vulkan::CommandPool transientCommandPool{};

		ext::StringHashMap<ImagePage> pages{};

		[[nodiscard]] ImageAtlas() = default;

		[[nodiscard]] explicit ImageAtlas(const Core::Vulkan::Context& context) :
			context{&context},
			transientCommandPool{
				context.device, context.physicalDevice.queues.graphicsFamily, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
			}{}

		[[nodiscard]] ImagePage* findPage(const std::string_view name){
			if(const auto page = pages.find(name); page != pages.end()) return &page->second;
			return nullptr;
		}

		[[nodiscard]] const ImagePage* findPage(const std::string_view name) const{
			if(const auto page = pages.find(name); page != pages.end()) return &page->second;
			return nullptr;
		}

		ImagePage& registerPage(std::string_view name, SizeType size = DefTexturePageSize){
			return pages.try_emplace(std::string(name), size, name).first->second;
		}

		[[nodiscard]] AllocatedImageViewRegion allocate(
			ImagePage& page,
			VkImage image, const Rect region) const{
			return page.allocate(context, obtainTransientCommand(), image, region);
		}

		[[nodiscard]] AllocatedImageViewRegion allocate(ImagePage& page, const Pixmap& pixmap) const{
			const Core::Vulkan::StagingBuffer buffer(context->physicalDevice, context->device, pixmap.sizeBytes());
			buffer.memory.loadData(pixmap.data(), pixmap.sizeBytes());

			return page.allocate(context, obtainTransientCommand(), buffer.get(), {pixmap.getWidth(), pixmap.getHeight()});
		}

		[[nodiscard]] AllocatedImageViewRegion allocate(const std::string_view pageName, const Pixmap& pixmap){
			if(const auto page = findPage(pageName)){
				return allocate(*page, pixmap);
			}

			throw std::invalid_argument("Undefined page name");
		}

		[[nodiscard]] AllocatedImageViewRegion allocate(
			const std::string_view pageName,
			VkImage image, const Rect region){
			if(const auto page = findPage(pageName)){
				return allocate(*page, image, region);
			}

			throw std::invalid_argument("Undefined page name");
		}

		decltype(auto) exportImages(const ImagePage& page) const{
			return page.exportImages(transientCommandPool, context->device.getGraphicsQueue());
		}

		[[nodiscard]] Core::Vulkan::TransientCommand obtainTransientCommand() const{
			return transientCommandPool.obtainTransient(context->device.getGraphicsQueue());
		}

		/**
		 * @param name in format of "<category>.<name>"
		 * @return Nullable
		 */
		AllocatedImageViewRegion* find(const std::string_view name){
			auto [category, localName] = splitKey(name);
			if(const auto page = findPage(category)){
				return page->find(localName);
			}

			return nullptr;
		}

		/**
		 * @param name in format of "<category>.<name>"
		 */
		AllocatedImageViewRegion& at(const std::string_view name){
			auto [category, localName] = splitKey(name);
			if(const auto page = findPage(category)){
				return *page->find(localName);
			}

			std::println(std::cerr, "TextureRegion Not Found: {}", name);
			throw std::invalid_argument("Undefined Region Name");
		}

		AllocatedImageViewRegion* registerNamedImageViewRegion(const std::string_view name, AllocatedImageViewRegion&& region){
			auto [category, localName] = splitKey(name);

			if(const auto page = findPage(category)){
				return &page->registerNamedRegion(name, std::move(region)).first;
			}

			return nullptr;
		}

		static std::pair<std::string_view, std::string_view> splitKey(const std::string_view name){
			const auto pos = name.find('.');
			const std::string_view category = name.substr(0, pos);
			const std::string_view localName = name.substr(pos + 1);

			return {category, localName};
		}
	};
}
