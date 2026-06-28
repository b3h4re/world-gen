#include "wavelet.hpp"

#include <stdexcept>


namespace wgen {

	    WaveletNoise2d::WaveletNoise2d(std::size_t gridWidth, std::size_t gridHeight, FloatFunction reconstructionKernel,
	                                   float frequency)
	    : WaveletNoise2d{gridWidth, gridHeight, std::random_device{}(), reconstructionKernel, frequency} {}

	    WaveletNoise2d::WaveletNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::uint32_t seed,
	                                   FloatFunction reconstructionKernel, float frequency)
	    : gridWidth_{gridWidth}, gridHeight_{gridHeight}, frequency_{frequency}, reconstructionKernel_{reconstructionKernel},
	        kernelWidth_{1}, kernelHeight_{1}, randomValues{gridWidth, gridHeight} {
	        if (frequency_ <= 0.0F) {
	            throw std::invalid_argument("Wavelet frequency must be positive");
	        }
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

    std::size_t WaveletNoise2d::wrapSignedIndex(std::ptrdiff_t index, std::size_t size) {
        const auto signedSize = static_cast<std::ptrdiff_t>(size);
        const auto wrapped = index % signedSize;

        if (wrapped < 0) {
            return static_cast<std::size_t>(wrapped + signedSize);
        }

        return static_cast<std::size_t>(wrapped);
    }

    float WaveletNoise2d::randomAt(std::ptrdiff_t i, std::ptrdiff_t j) const {
        return randomValues.at(
                    wrapSignedIndex(i, gridWidth_),
                    wrapSignedIndex(j, gridHeight_)
                );
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

        const float u_T = glm::mod<float>(u, gridWidth_);
        const float v_T = glm::mod<float>(v, gridHeight_);

        const auto i = static_cast<std::ptrdiff_t>(std::floor(u_T));
        const auto j = static_cast<std::ptrdiff_t>(std::floor(v_T));
        const auto kernelWidth = static_cast<std::ptrdiff_t>(kernelWidth_);
        const auto kernelHeight = static_cast<std::ptrdiff_t>(kernelHeight_);

        float noise = 0;
        for (auto m = i - kernelWidth; m <= i + kernelWidth; ++m ) {
            for (auto n = j - kernelHeight; n <= j + kernelHeight; ++n) {
                noise += waveletNoiseTile(m, n)
                        * reconstructionKernel_(u_T - static_cast<float>(m))
                        * reconstructionKernel_(v_T - static_cast<float>(n));
            }
        }
        return noise;
    }

}
