#version 430 core
layout(local_size_x = 8, local_size_y = 4, local_size_z = 1) in;
layout(rgba32f, binding = 0) uniform image2D screen;
layout(rgba32f, binding = 1) uniform image2D imgAccumulation;
uniform ivec2 iResolution;
uniform vec3 c_lookFrom;
uniform vec3 c_lookAt;
uniform vec3 c_lookUp;
uniform float c_FOVDegrees;
uniform uint c_numBounces;
uniform float c_defocusAngle;
uniform float c_focusDist;
uniform uint c_samplesPerPixel;
uniform bool c_sky;
uniform uint frameCounter;
uniform uint numOfSpheres;
uniform uint numOfQuads;

const float c_minimumRayHitTime = 0.00001f;
const float c_superFar = 10000.0f;
const float c_rayPosNormalNudge = 0.001f;
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

vec3 randomInUnitDisk(in uint state) {
    while (true) {
        vec3 p = vec3(RandomFloat(state) * 2.0 - 1.0, RandomFloat(state) * 2.0 - 1.0, 0.0);
        if (dot(p, p) < 1.0) {
            return p;
        }
    }
}

vec3 defocusDiskSample(in vec3 defocusDiskU, in vec3 defocusDiskV, in uint rngSeed) {
    vec3 p = randomInUnitDisk(rngSeed);
    return vec3(0) + (p.x * defocusDiskU) + (p.y * defocusDiskV);
}

struct Ray {
    vec3 origin;
    vec3 direction;
};

vec3 getRayPointAt(Ray r, float t) {
    return r.origin + t * r.direction;
}

struct HitRecord {
    vec3 p;
    vec3 normal;
    float t;
    bool frontFace;
    vec3 albedo;
    float reflectivity;
    float fuzz;
    float refractionIndex;
    vec3 emission;
    float emissionStrength;
};

void setFaceNormal(in Ray r, in vec3 outwardNormal, inout HitRecord rec) {
    rec.frontFace = dot(r.direction, outwardNormal) < 0;
    rec.normal = rec.frontFace ? outwardNormal : -outwardNormal;
}

struct Quad {
    vec3 a;
    float reflectivity;
    vec3 b;
    float fuzz;
    vec3 c;
    float refractionIndex;
    vec3 d;
    vec3 normal;
    vec3 albedo;
    vec3 emission;
    float emissionStrength;
};

layout(std430, binding = 1) buffer Quads {
    Quad quads[];
};

struct Sphere {
    vec3 center;
    float radius;
    vec3 albedo;
    float reflectivity;
    float fuzz;
    float refractionIndex;
    vec3 emission;
    float emissionStrength;
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

float reflectance(float cosine, float refraction_index) {
    float r0 = ((1.0 - refraction_index) / (1.0 + refraction_index));
    r0 = r0 * r0;
    return r0 + (1.0 - r0) * pow((1.0 - cosine), 5.0);
}

bool hitTriangle(in Ray ray, in vec3 v0, in vec3 v1, in vec3 v2, inout Interval interval, inout HitRecord rec, in vec3 normal, in vec3 albedo, float reflectivity, float fuzz, float refractionIndex) {
    // Check the angle between the ray direction and the normal
    if (dot(ray.direction, normal) > 0.0) {
        return false;
    }

    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);

    if (abs(a) < c_minimumRayHitTime) {
        return false;
    }

    float f = 1.0 / a;
    vec3 s = ray.origin - v0;
    float u = f * dot(s, h);

    if (u < 0.0 || u > 1.0) {
        return false;
    }

    vec3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);

    if (v < 0.0 || u + v > 1.0) {
        return false;
    }

    float t = f * dot(edge2, q);

    if (t > interval.min && t < interval.max) {
        rec.t = t;
        rec.p = getRayPointAt(ray, rec.t);
        rec.normal = normal;
        rec.albedo = albedo;
        rec.reflectivity = reflectivity;
        rec.fuzz = fuzz;
        rec.refractionIndex = refractionIndex;
        setFaceNormal(ray, normal, rec);
        return true;
    }

    return false;
}

