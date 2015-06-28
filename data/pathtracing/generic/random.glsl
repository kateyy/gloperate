#ifndef RANDOM
#define RANDOM

#include </data/pathtracing/generic/hash.glsl>

layout (location = 10) uniform vec2 randomOffset;
layout (binding = 2) uniform isampler1D randomIndexTexture;
layout (binding = 3) uniform sampler2D randomVectorTexture;

float pseudoRand(vec2 co)
{
	return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float randomFloat(float f)
{
	const uint mantissaMask	= 0x007FFFFFu;
	const uint one		= 0x3F800000u;

	uint h = hash(floatBitsToUint(f));
	h &= mantissaMask;
	h |= one;

	float  r2 = uintBitsToFloat(h);
	float n = r2 - 1.0;
	
	return n;
}

float randomFloat2(float f)
{
	return randomFloat(f)*2.0-1.0;
}

vec2 randomVec2(vec2 f)
{
	return vec2(randomFloat2(f.x), randomFloat2(f.y));
}

vec3 randomVec3(vec3 f)
{
	return vec3(randomFloat2(f.x), randomFloat2(f.y), randomFloat2(f.z));
}

struct Random
{
	vec2 index;
	vec3 vector;
	int t;
};

Random random;

void random_seed(in vec2 normalizedCoords, in int t)
{
	float p = pseudoRand(normalizedCoords);
	int i = texture(randomIndexTexture, t).x;

	vec2 lookup = randomVec2(normalizedCoords) + randomOffset;
		
	int x = texture(randomIndexTexture, lookup.x).x;
	int y = texture(randomIndexTexture, lookup.y).x;
	
	random.t = t;
	random.index = randomVec2(vec2(ivec2(x, y)))*float(i)+p;
}

vec3 randomDirection()
{	
	random.index = randomVec2(random.index);
	
	return normalize(texture(randomVectorTexture, random.index).xyz);
}

vec3 randomDirectionInHemisphere(in vec3 normal)
{	
	vec3 r = randomDirection();
	if (dot(r, normal)<0.0)
		r = -r;
		
	return r;
}

float randomFloat()
{
	vec3 r = randomDirection();
	return randomFloat(r.x)*randomFloat(r.y)*randomFloat(r.z);
}

#endif
