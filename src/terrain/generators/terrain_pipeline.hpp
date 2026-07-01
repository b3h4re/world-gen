#pragma once

#include "generator.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>


namespace wgen {

    template <typename Gen, typename... Args>
    std::unique_ptr<Gen> makeGenerator(Args&&... args) {
        return std::make_unique<Gen>(std::forward<Args>(args)...);
    }

    template <typename Func>
    concept HeightTransform =
        std::invocable<Func&, float> &&
        std::convertible_to<std::invoke_result_t<Func&, float>, float>;

    class TerrainPipeline : public Generator {
    public:
        TerrainPipeline() = default;

        float noise(std::size_t x, std::size_t y) const override;
        void setSeed(SeedType newSeed) override;
        void push_back(std::unique_ptr<Generator> generator);
        void push_back(std::unique_ptr<Generator> generator, HeightFunc impact);

        template <typename Gen, HeightTransform Func, typename... Args>
        void push_back(Func&& func, Args&&... args) {
            generators_.push_back(std::make_unique<Gen>(std::forward<Args>(args)...));
            generatorsImpact_.emplace_back(std::forward<Func>(func));
        }

        template <typename Gen>
        void push_back() {
            generators_.push_back(std::make_unique<Gen>());
            generatorsImpact_.push_back(multiplyFunction(1.0F));
        }

        template <typename Gen, typename First, typename... Rest>
            requires (!HeightTransform<First>)
        void push_back(First&& first, Rest&&... rest) {
            generators_.push_back(std::make_unique<Gen>(std::forward<First>(first), std::forward<Rest>(rest)...));
            generatorsImpact_.push_back(multiplyFunction(1.0F));
        }

        HeightMap<float> generateHeightMap(std::size_t width, std::size_t height) const override;

    private:
        std::vector<std::unique_ptr<Generator>> generators_{};
        std::vector<HeightFunc> generatorsImpact_{};

    };

}
