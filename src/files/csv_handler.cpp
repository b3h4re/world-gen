#include "csv_handler.hpp"


namespace lve {

CSVTable::CSVTable() : data_{} {}

glm::vec<2, std::size_t> CSVTable::getPosFromRowColumn(const std::size_t& row, const std::string& column) {
    if (row == 0) {
        throw std::invalid_argument("Table rows start with 1");
    }
    glm::vec<2, std::size_t> pos;
    pos.y = row - 1;


    if (column.empty()) {
        throw std::invalid_argument("Column name cannot be empty");
    }

    std::size_t columnNumber = 0;
    for (char ch : column) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        if (ch < 'A' || ch > 'Z') {
            throw std::invalid_argument("Column name must contain only letters A-Z");
        }

        const std::size_t value = static_cast<std::size_t>(ch - 'A' + 1);
        columnNumber = 26 * columnNumber + value;
    }
    pos.x = columnNumber - 1;

    return pos;
}

std::pair<std::size_t, std::string> getRowColumnFromPos(std::size_t columnId, std::size_t rowId) {
    std::pair<std::size_t, std::string> tablePos;
    tablePos.first = rowId + 1;

    std::string columnName = "";

    while (columnId > 0) {
        std::size_t value = columnId % 26;
        char ch = static_cast<char>(value) - 1 + 'A';
        columnId /= 26;
        columnName += ch;
    }
    tablePos.second = columnName;
    return tablePos;
}

}