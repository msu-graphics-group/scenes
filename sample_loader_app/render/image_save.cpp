#include "image_save.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool saveImageLDR(const std::string& a_filename, const std::vector<unsigned char> &a_data,
                  int width, int height, int channels)
{
  return stbi_write_png(a_filename.c_str(), width, height, channels, a_data.data(), width * channels);
}

bool saveImageLDR(const std::string& a_filename, const std::vector<uint32_t> &a_data,
                  int width, int height, int channels)
{
  return stbi_write_png(a_filename.c_str(), width, height, channels, (unsigned char*)a_data.data(), width * channels);
}