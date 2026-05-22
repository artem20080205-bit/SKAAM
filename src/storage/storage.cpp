#include "storage.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>

namespace storage {

static std::string data_dir;
namespace fs = std::filesystem;

void set_data_dir(const std::string& dir) {
    data_dir = dir;
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
}

std::string table_path(const std::string& name) {
    return data_dir + "/" + name + ".csv";
}

bool create_table(const std::string& name, const std::vector<Column>& cols) {
    if (table_exists(name)) return false;
    
    std::ofstream file(table_path(name));
    if (!file.is_open()) return false;
    
    // Первая строка: имена колонок
    for (size_t i = 0; i < cols.size(); ++i) {
        file << cols[i].name;
        if (i < cols.size() - 1) file << ",";
    }
    file << "\n";
    
    // Вторая строка: типы
    for (size_t i = 0; i < cols.size(); ++i) {
        file << cols[i].type;
        if (i < cols.size() - 1) file << ",";
    }
    file << "\n";
    
    file.close();
    return true;
}

bool drop_table(const std::string& name) {
    if (!table_exists(name)) return false;
    return fs::remove(table_path(name));
}

bool table_exists(const std::string& name) {
    return fs::exists(table_path(name));
}

Table load_table(const std::string& name) {
    Table table;
    table.name = name;
    
    std::ifstream file(table_path(name));
    if (!file.is_open()) return table;
    
    std::string line;
    
    // Читаем заголовки
    if (!std::getline(file, line)) return table;
    std::stringstream ss1(line);
    std::string col_name;
    while (std::getline(ss1, col_name, ',')) {
        Column col;
        col.name = col_name;
        table.columns.push_back(col);
    }
    
    // Читаем типы
    if (!std::getline(file, line)) return table;
    std::stringstream ss2(line);
    std::string col_type;
    size_t idx = 0;
    while (std::getline(ss2, col_type, ',')) {
        if (idx < table.columns.size()) {
            table.columns[idx++].type = col_type;
        }
    }
    
    // Читаем данные
    while (std::getline(file, line)) {
        std::vector<std::string> row;
        std::stringstream ss3(line);
        std::string value;
        while (std::getline(ss3, value, ',')) {
            row.push_back(value);
        }
        if (!row.empty() && row.size() == table.columns.size()) {
            table.rows.push_back(row);
        }
    }
    
    file.close();
    return table;
}

bool save_table(const Table& table) {
    std::ofstream file(table_path(table.name));
    if (!file.is_open()) return false;
    
    // Имена колонок
    for (size_t i = 0; i < table.columns.size(); ++i) {
        file << table.columns[i].name;
        if (i < table.columns.size() - 1) file << ",";
    }
    file << "\n";
    
    // Типы
    for (size_t i = 0; i < table.columns.size(); ++i) {
        file << table.columns[i].type;
        if (i < table.columns.size() - 1) file << ",";
    }
    file << "\n";
    
    // Данные
    for (const auto& row : table.rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            file << row[i];
            if (i < row.size() - 1) file << ",";
        }
        file << "\n";
    }
    
    file.close();
    return true;
}

} // namespace storage