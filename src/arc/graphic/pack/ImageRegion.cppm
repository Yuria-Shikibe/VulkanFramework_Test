module;

#include <vulkan/vulkan.h>

export module Graphic.ImageRegion;

import Geom.Vector2D;
import Geom.Rect_Orthogonal;
import std;
import ext.concepts;

export namespace Graphic {
    struct UVData{
        Geom::Vec2 v00{};
        Geom::Vec2 v10{};
        Geom::Vec2 v11{};
        Geom::Vec2 v01{};

        void flipY(){
            std::swap(v00.y, v01.y);
            std::swap(v10.y, v11.y);
        }

        template <ext::number T>
        [[nodiscard]] constexpr Geom::Rect_Orthogonal<T> projRegion(const Geom::Rect_Orthogonal<T>& rect) const noexcept{
            auto sz = rect.getSize();

            auto tv00 = sz;
            auto tv11 = sz;
            tv00.sclRound(v00);
            tv11.sclRound(v11);
            return Geom::Rect_Orthogonal<T>{tv00 + rect.src, tv11 + rect.src};
        }

        template <ext::number T>
        [[nodiscard]] constexpr Geom::Vector2D<T> projSize(Geom::Vector2D<T> size) const noexcept{
            auto tv00 = size;
            auto tv11 = size;
            tv00.sclRound(v00);
            tv11.sclRound(v11);

            return tv11 - tv00;
        }

        void setUV_fromInternal(const Geom::USize2 imageSize, const Geom::Rect_Orthogonal<std::uint32_t> internal){
            setUV_fromInternal(imageSize, internal.as<float>());
        }

        void setUV_fromInternal(const Geom::USize2 imageSize, const Geom::Rect_Orthogonal<float> internal){
            const auto size_f = imageSize.as<float>();

            v00 = internal.vert_00() / size_f;
            v10 = internal.vert_10() / size_f;
            v11 = internal.vert_11() / size_f;
            v01 = internal.vert_01() / size_f;
        }

        template<typename N0, typename N1>
            requires (std::is_arithmetic_v<N0> && std::is_arithmetic_v<N1>)
        void setUV(const Geom::USize2 imageSize, const Geom::Rect_Orthogonal<N0>& internal, const Geom::Rect_Orthogonal<N1>& external) {
            const Geom::Vec2 src{external.src.template as<float>() / imageSize.as<float>()};

            const Geom::Vec2 v00 = src + internal.getSrc().template as<float>() / external.getSize().template as<float>();
            const Geom::Vec2 v11 = src + internal.getEnd().template as<float>() / external.getSize().template as<float>();

            const Geom::OrthoRectFloat newBound{v00, v11};

            this->v00 = newBound.vert_00();
            this->v10 = newBound.vert_10();
            this->v01 = newBound.vert_01();
            this->v11 = newBound.vert_11();
        }

        void shrinkEdge(const Geom::USize2 imageSize, const Geom::Vec2 margin) {
            Geom::OrthoRectFloat newBound{projRegion(Geom::OrthoRectUInt{{}, imageSize}).as<float>()};
            newBound.shrink(margin);
            setUV_fromInternal(imageSize, newBound);
        }

        friend constexpr bool operator==(const UVData&, const UVData&) = default;

    };

    struct SizedImageView{
        VkImageView view{};
        Geom::USize2 size{};

        friend constexpr bool operator==(const SizedImageView&, const SizedImageView&) = default;
    };

    struct ImageViewRegion : SizedImageView, UVData{
        [[nodiscard]] ImageViewRegion() = default;

        [[nodiscard]] ImageViewRegion(VkImageView imageView, const Geom::USize2 srcImageSize, const Geom::Rect_Orthogonal<std::uint32_t> internal = {})
            : SizedImageView{imageView, srcImageSize}{
            setUV_fromInternal(internal);
        }

        [[nodiscard]] constexpr decltype(auto) getBound() const noexcept{
            return this->projRegion<std::uint32_t>({Geom::FromExtent, {}, size});
        }


        [[nodiscard]] constexpr decltype(auto) getSize() const noexcept{
            return this->projSize<std::int32_t>(size.as<int>()).toAbs().as<std::uint32_t>();
        }

        void setUV_fromInternal(const Geom::Rect_Orthogonal<std::uint32_t> internal){
            setUV_fromInternal(internal.as<float>());
        }

        void setUV_fromInternal(const Geom::Rect_Orthogonal<float> internal){
            UVData::setUV_fromInternal(size, internal);
        }

        template<typename N0, typename N1>
            requires (std::is_arithmetic_v<N0> && std::is_arithmetic_v<N1>)
        void setUV(const Geom::Rect_Orthogonal<N0>& internal, const Geom::Rect_Orthogonal<N1>& external) {
            UVData::setUV(size, internal, external);
        }

        void shrinkEdge(const Geom::Vec2 margin) {
            UVData::shrinkEdge(size, margin);
        }

        friend constexpr bool operator==(const ImageViewRegion&, const ImageViewRegion&) = default;
    };
}
