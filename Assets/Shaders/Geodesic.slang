struct VSInput  { float2 position : POSITION; float2 texCoord : TEXCOORD0; };
struct VSOutput { float4 position : SV_Position; float2 texCoord : TEXCOORD0; };

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.texCoord = input.texCoord;
    return output;
}

struct Camera
{
    float3 camPos;     float _pad0;
    float3 camRight;   float _pad1;
    float3 camUp;      float _pad2;
    float3 camForward; float _pad3;
    float  tanHalfFov;
    float  aspect;
    bool   moving;
    int    _pad4;
};
ConstantBuffer<Camera> cam;

struct Disk
{
    float disk_r1;
    float disk_r2;
    float disk_num;
    float thickness;
    float disk_density;
};
ConstantBuffer<Disk> disk;

struct Objects
{
    int    numObjects;
    float4 objPosRadius[16];
    float4 objColor[16];
    float  mass[16];
};
ConstantBuffer<Objects> obj;

struct Simulation
{
    int   maxStepsMoving;
    int   maxStepsStatic;
    float earlyExitDistance;
    float time;
};
ConstantBuffer<Simulation> sim;

SamplerCube u_HDRIEnvironment;

static const float SagA_rs   = 1.269e10;
static const float D_LAMBDA  = 1e7;
static const float ESCAPE_R  = 1e30;

static const int   DEFAULT_MAX_STEPS_MOVING    = 12000;
static const int   DEFAULT_MAX_STEPS_STATIC    = 8000;
static const float DEFAULT_EARLY_EXIT_DISTANCE = 2e12;

static const float MIN_STEP_SIZE = 1e6;
static const float MAX_STEP_SIZE = 5e7;

struct Hit
{
    float4 objectColor;
    float3 hitCenter;
    float  hitRadius;
};

float3 SampleHDRI(float3 direction)
{
    return u_HDRIEnvironment.Sample(direction).rgb;
}

