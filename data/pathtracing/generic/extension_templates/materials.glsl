#ifndef MATERIALS
#define MATERIALS

vec4 colors[] = vec4[](
    vec4(0.0, 0.0, 0.0, 1.0), // 0 black
    vec4(1.0, 1.0, 1.0, 1.0), // 1 white
    vec4(1.0, 0.0, 0.0, 1.0), // 2 red
    vec4(0.0, 1.0, 0.0, 1.0)  // 3 green
);

vec4 getBackgroundColor(in vec3 viewDirection)
{
    return mix(
        vec4(0.4, 0.5, 0.2, 1),
        vec4(0.6, 0.85, 0.91, 1),
        (smoothstep(-0.1, 0.1, viewDirection.y)));
}

void getMaterial(in RayIntersection intersection, out PathStackData stackData)
{
    stackData.materialID = int(indices[intersection.index].w);
    stackData.BRDF = 1.0;
}

vec4 getColor(in PathStackData stackData)
{
    return colors[stackData.materialID];
}

#endif
