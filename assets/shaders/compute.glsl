#version 460 core
layout(local_size_x = 8, local_size_y = 4, local_size_z = 1) in;
layout(rgba32f, binding = 0) uniform image2D screen;
uniform vec2 iResolution;

// Define the maximum number of spheres
#define MAX_SPHERES 2
#define SPHERE_DATA_POINTS 7

struct Sphere {
    vec3 center;
    float radius;
    vec3 color;
};

// Declare the SSBO for sphere data
layout(std430, binding = 1) buffer SpheresBuffer {
    float spheres[MAX_SPHERES][SPHERE_DATA_POINTS];
};


// The minimunm distance a ray must travel before we consider an intersection.
// This is to prevent a ray from intersecting a surface it just bounced off of.
const float c_minimumRayHitTime = 0.001f;

// the farthest we look for ray hits
const float c_superFar = 10000.0f;

struct SRayHitInfo {
    float dist;
    vec3 normal;
};

float ScalarTriple(vec3 u, vec3 v, vec3 w)
{
    return dot(cross(u, v), w);
}

bool SphereHit(in vec3 rayPos, in vec3 rayDir, inout SRayHitInfo info, in Sphere sphere)
{
    // Sphere center and radius
    vec3 center = sphere.center;
    float radius = sphere.radius;

    // Vector from the ray origin to the sphere center
    vec3 oc = rayPos - center;

    // Coefficients for the quadratic equation
    float a = dot(rayDir, rayDir);
    float b = 2.0 * dot(oc, rayDir);
    float c = dot(oc, oc) - radius * radius;

    // Calculate discriminant
    float discriminant = b * b - 4.0 * a * c;

    if (discriminant > 0.0) {
        // Calculate solutions to the quadratic equation
        float t1 = (-b - sqrt(discriminant)) / (2.0 * a);
        float t2 = (-b + sqrt(discriminant)) / (2.0 * a);

        // Check if either solution is within acceptable bounds
        if (t1 < info.dist) {
            info.dist = t1;
            info.normal = normalize(rayPos + t1 * rayDir - center);
            return true;
        } else if (t2 < info.dist) {
            info.dist = t2;
            info.normal = normalize(rayPos + t2 * rayDir - center);
            return true;
        }
    }

    return false;
}

void DestructSphere(in float data[MAX_SPHERES][SPHERE_DATA_POINTS], inout Sphere sphere) {
    sphere.center = vec3(data.data[0], data.data[1], data.data[2]);
    sphere.radius = data.data[3];
    sphere.color = vec3(data.data[4], data.data[5], data.data[6]);
}

vec4 GetColorForRay(in vec3 rayPos, in vec3 rayDir) {
    SRayHitInfo hitInfo;
    hitInfo.dist = c_superFar;

    vec4 ret = vec4(0.0f, 0.0f, 0.0f, 1.0f);

    for (int i = 1; i < MAX_SPHERES; ++i) {
        Sphere sphere;
        DestructSphere(spheres[i], sphere);
        if (SphereHit(rayPos, rayDir, hitInfo, sphere)) {
            ret = vec4(sphere.color, 1.0);
        }
    }

    return ret;
}

void main() {
    ivec2 fragCoord = ivec2(gl_GlobalInvocationID.xy);

    // The ray starts at the camera position (the origin)
    vec3 rayPosition = vec3(0.0f, 0.0f, 0.0f);

    // calculate the camera distance
    float cameraDistance = 1.0f / tan(90.0f * 0.5f * 3.141592f / 180.0f);

    // calculate coordinates of the ray target on the imaginary pixel plane.
    // -1 to +1 on x,y axis. 1 unit away on the z axis
    vec3 rayTarget = vec3(vec2(float(fragCoord.x) / float(iResolution.x), float(fragCoord.y) / float(iResolution.y)) * 2.0f - 1.0f, cameraDistance);

    // correct for aspect ratio
    float aspectRatio = iResolution.x / iResolution.y;
    rayTarget.y /= aspectRatio;

    // calculate a normalized vector for the ray direction.
    // it's pointing from the ray position to the ray target.
    vec3 rayDir = normalize(rayTarget - rayPosition);

    // raytrace for this pixel
    vec4 color = GetColorForRay(rayPosition, rayDir);


    imageStore(screen, fragCoord, color);
}
