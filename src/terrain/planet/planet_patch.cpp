#include "planet_patch.hpp"

#include <stdexcept>
#include <tuple>

namespace wgen {
namespace {

bool isKnownFace(CubeSphereFace face) noexcept {
    switch (face) {
        case CubeSphereFace::Top:
        case CubeSphereFace::Bottom:
        case CubeSphereFace::Right:
        case CubeSphereFace::Left:
        case CubeSphereFace::Back:
        case CubeSphereFace::Front:
            return true;
    }
    return false;
}

void hashCombine(std::size_t& seed, std::size_t value) noexcept {
    seed ^= value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
}

} // namespace

std::size_t PlanetPatchIdHash::operator()(const PlanetPatchId& id) const noexcept {
    std::size_t seed = static_cast<std::size_t>(id.face);
    hashCombine(seed, id.level);
    hashCombine(seed, id.x);
    hashCombine(seed, id.y);
    return seed;
}

bool PlanetPatchIdLess::operator()(const PlanetPatchId& lhs, const PlanetPatchId& rhs) const noexcept {
    return std::tie(lhs.face, lhs.level, lhs.x, lhs.y) <
        std::tie(rhs.face, rhs.level, rhs.x, rhs.y);
}

std::uint32_t patchesPerAxis(std::uint8_t level) {
    if (level > MAX_PLANET_PATCH_LEVEL) {
        throw std::invalid_argument{"planet patch level exceeds the supported maximum"};
    }
    return std::uint32_t{1} << level;
}

bool isValid(const PlanetPatchId& id) {
    if (!isKnownFace(id.face) || id.level > MAX_PLANET_PATCH_LEVEL) {
        return false;
    }

    const std::uint32_t count = std::uint32_t{1} << id.level;
    return id.x < count && id.y < count;
}

void validate(const PlanetPatchId& id) {
    if (!isKnownFace(id.face)) {
        throw std::invalid_argument{"planet patch face is invalid"};
    }
    if (id.level > MAX_PLANET_PATCH_LEVEL) {
        throw std::invalid_argument{"planet patch level exceeds the supported maximum"};
    }

    const std::uint32_t count = patchesPerAxis(id.level);
    if (id.x >= count || id.y >= count) {
        throw std::invalid_argument{"planet patch coordinates are outside the level bounds"};
    }
}

} // namespace wgen
