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
import ext.allocator_2D;
import ext.heterogeneous;
import ext.meta_programming;
import std;

namespace Graphic{
	export constexpr Geom::USize2 DefTexturePageSize = Geom::norBaseVec2<std::uint32_t> * (1 << 13);

	using T = std::uint32_t;
	using Rect = Geom::Rect_Orthogonal<T>;
	using PointType = Geom::Vector2D<T>;
	using SizeType = Geom::Vector2D<T>;


	export struct SubpageData{
		ext::allocator_2D allocator2D;
		Core::Vulkan::Texture texture{};

		struct AllocatedViewRegion : ImageViewRegion{
			ext::dependency<SubpageData*> srcAllocator{};
			Geom::OrthoRectUInt region{};

			[[nodiscard]] AllocatedViewRegion() = default;

			using ImageViewRegion::ImageViewRegion;

			[[nodiscard]] const ImageViewRegion& asView() const noexcept{
				return static_cast<const ImageViewRegion&>(*this);
			}

			[[nodiscard]] AllocatedViewRegion(
				VkImageView imageView,
				const Geom::USize2 srcImageSize,
				const Geom::Rect_Orthogonal<std::uint32_t> internal,
				SubpageData* srcAllocator)
				: ImageViewRegion{imageView, srcImageSize, internal},
				  srcAllocator{srcAllocator}, region{internal}{}

			void shrink(const std::uint32_t size){
				region.shrink(size);
				setUV_fromInternal(region);
			}

			~AllocatedViewRegion(){
				if(srcAllocator)srcAllocator->deallocate(region.getSrc());
			}

			AllocatedViewRegion(const AllocatedViewRegion& other) = delete;

			AllocatedViewRegion(AllocatedViewRegion&& other) noexcept = default;

			AllocatedViewRegion& operator=(const AllocatedViewRegion& other) = delete;

			AllocatedViewRegion& operator=(AllocatedViewRegion&& other) noexcept = default;
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

		template <typename T>
			requires (ext::is_any_of<T, VkImage, VkBuffer>)
		std::optional<AllocatedViewRegion> tryAllocate(VkCommandBuffer commandBuffer, T dataHandle, const Rect region, const std::uint32_t margin){
			if(const auto rect = allocate(region.getSize().add(margin))){
				const Rect rst = rect->copy().setSize(region.getSize());

				if constexpr(std::same_as<T, VkImage>){
					texture.writeImage(commandBuffer, dataHandle, region.as<int>(), rst.as<int>());
				} else if constexpr(std::same_as<T, VkBuffer>){
					texture.writeBuffer(commandBuffer, dataHandle, rst.as<int>());
				}else{
					static_assert(false, "Data Type is not supported");
				}

				return AllocatedViewRegion{texture.getView(), texture.getSize(), rst, this};
			}

			return std::nullopt;
		}
	};

	export struct ImagePage{
		std::deque<SubpageData> subpages{};
		ext::string_hash_map<SubpageData::AllocatedViewRegion> namedImageRegions{};

		std::uint32_t margin{4};

	private:
		std::string name{};
		SizeType imageSize{};
		std::shared_mutex writeMutex{};

	public:
		[[nodiscard]] explicit ImagePage(const std::string_view name, const SizeType size = DefTexturePageSize)
			: name{name},
			  imageSize{size}{}

