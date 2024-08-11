module;

export module Graphic.Pixmap;

import Geom.Vector2D;
import ext.RuntimeException;

import OS.File;
import Image;
import ext.Concepts;
import Graphic.Color;
import Math;
import std;



export namespace Graphic{
    class Pixmap{
    public:
        using Color = Graphic::Color;
        using ColorBits = Graphic::Color::ColorBits;
        using DataType = unsigned char;
        using size_type = std::uint32_t;
    protected:
        size_type width{ 1 };
        size_type height{ 1 };
        //TODO is this necessay?
        //unsigned int bpp{ 4 };

        std::unique_ptr<DataType[]> bitmapData{nullptr};

    public:
        static constexpr size_type Channels = 4; //FOR RGBA

        [[nodiscard]] Pixmap() = default;

        [[nodiscard]] size_type getWidth() const {
            return width;
        }

        [[nodiscard]] size_type getHeight() const {
            return height;
        }

        [[nodiscard]] Geom::Vector2D<size_type> size2D() const{
            return {width, height};
        }

        // [[nodiscard]] unsigned getBpp() const {
        //     return bpp;
        // }

        [[nodiscard]] DataType* release() && {
            return bitmapData.release();
        }

        [[nodiscard]] DataType* data() const & {
            return bitmapData.get();
        }

        [[nodiscard]] std::unique_ptr<DataType[]> data() && {
            return std::move(bitmapData);
        }

        static Pixmap createEmpty(const size_type width, const size_type height){
            Pixmap map{};
            map.width = width;
            map.height = height;
            return map;
        }

        [[nodiscard]] Pixmap(const size_type width, const size_type height)
            : width(width),
              height(height) {

            create(width, height);
        }

        explicit Pixmap(const OS::File& file){
            loadFrom(file);
        }

        Pixmap(const size_type width, const size_type height, std::unique_ptr<DataType[]>&& data)
                    : width(width),
                      height(height) {
            this->bitmapData = std::move(data);
        }

        Pixmap(const Pixmap& other)
            : width(other.width),
              height(other.height),
              //bpp(other.bpp),
              bitmapData(other.copyData()) {

        }

        Pixmap(Pixmap&& other) noexcept
            : width(other.width),
              height(other.height),
              //bpp(other.bpp),
              bitmapData(std::move(other.bitmapData)) {
        }

        Pixmap& operator=(const Pixmap& other) {
            if(this == &other) return *this;
            width = other.width;
            height = other.height;
            //bpp = other.bpp;
            bitmapData = other.copyData();
            return *this;
        }

        Pixmap& operator=(Pixmap&& other) noexcept {
            if(this == &other) return *this;
            width = other.width;
            height = other.height;
            //bpp = other.bpp;
            bitmapData = std::move(other.bitmapData);
            return *this;
        }

        [[nodiscard]] constexpr bool valid() const noexcept{
            return bitmapData != nullptr && pixelSize() > 0;
        }

        void create(const size_type width, const size_type height){
            bitmapData.reset();
            this->width = width;
            this->height = height;

            auto size = this->size();
            if(!size)return;

            bitmapData = std::make_unique<DataType[]>(size);
        }

        void free() {
            width = height = 0;
            bitmapData.reset(nullptr);
        }

        void fill(const Color color) const{
            std::fill_n(reinterpret_cast<ColorBits*>(bitmapData.get()), pixelSize(), color.rgba());
        }

        DataType& operator [](const size_t index){
            return bitmapData[index];
        }

        void mix(const Color color, const float alpha) const{
            for(size_type i = 0; i < pixelSize(); ++i){
                Color src = get(i);
                set(i, src.lerpRGB(color, alpha));
            }
        }

