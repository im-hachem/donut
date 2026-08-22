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
    float disk_r1;      // inner edge (clamped to the ISCO, 3 r_s, below)
    float disk_r2;      // outer edge
    float disk_num;     // turbulence strength (0 = smooth physical disk)
    float thickness;    // unused by the thin-disk model; kept for UBO layout
    float disk_density; // overall disk brightness / exposure
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

// Schwarzschild radius of Sgr A* (metres). Geometric units with c = G = 1 are
// used throughout the geodesic integration; the black-hole mass is M = r_s / 2.
static const float SagA_rs   = 1.269e10;
static const float D_LAMBDA  = 1e7;
static const float ESCAPE_R  = 1e30;

static const float R_ISCO     = 3.0 * SagA_rs;   // innermost stable circular orbit (6M)
static const float R_PHOTON   = 1.5 * SagA_rs;   // photon sphere (3M)
static const float FLUX_PEAK  = 0.0569;          // peak of the r^-3(1-sqrt(r_in/r)) profile (at r/r_in ~ 1.36)

static const int   DEFAULT_MAX_STEPS_MOVING    = 12000;
static const int   DEFAULT_MAX_STEPS_STATIC    = 8000;
static const float DEFAULT_EARLY_EXIT_DISTANCE = 2e12;

static const float MIN_STEP_SIZE = 1e6;
static const float MAX_STEP_SIZE = 2e10;

// Display mapping for the (relative) Novikov-Thorne flux -> visible colour.
// The RADIAL PROFILE is physical; the absolute temperature scale is a display
// choice (a real Sgr A* disk is far cooler / redder than this).
static const float T_PEAK        = 4800.0;   // Kelvin at the flux peak (display scale)
static const float DISK_EXPOSURE = 0.9;      // overall brightness of the disk

struct Hit
{
    float4 objectColor;
    float3 hitCenter;
    float  hitRadius;
};

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
    float3 shift = float3(100, 200, 300);
    for (int i = 0; i < octaves; ++i)
    {
        v += a * noise(x);
        x = x * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

// Planckian-locus blackbody colour (Tanner Helland approximation), T in Kelvin.
// Returns an sRGB-ish chromaticity normalised so the brightest channel ~ 1.
float3 Blackbody(float T)
{
    T = clamp(T, 1000.0, 40000.0);
    float t = T / 100.0;
    float3 c;

    c.r = (t <= 66.0) ? 1.0
                      : clamp(1.292936186 * pow(t - 60.0, -0.1332047592), 0.0, 1.0);

    c.g = (t <= 66.0) ? clamp(0.3900815788 * log(t) - 0.6318414438, 0.0, 1.0)
                      : clamp(1.1298908609 * pow(t - 60.0, -0.0755148492), 0.0, 1.0);

    c.b = (t >= 66.0) ? 1.0
        : (t <= 19.0) ? 0.0
                      : clamp(0.5432067891 * log(t - 10.0) - 1.1962540891, 0.0, 1.0);
    return c;
}

// Emission from the thin accretion disk at an equatorial crossing point P, seen
// along the (backward-traced) ray direction rayDir. Combines a Novikov-Thorne
// temperature profile with the full gravitational + Doppler redshift.
//   g = sqrt(1 - 3M/r) / (1 - beta . nhat)      (verified: g -> sqrt(1/2) at ISCO)
// Brightness follows relativistic beaming (I_obs = g^4 I_emit); colour follows
// the redshifted blackbody at T_obs = g * T_emit.
float3 DiskEmission(float3 P, float3 rayDir)
{
    float rc   = length(float2(P.x, P.z));   // cylindrical radius (disk axis = +Y)
    float rin  = max(disk.disk_r1, R_ISCO);
    float rout = disk.disk_r2;
    if (rc < rin || rc > rout)
        return float3(0.0);

    // Novikov-Thorne-style radial flux: F(r) ~ r^-3 (1 - sqrt(r_in/r)), zero at
    // the inner edge, peaking just outside it, then declining. T ~ F^(1/4).
    float xr   = rc / rin;
    float flux = max((1.0 - sqrt(1.0 / xr)) / (xr * xr * xr), 0.0);
    float Tn   = pow(flux / FLUX_PEAK, 0.25);            // normalised temperature, peak ~ 1
    float Temit = T_PEAK * Tn;

    // Keplerian orbit (prograde about +Y). Locally-measured orbital speed for a
    // Schwarzschild circular geodesic: v = sqrt( M / (r - 2M) ) = 0.5 c at ISCO.
    float3 rhat   = normalize(float3(P.x, 0.0, P.z));
    float3 phiHat = normalize(cross(float3(0.0, 1.0, 0.0), rhat));
    float  v      = sqrt((SagA_rs * 0.5) / max(rc - SagA_rs, 1.0));
    float3 beta   = v * phiHat;
    float3 nhat   = -normalize(rayDir);                  // photon direction toward the observer

    float g = sqrt(max(1.0 - 1.5 * SagA_rs / rc, 0.0)) / max(1.0 - dot(beta, nhat), 1e-3);

    float  Tobs   = g * Temit;
    float3 colour = Blackbody(Tobs);
    // Physical bolometric intensity is ~ T_emit^4 * g^4, an enormous dynamic
    // range. The g^4 relativistic beaming (the physical asymmetry) is kept; the
    // radial falloff is display-compressed (Tn^2) so the colour gradient across
    // the disk stays visible instead of collapsing to a thin saturated ring.
    float  bright = pow(Tn, 2.0) * pow(g, 4.0);

    // Soft inner/outer edges (disks have no hard rim); also tames rim aliasing.
    float edge = smoothstep(rin, rin * 1.12, rc) * (1.0 - smoothstep(rout * 0.88, rout, rc));
    bright *= edge;

    // Optional turbulence overlay (disk.disk_num = strength; 0 = smooth).
    if (disk.disk_num > 0.0)
    {
        float ang = sim.time * 0.3 / sqrt(xr);
        float3 rp = float3(P.x * cos(ang) - P.z * sin(ang),
                           0.0,
                           P.x * sin(ang) + P.z * cos(ang)) * 1e-10;
        float turb = 1.0 + disk.disk_num * (fbm(rp * 3.0, 3) - 0.5);
        bright *= max(turb, 0.0);
    }

    float exposure = DISK_EXPOSURE * max(disk.disk_density, 0.0) * 10.0;
    return colour * bright * exposure;
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

    float dx = dir.x, dy = dir.y, dz = dir.z;

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
                       sin(ray.theta)*sin(ray.theta)*ray.dphi*ray.dphi));
    ray.E = f * dt_dL;
    return ray;
}

