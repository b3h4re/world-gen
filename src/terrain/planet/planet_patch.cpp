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

bool isKnownEdge(PlanetPatchEdge edge) noexcept {
    switch (edge) {
        case PlanetPatchEdge::UMin:
        case PlanetPatchEdge::UMax:
        case PlanetPatchEdge::VMin:
        case PlanetPatchEdge::VMax:
            return true;
    }
    return false;
}

void hashCombine(std::size_t& seed, std::size_t value) noexcept {
    seed ^= value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
}

void validateChildBits(std::uint8_t xBit, std::uint8_t yBit) {
    if (xBit > 1 || yBit > 1) {
        throw std::invalid_argument{"planet patch child bits must be zero or one"};
    }
}

void validateCanSubdivide(const PlanetPatchId& id) {
    if (id.level == MAX_PLANET_PATCH_LEVEL) {
        throw std::overflow_error{"planet patch child level exceeds the supported maximum"};
    }
}

std::uint32_t childCoordinate(std::uint32_t coordinate, std::uint8_t bit) {
    const std::uint64_t result = std::uint64_t{coordinate} * 2 + bit;
    if (result > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error{"planet patch child coordinate overflows uint32_t"};
    }
    return static_cast<std::uint32_t>(result);
}

PlanetPatchId makeChild(const PlanetPatchId& id, std::uint8_t xBit, std::uint8_t yBit) {
    return {
        id.face,
        static_cast<std::uint8_t>(id.level + 1),
        childCoordinate(id.x, xBit),
        childCoordinate(id.y, yBit),
    };
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

std::optional<PlanetPatchId> parent(const PlanetPatchId& id) {
    validate(id);
    if (id.level == 0) {
        return std::nullopt;
    }

    return PlanetPatchId{
        id.face,
        static_cast<std::uint8_t>(id.level - 1),
        id.x / 2,
        id.y / 2,
    };
}

PlanetPatchId child(const PlanetPatchId& id, std::uint8_t xBit, std::uint8_t yBit) {
    validate(id);
    validateChildBits(xBit, yBit);
    validateCanSubdivide(id);
    return makeChild(id, xBit, yBit);
}

std::array<PlanetPatchId, 4> children(const PlanetPatchId& id) {
    validate(id);
    validateCanSubdivide(id);
    return {
        makeChild(id, 0, 0),
        makeChild(id, 1, 0),
        makeChild(id, 0, 1),
        makeChild(id, 1, 1),
    };
}

std::array<PlanetPatchId, 2> childrenTouchingEdge(
        const PlanetPatchId& id,
        PlanetPatchEdge edge) {
    validate(id);
    if (!isKnownEdge(edge)) {
        throw std::invalid_argument{"planet patch edge is invalid"};
    }
    validateCanSubdivide(id);

    switch (edge) {
        case PlanetPatchEdge::UMin:
            return {makeChild(id, 0, 0), makeChild(id, 0, 1)};
        case PlanetPatchEdge::UMax:
            return {makeChild(id, 1, 0), makeChild(id, 1, 1)};
        case PlanetPatchEdge::VMin:
            return {makeChild(id, 0, 0), makeChild(id, 1, 0)};
        case PlanetPatchEdge::VMax:
            return {makeChild(id, 0, 1), makeChild(id, 1, 1)};
    }
    throw std::invalid_argument{"planet patch edge is invalid"};
}

} // namespace wgen
