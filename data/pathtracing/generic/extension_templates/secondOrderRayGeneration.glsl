#ifndef SECONDORDERRAYGENERATION
#define SECONDORDERRAYGENERATION

#include </data/pathtracing/generic/dataTypes.glsl>
#include </data/pathtracing/generic/random.glsl>


void generateSecondOrderRay(in Ray lastRay, inout RayIntersection intersection)
{
    vec3 direction = randomDirectionInHemisphere(intersection.normal);
    
    intersection.nextRay.origin = intersection.point;
    intersection.nextRay.direction = direction;

    Ray_addEpsilonOffset(intersection.nextRay, intersection.normal);
}


#endif
