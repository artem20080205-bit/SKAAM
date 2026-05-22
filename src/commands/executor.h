#pragma once
#include <string>
#include "../api/database.h"

namespace executor {

mydb::Result create_table(const std::string& sql);
mydb::Result drop_table(const std::string& sql);
mydb::Result insert(const std::string& sql);
mydb::Result select(const std::string& sql);
mydb::Result update(const std::string& sql);
mydb::Result delete_from(const std::string& sql);
mydb::Result join_tables(const std::string& sql);

} // namespace executor