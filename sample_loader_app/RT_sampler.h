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
    const float offsetX    = x * width - x_texel + 0.5f;
    const float offsetY    = y * width - y_texel + 0.5f;
    return tracer->CastSingleRay(x_texel + offsetX, y_texel + offsetY);
  }

  SurfaceInfo fetch(uint32_t x, uint32_t y) const { return tracer->CastSingleRay(float(x)+0.5f, float(y)+0.5f); }
};

inline bool close_tex_data(SurfaceInfo a, SurfaceInfo b)
{
  return a.objId == b.objId;
}

