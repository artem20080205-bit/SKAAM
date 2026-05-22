#include "executor.h"
#include "../storage/storage.h"
#include "../api/database.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <set>
#include <vector>
#include <fstream>
#include <filesystem>

using namespace mydb;

namespace executor {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::string to_upper(const std::string& s) {
    std::string result = s;
    for (char& c : result) c = std::toupper(c);
    return result;
}

static bool check_condition(const std::string& condition, const std::vector<std::string>& row, 
                             const std::vector<storage::Column>& columns) {
    if (condition.empty()) return true;
    
    std::string cond = trim(condition);
    
    std::string op;
    size_t op_pos = std::string::npos;
    
    for (const auto& oper : {"!=", "=", ">=", "<=", ">", "<"}) {
        size_t pos = cond.find(oper);
        if (pos != std::string::npos) {
            op = oper;
            op_pos = pos;
            break;
        }
    }
    
    if (op_pos == std::string::npos) {
        return true;
    }
    
    std::string col_name = trim(cond.substr(0, op_pos));
    std::string value = trim(cond.substr(op_pos + op.length()));
    
    if (value.length() >= 2 && value.front() == '\'' && value.back() == '\'') {
        value = value.substr(1, value.length() - 2);
    }
    
    int col_index = -1;
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].name == col_name) {
            col_index = i;
            break;
        }
    }
    if (col_index == -1) return false;
    
    std::string row_value = row[col_index];
    
    if (op == "=") return row_value == value;
    if (op == "!=") return row_value != value;
    if (op == ">") return row_value > value;
    if (op == "<") return row_value < value;
    if (op == ">=") return row_value >= value;
    if (op == "<=") return row_value <= value;
    
    return true;
}

Result create_table(const std::string& sql) {
    std::string after_create = sql.substr(std::string("CREATE TABLE").length());
    after_create = trim(after_create);
    
    size_t paren_pos = after_create.find('(');
    if (paren_pos == std::string::npos) {
        return {false, "Invalid CREATE TABLE syntax: missing (", {}, 0};
    }
    
    std::string table_name = trim(after_create.substr(0, paren_pos));
    std::string columns_str = after_create.substr(paren_pos + 1);
    size_t close_paren = columns_str.rfind(')');
    if (close_paren == std::string::npos) {
        return {false, "Invalid CREATE TABLE syntax: missing )", {}, 0};
    }
    columns_str = columns_str.substr(0, close_paren);
    
    std::vector<storage::Column> columns;
    
    std::stringstream ss(columns_str);
    std::string col_def;
    
    while (std::getline(ss, col_def, ',')) {
        col_def = trim(col_def);
        if (col_def.empty()) continue;
        
        if (to_upper(col_def).find("FOREIGN") != std::string::npos) {
            continue;
        }
        
        std::stringstream col_ss(col_def);
        std::string col_name, col_type;
        col_ss >> col_name >> col_type;
        
        if (col_name.empty() || col_type.empty()) {
            return {false, "Invalid column definition: " + col_def, {}, 0};
        }
        
        storage::Column col;
        col.name = col_name;
        col.type = to_upper(col_type);
        
        std::set<std::string> valid_types = {"INT", "TEXT", "FLOAT", "BOOL"};
        if (valid_types.find(col.type) == valid_types.end()) {
            return {false, "Unknown type: " + col.type, {}, 0};
        }
        
        columns.push_back(col);
    }
    
    if (storage::create_table(table_name, columns)) {
        return {true, "", {}, 0};
    } else {
        return {false, "Table already exists: " + table_name, {}, 0};
    }
}

Result drop_table(const std::string& sql) {
    std::string after_drop = sql.substr(std::string("DROP TABLE").length());
    after_drop = trim(after_drop);
    
    if (storage::drop_table(after_drop)) {
        return {true, "", {}, 0};
    } else {
        return {false, "Table does not exist: " + after_drop, {}, 0};
    }
}

