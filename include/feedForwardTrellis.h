
#ifndef FEEDFORWARDTRELLIS_H
#define FEEDFORWARDTRELLIS_Hs

#include <vector>
#include "mla_types.h"

struct FeedForwardTrellis{
  FeedForwardTrellis(int k, int n, int v, std::vector<int> poly);
  FeedForwardTrellis(CodeInformation code);
  // ~FeedForwardTrellis(); 

  std::vector<int> encode(const std::vector<int>& message);

  private:
    int k_;
    int n_;
    int numInputSymbols_;
    int numOutputSymbols_;
    int numStates_;
    int code_rate_;
    std::vector<int> polynomials_;  // generator polynomial in octal
    std::vector<std::vector<int>> nextStates_;
    std::vector<std::vector<int>> output_;

    void computeNextStates();
    void computeOutput();
};

#endif