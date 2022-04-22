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
  
  std::vector<uint32_t> inverted(a_data.size());
  
  #pragma omp parallel for shared(inverted)
  for(int y=0;y<height;y++)
  {
    const int offset1 = y*width;
    const int offset2 = (height-y-1)*width;
    for(int x=0;x<width;x++)
      inverted[offset2+x] = a_data[offset1+x];
  }

  return stbi_write_png(a_filename.c_str(), width, height, channels, (unsigned char*)inverted.data(), width * channels);
}