Result insert(const std::string& sql) {
    std::string after_insert = sql.substr(std::string("INSERT").length());
    after_insert = trim(after_insert);
    
    if (after_insert.find("INTO") != 0) {
        return {false, "Missing INTO", {}, 0};
    }
    after_insert = after_insert.substr(std::string("INTO").length());
    after_insert = trim(after_insert);
    
    size_t values_pos = after_insert.find("VALUES");
    if (values_pos == std::string::npos) {
        return {false, "Missing VALUES", {}, 0};
    }
    
    std::string table_name = trim(after_insert.substr(0, values_pos));
    std::string values_str = after_insert.substr(values_pos + std::string("VALUES").length());
    values_str = trim(values_str);
    
    if (!values_str.empty() && values_str.back() == ';') {
        values_str.pop_back();
    }
    
    if (!storage::table_exists(table_name)) {
        return {false, "Table does not exist: " + table_name, {}, 0};
    }
    
    storage::Table table = storage::load_table(table_name);
    int rows_added = 0;
    
    size_t pos = 0;
    while (pos < values_str.length()) {
        while (pos < values_str.length() && std::isspace(values_str[pos])) pos++;
        
        if (values_str[pos] != '(') {
            break;
        }
        pos++;
        
        std::vector<std::string> row;
        std::string current_value;
        bool in_quote = false;
        
        while (pos < values_str.length()) {
            char c = values_str[pos];
            
            if (c == '\'' && !in_quote) {
                in_quote = true;
                current_value += c;
            } 
            else if (c == '\'' && in_quote) {
                in_quote = false;
                current_value += c;
            }
            else if (c == ',' && !in_quote) {
                row.push_back(trim(current_value));
                current_value.clear();
            }
            else if (c == ')' && !in_quote) {
                row.push_back(trim(current_value));
                rows_added++;
                
                if (row.size() != table.columns.size()) {
                    return {false, "Column count mismatch", {}, 0};
                }
                table.rows.push_back(row);
                pos++;
                break;
            }
            else {
                current_value += c;
            }
            pos++;
        }
        
        while (pos < values_str.length() && (values_str[pos] == ',' || std::isspace(values_str[pos]))) {
            pos++;
        }
    }
    
    if (rows_added == 0) {
        return {false, "No valid rows to insert", {}, 0};
    }
    
    if (storage::save_table(table)) {
        Result res;
        res.success = true;
        res.affected_rows = rows_added;
        return res;
    } else {
        return {false, "Failed to save table", {}, 0};
    }
}

Result select(const std::string& sql) {
    std::string after_select = sql.substr(std::string("SELECT").length());
    after_select = trim(after_select);
    
    size_t from_pos = after_select.find("FROM");
    if (from_pos == std::string::npos) {
        return {false, "Missing FROM", {}, 0};
    }
    
    std::string projection_str = trim(after_select.substr(0, from_pos));
    std::string after_from = trim(after_select.substr(from_pos + 4));
    
    std::string table_name;
    std::string condition;
    size_t where_pos = after_from.find("WHERE");
    
    if (where_pos != std::string::npos) {
        table_name = trim(after_from.substr(0, where_pos));
        condition = trim(after_from.substr(where_pos + 5));
        if (!condition.empty() && condition.back() == ';') condition.pop_back();
    } else {
        table_name = trim(after_from);
        if (!table_name.empty() && table_name.back() == ';') table_name.pop_back();
    }
    
    if (!storage::table_exists(table_name)) {
        return {false, "Table does not exist: " + table_name, {}, 0};
    }
    
    storage::Table table = storage::load_table(table_name);
    
    std::vector<int> selected_cols;
    bool select_all = (projection_str == "*");
    
    if (!select_all) {
        std::stringstream ss(projection_str);
        std::string col_name;
        while (std::getline(ss, col_name, ',')) {
            col_name = trim(col_name);
            for (size_t i = 0; i < table.columns.size(); ++i) {
                if (table.columns[i].name == col_name) {
                    selected_cols.push_back(i);
                    break;
                }
            }
        }
    }
    
    Result res;
    res.success = true;
    
    std::vector<std::string> headers;
    if (select_all) {
        for (const auto& col : table.columns) {
            headers.push_back(col.name);
        }
    } else {
        for (int idx : selected_cols) {
            headers.push_back(table.columns[idx].name);
        }
    }
    res.rows.push_back(headers);
    
    for (const auto& row : table.rows) {
        if (check_condition(condition, row, table.columns)) {
            std::vector<std::string> result_row;
            if (select_all) {
                result_row = row;
            } else {
                for (int idx : selected_cols) {
                    result_row.push_back(row[idx]);
                }
            }
            res.rows.push_back(result_row);
        }
    }
    
    return res;
}

