#version 430 core
layout(local_size_x = 8, local_size_y = 4, local_size_z = 1) in;
layout(rgba32f, binding = 0) uniform image2D screen;
layout(rgba32f, binding = 1) uniform image2D imgAccumulation;
uniform ivec2 iResolution;
uniform float c_FOVDegrees;
uniform uint c_numBounces;
uniform float c_focalLength;
uniform uint frameCounter;
uniform uint samplesPerPixel;
uniform uint numOfSpheres;

// The minimum distance a ray must travel before we consider an intersection.
const float c_minimumRayHitTime = 0.1f;

// The farthest we look for ray hits
const float c_superFar = 10000.0f;

// Define the value for ray position normal nudge
const float c_rayPosNormalNudge = 0.001f;

// Pi
const float c_pi = 3.141592;
const float c_twopi = 6.283185;

uint wang_hash(inout uint seed) {
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

float RandomFloat(inout uint state) {
    return float(wang_hash(state)) / 4294967296.0;
}

vec3 randomInUnitSphere(inout uint state) {
    while (true) {
        vec3 p = vec3(
        RandomFloat(state) * 2.0 - 1.0,
        RandomFloat(state) * 2.0 - 1.0,
        RandomFloat(state) * 2.0 - 1.0
        );
        if (dot(p, p) < 1.0) {
            return p;
        }
    }
}

vec3 randomUnitVector(inout uint state) {
    return normalize(randomInUnitSphere(state));
}

vec3 randomOnHemisphere(vec3 normal, inout uint state) {
    vec3 onUnitSphere = randomUnitVector(state);
    if (dot(onUnitSphere, normal) > 0.0) {
        return onUnitSphere;
    } else {
        return -onUnitSphere;
    }
}

struct Ray {
    vec3 origin;
    vec3 direction;
};

// Function to get the point at a distance t along the Ray
vec3 getRayPointAt(Ray r, float t) {
    return r.origin + t * r.direction;
}

struct HitRecord {
    vec3 p;
    vec3 normal;
    float t;
    bool frontFace;
    vec3 color;
};

void setFaceNormal(in Ray r, in vec3 outwardNormal, inout HitRecord rec) {
    // Sets the hit record normal vector.
    rec.frontFace = dot(r.direction, outwardNormal) < 0;
    rec.normal = rec.frontFace ? outwardNormal : -outwardNormal;
}

struct Sphere {
    vec3 center;
    float radius;
    vec3 color;
};

layout(std430, binding = 0) buffer Spheres {
    Sphere spheres[];
};

struct Interval {
    float min;
    float max;
};

float getIntervalSize(in Interval interval) {
    return interval.max - interval.min;
}

bool intervalContains(in Interval interval, in float x) {
    return interval.min <= x && x <= interval.max;
}

bool intervalSurrounds(in Interval interval, in float x) {
    return interval.min < x && interval.max > x;
}

float ScalarTriple(vec3 u, vec3 v, vec3 w) {
    return dot(cross(u, v), w);
}

bool hitSphere(in Ray ray, inout Interval interval, inout HitRecord rec, in Sphere sphere) {
    vec3 oc = sphere.center - ray.origin;
    float a = dot(ray.direction, ray.direction);
    float h = dot(ray.direction, oc);
    float c = dot(oc, oc) - sphere.radius * sphere.radius;
    float discriminant = h * h - a * c;

    if (discriminant > 0) {
        float sqrtd = sqrt(discriminant);
        float root = (h - sqrtd) / a;
        if (intervalSurrounds(interval, root)) {
            rec.t = root;
            rec.p = getRayPointAt(ray, rec.t);
            vec3 outwardNormal = (rec.p - sphere.center) / sphere.radius;
            setFaceNormal(ray, outwardNormal, rec);
            rec.color = sphere.color;
            return true;
        }
        root = (h + sqrtd) / a;
        if (intervalSurrounds(interval, root)) {
            rec.t = root;
            rec.p = getRayPointAt(ray, rec.t);
            vec3 outwardNormal = (rec.p - sphere.center) / sphere.radius;
            setFaceNormal(ray, outwardNormal, rec);
            rec.color = sphere.color;
            return true;
        }
    }
    return false;
}

bool TestSceneTrace(in Ray ray, inout HitRecord hitRecord) {
    bool hitAnything = false;
    Interval interval = Interval(c_minimumRayHitTime, c_superFar);

    for (uint i = 0; i < numOfSpheres; ++i) {
        if (hitSphere(ray, interval, hitRecord, spheres[i])) {
            hitAnything = true;
            interval.max = hitRecord.t;
        }
    }

    return hitAnything;
}

vec3 GetColorForRay(in Ray ray, inout uint rngState) {
    vec3 accumulatedColor = vec3(0.0);
    vec3 throughput = vec3(1.0);
    for (uint bounce = 0; bounce < c_numBounces; ++bounce) {
        HitRecord rec;
        if (TestSceneTrace(ray, rec)) {
            vec3 targetDirection = randomOnHemisphere(rec.normal, rngState);
            ray = Ray(rec.p + c_rayPosNormalNudge * rec.normal, targetDirection);
            throughput *= 0.5; // Adjust this factor as needed
        } else {
            vec3 unitDirection = normalize(ray.direction);
            float t = 0.5 * (unitDirection.y + 1.0);
            vec3 skyColor = (1.0 - t) * vec3(1.0, 1.0, 1.0) + t * vec3(0.5, 0.7, 1.0);
            accumulatedColor += throughput * skyColor;
            break;
        }
    }
    return accumulatedColor;
}

Ray getRay(in ivec2 fragCoord, in vec3 viewportUpperLeft, in vec3 pixelDeltaU, in vec3 pixelDeltaV, inout uint rngState) {
    vec2 offset = vec2(RandomFloat(rngState), RandomFloat(rngState));
    vec3 pixelSample = viewportUpperLeft
    + ((fragCoord.x + offset.x) * pixelDeltaU)
    + ((fragCoord.y + offset.y) * pixelDeltaV);

    vec3 rayOrigin = vec3(0.0f, 0.0f, 0.0f);
    vec3 rayDirection = normalize(pixelSample - rayOrigin);

    return Ray(rayOrigin, rayDirection);
}

void main() {
    ivec2 fragCoord = ivec2(gl_GlobalInvocationID.xy);

    // initialize a random number state based on frag coord and frame
    uint rngState = uint(uint(fragCoord.x) * uint(1973) + uint(fragCoord.y) * uint(9277) + uint(frameCounter) * uint(26699)) | uint(1);

    // Calculate camera distance
    float cameraDistance = 1.0f / tan(c_FOVDegrees * 0.5f * c_pi / 180.0f);

    // camera stuff
    float aspectRatio = float(iResolution.x) / float(iResolution.y);
    float viewportHeight = 2.0;
    float viewportWidth = aspectRatio * viewportHeight;

    vec3 viewportU = vec3(viewportWidth, 0.0, 0.0);
    vec3 viewportV = vec3(0.0, -viewportHeight, 0.0);

    vec3 pixelDeltaU = viewportU / float(iResolution.x);
    vec3 pixelDeltaV = viewportV / float(iResolution.y);

    vec3 viewportUpperLeft = vec3(0.0, 0.0, 0.0) - vec3(0.0, 0.0, c_focalLength) - viewportU * 0.5 - viewportV * 0.5;

    // Accumulated color from previous frames
    vec4 accumulatedColor = imageLoad(imgAccumulation, fragCoord);
    vec4 newColor = vec4(0.0);

    // Get color for current ray
    for (uint sampl = 0; sampl < samplesPerPixel; sampl++) {
        Ray ray = getRay(fragCoord, viewportUpperLeft, pixelDeltaU, pixelDeltaV, rngState);
        newColor += vec4(GetColorForRay(ray, rngState), 1.0f);
    }
    newColor /= float(samplesPerPixel);

    // Mix the old accumulated color with the new color
    float alpha = 1.0 / float(frameCounter + 1);
    vec4 color = mix(accumulatedColor, newColor, alpha);

    // Store the color in the accumulation buffer
    imageStore(imgAccumulation, fragCoord, color);

    fragCoord.y = iResolution.y - fragCoord.y;

    // Store the color in the image
    imageStore(screen, fragCoord, color);
}
