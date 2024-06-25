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

// The minimum distance a ray must travel before we consider an intersection.
const float c_minimumRayHitTime = 0.1f;

// The farthest we look for ray hits
const float c_superFar = 100000.0f;

// Pi
const float c_pi = 3.141592;
const float c_twopi = 6.283185;

// Define the value for ray position normal nudge
const float c_rayPosNormalNudge = 0.001f;

uint wang_hash(inout uint seed) {
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

float RandomFloat01(inout uint state) {
    return float(wang_hash(state)) / 4294967296.0;
}

vec3 RandomUnitVector(inout uint state) {
    float z = RandomFloat01(state) * 2.0f - 1.0f;
    float a = RandomFloat01(state) * c_twopi;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return vec3(x, y, z);
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

struct Interval {
    float min;
    float max;
};

float getIntervalSize(in Interval interval) {
    return interval.max - interval.min;
}

bool intervalContains (in Interval interval, in float x) {
    return interval.min <= x || interval.max >= x;
}

bool intervalSurrounds (in Interval interval, in float x) {
    return interval.min < x || interval.max > x;
}

float ScalarTriple(vec3 u, vec3 v, vec3 w) {
    return dot(cross(u, v), w);
}

bool hitSphere(in Ray ray, in Interval interval, inout HitRecord rec, in Sphere sphere) {
    vec3 oc = ray.origin - sphere.center;
    float a = dot(ray.direction, ray.direction);
    float b = dot(oc, ray.direction);
    float c = dot(oc, oc) - sphere.radius * sphere.radius;
    float discriminant = b * b - a * c;

    if (discriminant > 0) {
        float sqrtd = sqrt(discriminant);
        float root = (-b - sqrtd) / a;
        if (intervalSurrounds(interval, root)) {
            rec.t = root;
            rec.p = getRayPointAt(ray, rec.t);
            vec3 outwardNormal = (rec.p - sphere.center) / sphere.radius;
            setFaceNormal(ray, outwardNormal, rec);
            rec.color = sphere.color;
            return true;
        }
        root = (-b + sqrtd) / a;
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

void TestSceneTrace(in Ray ray, inout HitRecord hitRecord) {

}

vec3 GetColorForRay(in Ray ray, inout uint rngState) {
    HitRecord hitRecord;
    if (hitSphere(ray, Interval(c_minimumRayHitTime, c_superFar), hitRecord, Sphere(vec3(0,0,-1), 0.5f, vec3(1.0f)))) {
        return 0.5f * (hitRecord.normal + 1);
    }

    vec3 unitDirection = normalize(ray.direction);
    float t = 0.5 * (unitDirection.y + 1.0);
    return (1.0 - t) * vec3(1.0, 1.0, 1.0) + t * vec3(0.5, 0.7, 1.0);


    /* initialize
    vec3 ret = vec3(0.0f, 0.0f, 0.0f);
    vec3 throughput = vec3(1.0f, 1.0f, 1.0f);
    vec3 rayPos = startRayPos;
    vec3 rayDir = startRayDir;

    for (int bounceIndex = 0; bounceIndex <= c_numBounces; ++bounceIndex)
    {
        // shoot a ray out into the world
        SRayHitInfo hitInfo;
        hitInfo.dist = c_superFar;
        TestSceneTrace(rayPos, rayDir, hitInfo);

        // if the ray missed, we are done
        if (hitInfo.dist == c_superFar)
        break;

        // update the ray position
        rayPos = (rayPos + rayDir * hitInfo.dist) + hitInfo.normal * c_rayPosNormalNudge;

        // calculate new ray direction, in a cosine weighted hemisphere oriented at normal
        rayDir = normalize(hitInfo.normal + RandomUnitVector(rngState));

        // add in emissive lighting
        ret += hitInfo.emissive * throughput;

        // update the colorMultiplier
        throughput *= hitInfo.albedo;
    }

    // return pixel color
    return ret; */
}

void main() {
    ivec2 fragCoord = ivec2(gl_GlobalInvocationID.xy);

    // initialize a random number state based on frag coord and frame
    uint rngState = uint(uint(fragCoord.x) * uint(1973) + uint(fragCoord.y) * uint(9277) + uint(frameCounter) * uint(26699)) | uint(1);

    // Calculate normalized coordinates
    vec2 uv = (vec2(fragCoord) / iResolution) * 2.0f - 1.0f;

    // Ray starts at the camera position (the origin)
    Ray ray;
    ray.origin = vec3(0.0f, 0.0f, 0.0f);

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

    vec3 viewportUpperLeft = ray.origin - vec3(0.0, 0.0, c_focalLength) - viewportU * 0.5 - viewportV * 0.5;
    vec3 pixel00Loc = viewportUpperLeft + 0.5 * (pixelDeltaU + pixelDeltaV);

    // Calculate the pixel location
    vec3 pixelCenter = pixel00Loc + fragCoord.x * pixelDeltaU + fragCoord.y * pixelDeltaV;
    ray.direction = normalize(pixelCenter - ray.origin);

    // Accumulated color from previous frames
    vec4 accumulatedColor = imageLoad(imgAccumulation, fragCoord);
    vec4 newColor = vec4(0.0);

    // Get color for current ray
    newColor = vec4(GetColorForRay(ray, rngState), 1.0f);

    // Mix the old accumulated color with the new color
    float alpha = 1.0 / float(frameCounter + 1);
    vec4 color = mix(accumulatedColor, newColor, alpha);

    // Store the color in the accumulation buffer
    imageStore(imgAccumulation, fragCoord, color);

    fragCoord.y = iResolution.y - fragCoord.y;

    // Store the color in the image
    imageStore(screen, fragCoord, color);
}
