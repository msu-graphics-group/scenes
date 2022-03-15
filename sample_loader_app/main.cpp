#include <vector>
#include "render/raytracing.h"
#include "render/image_save.h"

#include "svm.h"
#include "RT_sampler.h"
#include "BSP_based_sampler.h"
#include <set>
#include <cassert>
#include <fstream>
#include <sstream>

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

float saturate(float x)
{
  return std::max(std::min(x, 1.0f), 0.0f);
}

void get_indices(const std::vector<float> &hamm, const std::vector<uint32_t> ids, const std::array<float, 3> &line, float x, float y, uint32_t &idx1, uint32_t &idx2)
{
  idx2 = ids[0];
  for (uint32_t i : ids)
  {
    if (i != idx2)
    {
      idx1 = i;
      break;
    }
  }
  float s1 = 0;
  float s2 = 0;
  for (uint32_t i = 0; i < ids.size(); ++i)
  {
    if (ids[i] == idx1)
    {
      s1 += ((line[0] * hamm[2 * i] + line[1] * hamm[2 * i + 1] + line[2]) * (x * line[0] + y * line[1] + line[2]) >= 0.0) ? 1 : -1;
    }
    else
    {
      s2 += ((line[0] * hamm[2 * i] + line[1] * hamm[2 * i + 1] + line[2]) * (x * line[0] + y * line[1] + line[2]) <= 0.0) ? 1 : -1;
    }
  }
  float s = std::abs(s1) > std::abs(s2) ? s1 : s2;
  if (s < 0.0) {
    std::swap(idx1, idx2);
  }
}

