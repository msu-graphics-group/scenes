#pragma once

#include <vector>

#include <cstdint>

// This is an implementation of the SVM classification algorithm
// Note that it works only for binary classification

// #############################################################
// ######################   PARAMETERS    ######################
// #############################################################

// etha: float(default - 0.01)
//     Learning rate, gradient step

// alpha: float, (default - 0.1)
//     Regularization parameter in 0.5*alpha*||w||^2

// epochs: int, (default - 200)
//     Number of epochs of training

// #############################################################
// #############################################################
// #############################################################


struct SVM
{
  float etha;
  float alpha;
  int epochs;

  std::vector<float> weights;
  std::vector<std::vector<float>> history_w;

  std::vector<float> train_errors;
  std::vector<float> val_errors;
  std::vector<float> train_loss_epoch;
  std::vector<float> val_loss_epoch;

  float hinge_loss(float *x, int y);
  float soft_margin_loss(float *x, int y);
  uint32_t finalValue = 0;

public:
  SVM(float etha_ = 0.01, float alpha_ = 0.1, int epochs_ = 1000):
  etha(etha_), alpha(alpha_), epochs(epochs_) {}

  void fit(
    std::vector<float> X_train,
    const std::vector<int> &Y_train,
    std::vector<float> X_val,
    const std::vector<int> &Y_val,
    bool verbose = false);

  std::vector<float> get_weights() const { return weights; }

};

