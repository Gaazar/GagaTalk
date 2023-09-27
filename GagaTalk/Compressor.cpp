#include "IAudioFrameProcessor.h"


Compressor::Compressor(int samplerate)
{
	this->samplerate = samplerate;
	attack_gain = gain_coefficient(attack_ms, samplerate);
	release_gain = gain_coefficient(release_ms, samplerate);
}
float Compressor::Ratio(float r)
{
	if (r > 1)
	{
		ratio = r;
		slope = 1.0f - (1.0f / ratio);
	}
	return ratio;
}
float Compressor::Ratio()
{
	return ratio;
}
float Compressor::Threshold(float t)
{
	if (t < 0)
		threshold = t;
	return threshold;
}
float Compressor::Threshold()
{
	return threshold;
}
float Compressor::Gain(float g)
{
	gain = db_to_mul(g);
	return g;
}
float Compressor::Gain()
{
	return mul_to_db(gain);
}
float Compressor::AttackTime(float a)
{
	if (a > 0)
	{
		attack_ms = a;
		attack_gain = gain_coefficient(a, samplerate);
	}
	return attack_ms;
}
float Compressor::AttackTime()
{
	return attack_ms;
}
float Compressor::ReleaseTime(float a)
{
	if (a > 0)
	{
		release_ms = a;
		release_ms = gain_coefficient(a, samplerate);
	}
	return release_ms;
}
float Compressor::ReleaseTime()
{
	return release_ms;
}
void Compressor::Process(AudioFrame& f)
{
	float env = envelope;
	for (int i = 0; i < f.nSamples; i++)
	{
		float env_in = 0.f;
		for (int n = 0; n < f.nChannel; n++)
		{
			env_in = fmaxf(env_in, fabsf(f.samples[i * f.nChannel + n]));
		}
		if (env < env_in) {
			env = env_in + attack_gain * (env - env_in);
		}
		else {
			env = env_in + release_gain * (env - env_in);
		}
		for (int n = 0; n < f.nChannel; n++)
		{
			const float env_db = mul_to_db(env);
			float g = (threshold - env_db) * slope;
			g = db_to_mul(fminf(0, gain));
			f.samples[i * f.nChannel + n] *= g * gain;
		}
	}
	envelope = env;
}