#pragma once

#include "renderer/objects/mesh_2d.hpp"

#include <glm/glm.hpp>
#include <memory>

namespace lve {

struct Transform2d {
    glm::vec2 translation{};
    glm::vec2 scale{1.0F};
    float rotation{};

    glm::mat4 mat4() const;
};

struct GameObject2d {
    std::shared_ptr<Mesh2d> mesh;
    Transform2d transform{};
};

} // namespace lve
