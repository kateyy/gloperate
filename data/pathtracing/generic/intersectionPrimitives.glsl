#ifndef INTERSECTIONS
#define INTERSECTIONS


// source: https://raw.githubusercontent.com/cgcostume/pathgl/master/trace.frag

const float EPSILON  = 1e-6;
const float INFINITY = 1e+4;

bool intersectionTriangle(
	const in vec3  triangle[3]
,	const in vec3  origin
,	const in vec3  ray
,	const in float tMax
,   out float t
,   out bool hitBackFace
,   in bool backFaceCulling)
{
	vec3 e0 = triangle[1] - triangle[0];
	vec3 e1 = triangle[2] - triangle[0];

	vec3  h = cross(ray, e1);
	float a = dot(e0, h);

    hitBackFace = a < 0;

    if (a < EPSILON &&
        (a > -EPSILON || backFaceCulling))  // optional back face culling
        return false;

	float f = 1.0 / a;

	vec3  s = origin - triangle[0];
	float u = f * dot(s, h);

	if(u < 0.0 || u > 1.0)
		return false;

	vec3  q = cross(s, e0);
	float v = f * dot(ray, q);

	if(v < 0.0 || u + v > 1.0)
		return false;

	t = f * dot(e1, q);

	if (t < EPSILON)
		return false;

	return (t > 0.0) && (t < tMax);
}

bool intersectionPlane(
    const in vec4  plane
,   const in vec3  origin
,   const in vec3  ray
,   const in float tMax
,   out float t)
{
    t = -(dot(plane.xyz, origin) + plane.w) / dot(plane.xyz, ray);
    return (t > 0.0) && (t < tMax);
}

// ray (direction) must be normalized!
bool intersectionSphere(
    const in vec3  sphereCenter
,   const in float radius2
,   const in vec3  origin
,   const in vec3  ray
,   const in float tMax
,   out float t)
{
    bool  r = false;
    vec3  d = origin - sphereCenter;  // distance

    float b = dot(ray, d);
    float c = dot(d, d) - radius2;

    t = b * b - c;

    if(t > 0.0)
    {
        t = -b - sqrt(t);
        r = (t > 0.0) && (t < tMax);
    }
    return r;
}

// ray (direction) must be normalized!
bool intersectionSphere(
    const in vec4  sphere
,   const in vec3  origin
,   const in vec3  ray
,   const in float tMax
,   out float t)
{
    return intersectionSphere(sphere.xyz, sphere.w*sphere.w, origin, ray, tMax, t);
}

vec3 triangleNormal(const in vec3 triangle[3])
{
	vec3 e0 = triangle[1] - triangle[0];
	vec3 e1 = triangle[2] - triangle[0];

	return normalize(cross(e0, e1));
}

// retrieve normal of triangle, and provide tangentspace
vec3 triangleNormal(
	const in vec3 triangle[3]
,	out mat3 tangentspace)
{
	vec3 e0 = triangle[1] - triangle[0];
	vec3 e1 = triangle[2] - triangle[0];

	// hemisphere samplepoints is oriented up

	tangentspace[0] = normalize(e0);
	tangentspace[1] = normalize(cross(e0, e1));
	tangentspace[2] = cross(tangentspace[1], tangentspace[0]);

	return tangentspace[1];
}

#endif // INTERSECTIONS
