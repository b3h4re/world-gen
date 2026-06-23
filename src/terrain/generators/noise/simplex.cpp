#include "simplex.hpp"

namespace wgen {

    SimplexNoise2d::SimplexNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell)
    : SimplexNoise2d{gridWidth, gridHeight, dotsPerCell, std::random_device{}()} {}

    SimplexNoise2d::SimplexNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::size_t dotsPerCell, std::uint32_t seed)
    : GradientNoise{gridWidth, gridHeight, dotsPerCell, seed} {}

    float SimplexNoise2d::noise(std::size_t x, std::size_t y) const {
        if (x >= sampleWidth() || y >= sampleHeight()) {
            throw std::out_of_range("Perlin sample coordinate is outside the gradient grid");
        }

        const std::size_t gridX = x / dotsPerCell_;
        const std::size_t gridY = y / dotsPerCell_;
        const float localX = static_cast<float>(x % dotsPerCell_) / static_cast<float>(dotsPerCell_);
        const float localY = static_cast<float>(y % dotsPerCell_) / static_cast<float>(dotsPerCell_);

        const float skewing = (localX + localY) * SKEWING_CONSTANT_F2;
        const float skewed_i = std::floor(localX + skewing);
        const float skewed_j = std::floor(localY + skewing);

        const float unskewing = (skewed_i + skewed_j) * UNSKEWING_CONSTANT_G2;
        const float simplex_cell_X0 = skewed_i - unskewing;
        const float simplex_cell_Y0 = skewed_j - unskewing;

        const glm::vec2 simplex_corner{simplex_cell_X0, simplex_cell_Y0};

        const float x0 = localX - simplex_cell_X0;
        const float y0 = localY - simplex_cell_Y0;

        // Deciding which triangle we are in
        // x0 > y0:  (i1​, j1​) = (1, 0)
        // x0 <= y0: (i1​, j1​) = (0, 1)
        const std::size_t i1 = x0 > y0 ? 1 : 0;
        const std::size_t j1 = x0 > y0 ? 0 : 1;
        // Now we have three corners in skewed coords: (i, j), (i + i1, j + j1), (i + 1, j + 1)
        // get gradients for those corners and then get points in real coordinates
        const glm::vec2 g0 = gradients_.at(gridX, gridY);
        const glm::vec2 g1 = gradients_.at(gridX + i1, gridY + j1);
        const glm::vec2 g2 = gradients_.at(gridX + 1, gridY + 1);

        const glm::vec2 d0 = {x0, y0};
        const glm::vec2 d1 = {
            x0 - static_cast<float>(i1) + UNSKEWING_CONSTANT_G2,
            y0 - static_cast<float>(j1) + UNSKEWING_CONSTANT_G2
        };
        const glm::vec2 d2 = {
            x0 - 1.0F + 2.0F * UNSKEWING_CONSTANT_G2,
            y0 - 1.0F + 2.0F * UNSKEWING_CONSTANT_G2
        };

        // now foreach corner we define rk**2 = xk**2 + yk**2
        // tk = 0.5 - rk**2
        const float t0 = 0.5F - d0.x*d0.x - d0.y*d0.y;
        const float t1 = 0.5F - d1.x*d1.x - d1.y*d1.y;
        const float t2 = 0.5F - d2.x*d2.x - d2.y*d2.y;

        const float n0 = t0 > 0.0F ? t0*t0*t0*t0 * glm::dot(g0, d0) : 0.0F;
        const float n1 = t0 > 0.0F ? t1*t1*t1*t1 * glm::dot(g1, d1) : 0.0F;
        const float n2 = t0 > 0.0F ? t2*t2*t2*t2 * glm::dot(g2, d2) : 0.0F;

        return NORMALIZATION_CONSTANT * (n0 + n1 + n2);
    }

}
