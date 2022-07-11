#include <vector>
#include "render/raytracing.h"
#include "render/image_save.h"

#include "svm.h"
#include "RT_sampler.h"
#include "SubPixelImageBSP.h"
#include "SubPixelImageNaive.h"

#include <set>
#include <cassert>
#include <fstream>
#include <sstream>
#include <random>

void PlaneHammersley(float *result, int n)
{
  for (int k = 0; k<n; k++)
  {
    float u = 0;
    int kk = k;

    for (float p = 0.5f; kk; p *= 0.5f, kk >>= 1)
      if (kk & 1)                           // kk mod 2 == 1
        u += p;

    float v = (k + 0.5f) / n;

    result[2 * k + 0] = u;
    result[2 * k + 1] = v;
  } 
}

static inline float saturate(float x) { return std::max(std::min(x, 1.0f), 0.0f); }


int main(int argc, char **argv)
{
  constexpr uint32_t WIDTH = 1024;
  constexpr uint32_t HEIGHT = 1024;
  
  std::cout << "[main]: loading scene ... " << std::endl;
  auto pRayTracerCPU = std::make_shared<RayTracer>(WIDTH, HEIGHT);
  //auto loaded = pRayTracerCPU->LoadScene("../01_simple_scenes/instanced_objects.xml");
  auto loaded = pRayTracerCPU->LoadScene("../01_simple_scenes/bunny_cornell.xml");

  if(!loaded)
    return -1;

  std::vector<uint32_t> image(WIDTH*HEIGHT);

  std::cout << "[main]: compute 'plain' image ... " << std::endl;
  #pragma omp parallel for default(none) shared(HEIGHT, WIDTH, image, pRayTracerCPU)
  for (int j = 0; j < HEIGHT; ++j)
    for (uint32_t i = 0; i < WIDTH; ++i)
      image[j*WIDTH+i] = pRayTracerCPU->CastSingleRay(float(i)+0.5f, float(j)+0.5f).color;

  saveImageLDR("00_output_aliased.png", image, WIDTH, HEIGHT, 4);
  memset(image.data(), 0, sizeof(uint32_t)*size_t(WIDTH*HEIGHT));
  
  const uint32_t aaSamples = 4;
  
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
  const uint32_t refSubSamples = aaSamples*aaSamples;
  std::vector<float> hammSamples(refSubSamples * 2);
  PlaneHammersley(hammSamples.data(), refSubSamples);
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////

  std::cout << "[main]: compute 'antialiased' reference image ... " << std::endl;

  std::vector<SurfaceInfo> raytracedImageData(WIDTH * HEIGHT);
  #pragma omp parallel for default(none) shared(HEIGHT, WIDTH, raytracedImageData, pRayTracerCPU, image, hammSamples)
  for (int j = 0; j < int(HEIGHT); ++j)
  {
    for (int i = 0; i < int(WIDTH); ++i)
    {
      float gt_red   = 0;
      float gt_green = 0;
      float gt_blue  = 0;

      for (int k = 0; k < int(refSubSamples); ++k)
      {
        SurfaceInfo sample = pRayTracerCPU->CastSingleRay(float(i) + hammSamples[k * 2 + 0], float(j) + hammSamples[k * 2 + 1]);

        gt_red   += float(sample.color & 0xFF)         / 255.0f;
        gt_green += float((sample.color >> 8) & 0xFF)  / 255.0f;
        gt_blue  += float((sample.color >> 16) & 0xFF) / 255.0f;
      }

      gt_red   = saturate(gt_red/refSubSamples)   * 255.0f;
      gt_green = saturate(gt_green/refSubSamples) * 255.0f;
      gt_blue  = saturate(gt_blue/refSubSamples)  * 255.0f;

      image[j*WIDTH+i] = 0xFF000000u | (uint32_t(gt_blue) << 16) | (uint32_t(gt_green) << 8) | (uint32_t)(gt_red);
    }
  }

  saveImageLDR("01_output_antialiased.png", image, WIDTH, HEIGHT, 4);
  memset(image.data(), 0, sizeof(uint32_t)*size_t(WIDTH*HEIGHT));

  SubPixelImageBSP<SurfaceInfo>::Config config;
  config.width  = WIDTH;
  config.height = HEIGHT;
  config.radius = 0.5f;
  config.additionalSamplesCnt = 64;
  SubPixelImageBSP<SurfaceInfo> sampler(config);
 
  std::cout << "[main]: building bsp image ... " << std::endl;
  sampler.configure(RTSampler(pRayTracerCPU, WIDTH, HEIGHT));
  
  std::cout << "[main]: compute 'antialiased_bsp' image ... " << std::endl;
  for (uint32_t j = 0; j < HEIGHT; ++j)
  {
    for (uint32_t i = 0; i < WIDTH; ++i)
    {
      float r = 0, g = 0, b = 0;

      for (int k = 0; k < int(refSubSamples); ++k)
      {
        float x = ( float(i) + hammSamples[k * 2 + 0] ) / float(WIDTH);
        float y = ( float(j) + hammSamples[k * 2 + 1] ) / float(HEIGHT);

        SurfaceInfo sample = sampler.sample(x, y);
        r += ((sample.color >> 16) & 0xFF);
        g += ((sample.color >> 8) & 0xFF);
        b += (sample.color & 0xFF);
      }

      r /= aaSamples * aaSamples;
      g /= aaSamples * aaSamples;
      b /= aaSamples * aaSamples;
      image[j*WIDTH+i] = (0xFF000000 | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)b);
    }
  }

  saveImageLDR("02_output_antialiased_bsp.png", image, WIDTH, HEIGHT, 4);
  memset(image.data(), 0, sizeof(uint32_t)*size_t(WIDTH*HEIGHT));

  std::cout << "dumping sublixels ..." << std::endl;
  std::string out2 = "./debug_pixels/";
  sampler.dumpSamples(out2.c_str());
  for (uint32_t y1 = 0; y1 < HEIGHT; ++y1)
    for (uint32_t x1 = 0; x1 < WIDTH; ++x1)
      sampler.dumpPixel(out2.c_str(), x1, y1);
  std::cout << "dumping sublixels finished!" << std::endl;

  // now check the same with our "stupid subpixel image" approach
  //
  SubPixelImageNaive<SurfaceInfo> samplerStupid(WIDTH,HEIGHT);
  std::cout << "[main]: building stupid subpixel image ... " << std::endl;
  samplerStupid.configure(RTSampler(pRayTracerCPU, WIDTH, HEIGHT));

  std::cout << "[main]: compute 'antialiased_stupid' image ... " << std::endl;
  for (uint32_t j = 0; j < HEIGHT; ++j)
  {
    for (uint32_t i = 0; i < WIDTH; ++i)
    {
      float r = 0, g = 0, b = 0;
      
      for (int k = 0; k < int(refSubSamples); ++k)
      {
        float x = ( float(i) + hammSamples[k * 2 + 0] ) / float(WIDTH);
        float y = ( float(j) + hammSamples[k * 2 + 1] ) / float(HEIGHT);

        SurfaceInfo sample = samplerStupid.sample(x, y);
        r += ((sample.color >> 16) & 0xFF);
        g += ((sample.color >> 8) & 0xFF);
        b += (sample.color & 0xFF);
      }

      r /= aaSamples * aaSamples;
      g /= aaSamples * aaSamples;
      b /= aaSamples * aaSamples;
      image[j*WIDTH+i] = 0xFF000000 | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)b;
    }
  }

  saveImageLDR("03_output_antialiased_stupid.png", image, WIDTH, HEIGHT, 4);
  memset(image.data(), 0, sizeof(uint32_t)*size_t(WIDTH*HEIGHT));

  return 0;
}
