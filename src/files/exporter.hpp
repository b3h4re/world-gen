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
    Exporter();
    Exporter(ExportConfig cfg);

    ExportConfig& cfg() { return cfg_; }

    void exportToFile(const wgen::HeightMap<float>& h);
    

private:
    void exportToCSV(const wgen::HeightMap<float>& h);
    ExportConfig cfg_;

};


}