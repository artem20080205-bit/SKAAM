#include "api/database.h"
#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <fstream>

class HTTPServer {
private:
    int server_fd;
    int port;
    
    std::string url_decode(const std::string& str) {
        std::string result;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '%' && i + 2 < str.length()) {
                int value;
                std::stringstream ss;
                ss << std::hex << str.substr(i + 1, 2);
                ss >> value;
                result += static_cast<char>(value);
                i += 2;
            } else if (str[i] == '+') {
                result += ' ';
            } else {
                result += str[i];
            }
        }
        return result;
    }
    
    std::string html_escape(const std::string& str) {
        std::string result;
        for (char c : str) {
            if (c == '<') result += "&lt;";
            else if (c == '>') result += "&gt;";
            else if (c == '&') result += "&amp;";
            else if (c == '"') result += "&quot;";
            else result += c;
        }
        return result;
    }
    
    std::string render_form() {
        return R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>MySQLite Web Client</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        
        .header {
            text-align: center;
            color: white;
            margin-bottom: 30px;
        }
        
        .header h1 {
            font-size: 3em;
            margin-bottom: 10px;
        }
        
        .header p {
            font-size: 1.1em;
            opacity: 0.9;
        }
        
        .card {
            background: white;
            border-radius: 10px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
            margin-bottom: 20px;
        }
        
        .card-header {
            background: #2d3748;
            color: white;
            padding: 15px 20px;
            font-size: 1.2em;
            font-weight: bold;
        }
        
        .card-body {
            padding: 20px;
        }
        
        textarea {
            width: 100%;
            padding: 15px;
            font-family: 'Courier New', monospace;
            font-size: 14px;
            border: 2px solid #e2e8f0;
            border-radius: 8px;
            resize: vertical;
            transition: border-color 0.3s;
        }
        
        textarea:focus {
            outline: none;
            border-color: #667eea;
        }
        
        button {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            padding: 12px 30px;
            font-size: 16px;
            border-radius: 8px;
            cursor: pointer;
            transition: transform 0.2s;
        }
        
        button:hover {
            transform: translateY(-2px);
        }
        
        .result-table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 10px;
        }
        
        .result-table th {
            background: #667eea;
            color: white;
            padding: 12px;
            text-align: left;
            font-weight: bold;
        }
        
        .result-table td {
            padding: 10px 12px;
            border-bottom: 1px solid #e2e8f0;
        }
        
        .result-table tr:hover {
            background: #f7fafc;
        }
        
        .error {
            background: #fed7d7;
            color: #c53030;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #c53030;
        }
        
        .success {
            background: #c6f6d5;
            color: #22543d;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #38a169;
        }
        
        .query-info {
            background: #edf2f7;
            padding: 10px;
            border-radius: 5px;
            font-family: monospace;
            font-size: 12px;
            margin-top: 10px;
        }
        
        .stats {
            margin-top: 10px;
            color: #718096;
            font-size: 0.9em;
        }
        
        .badge {
            display: inline-block;
            padding: 4px 8px;
            background: #e2e8f0;
            border-radius: 4px;
            font-size: 0.8em;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>SQL Веб-клиент</h1>
        </div>
        
        <div class="card">
            <div class="card-header">
                Редактор SQL-запросов
            </div>
            <div class="card-body">
                <form method="GET" action="/">
                    <textarea name="sql" rows="6" placeholder="Введите ваше SQL запрос...
                    
Примеры:
SELECT * FROM users;
INSERT INTO users VALUES (1, 'Michael', 25);
UPDATE users SET age = 26 WHERE name = 'Michael';
DELETE FROM users WHERE id = 1;"></textarea>
                    <br><br>
                    <button type="submit">Выполнить SQL-запрос</button>
                </form>
            </div>
        </div>
    </div>
</body>
</html>
)";
    }
    
    std::string render_result(const mydb::Result& result, const std::string& sql) {
        std::stringstream html;
        
        html << R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>MySQLite Result</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        
        .header {
            text-align: center;
            color: white;
            margin-bottom: 30px;
        }
        
        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
        }
        
        .card {
            background: white;
            border-radius: 10px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
            margin-bottom: 20px;
        }
        
        .card-header {
            background: #2d3748;
            color: white;
            padding: 15px 20px;
            font-size: 1.2em;
            font-weight: bold;
        }
        
        .card-body {
            padding: 20px;
        }
        
        .back-link {
            display: inline-block;
            background: #e2e8f0;
            color: #4a5568;
            padding: 8px 16px;
            text-decoration: none;
            border-radius: 6px;
            margin-bottom: 20px;
            transition: background 0.3s;
        }
        
        .back-link:hover {
            background: #cbd5e0;
        }
        
        .result-table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 10px;
            overflow-x: auto;
        }
        
        .result-table th {
            background: #667eea;
            color: white;
            padding: 12px;
            text-align: left;
            font-weight: bold;
        }
        
        .result-table td {
            padding: 10px 12px;
            border-bottom: 1px solid #e2e8f0;
        }
        
        .result-table tr:hover {
            background: #f7fafc;
        }
        
        .error {
            background: #fed7d7;
            color: #c53030;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #c53030;
        }
        
        .success {
            background: #c6f6d5;
            color: #22543d;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #38a169;
        }
        
        .query-info {
            background: #edf2f7;
            padding: 15px;
            border-radius: 8px;
            font-family: 'Courier New', monospace;
            font-size: 13px;
            overflow-x: auto;
            margin-top: 15px;
        }
        
        .stats {
            margin-top: 15px;
            color: #718096;
            font-size: 0.9em;
        }
        
        .badge {
            display: inline-block;
            padding: 4px 8px;
            background: #e2e8f0;
            border-radius: 4px;
            font-size: 0.8em;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Результат выолнения запроса</h1>
        </div>
        
        <div class="card">
            <div class="card-header">
                Результат выполнения
            </div>
            <div class="card-body">
                <a href="/" class="back-link">Новый запрос</a>
)";
        
        if (!result.success) {
            html << "<div class='error'>❌ " << html_escape(result.error) << "</div>";
        } 
        else if (!result.rows.empty()) {
            if (result.rows.size() > 0) {
                html << "<table class='result-table'>";
                // Заголовки
                html << "<thead><tr>";
                for (size_t i = 0; i < result.rows[0].size(); ++i) {
                    html << "<th>" << html_escape(result.rows[0][i]) << "</th>";
                }
                html << "</tr></thead><tbody>";
                // Данные
                for (size_t r = 1; r < result.rows.size(); ++r) {
                    html << "<tr>";
                    for (size_t c = 0; c < result.rows[r].size(); ++c) {
                        html << "<td>" << html_escape(result.rows[r][c]) << "</td>";
                    }
                    html << "</tr>";
                }
                html << "</tbody></table>";
                html << "<div class='stats'>📈 " << (result.rows.size() - 1) 
                     << " строк" << ((result.rows.size() - 1) != 1 ? "" : "") << " возвращено</div>";
            }
        } 
        else {
            html << "<div class='success'>Запрос выполнен успешно<br>";
            html << "Изменено строк: " << result.affected_rows << "</div>";
        }
        
        html << "<div class='query-info'>";
        html << "<strong>Выполненный запрос</strong><br>";
        html << html_escape(sql);
        html << "</div>";
        
        html << R"(
            </div>
        </div>
    </div>
