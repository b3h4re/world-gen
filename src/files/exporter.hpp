#pragma once

#include "terrain/terrain.hpp"
#include "utils/color_map.hpp"
#include "files/file_path.hpp"

namespace lve {

enum class ExportFormats {
    PNG,
    CSV
};

enum class DataFormats {
    ThreeDim,
    TwoDim
};

struct ExportConfig {
    ExportFormats fileFormat{ExportFormats::CSV};
    DataFormats dataFormat{DataFormats::TwoDim};
    wgen::files::Path path{""};
};


class Exporter {
public:
    explicit Exporter(ColorMapper& colorMapper);
    Exporter(ColorMapper& colorMapper, ExportConfig cfg);

    ExportConfig& cfg() { return cfg_; }

    void exportToFile(const wgen::HeightMap<float>& h);
    

private:
    void exportToCSV(const wgen::HeightMap<float>& h);
    void exportToPNG(const wgen::HeightMap<float>& h);
    ColorMapper& colorMapper_;
    ExportConfig cfg_;

};


}
