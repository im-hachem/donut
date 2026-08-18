// Geodesic ray tracer.
//
// Originally an OpenGL 4.3 compute shader that wrote its result with
// imageStore(). macOS OpenGL is frozen at 4.1 and has neither compute shaders
// nor image load/store, so this is expressed as a fullscreen vertex+fragment
// pass (GLSL 4.10) that renders into an FBO colour attachment instead. Each
// fragment does the work one compute invocation used to: gl_FragCoord replaces
// gl_GlobalInvocationID, and the shading maths below is unchanged.

#type vertex
#version 410 core

layout(location = 0) in vec2 a_Position;

void main()
{
    gl_Position = vec4(a_Position, 0.0, 1.0);
}

#type fragment
#version 410 core

out vec4 fragColor;

// Compute-resolution in pixels; supplied by the host each frame (replaces the
// compute shader's imageSize(outImage)).
uniform vec2 u_Resolution;

// GLSL 4.10 does not allow explicit binding qualifiers on uniform blocks or
// samplers; the host associates these with binding points via
// glUniformBlockBinding / glUniform1i.
uniform samplerCube u_HDRIEnvironment;

layout(std140) uniform Camera
{
    vec3  camPos;     float _pad0;
    vec3  camRight;   float _pad1;
    vec3  camUp;      float _pad2;
    vec3  camForward; float _pad3;
    float tanHalfFov;
    float aspect;
    bool  moving;
    int   _pad4;
} cam;

layout(std140) uniform Disk
{
    float disk_r1;
    float disk_r2;
    float disk_num;
    float thickness;
    float disk_density;
};

layout(std140) uniform Objects
{
    int    numObjects;
    vec4   objPosRadius[16];
    vec4   objColor[16];
    float  mass[16];
};

layout(std140) uniform Simulation
{
    int   maxStepsMoving;
    int   maxStepsStatic;
    float earlyExitDistance;
    float time;
};

const float  SagA_rs  = 1.269e10;
const float  D_LAMBDA = 1e7;
const float  ESCAPE_R = 1e30;

const int   DEFAULT_MAX_STEPS_MOVING   = 12000;
const int   DEFAULT_MAX_STEPS_STATIC   = 8000;
const float DEFAULT_EARLY_EXIT_DISTANCE = 2e12;

const float MIN_STEP_SIZE = 1e6;
const float MAX_STEP_SIZE = 5e7;
const float STEP_ADAPTATION_FACTOR = 1.5;

vec4 objectColor = vec4(0.0);
vec3 hitCenter = vec3(0.0);
float hitRadius = 0.0;

vec3 SampleHDRI(vec3 direction)
{
    return texture(u_HDRIEnvironment, direction).rgb;
}