</body>
</html>
)";
        
        return html.str();
    }
    
    void handle_request(int client_fd, const std::string& request) {
        std::string response;
        
        // Парсим GET запрос
        std::string sql;
        size_t get_pos = request.find("GET /");
        if (get_pos != std::string::npos) {
            size_t sql_pos = request.find("sql=");
            if (sql_pos != std::string::npos) {
                size_t end = request.find(" HTTP/", sql_pos);
                if (end != std::string::npos) {
                    sql = request.substr(sql_pos + 4, end - sql_pos - 4);
                    sql = url_decode(sql);
                }
            }
        }
        
        std::string content;
        
        if (sql.empty()) {
            content = render_form();
        } else {
            auto result = mydb::execute(sql);
            content = render_result(result, sql);
        }
        
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html; charset=utf-8\r\n";
        response += "Content-Length: " + std::to_string(content.length()) + "\r\n";
        response += "Подключение: закрыто\r\n";
        response += "\r\n";
        response += content;
        
        send(client_fd, response.c_str(), response.length(), 0);
        close(client_fd);
    }
    
public:
    HTTPServer(int p) : port(p) {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            exit(1);
        }
        
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Failed to bind to port " << port << std::endl;
            exit(1);
        }
        
        if (listen(server_fd, 5) < 0) {
            std::cerr << "Failed to listen" << std::endl;
            exit(1);
        }
    }
    
void start() {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  MySQLite Веб-сервер made by SKAAM\n";
    std::cout << "========================================\n";
    std::cout << "  Веб-интерфейс: http://localhost:" << port << "\n";
    std::cout << "  Статус: запущен\n";
    std::cout << "  Остановить: Ctrl+C\n";
    std::cout << "========================================\n\n";
    std::cout << "Готово! Откройте браузер по ссылке http://localhost:" << port << "\n\n";
    
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0) {
            char buf[16384];
            int bytes = recv(client_fd, buf, sizeof(buf) - 1, 0);
            if (bytes > 0) {
                buf[bytes] = '\0';
                handle_request(client_fd, buf);
                std::cout << "  📥 Request processed" << std::endl;
            }
        }
    }
}
    
    ~HTTPServer() {
        close(server_fd);
    }
};

int main(int argc, char* argv[]) {
    int port = 8080;
    
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--port" && i+1 < argc) {
            port = std::stoi(argv[++i]);
        }
    }
    
    mydb::init("./my_data");
    
    HTTPServer server(port);
    server.start();
    
    mydb::shutdown();
    
    return 0;
}