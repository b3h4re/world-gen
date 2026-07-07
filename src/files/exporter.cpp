#include "exporter.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>


namespace lve {

Exporter::Exporter(ColorMapper& colorMapper) : Exporter{static_cast<ColorMapSampler&>(colorMapper), ExportConfig{}} {}

Exporter::Exporter(ColorMapper& colorMapper, ExportConfig cfg)
    : Exporter{static_cast<ColorMapSampler&>(colorMapper), cfg} {}

Exporter::Exporter(ColorMapSampler& colorMapper) : Exporter{colorMapper, ExportConfig{}} {}

Exporter::Exporter(ColorMapSampler& colorMapper, ExportConfig cfg) : colorMapper_{colorMapper}, cfg_{cfg} {}


void Exporter::exportToFile(const wgen::HeightMap<float>& h) {
    switch (cfg_.fileFormat) {
        case ExportFormats::CSV:
            exportToCSV(h);
            return;
        case ExportFormats::PNG:
            exportToPNG(h);
            return;
    }
}

void Exporter::exportToCSV(const wgen::HeightMap<float>& h) {
    std::ofstream file(cfg_.path.get());
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + cfg_.path.get().string());
    }

    for (std::size_t y = 0; y < h.height(); ++y) {
        for (std::size_t x = 0; x < h.width(); ++x) {
            file << h.at(x, y);
            if (x < h.width() - 1) {
                file << ",";
            }
        }
        if (y < h.height() - 1) {
            file << "\n";
        }
    }
    file.close();
}

void Exporter::exportToPNG(const wgen::HeightMap<float>& h) {
    if (cfg_.dataFormat == DataFormats::ThreeDim) {
        throw std::runtime_error("3D PNG export is not implemented yet");
    }

    if (h.width() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        h.height() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("height map is too large to export as PNG");
    }

    std::vector<std::uint8_t> pixels(h.width() * h.height() * 4);
    for (std::size_t y = 0; y < h.height(); ++y) {
        for (std::size_t x = 0; x < h.width(); ++x) {
            const glm::vec4 color = colorMapper_.mapColor(h.at(x, y));
            const std::size_t offset = (y * h.width() + x) * 4;
            pixels[offset + 0] = static_cast<std::uint8_t>(color.r * 255.0F);
            pixels[offset + 1] = static_cast<std::uint8_t>(color.g * 255.0F);
            pixels[offset + 2] = static_cast<std::uint8_t>(color.b * 255.0F);
            pixels[offset + 3] = static_cast<std::uint8_t>(color.a * 255.0F);
        }
    }

    const int width = static_cast<int>(h.width());
    const int height = static_cast<int>(h.height());
    constexpr int channels = 4;
    const int strideBytes = width * channels;

    const int result = stbi_write_png(
        cfg_.path.string().c_str(),
        width,
        height,
        channels,
        pixels.data(),
        strideBytes
    );

    if (result == 0) {
        throw std::runtime_error("Could not write PNG file: " + cfg_.path.string());
    }
}

}