		SubpageData& createPage(const Core::Vulkan::Context* context){
			std::unique_lock lk{writeMutex};
			return subpages.emplace_back(SubpageData{context, imageSize});
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

		[[nodiscard]] constexpr std::string_view getName() const noexcept{
			return name;
		}

		SubpageData::AllocatedViewRegion* find(const std::string_view localName){
			if(const auto page = namedImageRegions.find(localName); page != namedImageRegions.end()){
				return &page->second;
			}

			return nullptr;
		}

		std::pair<SubpageData::AllocatedViewRegion&, bool> registerNamedRegion(std::string&& name, SubpageData::AllocatedViewRegion&& region){
			auto [itr, rst] = namedImageRegions.insert_or_assign(std::move(name), std::move(region));
			return {itr->second, rst};
		}

		decltype(auto) registerNamedRegion(const std::string_view name, SubpageData::AllocatedViewRegion&& region){
			return registerNamedRegion(std::string{name}, std::move(region));
		}

		decltype(auto) registerNamedRegion(const char* name, SubpageData::AllocatedViewRegion&& region){
			return registerNamedRegion(std::string{name}, std::move(region));
		}

		template <typename T>
			requires (std::same_as<T, VkImage> || std::same_as<T, VkBuffer>)
		decltype(auto) registerNamedRegion(
			const std::string_view name,
			const Core::Vulkan::Context* context, VkCommandBuffer commandBuffer,
			T data, const Rect region
		){
			return ImagePage::registerNamedRegion(std::string{name}, allocate<T>(context, commandBuffer, std::move(data), region));
		}

		std::vector<Pixmap> exportImages(const Core::Vulkan::CommandPool& commandPool, VkQueue queue) const{
			std::vector<Pixmap> pixmaps{};
			pixmaps.reserve(subpages.size());

			for (const auto & subpage : subpages){
				pixmaps.push_back(subpage.texture.exportToPixmap(commandPool.getTransient(queue)));
			}

			return pixmaps;
		}
	};

	export using AllocatedImageViewRegion = SubpageData::AllocatedViewRegion;

	export struct ImageAtlas{
		const Core::Vulkan::Context* context{};
		Core::Vulkan::CommandPool transientCommandPool{};

		ext::string_hash_map<ImagePage> pages{};

		[[nodiscard]] ImageAtlas() = default;

		[[nodiscard]] explicit ImageAtlas(const Core::Vulkan::Context& context) :
			context{&context},
			transientCommandPool{
				context.device, context.graphicFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
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
			return pages.try_emplace(std::string(name), name, size).first->second;
		}

		[[nodiscard]] AllocatedImageViewRegion allocate(
			ImagePage& page,
			VkImage image, const Rect region) const{
			return page.allocate(context, obtainTransientCommand(), image, region);
		}

		[[nodiscard]] AllocatedImageViewRegion allocate(ImagePage& page, const Pixmap& pixmap) const{
			const Core::Vulkan::StagingBuffer buffer(context->physicalDevice, context->device, pixmap.sizeBytes());
			buffer.memory.loadData(pixmap.data(), pixmap.sizeBytes());

			return page.allocate(context, obtainTransientCommand(), buffer.get(), Geom::Rect_Orthogonal<std::uint32_t>{pixmap.size2D()});
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
			return page.exportImages(transientCommandPool, context->device.getPrimaryGraphicsQueue());
		}

		[[nodiscard]] Core::Vulkan::TransientCommand obtainTransientCommand() const{
			return transientCommandPool.getTransient(context->device.getPrimaryGraphicsQueue());
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
				return &page->registerNamedRegion(localName, std::move(region)).first;
			}

			return nullptr;
		}

		AllocatedImageViewRegion* registerNamedImageViewRegion(const std::string_view name, const Pixmap& pixmap){
			auto [category, localName] = splitKey(name);

			if(const auto page = findPage(category)){
				return &page->registerNamedRegion(localName, allocate(*page, pixmap)).first;
			}

			return nullptr;
		}

		decltype(auto) registerNamedImageViewRegionGuaranteed(const std::string_view name, const Pixmap& pixmap){
			auto [category, localName] = splitKey(name);
			auto& page = registerPage(category);
			return page.registerNamedRegion(localName, allocate(page, pixmap));
		}

		decltype(auto) registerNamedImageViewRegion(ImagePage& page, const std::string_view name, const Pixmap& pixmap) const{
			return page.registerNamedRegion(name, allocate(page, pixmap));
		}

		static std::pair<std::string_view, std::string_view> splitKey(const std::string_view name){
			const auto pos = name.find('.');
			const std::string_view category = name.substr(0, pos);
			const std::string_view localName = name.substr(pos + 1);

			return {category, localName};
		}
	};
}
