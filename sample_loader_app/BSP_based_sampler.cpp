#include "BSP_based_sampler.h"

std::vector<float> gen_hammersley(uint32_t cnt, float radius)
{
  std::vector<float> values(cnt * 2);
  for (uint32_t k = 0; k < cnt; k++)
  {
    float u = 0;
    int kk = k;

    for (float p = 0.5f; kk; p *= 0.5f, kk >>= 1)
      if (kk & 1)
        u += p;

    float v = (k + 0.5f) / cnt;

    values[2 * k + 0] = u;
    values[2 * k + 1] = v;
  }
  for (float& v : values)
  {
    v = (v - 0.5f) * 2.0f * radius;
  }
  return values;
}

