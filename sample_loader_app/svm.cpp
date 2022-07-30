#include "svm.h"

#include <cstring>
#include <cassert>

#include <iostream>
#include <random>
#include <set>

template<uint32_t Length>
inline static float dot(const std::array<float, Length> &a, const std::array<float, Length> &b)
{
  float res = 0.f;
  for (uint32_t i = 0; i < Length; ++i)
    res += a[i] * b[i];
  return res;
}


void SVM::fit(const std::vector<float> &X_train, const std::vector<int> &Y_train)
{
  if (std::set(Y_train.begin(), Y_train.end()).size() != 2)
  {
    std::cout << "Number of classes in Y is not equal 2!" << std::endl;
    return;
  }

  const uint32_t pointDim = X_train.size() / Y_train.size();
  std::vector<std::array<float, ARRAY_SIZE>> X_mod(Y_train.size());
  for (uint32_t i = 0; i < X_mod.size(); ++i)
  {
    for (uint32_t j = 0; j < ARRAY_SIZE - 1; ++j)
      X_mod[i][j] = X_train[i * (ARRAY_SIZE - 1) + j] * Y_train[i];
    X_mod[i].back() = Y_train[i];
  }

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
      const float margin = dot<ARRAY_SIZE>(weights, X_mod[i]);
      for (uint32_t j = 0; j < weights.size(); ++j)
        weights[j] *= weightMult;
      if (margin < 0.0f) // классифицируем верно
      {
        for (uint32_t j = 0; j < weights.size(); ++j)
          weights[j] += etha * X_mod[i][j];
        errors++;
      }
    }
    if (errors == 0)
      break;
  }
}