        void mulWhite() const{
            for(size_type i = 0; i < pixelSize(); ++i){
                setRaw(i, getRaw(i) | 0x00'ff'ff'ff);
            }
        }

        void loadFrom(const OS::File& file) {
            int b;
            int w, h;
            this->bitmapData = ext::loadPng(file, w, h, b, Channels);
            width = w;
            height = h;
        }

        void write(const OS::File& file, const bool autoCreate = false) const {
            if(!valid())throw ext::RuntimeException{"Invalid Pixmap Data!"};
            if(!file.exist()) {
                bool result = false;

                if(autoCreate)result = file.createFile();

                if(!file.exist() || !result)throw ext::RuntimeException{"Inexist File!"};
            }

            // ReSharper disable once CppTooWideScopeInitStatement
            std::string&& ext = file.extension();

            if(ext == ".png") {
                ext::writePng(file, width, height, Channels, bitmapData.get());
            }else if(ext == ".bmp") {
                ext::writeBmp(file, width, height, Channels, bitmapData.get());
            }
        }

        void clear() const{
            std::fill_n(this->bitmapData.get(), size(), 0);
        }

        [[nodiscard]] constexpr auto dataIndex(const size_type x, const size_type y) const {
            if(x > width || y > height)throw ext::RuntimeException{"Array Index Out Of Bound!"};
            return (y * width + x);
        }

        [[nodiscard]] std::unique_ptr<DataType[]> copyData() const {

            const auto size = this->size();
            auto ptr = std::make_unique<DataType[]>(size);
            std::memcpy(ptr.get(), bitmapData.get(), size);

            return std::move(ptr);
        }

        void copyFrom(const DataType* data, const bool flipY = false) const {
            if(flipY){
                const size_t rowDataCount = static_cast<size_t>(getWidth()) * Channels;

                for(size_type y = 0; y < height; ++y){
                    std::memcpy(bitmapData.get() + rowDataCount * y, data + (getHeight() - y - 1) * rowDataCount, rowDataCount);
                }
            }else{
                std::memcpy(bitmapData.get(), data, size());
            }
        }

        [[nodiscard]] constexpr auto pixelSize() const {
            return width * height;
        }

        [[nodiscard]] constexpr auto size() const {
            return width * height * Channels;
        }

        void setRaw(const size_type index, const ColorBits colorBit) const{
            auto* ptr = reinterpret_cast<ColorBits*>(bitmapData.get()) + index;
            *ptr = colorBit;
        }

        void setRaw(const size_type x, const size_type y, const ColorBits colorBit) const{
            setRaw(dataIndex(x, y), colorBit);
        }

        void set(const size_type index, const Graphic::Color& color) const{
            setRaw(index, color.argb8888());
        }

        void set(const size_type x, const size_type y, const Graphic::Color& color) const{
            setRaw(x, y, color.argb8888());
        }

        [[nodiscard]] ColorBits getRaw(const size_type index) const {
            const auto* ptr = reinterpret_cast<ColorBits*>(bitmapData.get() + index * Channels);
            return *ptr;
        }

        [[nodiscard]] ColorBits getRaw(const size_type x, const size_type y) const {
            return getRaw(dataIndex(x, y));
        }

        [[nodiscard]] Graphic::Color get(const size_type index) const {
            Color color{};
            return color.rgba8888(getRaw(index));
        }

        [[nodiscard]] Graphic::Color get(const size_type x, const size_type y) const {
            return get(dataIndex(x, y));
        }

        void each(Concepts::Invokable<void(Pixmap&, size_type, size_type)> auto&& func) {
            for(size_type x = 0; x < width; x++) {
                for(size_type y = 0; y < height; ++y) {
                    func(*this, x, y);
                }
            }
        }

        void each(Concepts::Invokable<void(const Pixmap&, size_type, size_type)> auto&& func) const {
            for(size_type x = 0; x < width; x++) {
                for(size_type y = 0; y < height; ++y) {
                    func(*this, x, y);
                }
            }
        }

    protected:
        static bool empty(const ColorBits i){
            return (i & 0x000000ff) == 0;
        }

        static ColorBits blend(const ColorBits src, const ColorBits dst){
            const ColorBits src_a = src & Color::a_Mask;
            if(src_a == 0) return dst;

            ColorBits dst_a = dst & Color::a_Mask;
            if(dst_a == 0) return src;
            ColorBits dst_r = dst >> Color::r_Offset & Color::a_Mask;
            ColorBits dst_g = dst >> Color::g_Offset & Color::a_Mask;
            ColorBits dst_b = dst >> Color::b_Offset & Color::a_Mask;

            dst_a -=  static_cast<ColorBits>(static_cast<float>(dst_a) * (static_cast<float>(src_a) / Color::maxValF));
            const ColorBits a = dst_a + src_a;
            dst_r = static_cast<ColorBits>(static_cast<float>(dst_r * dst_a + (src >> Color::r_Offset & Color::a_Mask) * src_a) / static_cast<float>(static_cast<ColorBits>(a)));
            dst_g = static_cast<ColorBits>(static_cast<float>(dst_g * dst_a + (src >> Color::g_Offset & Color::a_Mask) * src_a) / static_cast<float>(static_cast<ColorBits>(a)));
            dst_b = static_cast<ColorBits>(static_cast<float>(dst_b * dst_a + (src >> Color::b_Offset & Color::a_Mask) * src_a) / static_cast<float>(static_cast<ColorBits>(a)));
            return
                dst_r << Color::r_Offset |
                dst_g << Color::g_Offset |
                dst_b << Color::b_Offset |
                    a << Color::a_Offset;
        }

    public:
        void flipY() const{
            const std::size_t rowDataCount = static_cast<std::size_t>(getWidth()) * Channels;

            for(std::size_t y = 0; y < height / 2; ++y){
                std::swap_ranges(
                    bitmapData.get() + rowDataCount * y, bitmapData.get() + rowDataCount * y + rowDataCount,
                    bitmapData.get() + rowDataCount * (height - y - 1)
                );
            }
        }
        void blend(const size_type x, const size_type y, const Color& color) const {
            const ColorBits raw = getRaw(x, y);
            setRaw(x, y, blend(raw, color.rgba8888()));
        }

        [[nodiscard]] Pixmap crop(const size_type srcX, const size_type srcY, const size_type tWidth, const size_type tHeight) const {
            if(!valid())throw ext::RuntimeException{"Resource Released!"};
            Pixmap pixmap{tWidth, tHeight};
            pixmap.draw(*this, 0, 0, srcX, srcY, width, height);
            return pixmap;
        }

        void draw(const Pixmap& pixmap, const bool blend = true) const {
            draw(pixmap, 0, 0, pixmap.width, pixmap.height, 0, 0, pixmap.width, pixmap.height, false, blend);
        }

        void draw(const Pixmap& pixmap) const {
            draw(pixmap, 0, 0);
        }

        void draw(const Pixmap& pixmap, const size_type x, const size_type y, const bool blending = true) const {
            draw(pixmap, 0, 0, pixmap.width, pixmap.height, x, y, pixmap.width, pixmap.height, false, blending);
        }

     /** Draws an area from another Pixmap to this Pixmap. */
        void draw(const Pixmap& pixmap, const size_type x, const size_type y, const size_type width, const size_type height, const bool filtering = true, const bool blending = true) const {
            draw(pixmap, 0, 0, pixmap.width, pixmap.height, x, y, width, height, filtering, blending);
        }

     /** Draws an area from another Pixmap to this Pixmap. */
        void draw(const Pixmap& pixmap, const size_type x, const size_type y, const size_type srcx, const size_type srcy, const size_type srcWidth, const size_type srcHeight) const {
            draw(pixmap, srcx, srcy, srcWidth, srcHeight, x, y, srcWidth, srcHeight);
        }

        void draw(const Pixmap& pixmap, const size_type srcx, const size_type srcy, const size_type srcWidth, const size_type srcHeight, const size_type dstx, const size_type dsty, const size_type dstWidth, const size_type dstHeight, const bool filtering = false, const bool blending = true) const {
            const size_type owidth = pixmap.width;
            const size_type oheight = pixmap.height;

            //don't bother drawing invalid regions
            if(srcWidth == 0 || srcHeight == 0 || dstWidth == 0 || dstHeight == 0){
                return;
            }

            if(srcWidth == dstWidth && srcHeight == dstHeight){
                size_type sx;
                size_type dx;
                size_type sy = srcy;
                size_type dy = dsty;

                if(blending){
                    for(; sy < srcy + srcHeight; sy++, dy++){
                        if(sy >= oheight || dy >= height) break;

                        for(sx = srcx, dx = dstx; sx < srcx + srcWidth; sx++, dx++){
                            if(sx >= owidth || dx >= width) break;
                            setRaw(dx, dy, blend(pixmap.getRaw(sx, sy), getRaw(dx, dy)));
                        }
                    }
                }else{
                        //TODO this can be optimized with scanlines, potentially
                    for(; sy < srcy + srcHeight; sy++, dy++){
                        if(sy >= oheight || dy >= height) break;

                        for(sx = srcx, dx = dstx; sx < srcx + srcWidth; sx++, dx++){
                            if(sx >= owidth || dx >= width) break;
                            setRaw(dx, dy, pixmap.getRaw(sx, sy));
                        }
                    }
                }
            }else{
                if(filtering){
                    //blit with bilinear filtering
                    const float x_ratio = (static_cast<float>(srcWidth) - 1) / static_cast<float>(dstWidth);
                    const float y_ratio = (static_cast<float>(srcHeight) - 1) / static_cast<float>(dstHeight);
                    const size_type rX = Math::max(Math::round<size_type>(x_ratio), 1u);
                    const size_type rY = Math::max(Math::round<size_type>(y_ratio), 1u);
                    const size_type spitch = Pixmap::Channels * owidth;

                    for(size_type i = 0; i < dstHeight; i++){
                        const size_type sy = static_cast<size_type>(static_cast<float>(i) * y_ratio) + srcy;
                        const size_type dy = i + dsty;
                        const float ydiff = y_ratio * static_cast<float>(i) + static_cast<float>(srcy - sy);

                        if(sy >= oheight || dy >= height) break;

                        for(size_type j = 0; j < dstWidth; j++){
                            const size_type sx = static_cast<size_type>(static_cast<float>(j) * x_ratio) + srcx;
                            const size_type dx = j + dstx;
                            const float xdiff = x_ratio * static_cast<float>(j) + static_cast<float>(srcx - sx);
                            if(sx >= owidth || dx >= width) break;

                            const size_type
                                srcp = (sx + sy * owidth) * Pixmap::Channels;

                            const ColorBits
                                c1 = pixmap.getRaw(srcp),
                                c2 = sx + rX < srcWidth  ? pixmap.getRaw(srcp + Pixmap::Channels *rX) : c1,
                                c3 = sy + rY < srcHeight ? pixmap.getRaw(srcp + spitch * rY) : c1,
                                c4 = sx + rX < srcWidth && sy + rY < srcHeight ? pixmap.getRaw(srcp + Pixmap::Channels * rX + spitch * rY) : c1;

                            const float ta = (1 - xdiff) * (1 - ydiff);
                            const float tb =      xdiff  * (1 - ydiff);
                            const float tc = (1 - xdiff) *      ydiff ;
                            const float td =      xdiff  *      ydiff ;

                            const ColorBits r = Color::a_Mask & static_cast<ColorBits>(
                                static_cast<float>((c1 & Color::r_Mask) >> Color::r_Offset) * ta +
                                static_cast<float>((c2 & Color::r_Mask) >> Color::r_Offset) * tb +
                                static_cast<float>((c3 & Color::r_Mask) >> Color::r_Offset) * tc +
                                static_cast<float>((c4 & Color::r_Mask) >> Color::r_Offset) * td
                            );
                            const ColorBits g = Color::a_Mask & static_cast<ColorBits>(
                                static_cast<float>((c1 & Color::g_Mask) >> Color::g_Offset) * ta +
                                static_cast<float>((c2 & Color::g_Mask) >> Color::g_Offset) * tb +
                                static_cast<float>((c3 & Color::g_Mask) >> Color::g_Offset) * tc +
                                static_cast<float>((c4 & Color::g_Mask) >> Color::g_Offset) * td
                            );
                            const ColorBits b = Color::a_Mask & static_cast<ColorBits>(
                                static_cast<float>((c1 & Color::b_Mask) >> Color::b_Offset) * ta +
                                static_cast<float>((c2 & Color::b_Mask) >> Color::b_Offset) * tb +
                                static_cast<float>((c3 & Color::b_Mask) >> Color::b_Offset) * tc +
                                static_cast<float>((c4 & Color::b_Mask) >> Color::b_Offset) * td
                            );
                            const ColorBits a = Color::a_Mask & static_cast<ColorBits>(
                                static_cast<float>((c1 & Color::a_Mask) >> Color::a_Offset) * ta +
                                static_cast<float>((c2 & Color::a_Mask) >> Color::a_Offset) * tb +
                                static_cast<float>((c3 & Color::a_Mask) >> Color::a_Offset) * tc +
                                static_cast<float>((c4 & Color::a_Mask) >> Color::a_Offset) * td
                            );

                            const size_type srccol = r << Color::r_Offset | g << Color::g_Offset | b << Color::b_Offset | a << Color::a_Offset;

                            setRaw(dx, dy, !blending ? srccol : blend(srccol, getRaw(dx, dy)));
                        }
                    }
                }else{
                    //blit with nearest neighbor filtering
                    const size_type xratio = (srcWidth << 16) / dstWidth + 1;
                    const size_type yratio = (srcHeight << 16) / dstHeight + 1;

                    for(size_type i = 0; i < dstHeight; i++){
                        const size_type sy = (i * yratio >> 16) + srcy;
                        const size_type dy = i + dsty;
                        if(sy >= oheight || dy >= height) break;

                        for(size_type j = 0; j < dstWidth; j++){
                            const size_type sx = (j * xratio >> 16) + srcx;
                            const size_type dx = j + dstx;
                            if(sx >= owidth || dx >= width) break;

                            setRaw(dx, dy, !blending ? pixmap.getRaw(sx, sy) : blend(pixmap.getRaw(sx, sy), getRaw(dx, dy)));
                        }
                    }
                }
            }
        }

        //If the area of the pixmaps are the same, the longer one row it be, the faster the function works.
        void set(const Pixmap& pixmap, const size_type dstx, const size_type dsty) const {
            const size_type rowDataCount = pixmap.getWidth() * Channels;

            for(size_type y = 0; y < pixmap.getHeight(); ++y) {
                const auto indexDst =  this->dataIndex(dstx, dsty + y) * Channels;
                const auto indexSrc = pixmap.dataIndex(0   ,        y) * Channels;

                std::memcpy(bitmapData.get() + indexDst, pixmap.bitmapData.get() + indexSrc, rowDataCount);
            }
        }

        [[nodiscard]] Pixmap subMap(const size_type srcx, const size_type srcy, const size_type width, const size_type height) const{
            if(!valid())throw ext::RuntimeException{"Resource Released!"};
            if(srcx + width > this->width || srcy + height > this->height) {
                throw ext::IllegalArguments{"Sub Region Larger Than Source Pixmap!"};
            }

            const size_type rowDataCount = width * Channels;

            Pixmap newMap{width, height};

            for(size_type y = 0; y < height; ++y) {
                const auto indexDst = newMap.dataIndex(0   ,        y) * Channels;
                const auto indexSrc =  this->dataIndex(srcx, srcy + y) * Channels;

                std::memcpy(newMap.bitmapData.get() + indexDst, bitmapData.get() + indexSrc, rowDataCount);
            }

            return newMap;
        }
    };
}

