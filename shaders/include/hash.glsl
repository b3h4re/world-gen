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

uint64_t hashValues(uint64_t seed, uint x) {
    uint64_t h = splitmix64(seed);
    h = splitmix64(h ^ splitmix64(uint64_t(x)));
    return h;
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

float toUnitFloat(uint64_t h)
{
    const float scale = 1.0 / 16777216.0; // 1 / 2^24

    return float((h >> 40) & uint64_t(0xFFFFFFu)) * scale;
}

float toSignedFloat(uint64_t h) {
    // [0, 1) -> [-1, 1)
    return 2.0f * toUnitFloat(h) - 1.0f;
}

uint64_t makeKey(int i, int j) {
    uint ui = uint(i);
    uint uj = uint(j);
    return (uint64_t(ui) << 32) | uint64_t(uj);
}

vec2 hash2(int i, int j) {
    uint64_t key = makeKey(i, j);
    uint64_t h1 = splitmix64(key);
    uint64_t h2 = splitmix64(h1);
    return vec2(toSignedFloat(h1), toSignedFloat(h2));
}

uint64_t hashSeed(uint64_t seed) {
    uint64_t h = splitmix64(seed);
    return h ^ (h >> 32);
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

uint64_t intAsUint64(int value) {
    return uint64_t(int64_t(value));
}

vec3 randomHashDir3D(int x, int y, int z, uint64_t seed) {
    uint64_t h = seed;
    const uint64_t c0x9E3779B97F4A7C15 = u64(0x9E3779B9u, 0x7F4A7C15u);
    const uint64_t c0xBF58476D1CE4E5B9 = u64(0xBF58476Du, 0x1CE4E5B9u);
    const uint64_t c0x94D049BB133111EB = u64(0x94D049BBu, 0x133111EBu);

    h ^= splitmix64(intAsUint64(x) + c0x9E3779B97F4A7C15);
    h ^= splitmix64(intAsUint64(y) + c0xBF58476D1CE4E5B9);
    h ^= splitmix64(intAsUint64(z) + c0x94D049BB133111EB);

    h = splitmix64(h);

    const float u1 = float(h >> 11) * (1.0 / 9007199254740992.0);

    h = splitmix64(h);

    const float u2 = float(h >> 11) * (1.0 / 9007199254740992.0);

    const float zNew = 1.0f - 2.0f * u1;
    const float phi = u2 * 2.0f * PI;
    const float r = sqrt(max(0.0f, 1.0f - zNew * zNew));
    vec3 res = vec3(
        r * cos(phi),
        r * sin(phi),
        zNew
    );
    return res;
}

float random(uint64_t seed, float min, float max) {
    return min + (max - min) * hashUnitFloat(seed);
}

int poisson(float lambda, uint64_t seed) {
    const float limit = exp(-lambda);
    float prod = 1.0f;
    int k = 0;

    while (prod > limit) {
        ++k;
        prod *= toUnitFloat(hashValues(seed, uint(k)));
    }

    return k - 1;
}
