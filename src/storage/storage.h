#pragma once
#include <string>
#include <vector>
#include <map>

namespace storage {

struct Column {
    std::string name;
    std::string type;  // INT, TEXT, FLOAT, BOOL
};

struct Table {
    std::string name;
    std::vector<Column> columns;
    std::vector<std::vector<std::string>> rows;  // все значения как строки
};

void set_data_dir(const std::string& dir);
bool create_table(const std::string& name, const std::vector<Column>& cols);
bool drop_table(const std::string& name);
bool table_exists(const std::string& name);
Table load_table(const std::string& name);
bool save_table(const Table& table);

} // namespace storage