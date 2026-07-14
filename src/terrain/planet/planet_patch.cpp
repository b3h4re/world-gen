#include "planet_patch.hpp"

#include <tuple>

namespace wgen {
namespace {

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

} // namespace wgen