float hash(float p)
{
    p = frac(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return frac(p);
}

float hash(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

float hash(float3 p)
{
    p = frac(p * float3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return frac((p.x + p.y) * p.z);
}

float noise(float3 x)
{
    float3 i  = floor(x);
    float3 fr = frac(x);

    float3 u = fr * fr * (3.0 - 2.0 * fr);

    float a = hash(i);
    float b = hash(i + float3(1.0, 0.0, 0.0));
    float c = hash(i + float3(0.0, 1.0, 0.0));
    float d = hash(i + float3(1.0, 1.0, 0.0));
    float e = hash(i + float3(0.0, 0.0, 1.0));
    float f = hash(i + float3(1.0, 0.0, 1.0));
    float g = hash(i + float3(0.0, 1.0, 1.0));
    float h = hash(i + float3(1.0, 1.0, 1.0));

    return lerp(lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y),
                lerp(lerp(e, f, u.x), lerp(g, h, u.x), u.y), u.z);
}

float fbm(float3 x, int octaves)
{
    float v = 0.0;
    float a = 0.5;
    float f = 1.0;
    float3 shift = float3(100, 200, 300);

    for (int i = 0; i < octaves; ++i)
    {
        v += a * noise(x * f);
        x = x * 2.0 + shift;
        a *= 0.5;
        f *= 2.0;
    }
    return v;
}

float GetCloudDensity(float3 pos)
{
    float r_cyl  = length(float2(pos.x, pos.z));
    float r_norm = (r_cyl - disk.disk_r1) / (disk.disk_r2 - disk.disk_r1);

    if (r_norm < 0.0 || r_norm > 1.0)
        return 0.0;

    float h_norm = abs(pos.y) / disk.thickness;
    float vertical_falloff = exp(-h_norm * h_norm * 3.0);
    float radial_density = 1.0 - r_norm * 0.5;

    float keplerian_speed = 1.0 / sqrt(r_norm + 0.1);

    float rotation_angle = sim.time * keplerian_speed * 0.5;
    float3 rotated_pos = float3(
        pos.x * cos(rotation_angle) - pos.z * sin(rotation_angle),
        pos.y,
        pos.x * sin(rotation_angle) + pos.z * cos(rotation_angle)
    ) * 1e-10;

    float large_turbulence = fbm(rotated_pos * 1.2, 5);
    float medium_wisps     = fbm(rotated_pos * 2.5, 4);
    float small_detail     = fbm(rotated_pos * 6.0, 3);
    float fine_detail      = fbm(rotated_pos * 10.0, 2);

    float noise_mask = large_turbulence * 0.4 +
                       medium_wisps * 0.3 +
                       small_detail * 0.2 +
                       fine_detail * 0.1;

    noise_mask = smoothstep(0.25, 0.75, noise_mask);

    float angle = atan2(pos.z, pos.x);
    float rotated_angle = angle + sim.time * 0.5;

    float spiral_arms = sin(rotated_angle * 3.0 + r_norm * 15.0) * 0.15 + 0.85;

    float orbital_angle = angle + sim.time * keplerian_speed * 0.8;
    float orbital_pattern = sin(orbital_angle * 2.0 + r_norm * 8.0) * 0.2 + 0.8;

    float density = vertical_falloff * radial_density * noise_mask * spiral_arms * orbital_pattern;
    return density * disk.disk_density;
}

struct Ray
{
    float x, y, z;
    float r, theta, phi;
    float dr, dtheta, dphi;
    float E, L;
};

Ray InitRay(float3 pos, float3 dir)
{
    Ray ray;
    ray.x     = pos.x;
    ray.y     = pos.y;
    ray.z     = pos.z;
    ray.r     = length(pos);
    ray.theta = acos(pos.z / ray.r);
    ray.phi   = atan2(pos.y, pos.x);

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

bool InterceptObject(Ray ray, inout Hit hit)
{
    float3 P = float3(ray.x, ray.y, ray.z);

    for (int i = 0; i < obj.numObjects; ++i)
    {
        float3 center = obj.objPosRadius[i].xyz;
        float  radius = obj.objPosRadius[i].w;

        float distSq = dot(P - center, P - center);
        if (distSq > radius * radius * 4.0)
            continue;

        if (distSq <= radius * radius)
        {
            hit.objectColor = obj.objColor[i];
            hit.hitCenter   = center;
            hit.hitRadius   = radius;
            return true;
        }
    }

    return false;
}

void GeodesicRHS(Ray ray, out float3 d1, out float3 d2)
{
    float r      = ray.r;
    float theta  = ray.theta;
    float dr     = ray.dr;
    float dtheta = ray.dtheta;
    float dphi   = ray.dphi;
    float f      = 1.0 - SagA_rs / r;
    float dt_dL  = ray.E / f;

    d1 = float3(dr, dtheta, dphi);
    d2.x = - (SagA_rs / (2.0 * r*r)) * f * dt_dL * dt_dL
           + (SagA_rs / (2.0 * r*r * f)) * dr * dr
           + r * (dtheta*dtheta + sin(theta)*sin(theta)*dphi*dphi);
    d2.y = -2.0*dr*dtheta/r + sin(theta)*cos(theta)*dphi*dphi;
    d2.z = -2.0*dr*dphi/r - 2.0*cos(theta)/(sin(theta)) * dtheta * dphi;
}

void RK4Step(inout Ray ray, float dL)
{
    float3 k1a, k1b;
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

bool IsInDiskVolume(float3 pos)
{
    float r_cyl = length(float2(pos.x, pos.z));
    return (r_cyl >= disk.disk_r1 && r_cyl <= disk.disk_r2 && abs(pos.y) <= disk.thickness);
}

float4 SampleDiskColor(float3 pos)
{
    float r_cyl  = length(float2(pos.x, pos.z));
    float r_norm = (r_cyl - disk.disk_r1) / (disk.disk_r2 - disk.disk_r1);

    float3 innerColor = float3(1.0, 0.9, 0.5);
    float3 midColor   = float3(1.0, 0.6, 0.2);
    float3 outerColor = float3(0.9, 0.3, 0.1);

    float3 baseColor;
    if (r_norm < 0.5)
        baseColor = lerp(innerColor, midColor, r_norm * 2.0);
    else
        baseColor = lerp(midColor, outerColor, (r_norm - 0.5) * 2.0);

    float r_norm_rot = (r_cyl - disk.disk_r1) / (disk.disk_r2 - disk.disk_r1);
    float keplerian_speed = 1.0 / sqrt(r_norm_rot + 0.1);

    float color_rotation_angle = sim.time * keplerian_speed * 0.3;
    float3 rotated_color_pos = float3(
        pos.x * cos(color_rotation_angle) - pos.z * sin(color_rotation_angle),
        pos.y,
        pos.x * sin(color_rotation_angle) + pos.z * cos(color_rotation_angle)
    ) * 1e-10;

    float large_color  = fbm(rotated_color_pos * 1.8, 4);
    float medium_color = fbm(rotated_color_pos * 4.0, 3);
    float small_color  = fbm(rotated_color_pos * 8.0, 2);
    float colorVariation = (large_color * 0.5 + medium_color * 0.3 + small_color * 0.2) * 0.6;
    baseColor = baseColor * (1.0 + colorVariation);

    float density = GetCloudDensity(pos);

    float brightness_rotation_angle = sim.time * keplerian_speed * 0.7;
    float3 rotated_brightness_pos = float3(
        pos.x * cos(brightness_rotation_angle) - pos.z * sin(brightness_rotation_angle),
        pos.y,
        pos.x * sin(brightness_rotation_angle) + pos.z * cos(brightness_rotation_angle)
    ) * 1e-10;

    float brightness_large  = fbm(rotated_brightness_pos * 3.0, 3);
    float brightness_medium = fbm(rotated_brightness_pos * 5.0, 2);
    float brightness_small  = fbm(rotated_brightness_pos * 7.0, 2);
    float brightness_noise  = (brightness_large * 0.6 + brightness_medium * 0.3 + brightness_small * 0.1);

    float baseBrightness = 1.0 + density * 1.5;
    float glowBrightness = brightness_noise * 0.8;
    float brightness = baseBrightness + glowBrightness;

    return float4(baseColor * brightness, density);
}

float CalculateAdaptiveStepSize(Ray ray, float baseStepSize)
{
    float r_factor         = clamp(ray.r / (SagA_rs * 10.0), 0.1, 1.0);
    float curvature        = length(float3(ray.dr, ray.dtheta * ray.r, ray.dphi * ray.r * sin(ray.theta)));
    float curvature_factor = clamp(1e12 / (curvature + 1e6), 0.1, 2.0);

    return clamp(baseStepSize * r_factor * curvature_factor, MIN_STEP_SIZE, MAX_STEP_SIZE);
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    float u = (2.0 * input.texCoord.x - 1.0) * cam.aspect * cam.tanHalfFov;
    float v = (1.0 - 2.0 * input.texCoord.y) * cam.tanHalfFov;
    float3 dir = normalize(u * cam.camRight - v * cam.camUp + cam.camForward);
    Ray ray = InitRay(cam.camPos, dir);

    float4 color = float4(0.0, 0.0, 0.0, 0.0);

    bool hitBlackHole = false;
    bool hitObject    = false;

    Hit hit;
    hit.objectColor = float4(0.0, 0.0, 0.0, 0.0);
    hit.hitCenter   = float3(0.0, 0.0, 0.0);
    hit.hitRadius   = 0.0;

    float4 accumulatedColor = float4(0.0, 0.0, 0.0, 0.0);
    float  transmittance = 1.0;

    int maxSteps = cam.moving ? sim.maxStepsMoving : sim.maxStepsStatic;

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

    float lambda = 0.0;
    int objectCheckInterval = 5;

    for (int i = 0; i < maxSteps; ++i)
    {
        float exitDistance = sim.earlyExitDistance > 0.0 ? sim.earlyExitDistance : DEFAULT_EARLY_EXIT_DISTANCE;
        if (ray.r > exitDistance)
            break;
        if (ray.r > ESCAPE_R)
            break;

        if (Intercept(ray, SagA_rs))
        {
            hitBlackHole = true;
            break;
        }

        float currentStepSize = CalculateAdaptiveStepSize(ray, D_LAMBDA);

        RK4Step(ray, currentStepSize);
        lambda += currentStepSize;

        float3 newPos = float3(ray.x, ray.y, ray.z);

        if (IsInDiskVolume(newPos))
        {
            float4 diskSample = SampleDiskColor(newPos);
            float  density    = diskSample.a;
            float3 diskColor  = diskSample.rgb;

            float stepLength = currentStepSize * 1e-8;

            float absorption = density * stepLength * 0.8;
            float scattering = density * stepLength * 1.5;
            float extinction = absorption + scattering;

            float stepTransmittance = exp(-extinction);

            float3 emission = diskColor * density * stepLength * 4.0 * sqrt(disk.disk_density);

            float3 glowColor       = lerp(diskColor, float3(1.0, 0.8, 0.6), 0.3);
            float  glowIntensity   = density * stepLength * 2.0;
            float3 atmosphericGlow = glowColor * glowIntensity * 0.8;

            float3 totalEmission = emission + atmosphericGlow;
            accumulatedColor.rgb += totalEmission * transmittance;

            transmittance *= stepTransmittance;

            if (transmittance < 0.01)
            {
                accumulatedColor.a = 1.0 - transmittance;
                break;
            }
        }

        if (i % objectCheckInterval == 0 && InterceptObject(ray, hit))
        {
            hitObject = true;
            break;
        }

        if (ray.dr > 0.0 && ray.r > SagA_rs * 100.0 && lambda > 2e8)
            break;
    }

    accumulatedColor.a = 1.0 - transmittance;

    if (hitBlackHole)
    {
        color = float4(0.0, 0.0, 0.0, 1.0);
    }
    else if (hitObject)
    {
        float3 P = float3(ray.x, ray.y, ray.z);
        float3 N = normalize(P - hit.hitCenter);
        float3 V = normalize(cam.camPos - P);

        float ambient   = 0.1;
        float diff      = max(dot(N, V), 0.0);
        float intensity = ambient + (1.0 - ambient) * diff;
        float3 shaded   = hit.objectColor.rgb * intensity;

        color = float4(shaded, hit.objectColor.a);
        color = lerp(accumulatedColor, color, color.a);
    }
    else
    {
        float3 rayDirection = normalize(float3(ray.x, ray.y, ray.z) - cam.camPos);
        float3 hdriColor = SampleHDRI(rayDirection);
        color = float4(lerp(accumulatedColor.rgb, hdriColor, 1.0 - accumulatedColor.a), 1.0);
    }

    return color;
}
