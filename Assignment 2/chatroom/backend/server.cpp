#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sqlite3.h>
#include <sstream>
#include <unordered_map>
#include <vector>

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

// SQLite 数据库
sqlite3 *db;
const std::string DB_FILE = "chatserver.db";

// 获取当前时间的字符串表示
std::string get_time_str() {
    auto t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::stringstream ss;
    ss << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S] ");
    return ss.str();
}

// 打开数据库连接
bool open_db() {
    int rc = sqlite3_open(DB_FILE.c_str(), &db);
    if (rc) {
        std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    return true;
}

// 执行 SQL 查询
int execute_sql(const std::string &sql) {
    char *err_msg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL 错误: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return rc;
    }
    return SQLITE_OK;
}

// 初始化数据库
void init_db() {
    // 创建用户表
    std::string create_user_table = "CREATE TABLE IF NOT EXISTS users ("
                                    "username TEXT PRIMARY KEY, "
                                    "password TEXT NOT NULL);";
    execute_sql(create_user_table);

    // 创建消息历史表
    std::string create_msg_table = "CREATE TABLE IF NOT EXISTS messages ("
                                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                   "sender TEXT, "
                                   "receiver TEXT, "
                                   "message TEXT, "
                                   "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";
    execute_sql(create_msg_table);
}

// 从数据库验证用户
bool verify_user(const std::string &username, const std::string &password) {
    sqlite3_stmt *stmt;
    std::string query = "SELECT password FROM users WHERE username = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "查询失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char *db_password = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        if (db_password && password == db_password) {
            sqlite3_finalize(stmt);
            return true;
        }
    }

    sqlite3_finalize(stmt);
    return false;
}

