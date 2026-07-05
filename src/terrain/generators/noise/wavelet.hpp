#pragma once

#include "terrain/generators/generator.hpp"

#include <functional>
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

        WaveletNoise2d(
            SeedType seed,
            glm::vec<2, std::size_t> ker = {1, 1},
            glm::vec4 params = glm::vec4{A, B, C, DEFAULT_FREQUENCY},
            FloatFunction reconstructionKernel = defaultReconstructionKernel
        );
        WaveletNoise2d(
            glm::vec<2, std::size_t> ker = {1, 1},
            glm::vec4 params = glm::vec4{A, B, C, DEFAULT_FREQUENCY},
            FloatFunction reconstructionKernel = defaultReconstructionKernel
        );

        void setKernelSize(std::size_t kernelWidth, std::size_t kernelHeight);
        void setKernelWidth(std::size_t kernelWidth);
        void setKernelHeight(std::size_t kernelHeight);

        std::size_t getKernelWidth() const;
        std::size_t getKernelHeight() const;

        float noise(std::size_t x, std::size_t y) const override;
        GeneratorCapabilities capabilities() const override;
        virtual std::string compShader() const override;
        virtual std::size_t specSize() const override;


    private:
        FloatFunction reconstructionKernel_;

        glm::vec3 filterParams_;
        std::size_t kernelWidth_;
        std::size_t kernelHeight_;

        float frequency_;

        static std::size_t wrapSignedIndex(std::ptrdiff_t index, std::size_t size);
        float randomAt(std::ptrdiff_t i, std::ptrdiff_t j) const;
        std::function<float(int)> hFilter;
        float separableFilter(int a, int b) const;
        float smoothedGridAt(std::ptrdiff_t i, std::ptrdiff_t j) const;
        float waveletNoiseTile(std::ptrdiff_t i, std::ptrdiff_t j) const;

    };

}