bool Intercept(Ray ray, float rs) { return ray.r <= rs; }

bool InterceptObject(Ray ray, inout Hit hit)
{
    float3 P = float3(ray.x, ray.y, ray.z);
    for (int i = 0; i < obj.numObjects; ++i)
    {
        float3 center = obj.objPosRadius[i].xyz;
        float  radius = obj.objPosRadius[i].w;
        float distSq = dot(P - center, P - center);
        if (distSq > radius * radius * 4.0) continue;
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

float CalculateAdaptiveStepSize(Ray ray, float baseStepSize)
{
    // Step proportional to the distance from the photon sphere: near-flat space
    // far from the hole is crossed in a few huge steps, while the sharply curved
    // region near the photon sphere is resolved with tiny ones. This keeps the
    // integration accurate near the hole regardless of how far the camera is.
    float step = 0.02 * max(ray.r - R_PHOTON, 0.0);

    // Slow down when near the disk plane (within its radial extent) so the thin
    // slab is never stepped over -- otherwise grazing rays leak through it.
    float rc = length(float2(ray.x, ray.z));
    if (rc < disk.disk_r2 * 3.0 && abs(ray.y) < disk.thickness * 8.0)
        step = min(step, disk.thickness);

    return clamp(step, MIN_STEP_SIZE, MAX_STEP_SIZE);
}

float3 ACESFilm(float3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// Trace one primary ray for the given image UV and return its linear,
// pre-tone-map radiance. Called once per sub-sample by fragmentMain.
float3 TracePixel(float2 texCoord)
{
    float u = (2.0 * texCoord.x - 1.0) * cam.aspect * cam.tanHalfFov;
    float v = (1.0 - 2.0 * texCoord.y) * cam.tanHalfFov;
    float3 dir = normalize(u * cam.camRight - v * cam.camUp + cam.camForward);
    Ray ray = InitRay(cam.camPos, dir);

    bool  hitBlackHole = false;
    bool  hitObject    = false;
    Hit   hit;
    hit.objectColor = float4(0.0);
    hit.hitCenter   = float3(0.0);
    hit.hitRadius   = 0.0;

    bool   hitDisk   = false;
    float3 diskColor = float3(0.0);   // emission of the first (opaque) disk surface hit

    int maxSteps = cam.moving ? sim.maxStepsMoving : sim.maxStepsStatic;
    if (maxSteps <= 0)
        maxSteps = cam.moving ? DEFAULT_MAX_STEPS_MOVING : DEFAULT_MAX_STEPS_STATIC;

    float exitDistance = sim.earlyExitDistance > 0.0 ? sim.earlyExitDistance : DEFAULT_EARLY_EXIT_DISTANCE;
    int   objectCheckInterval = 5;

    for (int i = 0; i < maxSteps; ++i)
    {
        if (Intercept(ray, SagA_rs)) { hitBlackHole = true; break; }
        if (ray.r > exitDistance || ray.r > ESCAPE_R) break;

        float3 prevPos = float3(ray.x, ray.y, ray.z);
        float  stepSize = CalculateAdaptiveStepSize(ray, D_LAMBDA);
        RK4Step(ray, stepSize);
        float3 newPos = float3(ray.x, ray.y, ray.z);

        // Opaque disk of small half-thickness H (a slab about the midplane y=0).
        // The ray hits when it first crosses the midplane OR enters the slab
        // while grazing along it. Real (nonzero) thickness stops the zero-height
        // edge-on "razor" from aliasing into a beam streaking across the frame.
        {
            float H       = disk.thickness;
            bool  crossed = prevPos.y * newPos.y < 0.0;
            bool  inSlab  = abs(newPos.y) <= H;
            if (crossed || inSlab)
            {
                float3 hitP = crossed
                    ? lerp(prevPos, newPos, prevPos.y / (prevPos.y - newPos.y))
                    : newPos;
                float rc = length(float2(hitP.x, hitP.z));
                if (rc >= max(disk.disk_r1, R_ISCO) && rc <= disk.disk_r2)
                {
                    diskColor = DiskEmission(hitP, newPos - prevPos);
                    hitDisk   = true;
                    break;
                }
            }
        }

        if (i % objectCheckInterval == 0 && InterceptObject(ray, hit)) { hitObject = true; break; }

        // Principled escape: once outbound in near-flat spacetime (r >> r_s) the
        // ray direction no longer changes, so stop and read the background.
        if (ray.dr > 0.0 && ray.r > 50.0 * SagA_rs) break;
    }

    // Escape direction + environment mip LOD from the ray's angular divergence.
    // Computed UNCONDITIONALLY (before the branch) so ddx/ddy are valid; strongly
    // lensed background rays diverge fast, so they read a blurred cubemap mip and
    // the starfield stops aliasing into a fan along the equatorial plane.
    float3 rayDir    = normalize(float3(ray.x, ray.y, ray.z) - cam.camPos);
    float  footprint = max(length(ddx(rayDir)), length(ddy(rayDir)));
    float  envLod    = clamp(log2(max(footprint / 0.0015, 1.0)), 0.0, 10.0);

    float3 shade;
    if (hitDisk)
    {
        shade = diskColor;                    // opaque, self-luminous disk surface
    }
    else if (hitBlackHole)
    {
        shade = float3(0.0);                  // event-horizon shadow
    }
    else if (hitObject)
    {
        float3 P = float3(ray.x, ray.y, ray.z);
        float3 N = normalize(P - hit.hitCenter);
        float3 V = normalize(cam.camPos - P);
        float  intensity = 0.1 + 0.9 * max(dot(N, V), 0.0);
        shade = hit.objectColor.rgb * intensity;
    }
    else
    {
        shade = u_HDRIEnvironment.SampleLevel(rayDir, envLod).rgb;
    }

    return shade;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    // Moving frame: one sample for responsiveness. Settled frame: rotated-grid
    // 4x supersampling (the 4-rook pattern gives 4 distinct sub-pixel positions
    // on BOTH axes, far better on the near-horizontal lensed edges than an
    // ordered grid). Radiance is averaged before tone-mapping; ddx/ddy give the
    // resolution-correct per-pixel UV footprint.
    if (cam.moving)
        return float4(ACESFilm(TracePixel(input.texCoord)), 1.0);

    float2 dUV = float2(ddx(input.texCoord.x), ddy(input.texCoord.y));
    float2 offs[4] = {
        float2( 0.125,  0.375), float2( 0.375, -0.125),
        float2(-0.125, -0.375), float2(-0.375,  0.125),
    };
    float3 sum = float3(0.0);
    for (int i = 0; i < 4; ++i)
        sum += TracePixel(input.texCoord + offs[i] * dUV);

    return float4(ACESFilm(sum * 0.25), 1.0);
}
