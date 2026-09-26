// Zenith optical depths: the Rayleigh coefficients over an 8 km scale height per channel, Mie over 1.2 km
static const float3 k_RayleighDepth = float3(0.0464, 0.1082, 0.2648);
static const float k_MieDepth = 0.025;
static const float k_MieAnisotropy = 0.76;

// Single scattering alone leaves the sky about a third darker than measured; this stands in for the higher orders
static const float k_MultipleScatteringBoost = 1.6;

// The real disc, 0.53 degrees across, and a cap on its radiance so the RGBA16F target never overflows
static const float k_SunAngularRadius = 0.00465;
static const float k_MaxSkyRadiance = 4096.0;

// Directions below the horizon see the sky mirrored and darkened to a ground bounce
static const float k_GroundAlbedo = 0.3;

// Kasten and Young relative air mass: 1 at the zenith, about 37 at the horizon, held there below it
float AirMass(float cosZenith)
{
    const float l_Cos = saturate(cosZenith);
    const float l_ZenithDegrees = degrees(acos(l_Cos));

    return 1.0 / (l_Cos + 0.15 * pow(93.885 - l_ZenithDegrees, -1.253));
}

float3 SkyTransmittance(float airMass)
{
    return exp(-(k_RayleighDepth + k_MieDepth) * airMass);
}

float RayleighPhase(float cosTheta)
{
    return 3.0 / (16.0 * k_Pi) * (1.0 + cosTheta * cosTheta);
}

// Cornette-Shanks Mie phase, forward peaked so the glow sits around the sun
float MiePhase(float cosTheta, float anisotropy)
{
    const float l_G2 = anisotropy * anisotropy;
    const float l_Numerator = 3.0 / (8.0 * k_Pi) * (1.0 - l_G2) * (1.0 + cosTheta * cosTheta);
    const float l_Denominator = (2.0 + l_G2) * pow(1.0 + l_G2 - 2.0 * anisotropy * cosTheta, 1.5);

    return l_Numerator / l_Denominator;
}

// Radiance of the sky along a unit direction, without the sun disc: the direct sun term on the surfaces covers that.
// The tint scales the Rayleigh term only, so a warm tint reddens the sky without touching the glow around the sun
float3 EvaluateSky(float3 direction, float3 towardsSun, float3 sunRadiance, float3 tint)
{
    const float3 l_Direction = float3(direction.x, abs(direction.y), direction.z);
    const float l_CosTheta = dot(l_Direction, towardsSun);

    // Light scattered towards the eye is what the sun had left after its own path through the air
    const float3 l_SunTransmittance = SkyTransmittance(AirMass(towardsSun.y));

    // The view ray integrates exp(-depth * s) over its air mass, per channel
    const float3 l_Depth = k_RayleighDepth + k_MieDepth;
    const float3 l_Integral = (1.0 - exp(-l_Depth * AirMass(l_Direction.y))) / l_Depth;

    const float3 l_Rayleigh = k_RayleighDepth * RayleighPhase(l_CosTheta) * tint;
    const float l_Mie = k_MieDepth * MiePhase(l_CosTheta, k_MieAnisotropy);

    float3 l_Sky = sunRadiance * l_SunTransmittance * (l_Rayleigh + l_Mie) * l_Integral * k_MultipleScatteringBoost;

    // Below the horizon the mirrored sky fades to a ground bounce over the first few degrees
    l_Sky *= lerp(1.0, k_GroundAlbedo, saturate(-direction.y * 10.0));

    return l_Sky;
}

// The sun disc itself, for the sky pass only: the sun's irradiance spread over its solid angle, dimmed by the air
float3 EvaluateSunDisc(float3 direction, float3 towardsSun, float3 sunRadiance)
{
    const float l_CosTheta = dot(direction, towardsSun);
    const float l_CosRadius = cos(k_SunAngularRadius);

    // A soft edge a tenth of the radius wide keeps the disc from shimmering
    const float l_Disc = smoothstep(l_CosRadius - 0.1 * (1.0 - l_CosRadius), l_CosRadius, l_CosTheta) * step(0.0, direction.y);
    const float l_SolidAngle = 2.0 * k_Pi * (1.0 - l_CosRadius);
    const float3 l_Radiance = min(sunRadiance * SkyTransmittance(AirMass(towardsSun.y)) / l_SolidAngle, k_MaxSkyRadiance);

    return l_Radiance * l_Disc;
}