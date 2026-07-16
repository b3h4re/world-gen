#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace lve {

class Camera3d {
public:
    void setPerspectiveProjection(float fovy, float aspectRatio, float near, float far);
    void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = {0.0F, 1.0F, 0.0F});
    void setGlobalViewTarget(
        glm::dvec3 position,
        glm::dvec3 target,
        glm::dvec3 up = {0.0, 1.0, 0.0});
    void rebaseRenderOrigin(glm::dvec3 origin);

    const glm::mat4 &projection() const { return projection_; }
    const glm::mat4 &view() const { return view_; }
    glm::mat4 projectionView() const { return projection_ * view_; }
    const glm::mat4& renderView() const { return renderView_; }
    glm::mat4 renderProjectionView() const { return projection_ * renderView_; }
    glm::vec3 positionRelativeToRenderOrigin(
        glm::dvec3 globalPosition) const noexcept;
    const glm::vec3& position() const { return position_; }
    const glm::vec3& forward() const { return forward_; }
    const glm::vec3& right() const { return right_; }
    const glm::vec3& up() const { return up_; }
    const glm::dvec3& globalPosition() const { return globalPosition_; }
    const glm::dvec3& globalForward() const { return globalForward_; }
    const glm::dvec3& globalRight() const { return globalRight_; }
    const glm::dvec3& globalUp() const { return globalUp_; }
    const glm::dvec3& renderOrigin() const { return renderOrigin_; }
    float verticalFov() const { return verticalFov_; }
    float aspectRatio() const { return aspectRatio_; }
    float nearPlane() const { return nearPlane_; }
    float farPlane() const { return farPlane_; }

private:
    void updateRenderView();

    glm::mat4 projection_{1.0F};
    glm::mat4 view_{1.0F};
    glm::mat4 renderView_{1.0F};
    glm::vec3 position_{};
    glm::vec3 forward_{0.0F, 0.0F, -1.0F};
    glm::vec3 right_{1.0F, 0.0F, 0.0F};
    glm::vec3 up_{0.0F, 1.0F, 0.0F};
    glm::dvec3 globalPosition_{};
    glm::dvec3 globalForward_{0.0, 0.0, -1.0};
    glm::dvec3 globalRight_{1.0, 0.0, 0.0};
    glm::dvec3 globalUp_{0.0, 1.0, 0.0};
    glm::dvec3 renderOrigin_{};
    float verticalFov_{};
    float aspectRatio_{};
    float nearPlane_{};
    float farPlane_{};
};

} // namespace lve
