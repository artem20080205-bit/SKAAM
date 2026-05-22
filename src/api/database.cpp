#include "database.h"
#include "../storage/storage.h"
#include "../commands/executor.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace mydb {

static std::string g_data_dir;

void init(const std::string& data_dir) {
    g_data_dir = data_dir;
    storage::set_data_dir(data_dir);
}

std::string to_upper(const std::string& s) {
    std::string result = s;
    for (char& c : result) c = std::toupper(c);
    return result;
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

Result execute(const std::string& sql) {
    std::string clean_sql = trim(sql);
    if (!clean_sql.empty() && clean_sql.back() == ';') {
        clean_sql.pop_back();
    }
    
    std::stringstream ss(clean_sql);
    std::string first_word;
    ss >> first_word;
    first_word = to_upper(first_word);
    
    if (first_word == "CREATE") {
        std::string second_word;
        ss >> second_word;
        if (to_upper(second_word) == "TABLE") {
            return executor::create_table(clean_sql);
        }
        return {false, "Only CREATE TABLE is supported", {}, 0};
    } 
    else if (first_word == "DROP") {
        std::string second_word;
        ss >> second_word;
        if (to_upper(second_word) == "TABLE") {
            return executor::drop_table(clean_sql);
        }
        return {false, "Only DROP TABLE is supported", {}, 0};
    } 
    else if (first_word == "INSERT") {
        return executor::insert(clean_sql);
    } 
    else if (first_word == "SELECT") {
        // Проверяем наличие JOIN
        std::string sql_upper = to_upper(clean_sql);
        if (sql_upper.find(" JOIN ") != std::string::npos) {
            return executor::join_tables(clean_sql);
        }
        return executor::select(clean_sql);
    } 
    else if (first_word == "UPDATE") {
        return executor::update(clean_sql);
    } 
    else if (first_word == "DELETE") {
        return executor::delete_from(clean_sql);
    }
    
    return {false, "Unknown command: " + first_word, {}, 0};
}

void shutdown() {}

} // namespace mydb