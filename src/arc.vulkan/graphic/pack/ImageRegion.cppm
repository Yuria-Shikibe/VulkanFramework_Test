module;

#include <vulkan/vulkan.h>

export module Graphic.ImageRegion;

import Geom.Vector2D;
import Geom.Rect_Orthogonal;
import std;

export namespace Graphic {
    struct ImageViewRegion {
        VkImageView imageView{};

        Geom::USize2 srcImageSize{};

        Geom::Vec2 v00{};
        Geom::Vec2 v10{};
        Geom::Vec2 v11{};
        Geom::Vec2 v01{};

        [[nodiscard]] ImageViewRegion() = default;

        [[nodiscard]] ImageViewRegion(VkImageView imageView, const Geom::USize2 srcImageSize, const Geom::Rect_Orthogonal<std::uint32_t> internal = {})
            : imageView{imageView},
              srcImageSize{srcImageSize}{
            const auto size_f = srcImageSize.as<float>();
            v00 = internal.vert_00().as<float>() / size_f;
            v10 = internal.vert_10().as<float>() / size_f;
            v11 = internal.vert_11().as<float>() / size_f;
            v01 = internal.vert_01().as<float>() / size_f;
        }
    };

    struct ImageRegion {
        VkImage image{};

        Geom::USize2 size{};

        //TODO
        VkFormat format{};
    };
}