std::vector<uint32_t> rayTraceCPU(std::shared_ptr<RayTracer> pRayTracer, int width, int height, uint32_t add_samples, float radius)
{
  std::vector<SampleInfo> raytracedImageData(width * height);
#pragma omp parallel for default(none) shared(height, width, raytracedImageData, pRayTracer)
  for (int j = 0; j < height; ++j)
  {
    for (int i = 0; i < width; ++i)
    {
      pRayTracer->CastSingleRay(i, j, raytracedImageData.data());
    }
  }

  std::vector<uint32_t> indicesToResample;
  for (int j = 0; j < height; ++j)
  {
    for (int i = 0; i < width; ++i)
    {
      uint32_t currentSample = raytracedImageData[j * width + i].objId;
      bool needResample = false;
      for (int x = std::max(i - 1, 0); x < std::min(i + 1, width) && !needResample; ++x)
      {
        for (int y = std::max(j - 1, 0); y < std::min(j + 1, height) && !needResample; ++y)
        {
          needResample = raytracedImageData[y * width + x].objId != currentSample;
        }
      }
      if (needResample)
      {
        indicesToResample.push_back(j * width + i);
      }
    }
  }
  std::cout << indicesToResample.size() << " pixels require additional samples" << std::endl;
  std::vector<float> hammSamples(add_samples * 2);
  PlaneHammersley(hammSamples.data(), add_samples);
  for (float& offset : hammSamples)
  {
    offset = (offset - 0.5f) * 2.0f * radius;
  }
  std::unordered_map<uint32_t, uint32_t> sampledColors;
  uint32_t complexCount = 0;
  uint32_t failedSplit = 0;
  uint32_t allSplit = 0;
  for (uint32_t pixelToResample: indicesToResample)
  {
    std::unordered_map<uint32_t, uint32_t> samplesStat; // Triangle id mapped to amount of samples for the triangle
    samplesStat[raytracedImageData[pixelToResample].objId] = 1;
    sampledColors[raytracedImageData[pixelToResample].objId] = raytracedImageData[pixelToResample].color;

    std::vector<uint32_t> ids;

    for (uint32_t i = 0; i < add_samples; ++i)
    {
      SampleInfo sample;
      pRayTracer->CastSingleRay(pixelToResample % width, pixelToResample / width, hammSamples[i * 2], hammSamples[i * 2 + 1], &sample);
      samplesStat[sample.objId] += 1;
      sampledColors[sample.objId] = sample.color;
      ids.push_back(sample.objId);
    }

    uint32_t gt_color = 0;
    float gt_red = 0;
    float gt_green = 0;
    float gt_blue = 0;
    for (auto [triId, count] : samplesStat)
    {
      gt_red += float(sampledColors[triId] & 0xFF) / 255.0f * count;
      gt_green += float((sampledColors[triId] >> 8) & 0xFF) / 255.0f * count;
      gt_blue += float((sampledColors[triId] >> 16) & 0xFF) / 255.0f * count;
    }
    gt_red = saturate(gt_red / (add_samples + 1)) * 255.0f;
    gt_green = saturate(gt_green / (add_samples + 1)) * 255.0f;
    gt_blue = saturate(gt_blue / (add_samples + 1)) * 255.0f;
    gt_color = 0xFF000000u | (uint32_t(gt_blue) << 16) | (uint32_t(gt_green) << 8) | uint32_t(gt_red);

    if (std::set(ids.begin(), ids.end()).size() == 2)
    {
      SVM svm;
      std::vector<int> labels(ids.size());
      for (uint32_t i = 0; i < labels.size(); ++i)
        labels[i] = ids[i] == ids[0] ? 1 : -1;
      svm.fit(hammSamples, labels, hammSamples, labels);
      auto weights = svm.get_weights();
      const float a = weights[0];
      const float b = weights[1];
      const float c = weights[2];
      // Pixel corners have positions (+-0.5, +-0.5). SVM has results in the same space.
      const float NO_INTERSECT = -100.f;
      // x = -0.5
      // ax + by + c = 0, y = (-c + 0.5 * a) / b
      float intersection1 = std::abs(b) < 1e-3f ? NO_INTERSECT : (-c + 0.5f * a) / b;
      if (std::abs(intersection1) >= 0.5f)
        intersection1 = NO_INTERSECT;
      // x = 0.5
      // ax + by + c = 0, y = (-c - 0.5 * a) / b
      float intersection2 = std::abs(b) < 1e-3f ? NO_INTERSECT : (-c - 0.5f * a) / b;
      if (std::abs(intersection2) >= 0.5f)
        intersection2 = NO_INTERSECT;
      // y = -0.5
      // ax + by + c = 0, x = (-c + 0.5 * b) / a
      float intersection3 = std::abs(a) < 1e-3f ? NO_INTERSECT : (-c + 0.5f * b) / a;
      if (std::abs(intersection3) >= 0.5f)
        intersection3 = NO_INTERSECT;
      // y = 0.5
      // ax + by + c = 0, x = (-c - 0.5 * b) / a
      float intersection4 = std::abs(a) < 1e-3f ? NO_INTERSECT : (-c - 0.5f * b) / a;
      if (std::abs(intersection4) >= 0.5f)
        intersection4 = NO_INTERSECT;
      
      // assert(
      //   (intersection1 == NO_INTERSECT ? 1 : 0)
      //   + (intersection2 == NO_INTERSECT ? 1 : 0)
      //   + (intersection3 == NO_INTERSECT ? 1 : 0)
      //   + (intersection4 == NO_INTERSECT ? 1 : 0)
      //   == 2 && "Something is wrong. We don't have exactly 2 intersections.");

      bool dump = false;
      if (   (intersection1 == NO_INTERSECT ? 1 : 0)
        + (intersection2 == NO_INTERSECT ? 1 : 0)
        + (intersection3 == NO_INTERSECT ? 1 : 0)
        + (intersection4 == NO_INTERSECT ? 1 : 0)
        != 2)
        dump = true;

      uint32_t idx1 = 0;
      uint32_t idx2 = 0;
      float w1, w2;

      bool debugColor = false;

      if (intersection1 != NO_INTERSECT && intersection2 != NO_INTERSECT)
      {
        get_indices(hammSamples, ids, weights, 0.0f, -0.5f, idx1, idx2);

        w1 = 0.5f * (intersection1 + intersection2) + 0.5f;
        w2 = 1.0f - w1;
      }

      if (intersection1 != NO_INTERSECT && intersection3 != NO_INTERSECT)
      {
        get_indices(hammSamples, ids, weights, -0.5f, -0.5f, idx1, idx2);

        w1 = (intersection1 + 0.5f) * (intersection3 + 0.5f) * 0.5f;
        w2 = 1.0f - w1;
      }

      if (intersection1 != NO_INTERSECT && intersection4 != NO_INTERSECT)
      {
        get_indices(hammSamples, ids, weights, -0.5f, 0.5f, idx1, idx2);

        w1 = (0.5f - intersection1) * (0.5f + intersection4) * 0.5f;
        w2 = 1.0f - w1;
      }

      if (intersection2 != NO_INTERSECT && intersection3 != NO_INTERSECT)
      {
        get_indices(hammSamples, ids, weights, 0.5f, -0.5f, idx1, idx2);

        w1 = (intersection2 + 0.5f) * (0.5f - intersection3) * 0.5f;
        w2 = 1.0f - w1;
      }

      if (intersection2 != NO_INTERSECT && intersection4 != NO_INTERSECT)
      {
        get_indices(hammSamples, ids, weights, 0.5f, 0.5f, idx1, idx2);

        w1 = (0.5f - intersection2) * (0.5f - intersection4) * 0.5f;
        w2 = 1.0f - w1;
      }

      if (intersection3 != NO_INTERSECT && intersection4 != NO_INTERSECT)
      {
        get_indices(hammSamples, ids, weights, -0.5f, 0.0f, idx1, idx2);

        w1 = 0.5f * (intersection3 + intersection4) + 0.5f;
        w2 = 1.0f - w1;
      }

      if (idx1 != idx2)// && !dump)
      {
        float red = (float(sampledColors[idx1] & 0xFF) * w1 + float(sampledColors[idx2] & 0xFF) * w2);
        float green = (float((sampledColors[idx1] >> 8) & 0xFF) * w1 + float((sampledColors[idx2] >> 8) & 0xFF) * w2);
        float blue = (float((sampledColors[idx1] >> 16) & 0xFF) * w1 + float((sampledColors[idx2] >> 16) & 0xFF) * w2);

        raytracedImageData[pixelToResample].color = 0xFF000000u
          | (uint32_t(blue) << 16)
          | (uint32_t(green) << 8)
          | (uint32_t(red) << 0);

        if (std::abs(red - gt_red) + std::abs(green - gt_green) + std::abs(blue - gt_blue) > 10) {
          // std::cout << "x = " << pixelToResample % width << " y = " << pixelToResample / width << std::endl;
          std::cout << "Failed loss " << svm.finalValue << std::endl;
          failedSplit++;
          // assert(false);
          dump = true;
        } else {
        }
        static int z = 0;
        if (dump) {
          std::cout << "Dumped loss " << svm.finalValue << std::endl;
          std::stringstream ss;
          ss << "failed_pixel" << pixelToResample << ".bin";
          std::ofstream out(ss.str(), std::ios::binary | std::ios::out);
          out.write((char*)&add_samples, sizeof(add_samples));
          out.write((char*)hammSamples.data(), sizeof(float) * hammSamples.size());
          out.write((char*)labels.data(), sizeof(labels[0]) * labels.size());
          out.write((char*)weights.data(), sizeof(float) * weights.size());
          out.close();
          // exit(1);
          std::cout << (float)(std::find(indicesToResample.begin(), indicesToResample.end(), pixelToResample) - indicesToResample.begin())
            / indicesToResample.size() * 100.f << "% processed" << std::endl;
        }
        // z++;
      }
      else if (!debugColor)
        raytracedImageData[pixelToResample].color = 0xFF000000u;
      allSplit++;
    }
    else
    {
      complexCount++;

      // Perform antialiasing here
      raytracedImageData[pixelToResample].color = gt_color;
    }
  }

  std::cout << complexCount << " complex pixels (more than 2 triangels)." << std::endl;
  std::cout << failedSplit << " splits failed of " << allSplit << std::endl;

  std::vector<uint32_t> raytracedImageDataOut(width * height, 0u);
  for (uint32_t i = 0; i < raytracedImageData.size(); ++i)
  {
    raytracedImageDataOut[i] = raytracedImageData[i].color;
  }

  return raytracedImageDataOut;
}

