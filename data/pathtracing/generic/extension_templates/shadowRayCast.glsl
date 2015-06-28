#ifndef SHADOWRAYCAST
#define SHADOWRAYCAST

// implicit includes: 
// #include </PATHTRACING_EXTENSIONS/geometryTraversal>

const int NUM_SAMPLES = 10;

vec4 shadowRayCast(in vec3 origin)
{
    uvec4 lightIndices0 = indices[0];    // two light source triangles, see testing data definition
    vec3 light0P1 = vertices[lightIndices0.x].xyz;
    vec3 light0P2 = vertices[lightIndices0.y].xyz;
    vec3 light0P3 = vertices[lightIndices0.z].xyz;
    uvec4 lightIndices1 = indices[1];
    vec3 light1P1 = vertices[lightIndices1.x].xyz;
    vec3 light1P2 = vertices[lightIndices1.y].xyz;
    vec3 light1P3 = vertices[lightIndices1.z].xyz;

    RayIntersection intersection;
    int numLightHits = 0;

    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
        // random barycentric coordinates in light triangle
        float lambda1 = randomFloat();
        float lambda2 = randomFloat();
        float lambda3 = 1 - lambda1 - lambda2;
        
        vec3 pointInLightSource = vec3(
            lambda1 * light0P1.x + lambda2 * light0P2.x + lambda3 * light0P3.x,
            lambda1 * light0P1.y + lambda2 * light0P2.y + lambda3 * light0P3.y,
            lambda1 * light0P1.z + lambda2 * light0P2.z + lambda3 * light0P3.z);

        Ray ray = Ray_fromTo(origin, pointInLightSource);
        
        numLightHits += int(rayCast(ray, intersection) && intersection.index < 2);
    }

    // same for second light triangle
    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
        float lambda1 = randomFloat();
        float lambda2 = randomFloat();
        float lambda3 = 1 - lambda1 - lambda2;
        
        vec3 pointInLightSource = vec3(
            lambda1 * light1P1.x + lambda2 * light1P2.x + lambda3 * light1P3.x,
            lambda1 * light1P1.y + lambda2 * light1P2.y + lambda3 * light1P3.y,
            lambda1 * light1P1.z + lambda2 * light1P2.z + lambda3 * light1P3.z);

        Ray ray = Ray_fromTo(origin, pointInLightSource);
        
        numLightHits += int(rayCast(ray, intersection) && intersection.index < 2);
    }

    vec3 lightColor = vec3(float(numLightHits) / float(2 * NUM_SAMPLES));

    return vec4(lightColor, 1.0);
}

#endif
