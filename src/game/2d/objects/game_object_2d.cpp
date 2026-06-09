#include "game_object_2d.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace lve {

glm::mat4 Transform2d::mat4() const {
    glm::mat4 transform{1.0F};
    transform = glm::translate(transform, {translation, 0.0F});
    transform = glm::rotate(transform, rotation, {0.0F, 0.0F, 1.0F});
    return glm::scale(transform, {scale, 1.0F});
}

} // namespace lve
