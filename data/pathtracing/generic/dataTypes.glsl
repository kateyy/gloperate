#ifndef DATATYPES
#define DATATYPES

#include </pathtracing/generic/ray.glsl>

uint pixelCoordsToIndex(in ivec2 coords, in ivec2 size)
{
    return uint(coords.y * size.x + coords.x);
}

struct IntersectionTestResult
{
    bool hit;
    int index;      // index used for geometry/material lookup
    float t;        // distance on the path of the ray (point = origin + t * direction)
    vec3 normal;    // normal on the intersected surface
};

struct RayIntersection
{
	int index;
    float t;
	vec3 point;
	vec3 normal;

	Ray nextRay;
};

struct PathStackData
{
    int materialID;     // not hit, if < 0
    float BRDF;
    int aux0;
    int aux1;
    vec4 lightColor;    // is view direction, if not hit
};

struct SecondOrderRay
{
	ivec2 pixel;
	vec2 _padding1;
	vec3 origin;
	int _padding2;
	vec3 direction;
	int _padding3;
};

#endif
