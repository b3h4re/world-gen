#include "wavelet.hpp"
#include "terrain/utils/hash_random.hpp"

#include <stdexcept>
#include <random>


namespace wgen {

    WaveletNoise2d::WaveletNoise2d(glm::vec<2, std::size_t> ker, glm::vec4 params, FloatFunction reconstructionKernel)
                : WaveletNoise2d{std::random_device{}(), ker, params, reconstructionKernel} {}

    WaveletNoise2d::WaveletNoise2d(SeedType seed, glm::vec<2, std::size_t> ker, glm::vec4 params, FloatFunction reconstructionKernel)
                : reconstructionKernel_{reconstructionKernel}, filterParams_{params.x, params.y, params.z}, kernelWidth_{ker.x}, kernelHeight_{ker.y}, frequency_{params.w} {
        if (frequency_ <= 0.0F) {
            std::string message = "Wavelet frequency must be positive: freq = ";
            message += std::to_string(frequency_);
            throw std::invalid_argument(message);
        }
        if (std::abs(filterParams_.x + filterParams_.y + filterParams_.z - 1.0F) > 0.0001f) {
            std::string message = "Wavelet separable filter params should sum up to one: A + B + C = ";
            message += std::to_string(filterParams_.x) + " + " + std::to_string(filterParams_.y) + " + " + std::to_string(filterParams_.z);
            message += " = " + std::to_string(filterParams_.x + filterParams_.y + filterParams_.z);
            throw std::invalid_argument(message);
        }
        hFilter = getLowPassFilter(filterParams_.x, filterParams_.y, filterParams_.z);
        setSeed(seed);
    }

    void WaveletNoise2d::setKernelSize(std::size_t kernelWidth, std::size_t kernelHeight) {
        setKernelHeight(kernelHeight);
        setKernelWidth(kernelWidth);
    }
    void WaveletNoise2d::setKernelWidth(std::size_t kernelWidth) {
        kernelWidth_ = kernelWidth;
    }
    void WaveletNoise2d::setKernelHeight(std::size_t kernelHeight) {
        kernelHeight_ = kernelHeight;
    }

    std::size_t WaveletNoise2d::getKernelWidth() const { return kernelWidth_; }
    std::size_t WaveletNoise2d::getKernelHeight() const { return kernelHeight_; }

    float WaveletNoise2d::separableFilter(int a, int b) const {
        return hFilter(a) * hFilter(b);
    }

    std::size_t WaveletNoise2d::wrapSignedIndex(std::ptrdiff_t index, std::size_t size) {
        const auto signedSize = static_cast<std::ptrdiff_t>(size);
        const auto wrapped = index % signedSize;

        if (wrapped < 0) {
            return static_cast<std::size_t>(wrapped + signedSize);
        }

        return static_cast<std::size_t>(wrapped);
    }

    float WaveletNoise2d::randomAt(std::ptrdiff_t i, std::ptrdiff_t j) const {
        // const auto x = wrapSignedIndex(i, gridWidth_);
        // const auto y = wrapSignedIndex(j, gridHeight_);
        const auto key = hashValues(getSeed(), static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j));

        HashUniformRealDistribution<float> noise{-1.0F, 1.0F};
        return noise(key);
    }

    float WaveletNoise2d::smoothedGridAt(std::ptrdiff_t i, std::ptrdiff_t j) const {
        float res = 0;
        for (int a = -1; a <= 1; ++a) {
            for (int b = -1; b <= 1; ++b) {
                res += separableFilter(a, b) * randomAt(i - a, j - b);
            }
        }
        return res;
    }

    float WaveletNoise2d::waveletNoiseTile(std::ptrdiff_t x, std::ptrdiff_t y) const {
        return randomAt(x, y) - smoothedGridAt(x, y);
    }

    float WaveletNoise2d::noise(std::size_t x, std::size_t y) const {
        const float u = static_cast<float>(x) * frequency_;
        const float v = static_cast<float>(y) * frequency_;

        // const float u_T = glm::mod<float>(u, gridWidth_);
        // const float v_T = glm::mod<float>(v, gridHeight_);

        const auto i = static_cast<std::ptrdiff_t>(std::floor(u));
        const auto j = static_cast<std::ptrdiff_t>(std::floor(v));
        const auto kernelWidth = static_cast<std::ptrdiff_t>(kernelWidth_);
        const auto kernelHeight = static_cast<std::ptrdiff_t>(kernelHeight_);

        float noise = 0;
        for (auto m = i - kernelWidth; m <= i + kernelWidth; ++m ) {
            for (auto n = j - kernelHeight; n <= j + kernelHeight; ++n) {
                noise += waveletNoiseTile(m, n)
                        * reconstructionKernel_(u - static_cast<float>(m))
                        * reconstructionKernel_(v - static_cast<float>(n));
            }
        }
        return noise;
    }

    GeneratorCapabilities WaveletNoise2d::capabilities() const {
        return {
            .cpu = true,
            .vulkanCompute = true,
            .octaves = true,
        };
    }

    std::string WaveletNoise2d::compShader() const {
        return "wavelet_noise";
    }

    std::size_t WaveletNoise2d::specSize() const {
        return sizeof(WaveletNoiseComputeSpec);
    }

}
