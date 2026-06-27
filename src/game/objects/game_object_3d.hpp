#pragma once

#include "renderer/objects/mesh_3d.hpp"

#include <glm/glm.hpp>
#include <memory>

namespace lve {

struct Transform3d {
    glm::vec3 translation{};
    glm::vec3 scale{1.0F};
    glm::vec3 rotation{};

    glm::mat4 mat4() const;
};

struct GameObject3d {
    std::shared_ptr<Mesh3d> mesh;
    Transform3d transform{};
};

} // namespace lve
