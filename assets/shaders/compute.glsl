#version 430 core
layout(local_size_x = 8, local_size_y = 4, local_size_z = 1) in;
layout(rgba32f, binding = 0) uniform image2D screen;
uniform vec2 iResolution;
uniform uint iFrame;

// Accumulation buffer
layout(rgba32f, binding = 1) uniform image2D accumulationBuffer;

// The minimunm distance a ray must travel before we consider an intersection.
// This is to prevent a ray from intersecting a surface it just bounced off of.
const float c_minimumRayHitTime = 0.1f;

// the farthest we look for ray hits
const float c_superFar = 10000.0f;

// camera FOV
const float c_FOVDegrees = 90.0f;

// pi
const float c_pi = 3.141592;
const float c_twopi = 6.283185;

// Define the maximum number of bounces
const int c_numBounces = 3;
const float c_rayPosNormalNudge = 0.001f;  // Define the value for ray position normal nudge


struct SRayHitInfo
{
    float dist;
    vec3 normal;
    vec3 albedo;    // Add albedo field
    vec3 emissive;  // Add emissive field
};

float ScalarTriple(vec3 u, vec3 v, vec3 w)
{
    return dot(cross(u, v), w);
}

uint wang_hash(inout uint seed)
{
    seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
    seed *= uint(9);
    seed = seed ^ (seed >> 4);
    seed *= uint(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

float RandomFloat01(inout uint state)
{
    return float(wang_hash(state)) / 4294967296.0;
}

vec3 RandomUnitVector(inout uint state)
{
    float z = RandomFloat01(state) * 2.0f - 1.0f;
    float a = RandomFloat01(state) * c_twopi;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return vec3(x, y, z);
}

bool TestSphereTrace(in vec3 rayPos, in vec3 rayDir, inout SRayHitInfo info, in vec4 sphere) {
    vec3 center = sphere.xyz;
    float radius = sphere.w;

    vec3 oc = rayPos - center;

    float a = dot(rayDir, rayDir);
    float b = 2.0 * dot(oc, rayDir);
    float c = dot(oc, oc) - radius * radius;

    float discriminant = b * b - 4.0 * a * c;

    if (discriminant > 0.0) {
        float t1 = (-b - sqrt(discriminant)) / (2.0 * a);
        float t2 = (-b + sqrt(discriminant)) / (2.0 * a);

        if (t1 > c_minimumRayHitTime && t1 < info.dist) {
            info.dist = t1;
            info.normal = normalize(rayPos + t1 * rayDir - center);
            return true;
        } else if (t2 > c_minimumRayHitTime && t2 < info.dist) {
            info.dist = t2;
            info.normal = normalize(rayPos + t2 * rayDir - center);
            return true;
        }
    }

    return false;
}

bool TestTriangleTrace(in vec3 rayPos, in vec3 rayDir, inout SRayHitInfo info, in vec3 v0, in vec3 v1, in vec3 v2) {
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 h = cross(rayDir, edge2);
    float a = dot(edge1, h);
    if (a > -0.00001 && a < 0.00001)
    return false;
    float f = 1.0 / a;
    vec3 s = rayPos - v0;
    float u = f * dot(s, h);
    if (u < 0.0 || u > 1.0)
    return false;
    vec3 q = cross(s, edge1);
    float v = f * dot(rayDir, q);
    if (v < 0.0 || u + v > 1.0)
    return false;
    float t = f * dot(edge2, q);
    if (t > c_minimumRayHitTime && t < info.dist) {
        info.dist = t;
        info.normal = normalize(cross(edge1, edge2));
        return true;
    } else {
        return false;
    }
}

bool TestQuadTrace(in vec3 rayPos, in vec3 rayDir, inout SRayHitInfo info, in vec3 a, in vec3 b, in vec3 c, in vec3 d) {
    bool hit = false;
    if (TestTriangleTrace(rayPos, rayDir, info, a, b, c)) {
        hit = true;
    }
    if (TestTriangleTrace(rayPos, rayDir, info, a, c, d)) {
        hit = true;
    }
    return hit;
}

void TestSceneTrace(in vec3 rayPos, in vec3 rayDir, inout SRayHitInfo hitInfo)
{
    {
        vec3 A = vec3(-15.0f, -15.0f, 22.0f);
        vec3 B = vec3( 15.0f, -15.0f, 22.0f);
        vec3 C = vec3( 15.0f,  15.0f, 22.0f);
        vec3 D = vec3(-15.0f,  15.0f, 22.0f);
        if (TestQuadTrace(rayPos, rayDir, hitInfo, A, B, C, D))
        {
            hitInfo.albedo = vec3(0.7f, 0.7f, 0.7f);
            hitInfo.emissive = vec3(0.0f, 0.0f, 0.0f);
        }
    }

    if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(-10.0f, 0.0f, 20.0f, 1.0f)))
    {
        hitInfo.albedo = vec3(1.0f, 0.1f, 0.1f);
        hitInfo.emissive = vec3(0.0f, 0.0f, 0.0f);
    }

    if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(0.0f, 0.0f, 20.0f, 1.0f)))
    {
        hitInfo.albedo = vec3(0.1f, 1.0f, 0.1f);
        hitInfo.emissive = vec3(0.0f, 0.0f, 0.0f);
    }

    if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(10.0f, 0.0f, 20.0f, 1.0f)))
    {
        hitInfo.albedo = vec3(0.1f, 0.1f, 1.0f);
        hitInfo.emissive = vec3(0.0f, 0.0f, 0.0f);
    }


    if (TestSphereTrace(rayPos, rayDir, hitInfo, vec4(10.0f, 10.0f, 20.0f, 5.0f)))
    {
        hitInfo.albedo = vec3(0.0f, 0.0f, 0.0f);
        hitInfo.emissive = vec3(1.0f, 0.9f, 0.7f) * 100.0f;
    }
}

