#ifndef WORLD_GEN_HYBRID_COVERAGE_GLSL
#define WORLD_GEN_HYBRID_COVERAGE_GLSL

float hybridSquareCoverage(
        vec2 localPositionMeters,
        vec4 coverageParameters) {
    float distanceToCenter = max(
        abs(localPositionMeters.x - coverageParameters.x),
        abs(localPositionMeters.y - coverageParameters.y));
    return 1.0 - smoothstep(
        coverageParameters.z,
        coverageParameters.w,
        distanceToCenter);
}

float hybridDitherThreshold(ivec2 pixel) {
    const float bayer4x4[16] = float[16](
         0.0,  8.0,  2.0, 10.0,
        12.0,  4.0, 14.0,  6.0,
         3.0, 11.0,  1.0,  9.0,
        15.0,  7.0, 13.0,  5.0);
    int x = pixel.x & 3;
    int y = pixel.y & 3;
    return (bayer4x4[y * 4 + x] + 0.5) / 16.0;
}

bool hybridKeepsLocal(float coverage, ivec2 pixel) {
    return hybridDitherThreshold(pixel) < coverage;
}

bool hybridKeepsGlobal(float coverage, ivec2 pixel) {
    return !hybridKeepsLocal(coverage, pixel);
}

vec2 hybridTangentPosition(
        vec3 surfaceDirection,
        vec3 frameEast,
        vec3 frameNorth,
        vec3 frameUp,
        float planetRadiusMeters) {
    vec3 direction = normalize(surfaceDirection);
    float cosine = clamp(dot(frameUp, direction), -1.0, 1.0);
    vec3 tangentComponent = direction - frameUp * cosine;
    float sine = length(tangentComponent);
    if (sine <= 1.0e-7) {
        return cosine >= 0.0 ? vec2(0.0) : vec2(1.0e30);
    }
    vec3 tangent = tangentComponent / sine;
    float distanceMeters = atan(sine, cosine) * planetRadiusMeters;
    return vec2(dot(tangent, frameEast), dot(tangent, frameNorth)) *
        distanceMeters;
}

#endif