// 注册新用户
bool register_user(const std::string &username, const std::string &password) {
    // 检查用户名是否已存在
    sqlite3_stmt *stmt;
    std::string check_query = "SELECT 1 FROM users WHERE username = ?";

    if (sqlite3_prepare_v2(db, check_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "查询失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return false; // 用户名已存在
    }

    sqlite3_finalize(stmt);

    // 插入新用户
    std::string insert_query = "INSERT INTO users (username, password) VALUES (?, ?)";
    if (sqlite3_prepare_v2(db, insert_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "插入用户失败: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "插入用户失败: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

// 保存消息到数据库
void save_message_to_db(const std::string &sender, const std::string &receiver, const std::string &message) {
    std::string query = "INSERT INTO messages (sender, receiver, message) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "插入消息失败: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, sender.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, receiver.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, message.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "插入消息失败: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
}

class Session : public std::enable_shared_from_this<Session> {
    websocket::stream<tcp::socket> ws_;
    std::set<std::shared_ptr<Session>> &sessions_;
    boost::beast::flat_buffer buffer_;
    std::string username_;

  public:
    Session(tcp::socket socket, std::set<std::shared_ptr<Session>> &sessions)
        : ws_(std::move(socket)), sessions_(sessions) {}

    void start() {
        ws_.async_accept([self = shared_from_this()](boost::system::error_code ec) {
            if (!ec) {
                self->ws_.text(true);
                const std::string prompt = "请输入用户名,密码（格式 user,pass）或注册（格式 register username, password）：";
                self->ws_.async_write(
                    boost::asio::buffer(prompt),
                    [self](boost::system::error_code, std::size_t) {
                        self->read_login();
                    });
            }
        });
    }

    const std::string &username() const { return username_; }

  private:
    void read_login() {
        ws_.async_read(buffer_, [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
            if (ec)
                return;

            auto msg = boost::beast::buffers_to_string(self->buffer_.data());
            self->buffer_.consume(self->buffer_.size());

            std::istringstream ss(msg);
            std::string cmd, user, pass;

            if (msg.find("register") == 0) {
                // 注册命令
                std::getline(ss, cmd, ' ');
                if (std::getline(ss, user, ',') && std::getline(ss, pass)) {
                    std::cout << "注册尝试 - 用户名: " << user << std::endl;
                    if (register_user(user, pass)) {
                        self->ws_.async_write(
                            boost::asio::buffer("注册成功！请使用该用户名和密码登录。\n"),
                            [self](boost::system::error_code ec, std::size_t) {
                                self->ws_.async_write(
                                    boost::asio::buffer("请输入用户名,密码（格式 user,pass）："),
                                    [self](boost::system::error_code, std::size_t) {
                                        self->read_login();
                                    });
                            });
                    } else {
                        self->ws_.async_write(
                            boost::asio::buffer("注册失败，用户名已存在。\n"),
                            [self](boost::system::error_code, std::size_t) {
                                self->read_login();
                            });
                    }
                }
                return;
            }

            if (std::getline(ss, user, ',') && std::getline(ss, pass)) {
                std::cout << "登录尝试 - 用户名: " << user << std::endl;

                if (verify_user(user, pass)) {
                    self->username_ = user;
                    self->sessions_.insert(self);

                    // 登录成功，发送欢迎消息
                    std::string welcome = "登录成功，欢迎 " + user + "\n";
                    self->ws_.async_write(
                        boost::asio::buffer(welcome),
                        [self](boost::system::error_code ec, std::size_t) {
                            if (!ec)
                                self->send_history();
                        });
                    return;
                }
            }

            // 登录失败，提示重试
            const std::string retry = "登录失败，请重新输入 user,pass：";
            self->ws_.async_write(
                boost::asio::buffer(retry),
                [self](boost::system::error_code, std::size_t) {
                    self->read_login();
                });
        });
    }

    void send_history() {
        // 发送消息历史
        std::string history_msg = "=== 最近20条消息历史 ===\n";
        sqlite3_stmt *stmt;
        std::string query = "SELECT sender, message FROM messages ORDER BY timestamp DESC LIMIT 20;";

        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "查询历史记录失败: " << sqlite3_errmsg(db) << std::endl;
            return;
        }

        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW && count < 20) {
            std::string sender = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            std::string message = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            history_msg += "[" + sender + "] " + message + "\n";
            count++;
        }

        history_msg += "=== 历史消息结束 ===\n";
        ws_.async_write(
            boost::asio::buffer(history_msg),
            [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                if (!ec)
                    self->send_users_list();
            });
    }

    void send_users_list() {
        std::string user_list = "在线用户列表：";
        for (auto &session : sessions_) {
            user_list += session->username_ + ",";
        }
        if (!sessions_.empty()) {
            user_list.pop_back(); // 移除最后一个逗号
        }

        for (auto &session : sessions_) {
            session->ws_.async_write(
                boost::asio::buffer(user_list),
                [](boost::system::error_code, std::size_t) {});
        }

        read(); // 登录成功后进入消息读取
    }

    void read() {
        ws_.async_read(buffer_, [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                auto msg = boost::beast::buffers_to_string(self->buffer_.data());
                self->buffer_.consume(self->buffer_.size());

                // 处理私聊消息：以 @target 开头
                if (!msg.empty() && msg[0] == '@') {
                    auto pos = msg.find(' ');
                    if (pos != std::string::npos) {
                        std::string target = msg.substr(1, pos - 1); // 获取目标用户名
                        std::string content = msg.substr(pos + 1);   // 获取消息内容
                        std::string time_prefix = get_time_str();
                        std::string formatted_msg = time_prefix + self->username_ + " (私) 对 " + target + " 说: " + content;

                        bool target_found = false;
                        // 查找目标用户并发送消息
                        for (auto &s : self->sessions_) {
                            if (s->username_ == target) {
                                target_found = true;
                                s->ws_.async_write(
                                    boost::asio::buffer(formatted_msg),
                                    [](boost::system::error_code, std::size_t) {});

                                // 给发送者自己也发一份
                                self->ws_.async_write(
                                    boost::asio::buffer(formatted_msg),
                                    [](boost::system::error_code, std::size_t) {});

                                // 保存私聊消息到数据库
                                save_message_to_db(self->username_, target, formatted_msg);
                                break;
                            }
                        }

                        if (!target_found) {
                            std::string error_msg = "系统: 用户 " + target + " 不在线或不存在";
                            self->ws_.async_write(
                                boost::asio::buffer(error_msg),
                                [](boost::system::error_code, std::size_t) {});
                        }

                        self->read();
                        return;
                    }
                }

                // 群聊消息（不是私聊消息）
                std::string time_prefix = get_time_str();
                std::string full_msg = time_prefix + self->username_ + " : " + msg;

                // 群聊消息广播给所有在线用户
                for (auto &s : self->sessions_) {
                    s->ws_.text(true);
                    s->ws_.async_write(
                        boost::asio::buffer(full_msg),
                        [](boost::system::error_code, std::size_t) {});
                }

                // 保存群聊消息到数据库
                save_message_to_db(self->username_, "all", full_msg);

                self->read();
            }
        });
    }
};

// 递归异步接受新连接
void do_accept(tcp::acceptor &acceptor,
               boost::asio::io_context &ioc,
               std::set<std::shared_ptr<Session>> &sessions) {
    acceptor.async_accept(
        [&](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket), sessions)->start();
            }
            do_accept(acceptor, ioc, sessions);
        });
}

int main() {
    try {
        // 初始化数据库
        if (!open_db()) {
            std::cerr << "数据库连接失败，退出..." << std::endl;
            return 1;
        }
        init_db();

        boost::asio::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {tcp::v4(), 9002}};
        std::set<std::shared_ptr<Session>> sessions;

        std::cout << "聊天服务器启动，监听端口 9002" << std::endl;

        do_accept(acceptor, ioc, sessions);
        ioc.run();

    } catch (std::exception &e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }
    return 0;
}
