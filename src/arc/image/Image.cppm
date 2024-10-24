module;

// ReSharper disable once CppUnusedIncludeDirective
#include "stbi_io.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <native/stbi/stbi_image_write.h>
#include <native/stbi/stbi_image.h>

export module Image;

import std;

export namespace ext{
	// std::shared_ptr<GLFWimage> obtain_GLFWimage(const OS::File& file, const int requiredBpp = 4) {
	// 	GLFWimage image{};
	// 	int b;
	// 	image.pixels = stbi_load(file.absolutePath().string().data(), &image.width, &image.height, &b, requiredBpp);
	// 	return std::make_shared<GLFWimage>(image);
	// }

	int writePng(char const *filename, const int x, const int y, const int comp, const void *data, const int stride_bytes) {
		return stbi_write_png(filename, x, y, comp, data, stride_bytes);
	}


	void setFlipVertically_load(const bool flag) {
		stbi_set_flip_vertically_on_load(flag);
	}

	void setFlipVertically_write(const bool flag) {
		stbi_flip_vertically_on_write(flag);
	}

	//byte per pixel
	//RGBA ~ 4 ~ 32 [0 ~ 255]
	//RGB ~ 3
	std::unique_ptr<unsigned char[]> loadPng(const std::filesystem::path& file, int& width, int& height, int& bpp, const int requiredBpp = 4) {
		int w, h, b;

		const auto data = stbi_load(std::filesystem::absolute(file).string().data(), &w, &h, &b, requiredBpp);
		width = w;
		height = h;
		bpp = requiredBpp;
		return std::unique_ptr<unsigned char[]>{data};
	}

	std::string_view getException() {
		return stbi_failure_reason();
	}

	[[maybe_unused]] int writePng(const std::filesystem::path& file, const unsigned int width, const unsigned int height, const unsigned int bpp, const unsigned char* data, const int stride = 0) {
		return stbi_write_png(std::filesystem::absolute(file).string().data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(bpp), data, stride);
	}

	[[maybe_unused]] int writeBmp(const std::filesystem::path& file, const unsigned int width, const unsigned int height, const unsigned int bpp, const unsigned char* data) {
		return stbi_write_bmp(std::filesystem::absolute(file).string().data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(bpp), data);
	}
}
