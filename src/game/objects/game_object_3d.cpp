#include "game_object_3d.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace lve {

glm::mat4 Transform3d::mat4() const {
    glm::mat4 transform{1.0F};
    transform = glm::translate(transform, translation);
    transform = glm::rotate(transform, rotation.y, {0.0F, 1.0F, 0.0F});
    transform = glm::rotate(transform, rotation.x, {1.0F, 0.0F, 0.0F});
    transform = glm::rotate(transform, rotation.z, {0.0F, 0.0F, 1.0F});
    return glm::scale(transform, scale);
}

} // namespace lve
