#pragma once


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
    ExportFormats fileFormat;
    DataFormats dataFormat;
};


}