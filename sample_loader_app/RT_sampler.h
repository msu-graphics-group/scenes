#pragma once

#include "render/raytracing.h"

class RTSampler
{
  std::shared_ptr<RayTracer> tracer;
  uint32_t width, height;
public:
  RTSampler(std::shared_ptr<RayTracer> tr, uint32_t w, uint32_t h) : tracer(tr), width(w), height(h) {}

  SampleInfo sample(float x, float y) const
  {
    SampleInfo sample;
    const uint32_t x_texel = x * width;
    const uint32_t y_texel = y * height;
    tracer->CastSingleRay(x_texel, y_texel, x * width - x_texel, y * width - y_texel, &sample);
    return sample;
  }

  SampleInfo fetch(uint32_t x, uint32_t y) const
  {
    SampleInfo sample;
    tracer->CastSingleRay(x, y, 0, 0, &sample);
    return sample;
  }
};

inline bool close_tex_data(SampleInfo a, SampleInfo b)
{
  return a.objId == b.objId;
}

