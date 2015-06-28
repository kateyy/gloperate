#ifndef BOX
#define BOX

#include </data/pathtracing/generic/ray.glsl>

struct Box
{
	vec3 bounds[2];
};

struct BoxIntersection
{
	float t_min;
	float t_max;
	int dimension;
};

const int DIMENSION_X = 0;
const int DIMENSION_Y = 1;
const int DIMENSION_Z = 2;


Box Box_new(in vec3 a, in vec3 b)
{
	Box box;
	box.bounds[0] = a;
	box.bounds[1] = b;
	return box;
}

vec3 Box_center(in Box box)
{
	return (box.bounds[0]+box.bounds[1])/2.0;
}

bool Box_intersection(in Box box, in Ray r, in float search_min, in float search_max, out BoxIntersection intersection) 
{
	float tmin, tmax, tymin, tymax, tzmin, tzmax;
	int dimmin, dimmax;
	
	int dimX = int(r.direction.x>=0);
	tmin = (box.bounds[1-dimX].x - r.origin.x) * r.inverseDirection.x;
	tmax = (box.bounds[dimX].x - r.origin.x) * r.inverseDirection.x;
		
	int dimY = int(r.direction.y>=0);
	tymin = (box.bounds[1-dimY].y - r.origin.y)  * r.inverseDirection.y;
	tymax = (box.bounds[dimY].y - r.origin.y)  * r.inverseDirection.y;
	
	if ((tmin > tymax) || (tymin > tmax))
		return false;
	
	
	dimmin = DIMENSION_X;
	dimmax = DIMENSION_X;
	
	if (tymin > tmin)
	{
		tmin = tymin;
		dimmin = DIMENSION_Y;
	}
	
	if (tymax < tmax)
	{
		tmax = tymax;
		dimmax = DIMENSION_Y;
	}
			
	int dimZ = int(r.direction.z>=0);
	tzmin = (box.bounds[1-dimZ].z - r.origin.z)  * r.inverseDirection.z;
	tzmax = (box.bounds[dimZ].z - r.origin.z)  * r.inverseDirection.z;
	
	if ((tmin > tzmax) || (tzmin>tmax))
		return false;
	
	if (tzmin > tmin)
	{
		tmin = tzmin;
		dimmin = DIMENSION_Z;
	}
	
	if (tzmax < tmax)
	{
		tmax = tzmax;
		dimmax = DIMENSION_Z;
	}
	
	intersection.t_min = tmin;
	intersection.t_max = tmax;
	
	intersection.dimension = tmin > 0.0 ? dimmin : dimmax;
	
	return tmin < search_max && tmax > search_min;
}

vec3 Box_normal(in int dimension, in Ray ray)
{
	vec3 normal = vec3(0.0);
	normal[dimension] = -sign(ray.direction[dimension]);
	return normal;
}
  
#endif
