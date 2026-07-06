#include "exporter.hpp"

#include "files/csv_handler.hpp"

#include <fstream>


namespace lve {

Exporter::Exporter() : Exporter{ExportConfig{}} {}

Exporter::Exporter(ExportConfig cfg) : cfg_{cfg} {}


void Exporter::exportToFile(const wgen::HeightMap<float>& h) {
    switch (cfg_.fileFormat) {
        case ExportFormats::CSV:
            exportToCSV(h);
            return;
        case ExportFormats::PNG:
            return;
    }
}

void Exporter::exportToCSV(const wgen::HeightMap<float>& h) {
    std::ofstream file(cfg_.path.get());
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + cfg_.path.get().string());
    }

    for (std::size_t x = 0; x < h.width(); ++x) {
        for (std::size_t y = 0; y < h.height(); ++y) {
            file << h.at(x, y);
            if (y < h.height() - 1) {
                file << ",";
            }
        }
        if (x < h.width() - 1) {
            file << "\n";
        }
    }
    file.close();
}


}