#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_gpu_shader_fp64 : require

#include "constants.glsl"


uint64_t u64(uint hi, uint lo)
{
    return packUint2x32(uvec2(lo, hi));
}


uint64_t splitmix64(uint64_t x) {
    x += uint64_t(0x9E3779B97F4A7C15ul);
    x = (x ^ (x >> 30ul)) * uint64_t(0xBF58476D1CE4E5B9ul);
    x = (x ^ (x >> 27ul)) * uint64_t(0x94D049BB133111EBul);
    return x ^ (x >> 31ul);
}

uint64_t hashValues(uint64_t seed, uint x, uint y) {
    uint64_t h = splitmix64(seed);
    h = splitmix64(h ^ splitmix64(uint64_t(x)));
    h = splitmix64(h ^ splitmix64(uint64_t(y)));
    return h;
}

float hashUnitFloat(uint64_t hash) {
    uint value = uint(hash >> 40ul);
    return float(value) * (1.0 / 16777216.0);
}

vec2 randomHashDir(uint x, uint y, uint64_t seed) {
    uint64_t h = seed;
    uint64_t c0x9E3779B97F4A7C15 = u64(0x9E3779B9u, 0x7F4A7C15u);
    uint64_t c0xBF58476D1CE4E5B9 = u64(0xBF58476Du, 0x1CE4E5B9u);

    h ^= splitmix64(uint64_t(x) + c0x9E3779B97F4A7C15);
    h ^= splitmix64(uint64_t(y) + c0xBF58476D1CE4E5B9);
    h = splitmix64(h);

    float u = float(h >> 11ul) * (1.0f / 9007199254740992.0f);
    float angle = u * 2.0f * PI;
    vec2 res = vec2(cos(angle), sin(angle));
    return res;
}
