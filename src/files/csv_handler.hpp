#pragma once

#include "terrain/terrain.hpp"

#include <string>


namespace lve {

// x - columns
// y - rows
// All csv on export will be comma separated 
class CSVTable {
public:

    CSVTable();

    template<typename T>
    requires requires (const T& t) { std::to_string(t); }
    CSVTable(const wgen::HeightMap<T>& h) : data_{h.width(), h.height()} {
        for (std::size_t x = 0; x < h.width(); ++x) {
            for (std::size_t y = 0; y < h.height(); ++y) {
                data_.at(x, y) = std::to_string(h.at(x, y));
            }
        }
    }
    
    template<typename T>
    requires requires (const T& t) { std::to_string(t); }
    void set(const T& value, const std::size_t& row, const std::string& column) {
        auto pos = getPosFromRowColumn(row, column);
        if (pos.x >= data_.width() || pos.y >= data_.height()) {
            data_.resize(pos.x + 1, pos.y + 1);
        }
        data_.at(pos) = std::to_string(value);
    }

    std::string get(const std::size_t& row, const std::string& column) { return data_.at(getPosFromRowColumn(row, column)); }


    static glm::vec<2, std::size_t> getPosFromRowColumn(const std::size_t& row, const std::string& column);

    static std::pair<std::size_t, std::string> getRowColumnFromPos(std::size_t rowId, std::size_t columnId);


private:
    wgen::HeightMap<std::string> data_;
};

}