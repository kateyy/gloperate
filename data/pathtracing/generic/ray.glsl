#ifndef RAY
#define RAY


const float epsilon = 0.0001;

struct Ray
{
    vec3 origin;
    vec3 direction;
    vec3 inverseDirection;
};


Ray Ray_new(in vec3 origin, in vec3 direction)
{
    Ray ray;
    ray.origin = origin;
    //~ ray.direction = normalize(direction);
    ray.direction = direction;
    ray.inverseDirection = vec3(1.0)/ray.direction;
    return ray;
}

Ray Ray_fromTo(in vec3 origin, in vec3 destination)
{
    return Ray_new(origin, destination - origin);
}

void Ray_addEpsilonOffset(inout Ray ray, in vec3 normal)
{
    ray.origin += ray.direction * (epsilon / dot(normal, ray.direction));
}

void Ray_setDirection(inout Ray ray, in vec3 direction)
{
    ray.direction = direction;
    ray.inverseDirection = vec3(1.0)/ray.direction;
}

#endif