int main(int argc, char **argv)
{
  constexpr uint32_t WIDTH = 1024;
  constexpr uint32_t HEIGHT = 1024;

  auto pRayTracerCPU = std::make_shared<RayTracer>(WIDTH, HEIGHT);
  auto loaded = pRayTracerCPU->LoadScene("../01_simple_scenes/instanced_objects.xml");

  if(!loaded)
    return -1;

  float radius = 0.5;
  uint32_t add_samples = 256;
  for (int i = 1; i < argc - 1; ++i)
  {
    if (strcmp("-add_samples", argv[i]) == 0)
    {
      add_samples = std::atoi(argv[i + 1]);
    }
    if (strcmp("-radius", argv[i]) == 0)
    {
      radius = std::atof(argv[i + 1]);
    }
  }

  // auto image = rayTraceCPU(pRayTracerCPU, WIDTH, HEIGHT, add_samples, radius);
  std::vector<uint32_t> image;

  BSPBasedSampler<SampleInfo>::Config config;
  config.width = WIDTH;
  config.height = HEIGHT;
  config.windowHalfSize = 1;
  config.radius = 0.5f;
  config.additionalSamplesCnt = 4;
  BSPBasedSampler<SampleInfo> sampler(config);
  sampler.configure(RTSampler(pRayTracerCPU, WIDTH, HEIGHT));
  const uint32_t aaSamples = 4;
  for (uint32_t j = 0; j < HEIGHT; ++j)
  {
    for (uint32_t i = 0; i < WIDTH; ++i)
    {
      float r = 0, g = 0, b = 0;
      for (uint32_t y = 0; y < aaSamples; ++y)
      {
        for (uint32_t x = 0; x < aaSamples; ++x)
        {
          const float x_coord = (float)(x + i * aaSamples) / (aaSamples * WIDTH);
          const float y_coord = (float)(y + j * aaSamples) / (aaSamples * HEIGHT);
          SampleInfo sample = sampler.sample(x_coord, y_coord);
          r += ((sample.color >> 16) & 0xFF);
          g += ((sample.color >> 8) & 0xFF);
          b += (sample.color & 0xFF);
        }
      }
      r /= aaSamples * aaSamples;
      g /= aaSamples * aaSamples;
      b /= aaSamples * aaSamples;
      image.push_back(0xFF000000 | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)b);
    }
  }

  saveImageLDR("output_antialiased_bsp.png", image, WIDTH, HEIGHT, 4);

  return 0;
}
