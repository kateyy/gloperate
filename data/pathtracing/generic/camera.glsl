#ifndef CAMERA
#define CAMERA

#include </data/pathtracing/generic/ray.glsl>

struct Camera
{
	vec3 eye;
	mat3 rayMatrix;
	float zNear;
	float zFar;
};

Camera Camera_new(in vec3 eye, in mat3 rayMatrix, in float zNear, in float zFar)
{
	Camera camera;
	camera.eye = eye;
	camera.rayMatrix = rayMatrix;
	camera.zNear = zNear;
	camera.zFar = zFar;
	
	return camera;
}

Camera Camera_new(in vec3 eye, in vec3 center, in vec3 up, in float fovy, in float aspect, in float zNear, in float zFar)
{
	vec3 eyeDir = normalize(center-eye);
	vec3 side = normalize(cross(eyeDir, up));
	vec3 correctUp = normalize(cross(side, eyeDir));
	
	float l = 1.0;
	float h = 2.0*l*tan(fovy/2.0);
	float w = h*aspect;
	
	mat3 rayMatrix = mat3(side * w / 2.0, correctUp * h / 2.0, eyeDir * l);
	
	return Camera_new(eye, rayMatrix, zNear, zFar);
}

// normalizedCoords in [0..1]
Ray Camera_ray(in Camera camera, in vec2 normalizedCoords)
{
	vec2 camCoords = normalizedCoords*2.0-1.0; // in [-1..1]

	return  Ray_new(
		camera.eye, 
		camera.rayMatrix * vec3(camCoords, 1.0)
	);
}

float linearize(float zNear, float zFar, float depth) {
    // d = (2.0 * zfar * znear / (zfar + znear - (zfar - znear) * (2.0 * z- 1.0)));
    // normalized to [0,1]
    // d = (d - znear) / (zfar - znear);

    // simplyfied with wolfram alpha
    return - zNear * depth / (zFar * depth - zFar - zNear * depth);
}

float Camera_depth(in Camera camera, in float dist)
{
	//- znear * depth / (zfar * depth - zfar - znear * depth);
	//~ float a = (d-camera.zNear)/(camera.zFar-camera.zNear);
	// float depth = (camera.zFar-camera.zNear*camera.zFar/d)/(camera.zFar-camera.zNear);
    
    float depth = (dist - camera.zNear) / (camera.zFar - camera.zNear);
    return depth;
    // return linearize(camera.zNear, camera.zFar, depth);
}

#endif
