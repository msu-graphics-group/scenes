#include <vector>
#include "render/raytracing.h"
#include "render/image_save.h"

#include "svm.h"
#include <set>
#include <cassert>

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
      // Pixel corners have positions (+-1, +-1). SVM has results in the same space.
      const float NO_INTERSECT = -100.f;
      // x = -1
      // ax + by + c = 0, y = (-c + a) / b
      float intersection1 = std::abs(b) < 1e-3f ? NO_INTERSECT : (-c + a) / b;
      if (std::abs(intersection1) >= 1.f)
        intersection1 = NO_INTERSECT;
      // x = 1
      // ax + by + c = 0, y = (-c - a) / b
      float intersection2 = std::abs(b) < 1e-3f ? NO_INTERSECT : (-c - a) / b;
      if (std::abs(intersection2) >= 1.f)
        intersection2 = NO_INTERSECT;
      // y = -1
      // ax + by + c = 0, x = (-c + b) / a
      float intersection3 = std::abs(a) < 1e-3f ? NO_INTERSECT : (-c + b) / a;
      if (std::abs(intersection3) >= 1.f)
        intersection3 = NO_INTERSECT;
      // y = 1
      // ax + by + c = 0, x = (-c - b) / a
      float intersection4 = std::abs(a) < 1e-3f ? NO_INTERSECT : (-c - b) / a;
      if (std::abs(intersection4) >= 1.f)
        intersection4 = NO_INTERSECT;
      
      assert(
        (intersection1 == NO_INTERSECT ? 1 : 0)
        + (intersection2 == NO_INTERSECT ? 1 : 0)
        + (intersection3 == NO_INTERSECT ? 1 : 0)
        + (intersection4 == NO_INTERSECT ? 1 : 0)
        == 2 && "Something is wrong. We don't have exactly 2 intersections.");

      uint32_t idx1 = 0;
      uint32_t idx2 = 0;
      float w1, w2;

      bool debugColor = false;

      if (intersection1 != NO_INTERSECT && intersection2 != NO_INTERSECT)
      {
        for (uint32_t i = 0; i < ids.size(); ++i)
        {
          if ((a * hammSamples[2 * i] + b * hammSamples[2 * i + 1] + c) * (-a + c) >= 0.0)
          {
            idx1 = ids[i];
            break;
          }
        }
        for (uint32_t i : ids)
        {
          if (i != idx1)
          {
            idx2 = i;
            break;
          }
        }

        w1 = (0.5f * (intersection1 + intersection2) + 1.f) * 0.5f;
        w2 = 1.0f - w1;
      }

      if (intersection1 != NO_INTERSECT && intersection3 != NO_INTERSECT)
      {
        for (uint32_t i = 0; i < ids.size(); ++i)
        {
          if ((a * hammSamples[2 * i] + b * hammSamples[2 * i + 1] + c) * (-a - b + c) >= 0.0)
          {
            idx1 = ids[i];
            break;
          }
        }
        for (uint32_t i : ids)
        {
          if (i != idx1)
          {
            idx2 = i;
            break;
          }
        }

        w1 = (intersection1 + 1.f) * (intersection3 + 1.f) * 0.5f * 0.25f;
        w2 = 1.0f - w1;
      }

      if (intersection1 != NO_INTERSECT && intersection4 != NO_INTERSECT)
      {
        for (uint32_t i = 0; i < ids.size(); ++i)
        {
          if ((a * hammSamples[2 * i] + b * hammSamples[2 * i + 1] + c) * (-a + b + c) >= 0.0)
          {
            idx1 = ids[i];
            break;
          }
        }
        for (uint32_t i : ids)
        {
          if (i != idx1)
          {
            idx2 = i;
            break;
          }
        }

        w1 = (intersection1 + 1.f) * (1.f - intersection4) * 0.5f * 0.25f;
        w2 = 1.0f - w1;
      }

      if (intersection2 != NO_INTERSECT && intersection3 != NO_INTERSECT)
      {
        for (uint32_t i = 0; i < ids.size(); ++i)
        {
          if ((a * hammSamples[2 * i] + b * hammSamples[2 * i + 1] + c) * (a - b + c) >= 0.0)
          {
            idx1 = ids[i];
            break;
          }
        }
        for (uint32_t i : ids)
        {
          if (i != idx1)
          {
            idx2 = i;
            break;
          }
        }

        w1 = (1.f - intersection2) * (intersection3 + 1.f) * 0.5f * 0.25f;
        w2 = 1.0f - w1;
      }

      if (intersection2 != NO_INTERSECT && intersection4 != NO_INTERSECT)
      {
        for (uint32_t i = 0; i < ids.size(); ++i)
        {
          if ((a * hammSamples[2 * i] + b * hammSamples[2 * i + 1] + c) * (a + b + c) >= 0.0)
          {
            idx1 = ids[i];
            break;
          }
        }
        for (uint32_t i : ids)
        {
          if (i != idx1)
          {
            idx2 = i;
            break;
          }
        }

        w1 = (1.f - intersection2) * (1.f - intersection4) * 0.5f * 0.25f;
        w2 = 1.0f - w1;
      }

      if (intersection3 != NO_INTERSECT && intersection4 != NO_INTERSECT)
      {
        for (uint32_t i = 0; i < ids.size(); ++i)
        {
          if ((a * hammSamples[2 * i] + b * hammSamples[2 * i + 1] + c) * (-b + c) >= 0.0)
          {
            idx1 = ids[i];
            break;
          }
        }
        for (uint32_t i : ids)
        {
          if (i != idx1)
          {
            idx2 = i;
            break;
          }
        }

        w1 = (0.5f * (intersection3 + intersection4) + 1.f) * 0.5f;
        w2 = 1.0f - w1;
      }

      if (idx1 != idx2)
      {
        float red = (float(sampledColors[idx1] & 0xFF) * w1 + float(sampledColors[idx2] & 0xFF) * w2);
        float green = (float((sampledColors[idx1] >> 8) & 0xFF) * w1 + float((sampledColors[idx2] >> 8) & 0xFF) * w2);
        float blue = (float((sampledColors[idx1] >> 16) & 0xFF) * w1 + float((sampledColors[idx2] >> 16) & 0xFF) * w2);

        raytracedImageData[pixelToResample].color = 0xFF000000u
          | (uint32_t(blue) << 16)
          | (uint32_t(green) << 8)
          | (uint32_t(red) << 0);
      }
      else if (!debugColor)
        raytracedImageData[pixelToResample].color = 0xFF000000u;
    }
    else
    {
      complexCount++;

      // Perform antialiasing here
      float red = 0;
      float green = 0;
      float blue = 0;
      for (auto [triId, count] : samplesStat)
      {
        red += float(sampledColors[triId] & 0xFF) / 255.0f * count;
        green += float((sampledColors[triId] >> 8) & 0xFF) / 255.0f * count;
        blue += float((sampledColors[triId] >> 16) & 0xFF) / 255.0f * count;
      }
      red = saturate(red / (add_samples + 1));
      green = saturate(green / (add_samples + 1));
      blue = saturate(blue / (add_samples + 1));
      raytracedImageData[pixelToResample].color = 0xFF000000u | (uint32_t(blue * 255.0f) << 16) | (uint32_t(green * 255.0f) << 8) | uint32_t(red * 255.0f);
    }
  }

  std::cout << complexCount << " complex pixels (more than 2 triangels)." << std::endl;

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
  uint32_t add_samples = 4;
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

  auto image = rayTraceCPU(pRayTracerCPU, WIDTH, HEIGHT, add_samples, radius);

  saveImageLDR("output_antialiased_svm2.png", image, WIDTH, HEIGHT, 4);

  return 0;
}
