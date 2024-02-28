#include <iostream>
#include <vector>
#include <random>
#include "BitonicSort.h"

int main(void)
{
  std::vector<int> xs(10);
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> unf(0, 9);
  for (int i = 0; i < xs.size(); i++)
    xs[i] = unf(rng);
  printf("before sort: ");
  for (int i = 0; i < xs.size(); i++)
    printf("%d ", xs[i]);
  printf("\n");

  BitonicSort bitonic;
  bitonic.Init();
  bitonic.Run(xs.data(), static_cast<UINT>(xs.size()));
  bitonic.Destroy();

  printf("after sort: ");
  for (int i = 0; i < xs.size(); i++)
    printf("%d ", xs[i]);
  printf("\n");
  return 0;
}