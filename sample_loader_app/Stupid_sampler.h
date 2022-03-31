#pragma once

#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>
#include <cstdint>
#include <cassert>
#include <stack>
#include "svm.h"

template<typename TexType>
class StupidImageSampler
{
  public:
  static constexpr uint32_t SUBPIXEL_SIZE_X = 4;
  static constexpr uint32_t SUBPIXEL_SIZE_Y = 4;
  static constexpr uint32_t SUBPIXEL_SIZE   = SUBPIXEL_SIZE_X*SUBPIXEL_SIZE_Y;

  struct Config
  {
    uint32_t width, height;
    float fwidth, fheight;
    float invWidth;
    float invHeight;
  } config;

  StupidImageSampler(uint32_t a_width, uint32_t a_height)
  {
    config.width   = a_width;
    config.height  = a_height;
    config.fwidth  = float(a_width);
    config.fheight = float(a_height);
    config.invWidth  = 1.0f/config.fwidth;
    config.invHeight = 1.0f/config.fheight;
    singleRayData.resize(config.width * config.height);
  }

  struct SubPixel {
    TexType data[SUBPIXEL_SIZE_X][SUBPIXEL_SIZE_Y] = {};
  };

  std::vector<TexType>                   singleRayData;
  std::unordered_map<uint32_t, SubPixel> m_subPixels;

  static inline uint32_t PackXY(uint32_t x, uint32_t y) { return ((y & 0x0000FFFF) << 16) | (x & 0x0000FFFF); }
  static inline uint32_t UnpackX(uint32_t a_hash)       { return (a_hash & 0x0000FFFF); }
  static inline uint32_t UnpackY(uint32_t a_hash)       { return (a_hash & 0xFFFF0000) >> 16; }

  template<typename BaseSampler>
  void configure(const BaseSampler &sampler)
  {
    // Fill initial grid. One ray per texel.
    #pragma omp parallel for default(none) shared(singleRayData, sampler, config)
    for (int y = 0; y < int(config.height); ++y)
      for (int x = 0; x < int(config.width); ++x)
        singleRayData[y*config.width + x] = sampler.fetch(x, y);

    // Collect suspicious (aliased, with high divergence in neighbourhood) texels.
    m_subPixels.reserve(10*(config.width + config.height));
    for (int y1 = 0; y1 < int(config.height); ++y1) {
      for (int x1 = 0; x1 < int(config.width); ++x1) {
        bool needResample = false;
        const TexType texel = singleRayData[y1 * config.width + x1];
        for (int x = std::max(x1 - 1, 0); x <= std::min(x1 + 1, (int)config.width - 1) && !needResample; ++x)
          for (int y = std::max(y1 - 1, 0); y <= std::min(y1 + 1, (int)config.height - 1) && !needResample; ++y)
            needResample = !close_tex_data(singleRayData[y * config.width + x], texel);

        if (needResample)
          m_subPixels[PackXY(x1,y1)] = SubPixel();
      }
    }

    // Process suspicious texels
    //
    for(auto& pair : m_subPixels)
    {
      const uint32_t px = UnpackX(pair.first);
      const uint32_t py = UnpackY(pair.first);
      //const float   fpx = float(px);
      //const float   fpy = float(py);

      auto& subPixel = pair.second;
      for(uint32_t sy=0; sy < SUBPIXEL_SIZE_Y; sy++)
      {
        //const float yNormalized = (fpy + (float(sy) + 0.5f)/float(SUBPIXEL_SIZE_Y))*config.invHeight;
        const float yNormalized = (float)(sy + py * SUBPIXEL_SIZE_Y) / float(SUBPIXEL_SIZE_Y * config.height);
        for(uint32_t sx=0; sx < SUBPIXEL_SIZE_X; sx++)
        {
          //const float xNormalized = (fpx + (float(sx)+ 0.5f)/float(SUBPIXEL_SIZE_X))*config.invWidth;
          const float xNormalized = (float)(sx + px * SUBPIXEL_SIZE_X) / float(SUBPIXEL_SIZE_X * config.width);
          subPixel.data[sx][sy] = sampler.sample(xNormalized, yNormalized);
        }
      }
    }
  }

  TexType& access(float x, float y)
  {
    const float fx = x * config.fwidth;
    const float fy = y * config.fheight;
    const uint32_t x_texel = uint32_t(fx);
    const uint32_t y_texel = uint32_t(fy);

    auto p = m_subPixels.find(PackXY(x_texel,y_texel));
    if (p == m_subPixels.end())
      return singleRayData[y_texel * config.width + x_texel];
    
    const float offsetX = (fx - float(x_texel))*float(SUBPIXEL_SIZE_X);
    const float offsetY = (fy - float(y_texel))*float(SUBPIXEL_SIZE_Y);
    const uint32_t ux = std::min(uint32_t(offsetX),SUBPIXEL_SIZE_X-1);
    const uint32_t uy = std::min(uint32_t(offsetY),SUBPIXEL_SIZE_Y-1);
    return p->second.data[ux][uy];
  }
  
  TexType sample(float x, float y) { return access(x,y); }

  //TexType sample(float x, float y) // for debug
  //{
  //  const float fx = x * config.fwidth;
  //  const float fy = y * config.fheight;
  //  const uint32_t x_texel = uint32_t(fx);
  //  const uint32_t y_texel = uint32_t(fy);
  //
  //  auto p = m_subPixels.find(PackXY(x_texel,y_texel));
  //  if (p == m_subPixels.end())
  //    return singleRayData[y_texel * config.width + x_texel];
  //  
  //  return TexType();
  //}
};