Result update(const std::string& sql) {
    std::string after_update = sql.substr(std::string("UPDATE").length());
    after_update = trim(after_update);
    
    size_t set_pos = after_update.find("SET");
    if (set_pos == std::string::npos) {
        return {false, "Missing SET", {}, 0};
    }
    
    std::string table_name = trim(after_update.substr(0, set_pos));
    std::string after_set = trim(after_update.substr(set_pos + 3));
    
    size_t where_pos = after_set.find("WHERE");
    std::string assignments_str;
    std::string condition;
    
    if (where_pos != std::string::npos) {
        assignments_str = trim(after_set.substr(0, where_pos));
        condition = trim(after_set.substr(where_pos + 5));
        if (!condition.empty() && condition.back() == ';') condition.pop_back();
    } else {
        assignments_str = trim(after_set);
        if (!assignments_str.empty() && assignments_str.back() == ';') assignments_str.pop_back();
    }
    
    if (!storage::table_exists(table_name)) {
        return {false, "Table does not exist: " + table_name, {}, 0};
    }
    
    storage::Table table = storage::load_table(table_name);
    
    std::vector<std::pair<int, std::string>> updates;
    std::stringstream ss(assignments_str);
    std::string assignment;
    
    while (std::getline(ss, assignment, ',')) {
        assignment = trim(assignment);
        size_t eq_pos = assignment.find('=');
        if (eq_pos == std::string::npos) {
            return {false, "Invalid assignment: " + assignment, {}, 0};
        }
        
        std::string col_name = trim(assignment.substr(0, eq_pos));
        std::string value = trim(assignment.substr(eq_pos + 1));
        
        if (value.length() >= 2 && value.front() == '\'' && value.back() == '\'') {
            value = value.substr(1, value.length() - 2);
        }
        
        int col_idx = -1;
        for (size_t i = 0; i < table.columns.size(); ++i) {
            if (table.columns[i].name == col_name) {
                col_idx = i;
                break;
            }
        }
        if (col_idx == -1) {
            return {false, "Column not found: " + col_name, {}, 0};
        }
        updates.push_back({col_idx, value});
    }
    
    int affected = 0;
    for (auto& row : table.rows) {
        if (check_condition(condition, row, table.columns)) {
            for (const auto& [idx, val] : updates) {
                row[idx] = val;
            }
            affected++;
        }
    }
    
    if (storage::save_table(table)) {
        Result res;
        res.success = true;
        res.affected_rows = affected;
        return res;
    } else {
        return {false, "Failed to save table", {}, 0};
    }
}

Result delete_from(const std::string& sql) {
    std::string after_delete = sql.substr(std::string("DELETE").length());
    after_delete = trim(after_delete);
    
    if (after_delete.find("FROM") != 0) {
        return {false, "Missing FROM", {}, 0};
    }
    after_delete = after_delete.substr(4);
    after_delete = trim(after_delete);
    
    size_t where_pos = after_delete.find("WHERE");
    std::string table_name;
    std::string condition;
    
    if (where_pos != std::string::npos) {
        table_name = trim(after_delete.substr(0, where_pos));
        condition = trim(after_delete.substr(where_pos + 5));
        if (!condition.empty() && condition.back() == ';') condition.pop_back();
    } else {
        table_name = trim(after_delete);
        if (!table_name.empty() && table_name.back() == ';') table_name.pop_back();
    }
    
    if (!storage::table_exists(table_name)) {
        return {false, "Table does not exist: " + table_name, {}, 0};
    }
    
    storage::Table table = storage::load_table(table_name);
    
    int affected = 0;
    std::vector<std::vector<std::string>> new_rows;
    
    for (const auto& row : table.rows) {
        if (check_condition(condition, row, table.columns)) {
            affected++;
        } else {
            new_rows.push_back(row);
        }
    }
    
    table.rows = new_rows;
    
    if (storage::save_table(table)) {
        Result res;
        res.success = true;
        res.affected_rows = affected;
        return res;
    } else {
        return {false, "Failed to save table", {}, 0};
    }
}

