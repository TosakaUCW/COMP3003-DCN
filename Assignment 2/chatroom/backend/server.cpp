// chatroom/backend/server.cpp

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

// 从 CSV 文件加载用户（格式：username,password）
std::unordered_map<std::string, std::string> load_users(const std::string &file) {
    std::unordered_map<std::string, std::string> users;
    std::ifstream in(file);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string user, pass;
        if (std::getline(ss, user, ',') && std::getline(ss, pass)) {
            users[user] = pass;
        }
    }
    return users;
}

class Session : public std::enable_shared_from_this<Session> {
    websocket::stream<tcp::socket> ws_;
    std::set<std::shared_ptr<Session>> &sessions_;
    boost::beast::flat_buffer buffer_;
    std::string username_;
    static std::unordered_map<std::string, std::string> users_; // 全局用户表

  public:
    Session(tcp::socket socket, std::set<std::shared_ptr<Session>> &sessions)
        : ws_(std::move(socket)), sessions_(sessions) {}

    void start() {
        ws_.async_accept([self = shared_from_this()](boost::system::error_code ec) {
            if (!ec) {
                self->ws_.text(true);
                const std::string prompt = "请输入用户名,密码（格式 user,pass）：";
                self->ws_.async_write(
                    boost::asio::buffer(prompt),
                    [self](boost::system::error_code, std::size_t) {
                        self->read_login();
                    });
            }
        });
    }

  private:
    void read_login() {
        ws_.async_read(buffer_, [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
            if (ec)
                return;
            auto msg = boost::beast::buffers_to_string(self->buffer_.data());
            self->buffer_.consume(self->buffer_.size());

            std::istringstream ss(msg);
            std::string user, pass;
            if (std::getline(ss, user, ',') && std::getline(ss, pass)) {
                auto it = users_.find(user);
                if (it != users_.end() && it->second == pass) {
                    self->username_ = user;
                    self->sessions_.insert(self);

                    // 登录成功，发送欢迎消息和在线用户列表
                    std::string welcome = "登录成功，欢迎 " + user + "\n";
                    self->ws_.async_write(
                        boost::asio::buffer(welcome),
                        [self](boost::system::error_code ec, std::size_t) {
                            if (!ec)
                                self->send_users_list();
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

    void send_users_list() {
        std::string user_list = "在线用户列表：";
        for (auto &session : sessions_) {
            user_list += session->username_ + ",";
        }
        user_list.pop_back(); // 移除最后一个逗号
        for (auto &session : sessions_) {
            session->ws_.async_write(boost::asio::buffer(user_list), [](boost::system::error_code, std::size_t) {});
        }
        read(); // 登录成功后进入消息读取
    }

    void read() {
        ws_.async_read(buffer_, [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                auto msg = boost::beast::buffers_to_string(self->buffer_.data());
                self->buffer_.consume(self->buffer_.size());

                // 私聊格式：@target message
                if (!msg.empty() && msg[0] == '@') {
                    auto pos = msg.find(' ');
                    if (pos != std::string::npos) {
                        std::string target = msg.substr(1, pos - 1);
                        std::string content = self->username_ + " (私) : " + msg.substr(pos + 1);
                        for (auto &s : self->sessions_) {
                            if (s->username_ == target) {
                                s->ws_.async_write(
                                    boost::asio::buffer(content),
                                    [](boost::system::error_code, std::size_t) {});
                                break;
                            }
                        }
                        self->read();
                        return;
                    }
                }

                // 群聊广播
                std::string full = self->username_ + " : " + msg;
                for (auto &s : self->sessions_) {
                    s->ws_.text(true);
                    s->ws_.async_write(
                        boost::asio::buffer(full),
                        [](boost::system::error_code, std::size_t) {});
                }
                self->read();
            } else {
                // 断开时移除会话
                self->sessions_.erase(self);
            }
        });
    }
};

// 定义静态用户表
std::unordered_map<std::string, std::string> Session::users_ = load_users("users.csv");

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
        boost::asio::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {tcp::v4(), 9002}};
        std::set<std::shared_ptr<Session>> sessions;

        std::cout << "服务器启动，监听端口 9002" << std::endl;

        do_accept(acceptor, ioc, sessions);
        ioc.run();

    } catch (std::exception &e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }
    return 0;
}
