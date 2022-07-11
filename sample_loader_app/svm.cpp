#include "svm.h"

#include <cstring>
#include <cassert>

#include <iostream>
#include <random>
#include <set>

template<uint32_t Length>
inline static float dot(const std::array<float, Length> &a, const float *b)
{
  float res = 0.f;
  for (uint32_t i = 0; i < Length - 1; ++i)
    res += a[i] * b[i];
  return res + a.back();
}


void SVM::fit(const std::vector<float> &X_train, const std::vector<int> &Y_train)
{
  if (std::set(Y_train.begin(), Y_train.end()).size() != 2)
  {
    std::cout << "Number of classes in Y is not equal 2!" << std::endl;
    return;
  }

  const uint32_t pointDim = X_train.size() / Y_train.size();

  std::default_random_engine generator;
  std::normal_distribution<float> distribution(0.0, 0.05f);

  assert(weights.size() == pointDim + 1);
  for (uint32_t i = 0; i < weights.size(); ++i)
    weights[i] = distribution(generator);

  const float weightMult = 1.0f - etha * alpha / epochs;
  for (int epoch = 0; epoch < epochs; epoch++)
  {
    uint32_t errors = 0;
    for (uint32_t i = 0, x_offset = 0; i < Y_train.size(); ++i, x_offset += pointDim)
    {
      const float margin = Y_train[i] * dot<ARRAY_SIZE>(weights, X_train.data() + x_offset);       
      if (margin >= 0.0f) // классифицируем верно
      {
        for (uint32_t j = 0; j < weights.size(); ++j)
          weights[j] *= weightMult;
      }
      else // классифицируем неверно или попадаем на полосу разделения при 0<m<1
      {
        const float scaledLabel = etha * Y_train[i];
        for (uint32_t j = 0; j < weights.size() - 1; ++j)
          weights[j] = weights[j] * weightMult + scaledLabel * X_train[x_offset + j];
        weights.back() = weights.back() * weightMult + scaledLabel;
        errors++;
      }
    }
    if (errors == 0)
      break;
  }
}
