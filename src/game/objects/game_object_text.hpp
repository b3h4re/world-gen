#pragma once

#include "game/objects/game_object_2d.hpp"
#include "renderer/objects/text_mesh.hpp"

#include <memory>

namespace lve {

struct GameObjectText {
    std::shared_ptr<TextMesh> mesh;
    Transform2d transform{};
};

} // namespace lve