bool hitQuad(in Ray ray, inout Interval interval, inout HitRecord rec, in Quad quad) {
    bool hit = false;

    if (hitTriangle(ray, quad.a, quad.b, quad.c, interval, rec, quad.normal, quad.albedo, quad.reflectivity, quad.fuzz, quad.refractionIndex)) {
        hit = true;
        interval.max = rec.t;
        rec.emission = quad.emission;
        rec.emissionStrength = quad.emissionStrength;
    }

    if (hitTriangle(ray, quad.a, quad.c, quad.d, interval, rec, quad.normal, quad.albedo, quad.reflectivity, quad.fuzz, quad.refractionIndex)) {
        hit = true;
        interval.max = rec.t;
        rec.emission = quad.emission;
        rec.emissionStrength = quad.emissionStrength;
    }

    return hit;
}


bool hitSphere(in Ray ray, inout Interval interval, inout HitRecord rec, in Sphere sphere) {
    vec3 oc = ray.origin - sphere.center;
    float a = dot(ray.direction, ray.direction);
    float half_b = dot(oc, ray.direction);
    float c = dot(oc, oc) - sphere.radius * sphere.radius;
    float discriminant = half_b * half_b - a * c;

    if (discriminant > 0) {
        float sqrtd = sqrt(discriminant);
        float root = (-half_b - sqrtd) / a;
        if (intervalSurrounds(interval, root)) {
            rec.t = root;
            rec.p = getRayPointAt(ray, rec.t);
            vec3 outwardNormal = (rec.p - sphere.center) / sphere.radius;
            setFaceNormal(ray, outwardNormal, rec);
            rec.albedo = sphere.albedo;
            rec.reflectivity = sphere.reflectivity;
            rec.fuzz = sphere.fuzz;
            rec.refractionIndex = sphere.refractionIndex;
            rec.emission = sphere.emission;
            rec.emissionStrength = sphere.emissionStrength;
            return true;
        }
        root = (-half_b + sqrtd) / a;
        if (intervalSurrounds(interval, root)) {
            rec.t = root;
            rec.p = getRayPointAt(ray, rec.t);
            vec3 outwardNormal = (rec.p - sphere.center) / sphere.radius;
            setFaceNormal(ray, outwardNormal, rec);
            rec.albedo = sphere.albedo;
            rec.reflectivity = sphere.reflectivity;
            rec.fuzz = sphere.fuzz;
            rec.refractionIndex = sphere.refractionIndex;
            rec.emission = sphere.emission;
            rec.emissionStrength = sphere.emissionStrength;
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

    for (uint i = 0; i < numOfQuads; ++i) {
        if (hitQuad(ray, interval, hitRecord, quads[i])) {
            hitAnything = true;
            interval.max = hitRecord.t;
        }
    }

    return hitAnything;
}

vec3 GetColorForRay(in Ray ray, inout uint rngState) {
    vec3 incomingLight = vec3(0.0);
    vec3 rayColour = vec3(1.0);

    for (uint bounce = 0; bounce < c_numBounces; ++bounce) {
        HitRecord rec;
        if (TestSceneTrace(ray, rec)) {
            // Determine new ray direction (diffuse or specular)
            vec3 newDirection;
            bool isSpecularBounce = rec.fuzz > 0.0 && RandomFloat(rngState) < rec.fuzz;
            if (isSpecularBounce) {
                newDirection = reflect(ray.direction, rec.normal) + rec.fuzz * randomUnitVector(rngState);
            } else {
                newDirection = rec.normal + randomUnitVector(rngState);
            }

            ray = Ray(rec.p + c_rayPosNormalNudge * rec.normal, newDirection);

            // Accumulate emitted light
            vec3 emittedLight = rec.emission * rec.emissionStrength;
            incomingLight += emittedLight * rayColour;

            // Modulate ray colour based on material properties
            rayColour *= rec.albedo;

            // Russian Roulette termination
            float probability = max(rayColour.r, max(rayColour.g, rayColour.b));
            if (RandomFloat(rngState) >= probability) {
                break;
            }
            rayColour /= probability; // Normalize ray colour
        } else if (c_sky) {
            // If ray doesn't hit anything and sky is enabled, accumulate sky color
            vec3 unitDirection = normalize(ray.direction);
            float t = 0.5 * (unitDirection.y + 1.0);
            vec3 skyColor = (1.0 - t) * vec3(1.0, 1.0, 1.0) + t * vec3(0.5, 0.7, 1.0);
            incomingLight += skyColor * rayColour;
            break;
        } else {
            // If ray doesn't hit anything and sky is not enabled, terminate
            incomingLight = vec3(0.0);
            break;
        }
    }

    return incomingLight;
}

Ray getRay(in ivec2 fragCoord, in vec3 viewportUpperLeft, in vec3 pixelDeltaU, in vec3 pixelDeltaV, in vec3 defocusDiskU, in vec3 defocusDiskV, inout uint rngState) {
    vec2 offset = vec2(RandomFloat(rngState), RandomFloat(rngState));
    vec3 pixelSample = viewportUpperLeft
    + (float(fragCoord.x) + offset.x) * pixelDeltaU
    + (float(fragCoord.y) + offset.y) * pixelDeltaV;

    vec3 rayOrigin = (c_defocusAngle <= 0) ? c_lookFrom : defocusDiskSample(defocusDiskU, defocusDiskV, rngState) + c_lookFrom;
    vec3 rayDirection = normalize(pixelSample - rayOrigin);

    return Ray(rayOrigin, rayDirection);
}

void main() {
    ivec2 fragCoord = ivec2(gl_GlobalInvocationID.xy);

    uint rngState = uint(uint(fragCoord.x) * uint(1973) + uint(fragCoord.y) * uint(9277) + uint(frameCounter) * uint(26699)) | uint(1);

    float aspectRatio = float(iResolution.x) / float(iResolution.y);

    float theta = radians(c_FOVDegrees);
    float h = tan(theta / 2);
    float viewportHeight = 2.0 * h * c_focusDist;
    float viewportWidth = aspectRatio * viewportHeight;

    vec3 w = normalize(c_lookFrom - c_lookAt);
    vec3 u = normalize(cross(c_lookUp, w));
    vec3 v = cross(w, u);

    vec3 viewportU = viewportWidth * u;
    vec3 viewportV = viewportHeight * -v;

    vec3 pixelDeltaU = viewportU / float(iResolution.x);
    vec3 pixelDeltaV = viewportV / float(iResolution.y);

    vec3 viewportUpperLeft = c_lookFrom - (c_focusDist * w) - viewportV / 2.0 - viewportU / 2;

    float defocusRadius = c_defocusAngle <= 0.0 ? 0.0 : c_focusDist * tan(radians(c_defocusAngle / 2.0));
    vec3 defocusDiskU = u * defocusRadius;
    vec3 defocusDiskV = v * defocusRadius;

    vec4 accumulatedColor = imageLoad(imgAccumulation, fragCoord);
    vec4 newColor = vec4(0.0);

    for (uint sampl = 0; sampl < c_samplesPerPixel; sampl++) {
        Ray ray = getRay(fragCoord, viewportUpperLeft, pixelDeltaU, pixelDeltaV, defocusDiskU, defocusDiskV, rngState);
        newColor += vec4(GetColorForRay(ray, rngState), 1.0);
    }
    newColor.rgb /= float(c_samplesPerPixel);

    newColor = vec4(sqrt(newColor.r), sqrt(newColor.g), sqrt(newColor.b), 1);

    float alpha = 1.0 / float(frameCounter + 1);
    vec4 color = mix(accumulatedColor, newColor, alpha);

    imageStore(imgAccumulation, fragCoord, color);

    fragCoord.y = iResolution.y - fragCoord.y - 1;

    imageStore(screen, fragCoord, color);
}
