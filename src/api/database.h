#pragma once
#include <string>
#include <vector>

namespace mydb {

struct Result {
    bool success;
    std::string error;
    std::vector<std::vector<std::string>> rows;  // для SELECT
    int affected_rows;                            // для INSERT/UPDATE/DELETE
};

// Клиент работает только через эти 3 функции
void init(const std::string& data_dir);  // указываем папку для БД
Result execute(const std::string& sql);  // выполняем любой SQL
void shutdown();                          // закрываем БД

} // namespace mydb