float hash(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

float hash(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float hash(vec3 p)
{
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}

float noise(vec3 x)
{
    vec3 i = floor(x);
    vec3 frac = fract(x);

    vec3 u = frac * frac * (3.0 - 2.0 * frac);

    float a = hash(i);
    float b = hash(i + vec3(1.0, 0.0, 0.0));
    float c = hash(i + vec3(0.0, 1.0, 0.0));
    float d = hash(i + vec3(1.0, 1.0, 0.0));
    float e = hash(i + vec3(0.0, 0.0, 1.0));
    float f = hash(i + vec3(1.0, 0.0, 1.0));
    float g = hash(i + vec3(0.0, 1.0, 1.0));
    float h = hash(i + vec3(1.0, 1.0, 1.0));

    return mix(mix(mix(a, b, u.x), mix(c, d, u.x), u.y),
               mix(mix(e, f, u.x), mix(g, h, u.x), u.y), u.z);
}

float fbm(vec3 x, int octaves)
{
    float v = 0.0;
    float a = 0.5;
    float f = 1.0;
    vec3 shift = vec3(100, 200, 300);

    for (int i = 0; i < octaves; ++i)
    {
        v += a * noise(x * f);
        x = x * 2.0 + shift;
        a *= 0.5;
        f *= 2.0;
    }
    return v;
}

float GetCloudDensity(vec3 pos)
{
    float r_cyl = length(vec2(pos.x, pos.z));
    float r_norm = (r_cyl - disk_r1) / (disk_r2 - disk_r1);

    if (r_norm < 0.0 || r_norm > 1.0)
        return 0.0;

    float h_norm = abs(pos.y) / thickness;
    float vertical_falloff = exp(-h_norm * h_norm * 3.0);
    float radial_density = 1.0 - r_norm * 0.5;

    vec3 noise_pos = pos * 1e-10;
    float keplerian_speed = 1.0 / sqrt(r_norm + 0.1);

    float rotation_angle = time * keplerian_speed * 0.5;
    vec3 rotated_pos = vec3(
        pos.x * cos(rotation_angle) - pos.z * sin(rotation_angle),
        pos.y,
        pos.x * sin(rotation_angle) + pos.z * cos(rotation_angle)
    ) * 1e-10;

    float large_turbulence = fbm(rotated_pos * 1.2, 5);

    float medium_wisps = fbm(rotated_pos * 2.5, 4);
    float small_detail = fbm(rotated_pos * 6.0, 3);
    float fine_detail  = fbm(rotated_pos * 10.0, 2);

    float noise_mask = large_turbulence * 0.4 +
                       medium_wisps * 0.3 +
                       small_detail * 0.2 +
                       fine_detail * 0.1;

    noise_mask = smoothstep(0.25, 0.75, noise_mask);

    float angle = atan(pos.z, pos.x);
    float rotated_angle = angle + time * 0.5;

    float spiral_arms = sin(rotated_angle * 3.0 + r_norm * 15.0) * 0.15 + 0.85;

    float orbital_angle = angle + time * keplerian_speed * 0.8;
    float orbital_pattern = sin(orbital_angle * 2.0 + r_norm * 8.0) * 0.2 + 0.8;

    float density = vertical_falloff * radial_density * noise_mask * spiral_arms * orbital_pattern;
    return density * disk_density;
}

struct Ray
{
    float x, y, z;
    float r, theta, phi;
    float dr, dtheta, dphi;
    float E, L;
};

Ray InitRay(vec3 pos, vec3 dir)
{
    Ray ray;
    ray.x     = pos.x;
    ray.y     = pos.y;
    ray.z     = pos.z;
    ray.r     = length(pos);
    ray.theta = acos(pos.z / ray.r);
    ray.phi   = atan(pos.y, pos.x);

    float dx = dir.x;
    float dy = dir.y;
    float dz = dir.z;

    ray.dr     = sin(ray.theta)*cos(ray.phi)*dx +
                 sin(ray.theta)*sin(ray.phi)*dy +
                 cos(ray.theta)*dz;

    ray.dtheta = (cos(ray.theta)*cos(ray.phi)*dx +
                  cos(ray.theta)*sin(ray.phi)*dy -
                  sin(ray.theta)*dz) / ray.r;

    ray.dphi   = (-sin(ray.phi)*dx + cos(ray.phi)*dy) /
                 (ray.r * sin(ray.theta));

    ray.L = ray.r * ray.r * sin(ray.theta) * ray.dphi;
    float f = 1.0 - SagA_rs / ray.r;
    float dt_dL = sqrt((ray.dr*ray.dr)/f +
                       ray.r*ray.r*(ray.dtheta*ray.dtheta +
                       sin(ray.theta)*sin(ray.theta)*
                       ray.dphi*ray.dphi));
    ray.E = f * dt_dL;

    return ray;
}

bool Intercept(Ray ray, float rs)
{
    return ray.r <= rs;
}

bool InterceptObject(Ray ray)
{
    vec3 P = vec3(ray.x, ray.y, ray.z);

    for (int i = 0; i < numObjects; ++i)
    {
        vec3  center = objPosRadius[i].xyz;
        float radius = objPosRadius[i].w;

        float distSq = dot(P - center, P - center);
        if (distSq > radius * radius * 4.0)
            continue;

        if (distSq <= radius * radius)
        {
            objectColor = objColor[i];
            hitCenter = center;
            hitRadius = radius;
            return true;
        }
    }

    return false;
}

void GeodesicRHS(Ray ray, out vec3 d1, out vec3 d2)
{
    float r      = ray.r;
    float theta  = ray.theta;
    float dr     = ray.dr;
    float dtheta = ray.dtheta;
    float dphi   = ray.dphi;
    float f      = 1.0 - SagA_rs / r;
    float dt_dL  = ray.E / f;

    d1 = vec3(dr, dtheta, dphi);
    d2.x = - (SagA_rs / (2.0 * r*r)) * f * dt_dL * dt_dL
           + (SagA_rs / (2.0 * r*r * f)) * dr * dr
           + r * (dtheta*dtheta + sin(theta)*sin(theta)*dphi*dphi);
    d2.y = -2.0*dr*dtheta/r + sin(theta)*cos(theta)*dphi*dphi;
    d2.z = -2.0*dr*dphi/r - 2.0*cos(theta)/(sin(theta)) * dtheta * dphi;
}

void RK4Step(inout Ray ray, float dL)
{
    vec3 k1a, k1b;
    GeodesicRHS(ray, k1a, k1b);

    ray.r      += dL * k1a.x;
    ray.theta  += dL * k1a.y;
    ray.phi    += dL * k1a.z;
    ray.dr     += dL * k1b.x;
    ray.dtheta += dL * k1b.y;
    ray.dphi   += dL * k1b.z;

    ray.x = ray.r * sin(ray.theta) * cos(ray.phi);
    ray.y = ray.r * sin(ray.theta) * sin(ray.phi);
    ray.z = ray.r * cos(ray.theta);
}

bool IsInDiskVolume(vec3 pos)
{
    float r_cyl = length(vec2(pos.x, pos.z));
    return (r_cyl >= disk_r1 && r_cyl <= disk_r2 && abs(pos.y) <= thickness);
}

vec4 SampleDiskColor(vec3 pos)
{
    float r_cyl = length(vec2(pos.x, pos.z));
    float r_norm = (r_cyl - disk_r1) / (disk_r2 - disk_r1);

    vec3 innerColor = vec3(1.0, 0.9, 0.5);
    vec3 midColor = vec3(1.0, 0.6, 0.2);
    vec3 outerColor = vec3(0.9, 0.3, 0.1);

    vec3 baseColor;
    if (r_norm < 0.5)
        baseColor = mix(innerColor, midColor, r_norm * 2.0);
    else
        baseColor = mix(midColor, outerColor, (r_norm - 0.5) * 2.0);

    float r_norm_rot = (r_cyl - disk_r1) / (disk_r2 - disk_r1);
    float keplerian_speed = 1.0 / sqrt(r_norm_rot + 0.1);

    vec3 noise_pos = pos * 1e-10;

    float color_rotation_angle = time * keplerian_speed * 0.3;
    vec3 rotated_color_pos = vec3(
        pos.x * cos(color_rotation_angle) - pos.z * sin(color_rotation_angle),
        pos.y,
        pos.x * sin(color_rotation_angle) + pos.z * cos(color_rotation_angle)
    ) * 1e-10;

    float large_color = fbm(rotated_color_pos * 1.8, 4);
    float medium_color = fbm(rotated_color_pos * 4.0, 3);
    float small_color = fbm(rotated_color_pos * 8.0, 2);
    float colorVariation = (large_color * 0.5 + medium_color * 0.3 + small_color * 0.2) * 0.6;
    baseColor = baseColor * (1.0 + colorVariation);

    float density = GetCloudDensity(pos);
    vec3 brightness_noise_pos = pos * 1e-10;

    float brightness_rotation_angle = time * keplerian_speed * 0.7;
    vec3 rotated_brightness_pos = vec3(
        pos.x * cos(brightness_rotation_angle) - pos.z * sin(brightness_rotation_angle),
        pos.y,
        pos.x * sin(brightness_rotation_angle) + pos.z * cos(brightness_rotation_angle)
    ) * 1e-10;

    float brightness_large = fbm(rotated_brightness_pos * 3.0, 3);
    float brightness_medium = fbm(rotated_brightness_pos * 5.0, 2);
    float brightness_small = fbm(rotated_brightness_pos * 7.0, 2);
    float brightness_noise = (brightness_large * 0.6 + brightness_medium * 0.3 + brightness_small * 0.1);

    // Enhanced brightness with glow effect
    float baseBrightness = 1.0 + density * 1.5; // Increased density contribution
    float glowBrightness = brightness_noise * 0.8; // Enhanced noise contribution
    float brightness = baseBrightness + glowBrightness;

    return vec4(baseColor * brightness, density);
}

float CalculateAdaptiveStepSize(Ray ray, float baseStepSize)
{
    float r_factor         = clamp(ray.r / (SagA_rs * 10.0), 0.1, 1.0);
    float curvature        = length(vec3(ray.dr, ray.dtheta * ray.r, ray.dphi * ray.r * sin(ray.theta)));
    float curvature_factor = clamp(1e12 / (curvature + 1e6), 0.1, 2.0);

    return clamp(baseStepSize * r_factor * curvature_factor, MIN_STEP_SIZE, MAX_STEP_SIZE);
}

void main()
{
    ivec2 pix  = ivec2(gl_FragCoord.xy);
    int WIDTH  = int(u_Resolution.x);
    int HEIGHT = int(u_Resolution.y);

    float u = (2.0 * (pix.x + 0.5) / WIDTH - 1.0) *
              cam.aspect * cam.tanHalfFov;
    float v = (1.0 - 2.0 * (pix.y + 0.5) / HEIGHT) *
               cam.tanHalfFov;
    vec3 dir = normalize(u * cam.camRight -
                         v * cam.camUp    +
                         cam.camForward);
    Ray ray = InitRay(cam.camPos, dir);

    vec4  color   = vec4(0.0);
    vec3  prevPos = vec3(ray.x, ray.y, ray.z);
    float lambda  = 0.0;

    bool hitBlackHole = false;
    bool hitObject    = false;

    vec4 accumulatedColor = vec4(0.0);
    float transmittance = 1.0;

    int maxSteps = cam.moving ? maxStepsMoving : maxStepsStatic;

    if (maxSteps <= 0)
        maxSteps = cam.moving ? DEFAULT_MAX_STEPS_MOVING : DEFAULT_MAX_STEPS_STATIC;

    float cameraDistance = length(cam.camPos);
    if (cameraDistance > 2e12)
        maxSteps = maxSteps / 2;
    else if (cameraDistance > 1e12)
        maxSteps = int(maxSteps * 0.75);

    float initialEscapeVelocity = sqrt(2.0 * SagA_rs / ray.r);
    if (ray.dr > initialEscapeVelocity * 0.95 &&
        ray.r  > SagA_rs * 200.0)
        maxSteps = maxSteps / 2;

    float currentStepSize = D_LAMBDA;
    int objectCheckInterval = 5;

    for (int i = 0; i < maxSteps; ++i)
    {
        float exitDistance = earlyExitDistance > 0.0 ? earlyExitDistance : DEFAULT_EARLY_EXIT_DISTANCE;
        if (ray.r > exitDistance)
            break;
        if (ray.r > ESCAPE_R)
            break;

        if (Intercept(ray, SagA_rs))
        {
            hitBlackHole = true;
            break;
        }

        currentStepSize = CalculateAdaptiveStepSize(ray, D_LAMBDA);

        RK4Step(ray, currentStepSize);
        lambda += currentStepSize;

        vec3 newPos = vec3(ray.x, ray.y, ray.z);

        if (IsInDiskVolume(newPos))
        {
            vec4 diskSample = SampleDiskColor(newPos);
            float density = diskSample.a;
            vec3 diskColor = diskSample.rgb;

            float stepLength = currentStepSize * 1e-8;

            float absorption = density * stepLength * 0.8;
            float scattering = density * stepLength * 1.5;
            float extinction = absorption + scattering;

            float stepTransmittance = exp(-extinction);

            vec3 emission = diskColor * density * stepLength * 4.0 * sqrt(disk_density);

            vec3 glowColor       = mix(diskColor, vec3(1.0, 0.8, 0.6), 0.3);
            float glowIntensity  = density * stepLength * 2.0;
            vec3 atmosphericGlow = glowColor * glowIntensity * 0.8;

            vec3 totalEmission = emission + atmosphericGlow;
            accumulatedColor.rgb += totalEmission * transmittance;

            transmittance *= stepTransmittance;

            if (transmittance < 0.01)
            {
                accumulatedColor.a = 1.0 - transmittance;
                break;
            }
        }

        if (i % objectCheckInterval == 0 && InterceptObject(ray))
        {
            hitObject = true;
            break;
        }

        prevPos = newPos;

        if (ray.dr > 0.0 && ray.r > SagA_rs * 100.0 && lambda > 2e8)
            break;
    }

    accumulatedColor.a = 1.0 - transmittance;

    if (hitBlackHole)
        color = vec4(0.0, 0.0, 0.0, 1.0);
    else if (hitObject)
    {
        vec3 P = vec3(ray.x, ray.y, ray.z);
        vec3 N = normalize(P - hitCenter);
        vec3 V = normalize(cam.camPos - P);

        float ambient   = 0.1;
        float diff      = max(dot(N, V), 0.0);
        float intensity = ambient + (1.0 - ambient) * diff;
        vec3  shaded    = objectColor.rgb * intensity;

        color = vec4(shaded, objectColor.a);
        color = mix(accumulatedColor, color, color.a);
    } else
    {
        vec3 rayDirection = normalize(vec3(ray.x, ray.y, ray.z) - cam.camPos);
        vec3 hdriColor = SampleHDRI(rayDirection);
        color  = vec4(mix(accumulatedColor.rgb, hdriColor, 1.0 - accumulatedColor.a), 1.0);
    }

    fragColor = color;
}
