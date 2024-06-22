#pragma once
#include <stdint.h>
#include <math.h>
#define MIN_DB 30
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
	if (p < 0.01) return -INFINITY;
	return MIN_DB * p - MIN_DB;
}

static inline float db_to_percent(const float db)
{
	if (isinf((double)db)) return 0;
	if (db <= -MIN_DB) return 0.01f;
	return db / MIN_DB + 1;
}