#include <iostream>
#include <vector>
#include "SimpleCompute.h"

int main(void)
{
  std::vector<float> in1(1 << 26);
  std::vector<float> in2(1 << 26);
  std::vector<float> out(1 << 26);
  float alpha = 5;
  for (int i = 0; i < out.size(); i++)
  {
    in1[i] = 1.f;
    in2[i] = 10.f;
  }

  SimpleCompute compute;
  compute.Init();
  compute.Run(out.data(), in1.data(), in2.data(), alpha, static_cast<UINT>(out.size()));
  compute.Destroy();

  for (int i = 0; i < 16; i++)
    printf("%f ", out[i]);
  printf("\n");
  return 0;
}