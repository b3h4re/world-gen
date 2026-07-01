#pragma once

#include "terrain/generators/generator.hpp"

#include <cstddef>
#include <cstdint>

namespace wgen {

    class WaveletNoise2d : public Generator {
    public:
        using FloatFunction = float (*)(float);
        // Filter parameters
        constexpr static float A = 0.25F;
        constexpr static float B = 0.5F;
        constexpr static float C = 0.25F;


        constexpr static float DEFAULT_FREQUENCY = 0.014231234F;

        WaveletNoise2d(std::size_t gridWidth, std::size_t gridHeight, std::uint32_t seed,
                        FloatFunction reconstructionKernel = defaultReconstructionKernel,
                        float frequency = DEFAULT_FREQUENCY);
        WaveletNoise2d(std::size_t gridWidth, std::size_t gridHeight,
                        FloatFunction reconstructionKernel = defaultReconstructionKernel,
                        float frequency = DEFAULT_FREQUENCY);

        void setSeed(std::uint32_t newSeed) override {
            Generator::setSeed(newSeed);
            generateRandomValues();
        }

        void setKernelSize(std::size_t kernelWidth, std::size_t kernelHeight);
        void setKernelWidth(std::size_t kernelWidth);
        void setKernelHeight(std::size_t kernelHeight);

        std::size_t getKernelWidth() const;
        std::size_t getKernelHeight() const;

        float noise(std::size_t x, std::size_t y) const override;


    private:
        FloatFunction reconstructionKernel_;

        std::size_t kernelWidth_;
        std::size_t kernelHeight_;

	        std::size_t gridWidth_;
	        std::size_t gridHeight_;
	        float frequency_;
	        HeightMap<float> randomValues;

        void generateRandomValues();
        static std::size_t wrapSignedIndex(std::ptrdiff_t index, std::size_t size);
        float randomAt(std::ptrdiff_t i, std::ptrdiff_t j) const;
        inline constexpr static auto hFilter = &lowPassFilter<A, B, C>;
        float separableFilter(int a, int b) const;
        float smoothedGridAt(std::ptrdiff_t i, std::ptrdiff_t j) const;
        float waveletNoiseTile(std::ptrdiff_t i, std::ptrdiff_t j) const;

    };

}
