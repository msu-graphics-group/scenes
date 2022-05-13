#include "svm.h"

#include <cstring>
#include <cassert>

#include <iostream>
#include <random>
#include <set>

static void add_bias_feature(std::vector<float> &a, uint32_t axis1)
{
  // TODO: avoid allocation of another vector. We can resize existed one.
  const uint32_t pointsCount = a.size() / axis1;
  std::vector<float> extended(pointsCount * (axis1 + 1));
  for (uint32_t i = 0; i < pointsCount; ++i)
  {
    for (uint32_t j = 0; j < axis1; ++j)
      extended[i * (axis1 + 1) + j] = a[i * axis1 + j];
    // memcpy((void*)&extended[i * (axis1 + 1)], (void*)&a[i * axis1], sizeof(float) * axis1);
    extended[i * (axis1 + 1) + axis1] = 1;
  }
  a = extended;
}

static float dot(float *a, float *b, uint32_t count)
{
  float res = 0.f;
  for (uint32_t i = 0; i < count; ++i)
    res += a[i] * b[i];
  return res;
}


void SVM::fit(
    std::vector<float> X_train,
    const std::vector<int> &Y_train,
    std::vector<float> X_val,
    const std::vector<int> &Y_val,
    bool verbose)
{
  if (std::set(Y_train.begin(), Y_train.end()).size() != 2
    || std::set(Y_val.begin(), Y_val.end()).size() != 2)
  {
    std::cout << "Number of classes in Y is not equal 2!" << std::endl;
    return;
  }

  const uint32_t pointDim = X_train.size() / Y_train.size();

  add_bias_feature(X_train, pointDim);
  add_bias_feature(X_val, pointDim);

  std::default_random_engine generator;
  std::normal_distribution<float> distribution(0.0, 0.05);

  assert(weights.size() == pointDim + 1);
  for (uint32_t i = 0; i < weights.size(); ++i)
    weights[i] = distribution(generator);

  // history_w.push_back(weights);

  for (int epoch = 0; epoch < epochs; epoch++)
  {
    float tr_err = 0.f;
    float val_err = 0.f;
    float tr_loss = 0.f;
    float val_loss = 0.f;
    for (uint32_t i = 0; i < Y_train.size(); ++i)
    {
      const uint32_t x_offset = i * (pointDim + 1);
      const float margin = Y_train[i] * dot(weights.data(), X_train.data() + x_offset, pointDim + 1);
      if (margin >= 1.f * 0.0f) // классифицируем верно
      {
        for (uint32_t j = 0; j < weights.size(); ++j)
          weights[j] = weights[j] - etha * alpha * weights[j] / epochs;
        tr_loss += soft_margin_loss(X_train.data() + x_offset, Y_train[i]);
      }
      else // классифицируем неверно или попадаем на полосу разделения при 0<m<1
      {
        for (uint32_t j = 0; j < weights.size(); ++j)
          weights[j] = weights[j] + etha * (Y_train[i] * X_train[x_offset + j] - alpha * weights[j] / epochs);
        tr_err += 1.f;
        tr_loss += soft_margin_loss(X_train.data() + x_offset, Y_train[i]);
      }
      // history_w.push_back(weights);
    }
    for (uint32_t i = 0; i < Y_val.size(); ++i)
    {
      const uint32_t x_offset = i * (pointDim + 1);
      val_loss += soft_margin_loss(X_val.data() + x_offset, Y_val[i]);
      if (Y_val[i] * dot(weights.data(), X_val.data() + x_offset, weights.size()) < 1.f)
        val_err += 1.0f;
    }
    if (verbose)
    {
      std::cout << "epoch " << epoch << ". Errors=" << val_err << ". Mean Hinge_loss=" << val_loss << std::endl;
    }
    // train_errors.push_back(tr_err);
    // val_errors.push_back(val_err);
    // train_loss_epoch.push_back(tr_loss);
    // val_loss_epoch.push_back(val_loss);
  }
  for (uint32_t i = 0; i < Y_train.size(); ++i)
  {
    const uint32_t x_offset = i * (pointDim + 1);
    const float margin = Y_train[i] * dot(weights.data(), X_train.data() + x_offset, pointDim + 1);
    if (margin < 0) {
      finalValue++;
    }
  }
}

float SVM::hinge_loss(float *x, int y)
{
  return std::max(0.f, 1.f - y * dot(x, weights.data(), weights.size()));
}

float SVM::soft_margin_loss(float *x, int y)
{
  return hinge_loss(x, y) + alpha * dot(weights.data(), weights.data(), weights.size());
}


    // def predict(self, X:np.array) -> np.array:
    //     y_pred = []
    //     X_extended = add_bias_feature(X)
    //     for i in range(len(X_extended)):
    //         y_pred.append(np.sign(np.dot(self._w,X_extended[i])))
    //     return np.array(y_pred)      