// ============== JOIN ФУНКЦИЯ ==============
Result join_tables(const std::string& sql) {
    // Пример: SELECT * FROM users JOIN orders ON users.id = orders.user_id
    std::string after_select = sql.substr(std::string("SELECT").length());
    after_select = trim(after_select);
    
    size_t from_pos = after_select.find("FROM");
    if (from_pos == std::string::npos) {
        return {false, "Missing FROM", {}, 0};
    }
    
    std::string projection_str = trim(after_select.substr(0, from_pos));
    std::string after_from = trim(after_select.substr(from_pos + 4));
    
    // Ищем JOIN
    size_t join_pos = after_from.find("JOIN");
    if (join_pos == std::string::npos) {
        return {false, "Missing JOIN keyword", {}, 0};
    }
    
    std::string left_table = trim(after_from.substr(0, join_pos));
    std::string after_join = trim(after_from.substr(join_pos + 4));
    
    size_t on_pos = after_join.find("ON");
    if (on_pos == std::string::npos) {
        return {false, "Missing ON condition", {}, 0};
    }
    
    std::string right_table = trim(after_join.substr(0, on_pos));
    std::string condition = trim(after_join.substr(on_pos + 2));
    
    // Убираем точку с запятой
    if (!condition.empty() && condition.back() == ';') {
        condition.pop_back();
    }
    
    // Проверяем существование таблиц
    if (!storage::table_exists(left_table)) {
        return {false, "Table does not exist: " + left_table, {}, 0};
    }
    if (!storage::table_exists(right_table)) {
        return {false, "Table does not exist: " + right_table, {}, 0};
    }
    
    // Загружаем таблицы
    storage::Table left = storage::load_table(left_table);
    storage::Table right = storage::load_table(right_table);
    
    // Парсим условие: users.id = orders.user_id
    size_t eq_pos = condition.find('=');
    if (eq_pos == std::string::npos) {
        return {false, "Invalid JOIN condition, expected '='", {}, 0};
    }
    
    std::string left_cond = trim(condition.substr(0, eq_pos));
    std::string right_cond = trim(condition.substr(eq_pos + 1));
    
    // Разбираем table.column
    size_t dot_left = left_cond.find('.');
    size_t dot_right = right_cond.find('.');
    
    if (dot_left == std::string::npos || dot_right == std::string::npos) {
        return {false, "JOIN condition must be table.column = table.column", {}, 0};
    }
    
    std::string left_join_table = trim(left_cond.substr(0, dot_left));
    std::string left_join_col = trim(left_cond.substr(dot_left + 1));
    std::string right_join_table = trim(right_cond.substr(0, dot_right));
    std::string right_join_col = trim(right_cond.substr(dot_right + 1));
    
    // Находим индексы колонок для JOIN
    int left_idx = -1, right_idx = -1;
    for (size_t i = 0; i < left.columns.size(); ++i) {
        if (left.columns[i].name == left_join_col) {
            left_idx = i;
            break;
        }
    }
    for (size_t i = 0; i < right.columns.size(); ++i) {
        if (right.columns[i].name == right_join_col) {
            right_idx = i;
            break;
        }
    }
    
    if (left_idx == -1) {
        return {false, "Column not found: " + left_join_table + "." + left_join_col, {}, 0};
    }
    if (right_idx == -1) {
        return {false, "Column not found: " + right_join_table + "." + right_join_col, {}, 0};
    }
    
    // Формируем результат
    Result res;
    res.success = true;
    
    // Заголовки с префиксом таблицы
    std::vector<std::string> headers;
    for (const auto& col : left.columns) {
        headers.push_back(left_table + "." + col.name);
    }
    for (const auto& col : right.columns) {
        headers.push_back(right_table + "." + col.name);
    }
    res.rows.push_back(headers);
    
    // Выполняем INNER JOIN
    for (const auto& left_row : left.rows) {
        for (const auto& right_row : right.rows) {
            if (left_row[left_idx] == right_row[right_idx]) {
                std::vector<std::string> joined_row = left_row;
                joined_row.insert(joined_row.end(), right_row.begin(), right_row.end());
                res.rows.push_back(joined_row);
            }
        }
    }
    
    return res;
}

} // namespace executor