#pragma once

#include "render/raytracing.h"

class RTSampler
{
  std::shared_ptr<RayTracer> tracer;
  uint32_t width, height;
public:
  RTSampler(std::shared_ptr<RayTracer> tr, uint32_t w, uint32_t h) : tracer(tr), width(w), height(h) {}

  SurfaceInfo sample(float x, float y) const
  {
    const uint32_t x_texel = x * width;
    const uint32_t y_texel = y * height;
    return tracer->CastSingleRay(float(x_texel) + x * float(width)  - float(x_texel),  
                                 float(y_texel) + y * float(height) - float(y_texel)); // fixed width to height
  }

  SurfaceInfo fetch(uint32_t x, uint32_t y) const { return tracer->CastSingleRay(float(x), float(y) ); } // + 0.5f ?
};

inline bool close_tex_data(SurfaceInfo a, SurfaceInfo b)
{
  return a.objId == b.objId;
}

