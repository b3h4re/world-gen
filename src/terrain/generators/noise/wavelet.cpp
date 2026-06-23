#include "wavelet.hpp"


namespace wgen {

    WaveletNoise2d::WaveletNoise2d(std::size_t gridWidth, std::size_t gridHeight, FloatFunction funcInterpolate)
    : WaveletNoise2d{gridWidth, gridHeight, std::random_device{}(), funcInterpolate} {}

    WaveletNoise2d::WaveletNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::uint32_t seed, FloatFunction funcInterpolate)
    : gridWidth_{gridWidth}, gridHeight_{gridHeight}, funcInterpolate_{funcInterpolate}, randomValues{gridWidth, gridHeight} {
        setSeed(seed);
    }

    void WaveletNoise2d::generateRandomValues() {
        std::mt19937 random{getSeed()};
        std::uniform_real_distribution<float> noise{-1.0F, 1.0F};

        for (std::size_t y = 0; y < gridHeight_; ++y) {
            for (std::size_t x = 0; x < gridWidth_; ++x) {
                randomValues.at(x, y) = noise(random);
            }
        }
    }

    float WaveletNoise2d::separableFilter(int a, int b) const {
        return hFilter(a) * hFilter(b);
    }

    float WaveletNoise2d::randomAt(std::size_t i, std::size_t j) const {
        return randomValues.at(
                    wrapIndex(i, gridWidth_),
                    wrapIndex(j, gridHeight_)
                );
    }

    float WaveletNoise2d::smoothedGridAt(std::size_t i, std::size_t j) const {
        float res = 0;
        for (int a = -1; a <= 1; ++a) {
            for (int b = -1; b <= 1; ++b) {
                res += separableFilter(a, b) * randomAt(i - a, j - b);
            }
        }
        return res;
    }

    float WaveletNoise2d::waveletNoiseTile(std::size_t x, std::size_t y) const {
        return randomAt(x, y) - smoothedGridAt(x, y);
    }

    float WaveletNoise2d::noise(std::size_t x, std::size_t y) const {
        const float u = static_cast<float>(x) * FREQUENCY_SCALE;
        const float v = static_cast<float>(y) * FREQUENCY_SCALE;

        const float u_T = glm::mod<float>(u, gridWidth_);
        const float v_T = glm::mod<float>(v, gridHeight_);

        const std::size_t i = static_cast<std::size_t>(std::floor(u_T));
        const std::size_t j = static_cast<std::size_t>(std::floor(v_T));

        // local coordinates
        const float alpha = u_T - static_cast<float>(i);
        const float beta = v_T - static_cast<float>(j);

        const float W00 = waveletNoiseTile(i, j);
        const float W01 = waveletNoiseTile(i, j + 1);
        const float W10 = waveletNoiseTile(i + 1, j);
        const float W11 = waveletNoiseTile(i + 1, j + 1);

        const float alpha_smooth = funcInterpolate_(alpha);
        const float beta_smooth = funcInterpolate_(beta);
        const float AA = lerp(W00, W10, alpha_smooth);
        const float BB = lerp(W01, W11, beta_smooth);

        return lerp(AA, BB, beta_smooth);
    }

}
