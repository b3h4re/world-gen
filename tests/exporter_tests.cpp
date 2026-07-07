#include "files/exporter.hpp"
#include "files/file_path.hpp"
#include "helpers.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class FakeColorMapSampler : public lve::ColorMapSampler {
public:
    glm::vec4 mapColor(float height) const override {
        const float red = std::clamp((height + 1.0F) / 2.0F, 0.0F, 1.0F);
        return {red, 0.25F, 0.5F, 1.0F};
    }
};

std::filesystem::path exporterTestDirectory() {
    const std::filesystem::path path = wgen::files::Path::tmpDirectory() / "world-gen-exporter-tests";
    std::filesystem::create_directories(path);
    return path;
}

std::filesystem::path exporterTestPath(const std::string& fileName) {
    const std::filesystem::path path = exporterTestDirectory() / fileName;
    std::filesystem::remove(path);
    return path;
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file{path};
    if (!file) {
        throw std::runtime_error{"failed to open exported text file"};
    }

    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

std::vector<std::uint8_t> readBinaryFile(const std::filesystem::path& path) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        throw std::runtime_error{"failed to open exported binary file"};
    }

    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

void testTmpDirectoryExists() {
    const std::filesystem::path tmp = wgen::files::Path::tmpDirectory();
    wgen::tests::require(!tmp.empty(), "tmp directory should not be empty");
    wgen::tests::require(std::filesystem::exists(tmp), "tmp directory should exist");
}

void testCsvExportWritesHeightMapRows() {
    FakeColorMapSampler colorMap;
    const std::filesystem::path path = exporterTestPath("terrain.csv");

    lve::Exporter exporter{
        colorMap,
        lve::ExportConfig{
            .fileFormat = lve::ExportFormats::CSV,
            .dataFormat = lve::DataFormats::TwoDim,
            .path = wgen::files::Path{path},
        }
    };

    const auto map = wgen::tests::makeMap<float>(3, 2, {
        1.0F, 2.0F, 3.0F,
        4.0F, 5.0F, 6.0F,
    });
    exporter.exportToFile(map);

    wgen::tests::require(
        readTextFile(path) == "1,2,3\n4,5,6",
        "CSV export should write one terrain row per line"
    );
}

void testPngExportWritesPngFile() {
    FakeColorMapSampler colorMap;
    const std::filesystem::path path = exporterTestPath("terrain.png");

    lve::Exporter exporter{
        colorMap,
        lve::ExportConfig{
            .fileFormat = lve::ExportFormats::PNG,
            .dataFormat = lve::DataFormats::TwoDim,
            .path = wgen::files::Path{path},
        }
    };

    const auto map = wgen::tests::makeMap<float>(2, 2, {
        -1.0F, 0.0F,
        0.5F, 1.0F,
    });
    exporter.exportToFile(map);

    const std::vector<std::uint8_t> bytes = readBinaryFile(path);
    const std::vector<std::uint8_t> pngSignature{137, 80, 78, 71, 13, 10, 26, 10};
    wgen::tests::require(bytes.size() > pngSignature.size(), "PNG export should write non-empty file");
    wgen::tests::require(
        std::equal(pngSignature.begin(), pngSignature.end(), bytes.begin()),
        "PNG export should write a PNG signature"
    );

    wgen::tests::require(bytes[16] == 0 && bytes[17] == 0 && bytes[18] == 0 && bytes[19] == 2,
                         "PNG export should write width 2 in IHDR");
    wgen::tests::require(bytes[20] == 0 && bytes[21] == 0 && bytes[22] == 0 && bytes[23] == 2,
                         "PNG export should write height 2 in IHDR");
}

void testThreeDimensionalPngExportThrows() {
    FakeColorMapSampler colorMap;
    const std::filesystem::path path = exporterTestPath("terrain_3d.png");

    lve::Exporter exporter{
        colorMap,
        lve::ExportConfig{
            .fileFormat = lve::ExportFormats::PNG,
            .dataFormat = lve::DataFormats::ThreeDim,
            .path = wgen::files::Path{path},
        }
    };

    const auto map = wgen::tests::makeMap<float>(2, 2, {
        -1.0F, 0.0F,
        0.5F, 1.0F,
    });

    wgen::tests::requireThrows<std::runtime_error>(
        [&] { exporter.exportToFile(map); },
        "3D PNG export should throw until screenshot export exists"
    );
}

} // namespace

int main() {
    try {
        wgen::tests::runTest("tmp directory exists", testTmpDirectoryExists);
        wgen::tests::runTest("CSV export writes heightmap rows", testCsvExportWritesHeightMapRows);
        wgen::tests::runTest("PNG export writes PNG file", testPngExportWritesPngFile);
        wgen::tests::runTest("3D PNG export throws", testThreeDimensionalPngExportThrows);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
