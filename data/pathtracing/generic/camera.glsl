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

float Camera_depth(in Camera camera, in float d)
{
	//- znear * depth / (zfar * depth - zfar - znear * depth);
	//~ float a = (d-camera.zNear)/(camera.zFar-camera.zNear);
	float depth = (camera.zFar-camera.zNear*camera.zFar/d)/(camera.zFar-camera.zNear);
	
	return depth;
}

#endif
