#pragma once

#include "render/raytracing.h"


class RTSampler
{
  std::shared_ptr<RayTracer> tracer;
  uint32_t width, height;
public:
  RTSampler(std::shared_ptr<RayTracer> tr, uint32_t w, uint32_t h) : tracer(tr), width(w), height(h) {}

  SurfaceInfo sample(float x, float y)      const { return tracer->CastSingleRay(x * float(width), y * float(height)); }
  SurfaceInfo fetch(uint32_t x, uint32_t y) const { return tracer->CastSingleRay(float(x)+0.5f, float(y)+0.5f ); } // + 0.5f ?
};


/*
class RTSampler
{
  std::shared_ptr<RayTracer> tracer;
  uint32_t width, height;
public:
  RTSampler(std::shared_ptr<RayTracer> tr, uint32_t w, uint32_t h) : tracer(tr), width(w), height(h) {}

  SurfaceInfo sample(float x, float y) const
  {
    SurfaceInfo sample;
    const uint32_t x_texel = x * width;
    const uint32_t y_texel = y * height;
    tracer->CastSingleRay(x_texel, y_texel, x * width - x_texel + 0.5f, 
                                            y * width - y_texel + 0.5f, 
                                            &sample);
    return sample;
  }

  SurfaceInfo fetch(uint32_t x, uint32_t y) const
  {
    SurfaceInfo sample;
    tracer->CastSingleRay(x, y, 0, 0, &sample);
    return sample;
  }
};
*/


inline bool close_tex_data(SurfaceInfo a, SurfaceInfo b)
{
  return a.objId == b.objId;
}

