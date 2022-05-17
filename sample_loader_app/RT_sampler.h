#pragma once

#include "render/raytracing.h"

class RTSampler
{
  std::shared_ptr<RayTracer> tracer;
  uint32_t width, height;
public:
  RTSampler(std::shared_ptr<RayTracer> tr, uint32_t w, uint32_t h) : tracer(tr), width(w), height(h) {}

  SurfaceInfo sample(float x, float y)      const { return tracer->CastSingleRay(x * float(width), y * float(height)); }
  SurfaceInfo fetch(uint32_t x, uint32_t y) const { return tracer->CastSingleRay(float(x)+0.5f, float(y)+0.5f); }
};


