module;

#include <vulkan/vulkan.h>

export module Graphic.ImageRegion;

import Geom.Vector2D;

export namespace Graphic {
    struct ImageViewRegion {
        VkImageView imageView{};

        Geom::USize2 srcImageSize{};

        Geom::Vec2 v00{};
        Geom::Vec2 v10{};
        Geom::Vec2 v11{};
        Geom::Vec2 v01{};
    };

    struct ImageRegion {
        VkImage image{};

        Geom::USize2 size{};

        //TODO
        VkFormat format{};
    };
}