vec3 GetColorForRay(in vec3 startRayPos, in vec3 startRayDir, inout uint rngState)
{
    // initialize
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
    return ret;
}

void main() {
    ivec2 fragCoord = ivec2(gl_GlobalInvocationID.xy);

    // initialize a random number state based on frag coord and frame
    uint rngState = uint(uint(fragCoord.x) * uint(1973) + uint(fragCoord.y) * uint(9277) + uint(iResolution) * uint(26699)) | uint(1);

    // Calculate normalized coordinates
    vec2 uv = abs((vec2(fragCoord) / iResolution) * 2.0f - 1.0f);

    // The ray starts at the camera position (the origin)
    vec3 rayPosition = vec3(0.0f, 0.0f, 0.0f);

    // calculate the camera distance
    float cameraDistance = 1.0f / tan(c_FOVDegrees * 0.5f * c_pi / 180.0f);

    // calculate coordinates of the ray target on the imaginary pixel plane.
    // -1 to +1 on x,y axis. 1 unit away on the z axis
    vec3 rayTarget = vec3((fragCoord/iResolution.xy) * 2.0f - 1.0f, cameraDistance);

    // correct for aspect ratio
    float aspectRatio = iResolution.x / iResolution.y;
    rayTarget.y /= aspectRatio;

    // calculate a normalized vector for the ray direction.
    // it's pointing from the ray position to the ray target.
    vec3 rayDir = normalize(rayTarget - rayPosition);

    // Get pixel color
    vec3 color = GetColorForRay(rayPosition, rayDir, rngState);

    // average the frames together
    vec3 lastFrameColor = texture(accumulationBuffer, fragCoord / iResolution.xy).rgb;
    color = mix(lastFrameColor, color, 1.0f / float(iFrame+1));

    // Store the color in the image
    imageStore(screen, fragCoord, vec4(color, 1.0f));
}
