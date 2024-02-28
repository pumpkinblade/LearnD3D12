#pragma once

#include <random>

class Random
{
public:
	static void Init()
	{
		sRandomEngine.seed(std::random_device()());
	}

	static float Float()
	{
		return (float)sDistribution(sRandomEngine) / (float)0xFFFFFFFF;
	}

private:
	static std::mt19937 sRandomEngine;
	static std::uniform_int_distribution<std::mt19937::result_type> sDistribution;
};