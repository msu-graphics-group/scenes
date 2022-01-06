#include <vector>
#include "render/raytracing.h"
#include "render/image_save.h"


std::vector<uint32_t> rayTraceCPU(std::shared_ptr<RayTracer> pRayTracer, uint32_t width, uint32_t height)
{
  std::vector<uint32_t> raytracedImageData(width * height, 0u);
#pragma omp parallel for default(none) shared(height, width, raytracedImageData, pRayTracer)
  for (size_t j = 0; j < height; ++j)
  {
    for (size_t i = 0; i < width; ++i)
    {
      pRayTracer->CastSingleRay(i, j, raytracedImageData.data());
    }
  }

  return raytracedImageData;
}

int main()
{
  constexpr uint32_t WIDTH = 1024;
  constexpr uint32_t HEIGHT = 1024;

  auto pRayTracerCPU = std::make_shared<RayTracer>(WIDTH, HEIGHT);
  auto loaded = pRayTracerCPU->LoadScene("../01_simple_scenes/instanced_objects.xml");

  if(!loaded)
    return -1;

  auto image = rayTraceCPU(pRayTracerCPU, WIDTH, HEIGHT);

  saveImageLDR("output.png", image, WIDTH, HEIGHT, 4);

  return 0;
}
