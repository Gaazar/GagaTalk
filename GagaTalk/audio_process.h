#pragma once
#include <stdint.h>
#include <math.h>
static inline float mul_to_db(const float mul)
{
	return (mul == 0.0f) ? -INFINITY : (20.0f * log10f(mul));
}
static inline float db_to_mul(const float db)
{
	return isfinite((double)db) ? powf(10.0f, db / 20.0f) : 0.0f;
}
static inline float percent_to_db(const float p)
{
	//0.01	= -60
	//1		=  0
	if (p < 0) return -INFINITY;
	return mul_to_db(p * p * p * p);
}