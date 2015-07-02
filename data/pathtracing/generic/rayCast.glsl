#ifndef RAYCAST
#define RAYCAST

#include </pathtracing/generic/dataTypes.glsl>

#include </PATHTRACING_EXTENSIONS/geometryTraversal>
#include </PATHTRACING_EXTENSIONS/secondOrderRayGeneration>

// required
void checkGeomertryIntersection(in Ray ray, out IntersectionTestResult result);
void generateSecondOrderRay(in Ray lastRay, inout RayIntersection intersection);


bool rayCast(in Ray ray, out RayIntersection intersection)
{
    IntersectionTestResult result;

    checkGeomertryIntersection(ray, result);
    if (!result.hit)
        return false;
    
    intersection.index = result.index;
    intersection.t = result.t;
    intersection.point = ray.origin + ray.direction * result.t;
    intersection.normal = result.normal;
    
    // TODO check pass through etc
    
    generateSecondOrderRay(ray, intersection);
    
    return true;
}


#endif
