#pragma once

#include "device/lve_device.hpp"
#include "pipeline/lve_pipeline.hpp"
#include "renderer/lve_frame_info.hpp"

#include <memory>

namespace lve {

class RenderSystemLocalClipmap {
public:
    RenderSystemLocalClipmap(LveDevice& device, VkRenderPass renderPass);
    ~RenderSystemLocalClipmap();

    RenderSystemLocalClipmap(const RenderSystemLocalClipmap&) = delete;
    RenderSystemLocalClipmap& operator=(
        const RenderSystemLocalClipmap&) = delete;

    void render(FrameInfo& frameInfo) const;

private:
    void createPipelineLayout();
    void createPipelines(VkRenderPass renderPass);

    LveDevice& device_;
    VkPipelineLayout pipelineLayout_{};
    std::unique_ptr<LvePipeline> fillPipeline_{};
    std::unique_ptr<LvePipeline> linePipeline_{};
};

} // namespace lve
