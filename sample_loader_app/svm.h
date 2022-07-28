#pragma once

#include <array>
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

  std::array<float, 3> weights;
public:
  SVM(float etha_ = 0.01, float alpha_ = 0.1, int epochs_ = 1000):
  etha(etha_), alpha(alpha_), epochs(epochs_) {}

  void fit(const std::vector<float> &X_train, const std::vector<int> &Y_train);

  static constexpr int ARRAY_SIZE = 3;
  std::array<float, ARRAY_SIZE> get_weights() const { return weights; }

};

