#pragma once

#include "device/lve_device.hpp"
#include "game/objects/game_object_3d.hpp"
#include "model/buffer/lve_buffer.hpp"
#include "terrain/planet/local_clipmap_mesh.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace lve {

inline constexpr std::size_t LOCAL_CLIPMAP_CENTER_TRIANGLES_INDEX_VARIANT = 0;
inline constexpr std::size_t LOCAL_CLIPMAP_RING_TRIANGLES_INDEX_VARIANT = 1;
inline constexpr std::size_t LOCAL_CLIPMAP_CENTER_LINES_INDEX_VARIANT = 2;
inline constexpr std::size_t LOCAL_CLIPMAP_RING_LINES_INDEX_VARIANT = 3;

struct LocalClipmapRenderObject {
    GameObject3d object{};
    std::uint32_t level{};
    bool cacheCurrent{};
};

// Owns every Vulkan object specific to the standalone local clipmap path.
// Grid/ring indices are immutable. Heights are updated in mapped per-level
// buffers using Step 10's toroidal upload batches; CPU-position meshes are the
// temporary fallback until displacement moves into the vertex shader.
class LocalClipmapRenderResources {
public:
    LocalClipmapRenderResources(
        LveDevice& device,
        wgen::LocalClipmapConfig config);

    LocalClipmapRenderResources(const LocalClipmapRenderResources&) = delete;
    LocalClipmapRenderResources& operator=(
        const LocalClipmapRenderResources&) = delete;

    void applyHeightUpload(const wgen::LocalClipmapGpuUploadBatch& batch);
    void applyMeshUpdate(
        LocalClipmapMeshUpdate update,
        std::vector<GameObject3d>& retiredObjects);
    void clear();

    const std::vector<LocalClipmapRenderObject>& objects() const {
        return objects_;
    }
    const wgen::LocalClipmapConfig& config() const { return config_; }

private:
    struct ResidentLevel {
        GameObject3d object{};
        wgen::LocalClipmapUpdateIdentity identity{};
        bool cacheCurrent{};
    };

    static std::vector<std::uint32_t> triangleEdges(
        std::span<const std::uint32_t> triangles);
    void rebuildObjects();

    LveDevice& device_;
    wgen::LocalClipmapConfig config_{};
    wgen::LocalClipmapTopology topology_{};
    std::shared_ptr<const Mesh3dIndexSet> indexSet_{};
    std::vector<std::unique_ptr<LveBuffer>> heightBuffers_{};
    std::vector<std::optional<wgen::LocalClipmapUpdateIdentity>>
        heightBufferIdentities_{};
    std::vector<std::optional<ResidentLevel>> residentLevels_{};
    std::vector<LocalClipmapRenderObject> objects_{};
};

} // namespace lve
