#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <sqlite3.h>

using tcp = boost::asio::ip::tcp;
namespace ws = boost::beast::websocket;
using json = nlohmann::json;

// ────────── SQLite 基础 ──────────
sqlite3 *g_db = nullptr;
constexpr char DB_FILE[] = "chatserver.db";

inline std::string now_str() {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::ostringstream ss;
    ss << '[' << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ']';
    return ss.str();
}
bool exec_sql(std::string const &sql) {
    char *err = nullptr;
    if (sqlite3_exec(g_db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "SQL error: " << err << '\n';
        sqlite3_free(err);
        return false;
    }
    return true;
}
bool db_open() {
    if (sqlite3_open(DB_FILE, &g_db) != SQLITE_OK) {
        std::cerr << "无法打开数据库\n";
        return false;
    }
    return true;
}
void db_init() {
    /* ── 运行时优化 ─────────────────────────────── */
    exec_sql("PRAGMA journal_mode=WAL;");   // 读写并发
    exec_sql("PRAGMA synchronous=NORMAL;"); // 更快落盘
    exec_sql("PRAGMA busy_timeout=3000;");  // 写锁最多等 3s
    exec_sql("PRAGMA foreign_keys = ON;");

    /* ── 业务表 ─────────────────────────────────── */
    exec_sql("CREATE TABLE IF NOT EXISTS users ("
             " username TEXT PRIMARY KEY,"
             " password TEXT NOT NULL);");

    exec_sql("CREATE TABLE IF NOT EXISTS messages ("
             " id        INTEGER PRIMARY KEY AUTOINCREMENT,"
             " sender    TEXT,"
             " receiver  TEXT," // 'all' 表示公共
             " message   TEXT,"
             " timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);");

    exec_sql("CREATE TABLE IF NOT EXISTS groups ("
             " id     INTEGER PRIMARY KEY AUTOINCREMENT,"
             " name   TEXT UNIQUE,"
             " owner  TEXT);");

    exec_sql("CREATE TABLE IF NOT EXISTS group_members ("
             " id        INTEGER PRIMARY KEY AUTOINCREMENT,"
             " group_id  INTEGER,"
             " username  TEXT,"
             " is_owner  INTEGER DEFAULT 0,"
             " UNIQUE(group_id,username),"
             " FOREIGN KEY(group_id) REFERENCES groups(id) ON DELETE CASCADE);");

    exec_sql("CREATE TABLE IF NOT EXISTS group_messages ("
             " id        INTEGER PRIMARY KEY AUTOINCREMENT,"
             " group_id  INTEGER,"
             " sender    TEXT,"
             " message   TEXT,"
             " timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
             " FOREIGN KEY(group_id) REFERENCES groups(id) ON DELETE CASCADE);");

    /* ── 辅助日志表（可选） ─────────────────────── */
    exec_sql("CREATE TABLE IF NOT EXISTS events ("
             " id      INTEGER PRIMARY KEY AUTOINCREMENT,"
             " type    TEXT,"
             " actor   TEXT,"
             " payload TEXT," // JSON 字符串
             " ts DATETIME DEFAULT CURRENT_TIMESTAMP);");

    /* ── 高频列索引 ─────────────────────────────── */
    exec_sql("CREATE INDEX IF NOT EXISTS idx_messages_receiver ON messages(receiver);");
    exec_sql("CREATE INDEX IF NOT EXISTS idx_messages_time     ON messages(timestamp);");

    exec_sql("CREATE INDEX IF NOT EXISTS idx_grp_msg_gid_id    ON group_messages(group_id,id);");
    exec_sql("CREATE INDEX IF NOT EXISTS idx_grp_mem_gid_user  ON group_members(group_id,username);");
}

// ── 前向声明
class Session;
void broadcast_json(json const &);

// ── 在线会话集合
std::set<std::shared_ptr<Session>> g_sessions;

// ── SQLite 辅助
bool user_exists(std::string const &u) {
    sqlite3_stmt *st;
    sqlite3_prepare_v2(g_db, "SELECT 1 FROM users WHERE username=?;", -1, &st, 0);
    sqlite3_bind_text(st, 1, u.c_str(), -1, SQLITE_STATIC);
    bool ok = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return ok;
}
bool verify_user(std::string const &u, std::string const &p) {
    sqlite3_stmt *st;
    sqlite3_prepare_v2(g_db, "SELECT password FROM users WHERE username=?;", -1, &st, 0);
    sqlite3_bind_text(st, 1, u.c_str(), -1, SQLITE_STATIC);
    bool ok = false;
    if (sqlite3_step(st) == SQLITE_ROW)
        ok = p == reinterpret_cast<const char *>(sqlite3_column_text(st, 0));
    sqlite3_finalize(st);
    return ok;
}
bool register_user(std::string const &u, std::string const &p) {
    if (user_exists(u))
        return false;
    sqlite3_stmt *st;
    sqlite3_prepare_v2(g_db,
                       "INSERT INTO users(username,password) VALUES(?,?);",
                       -1, &st, 0);
    sqlite3_bind_text(st, 1, u.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, p.c_str(), -1, SQLITE_STATIC);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}
static sqlite3_stmt *ins_msg_stmt = nullptr;
void insert_message(const std::string &sender,
                    const std::string &receiver,
                    const std::string &body) {
    if (!ins_msg_stmt) {
        sqlite3_prepare_v2(g_db,
                           "INSERT INTO messages(sender,receiver,message) VALUES(?,?,?);",
                           -1, &ins_msg_stmt, 0);
    }
    sqlite3_reset(ins_msg_stmt);
    sqlite3_bind_text(ins_msg_stmt, 1, sender.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(ins_msg_stmt, 2, receiver.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(ins_msg_stmt, 3, body.c_str(), -1, SQLITE_STATIC);

    // 按 100 条为批量阈值；也可以直接 BEGIN/COMMIT 每条，看场景
    static int pending = 0;
    if (pending == 0)
        exec_sql("BEGIN;");
    sqlite3_step(ins_msg_stmt);
    if (++pending >= 100) {
        exec_sql("COMMIT;");
        pending = 0;
    }
}
static sqlite3_stmt *ins_grp_msg_stmt = nullptr;
int insert_group_message(int gid, const std::string &sender, const std::string &body) {
    if (!ins_grp_msg_stmt) {
        sqlite3_prepare_v2(g_db,
                           "INSERT INTO group_messages(group_id,sender,message) VALUES(?,?,?);",
                           -1, &ins_grp_msg_stmt, 0);
    }
    sqlite3_reset(ins_grp_msg_stmt);
    sqlite3_bind_int(ins_grp_msg_stmt, 1, gid);
    sqlite3_bind_text(ins_grp_msg_stmt, 2, sender.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(ins_grp_msg_stmt, 3, body.c_str(), -1, SQLITE_STATIC);

    exec_sql("BEGIN;");
    sqlite3_step(ins_grp_msg_stmt);
    exec_sql("COMMIT;");

    return (int)sqlite3_last_insert_rowid(g_db); // 返回 id 方便取 timestamp
}
bool user_in_group(int gid, std::string const &u) {
    sqlite3_stmt *st;
    sqlite3_prepare_v2(g_db,
                       "SELECT 1 FROM group_members WHERE group_id=? AND username=?;",
                       -1, &st, 0);
    sqlite3_bind_int(st, 1, gid);
    sqlite3_bind_text(st, 2, u.c_str(), -1, SQLITE_STATIC);
    bool ok = sqlite3_step(st) == SQLITE_ROW;
    sqlite3_finalize(st);
    return ok;
}
bool is_owner(int gid, std::string const &u) {
    sqlite3_stmt *st;
    sqlite3_prepare_v2(g_db,
                       "SELECT is_owner FROM group_members WHERE group_id=? AND username=?;",
                       -1, &st, 0);
    sqlite3_bind_int(st, 1, gid);
    sqlite3_bind_text(st, 2, u.c_str(), -1, SQLITE_STATIC);
    bool owner = false;
    if (sqlite3_step(st) == SQLITE_ROW)
        owner = sqlite3_column_int(st, 0) != 0;
    sqlite3_finalize(st);
    return owner;
}

static sqlite3_stmt *ins_mem_stmt = nullptr;
bool insert_group_member(int gid, const std::string &user, bool owner_flag) {
    if (!ins_mem_stmt) {
        sqlite3_prepare_v2(g_db,
                           "INSERT OR IGNORE INTO group_members(group_id,username,is_owner)"
                           " VALUES(?,?,?);",
                           -1, &ins_mem_stmt, nullptr);
    }
    sqlite3_reset(ins_mem_stmt);
    sqlite3_bind_int(ins_mem_stmt, 1, gid);
    sqlite3_bind_text(ins_mem_stmt, 2, user.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(ins_mem_stmt, 3, owner_flag ? 1 : 0);

    exec_sql("BEGIN;");
    sqlite3_step(ins_mem_stmt);
    exec_sql("COMMIT;");

    /* 插入成功时 sqlite3_changes(g_db) == 1 */
    return sqlite3_changes(g_db) == 1;
}
static sqlite3_stmt *del_mem_stmt = nullptr;
bool remove_group_member(int gid, const std::string &user) {
    if (!del_mem_stmt) {
        sqlite3_prepare_v2(g_db,
                           "DELETE FROM group_members WHERE group_id=? AND username=?;",
                           -1, &del_mem_stmt, nullptr);
    }
    sqlite3_reset(del_mem_stmt);
    sqlite3_bind_int(del_mem_stmt, 1, gid);
    sqlite3_bind_text(del_mem_stmt, 2, user.c_str(), -1, SQLITE_STATIC);

    exec_sql("BEGIN;");
    sqlite3_step(del_mem_stmt);
    exec_sql("COMMIT;");

    return sqlite3_changes(g_db) == 1;
}
json query_group_members(int gid) {
    sqlite3_stmt *st = nullptr;
    json members = json::array();

    sqlite3_prepare_v2(g_db,
                       "SELECT username,is_owner FROM group_members WHERE group_id=?;",
                       -1, &st, nullptr);
    sqlite3_bind_int(st, 1, gid);

    while (sqlite3_step(st) == SQLITE_ROW) {
        members.push_back({{"username", reinterpret_cast<const char *>(sqlite3_column_text(st, 0))},
                           {"is_owner", sqlite3_column_int(st, 1) != 0}});
    }
    sqlite3_finalize(st);
    return members;
}

// ─────────────────────── Session ───────────────────────
class Session : public std::enable_shared_from_this<Session> {
    ws::stream<tcp::socket> ws_;
    boost::beast::flat_buffer buf_;
    std::string username_;

    // 发送队列
    std::deque<std::string> write_q_;

    // 启动真正写
    void do_write() {
        if (write_q_.empty())
            return;
        auto self = shared_from_this();
        ws_.text(true);
        ws_.async_write(boost::asio::buffer(write_q_.front()),
                        [self](boost::system::error_code ec, std::size_t) {
                            if (!ec) {
                                self->write_q_.pop_front();
                                self->do_write();
                            }
                        });
    }
    // 投递文本（JSON 或普通）
    void queue_text(std::string text) {
        auto self = shared_from_this();
        boost::asio::post(ws_.get_executor(),
                          [self, txt = std::move(text)]() mutable {
                              bool writing = !self->write_q_.empty();
                              self->write_q_.push_back(std::move(txt));
                              if (!writing)
                                  self->do_write();
                          });
    }
    // helpers
    void queue_json(json const &j) { queue_text(j.dump()); }

  public:
    explicit Session(tcp::socket sock) : ws_(std::move(sock)) {}
    ws::stream<tcp::socket> &ws() { return ws_; }
    std::string const &name() const { return username_; }

    void push_json(const json &j) { queue_json(j); }

    void start() {
        ws_.async_accept([self = shared_from_this()](boost::system::error_code ec) {
            if (!ec) {
                self->ws_.text(true);
                self->prompt_login();
            }
        });
    }

  private:
    // ——— 登录流程 ———
    void prompt_login() {
        static const std::string prompt =
            "请输入用户名,密码（格式 user,pass）或注册（格式 register username, password）：";
        queue_text(prompt);
        read_login();
    }
    static void trim(std::string &s) {
        auto issp = [](int c) { return std::isspace(c); };
        s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), issp));
        s.erase(std::find_if_not(s.rbegin(), s.rend(), issp).base(), s.end());
    }
    void read_login() {
        ws_.async_read(buf_,
                       [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                           if (ec)
                               return;
                           std::string msg = boost::beast::buffers_to_string(self->buf_.data());
                           self->buf_.consume(self->buf_.size());
                           trim(msg);

                           // 注册
                           if (msg.rfind("register", 0) == 0) {
                               msg.erase(0, 8);
                               trim(msg);
                               auto pos = msg.find(',');
                               if (pos != std::string::npos) {
                                   std::string u = msg.substr(0, pos), p = msg.substr(pos + 1);
                                   trim(u);
                                   trim(p);
                                   if (register_user(u, p))
                                       self->queue_text("注册成功！请登录。\n");
                                   else
                                       self->queue_text("注册失败，用户名已存在。\n");
                               }
                               self->prompt_login();
                               return;
                           }

                           // 登录
                           auto pos = msg.find(',');
                           if (pos == std::string::npos) {
                               self->prompt_login();
                               return;
                           }
                           std::string u = msg.substr(0, pos), p = msg.substr(pos + 1);
                           trim(u);
                           trim(p);
                           if (!verify_user(u, p)) {
                               self->queue_text("登录失败，请重新输入 user,pass：");
                               self->read_login();
                               return;
                           }

                           self->username_ = u;
                           g_sessions.insert(self);
                           self->queue_text("登录成功，欢迎 " + u + "\n");
                           self->push_meta();
                           self->send_history();

                           // 广播更新用户列表
                           json uj = {{"type", "users_list"}, {"users", json::array()}};
                           for (auto &s : g_sessions)
                               uj["users"].push_back(s->name());
                           broadcast_json(uj);

                           self->do_read();
                       });
    }

    // ——— 推送在线用户/群组列表 ———
    void push_meta() {
        json uj = {{"type", "users_list"}, {"users", json::array()}};
        for (auto &s : g_sessions)
            uj["users"].push_back(s->name());
        queue_json(uj);

        json gl = {{"type", "groups_list"}, {"groups", json::array()}};
        sqlite3_stmt *st;
        sqlite3_prepare_v2(g_db,
                           "SELECT g.id,g.name,gm.is_owner "
                           "FROM groups g JOIN group_members gm ON gm.group_id=g.id "
                           "WHERE gm.username=?;",
                           -1, &st, 0);
        sqlite3_bind_text(st, 1, username_.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(st) == SQLITE_ROW) {
            gl["groups"].push_back({{"id", sqlite3_column_int(st, 0)},
                                    {"name", reinterpret_cast<const char *>(sqlite3_column_text(st, 1))},
                                    {"is_owner", sqlite3_column_int(st, 2) != 0}});
        }
        sqlite3_finalize(st);
        queue_json(gl);
    }

    // ——— 公共历史 20 条 ———
    void send_history() {
        json hist = {{"type", "history"}, {"messages", json::array()}};
        sqlite3_stmt *st;
        sqlite3_prepare_v2(g_db,
                           "SELECT sender,message,timestamp FROM messages "
                           "ORDER BY id DESC LIMIT 20;",
                           -1, &st, 0);
        while (sqlite3_step(st) == SQLITE_ROW) {
            hist["messages"].push_back({{"sender", reinterpret_cast<const char *>(sqlite3_column_text(st, 0))},
                                        {"raw", reinterpret_cast<const char *>(sqlite3_column_text(st, 1))},
                                        {"time", reinterpret_cast<const char *>(sqlite3_column_text(st, 2))}});
        }
        sqlite3_finalize(st);
        queue_json(hist);
    }

    // ——— 主读循环 ———
    void do_read() {
        ws_.async_read(buf_,
                       [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
                           if (ec) {
                               self->on_close();
                               return;
                           }
                           std::string msg = boost::beast::buffers_to_string(self->buf_.data());
                           self->buf_.consume(self->buf_.size());
                           self->handle_msg(msg);
                           self->do_read();
                       });
    }
    void on_close() {
        g_sessions.erase(shared_from_this());
        json uj = {{"type", "users_list"}, {"users", json::array()}};
        for (auto &s : g_sessions)
            uj["users"].push_back(s->name());
        broadcast_json(uj);
    }

    // ——— 处理单条消息 ———
    void handle_msg(std::string const &raw) {
        if (!raw.empty() && raw.front() == '{') {
            handle_json(raw);
            return;
        }

        // 私聊
        if (!raw.empty() && raw[0] == '@') {
            auto pos = raw.find(' ');
            if (pos == std::string::npos)
                return;
            std::string target = raw.substr(1, pos - 1), text = raw.substr(pos + 1);
            std::string out = now_str() + " " + username_ + " (私) 对 " + target + " 说: " + text;
            bool found = false;
            for (auto &s : g_sessions)
                if (s->name() == target) {
                    found = true;
                    s->queue_text(out);
                }
            queue_text(out); // 回显
            insert_message(username_, target, out);
            if (!found)
                queue_text("系统: 用户 " + target + " 不在线或不存在");
            return;
        }

        // 公共
        std::string out = now_str() + " " + username_ + " : " + raw;
        for (auto &s : g_sessions)
            s->queue_text(out);
        insert_message(username_, "all", out);
        ;
    }

    // ——— JSON 协议 ———
    void handle_json(std::string const &s) {
        json j;
        try {
            j = json::parse(s);
        } catch (...) {
            return;
        }
        std::string type = j.value("type", "");
        if (type == "create_group")
            on_create_group(j);
        else if (type == "add_group_member")
            on_add_member(j);
        else if (type == "remove_group_member")
            on_remove_member(j);
        else if (type == "get_group_members")
            on_get_members(j);
        else if (type == "get_group_messages")
            on_get_group_msgs(j);
        else if (type == "group_message")
            on_group_msg(j);
    }

    // ——— 群组子函数（与之前基本一致，但用 queue_text/queue_json） ———
    void on_create_group(json const &j) {
        std::string name = j.value("group_name", "");
        json resp = {{"type", "create_group_response"}};
        if (name.empty()) {
            resp["message"] = "群名不能为空";
            queue_json(resp);
            return;
        }

        sqlite3_stmt *st;
        sqlite3_prepare_v2(g_db,
                           "INSERT INTO groups(name,owner) VALUES(?,?);", -1, &st, 0);
        sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(st, 2, username_.c_str(), -1, SQLITE_STATIC);
        bool ok = sqlite3_step(st) == SQLITE_DONE;
        sqlite3_finalize(st);
        if (!ok) {
            resp["message"] = "创建失败(重名)";
            queue_json(resp);
            return;
        }

        int gid = (int)sqlite3_last_insert_rowid(g_db);
        // exec_sql("INSERT INTO group_members(group_id,username,is_owner) VALUES(" + std::to_string(gid) + ",'" + username_ + "',1);");
        insert_group_member(gid, username_, /*owner_flag=*/true);
        resp["message"] = "群组创建成功";
        resp["group"] = {{"id", gid}, {"name", name}, {"is_owner", true}};
        queue_json(resp);
        push_meta();
    }
    void on_add_member(json const &j) {
        int gid = j.value("group_id", -1);
        std::string user = j.value("username", "");
        json resp = {{"type", "add_member_response"}};
        if (gid < 0 || user.empty()) {
            resp["message"] = "参数错误";
            queue_json(resp);
            return;
        }
        if (!is_owner(gid, username_)) {
            resp["message"] = "只有群主能加人";
            queue_json(resp);
            return;
        }

        sqlite3_stmt *st;
        sqlite3_prepare_v2(g_db,
                           "INSERT OR IGNORE INTO group_members(group_id,username,is_owner)VALUES(?,?,0);",
                           -1, &st, 0);
        sqlite3_bind_int(st, 1, gid);
        sqlite3_bind_text(st, 2, user.c_str(), -1, SQLITE_STATIC);
        // bool ok = sqlite3_step(st) == SQLITE_DONE;
        bool ok = insert_group_member(gid, user, false);
        sqlite3_finalize(st);
        resp["message"] = ok ? "成员已添加" : "添加失败(可能已存在)";
        queue_json(resp);
        for (auto &s : g_sessions)
            if (s->name() == user)
                s->push_meta();
    }
    void on_remove_member(json const &j) {
        int gid = j.value("group_id", -1);
        std::string user = j.value("username", "");
        json resp = {{"type", "remove_member_response"}};
        if (gid < 0 || user.empty()) {
            resp["message"] = "参数错误";
            queue_json(resp);
            return;
        }
        if (!is_owner(gid, username_)) {
            resp["message"] = "只有群主能踢人";
            queue_json(resp);
            return;
        }
        if (user == username_) {
            resp["message"] = "不能移除自己";
            queue_json(resp);
            return;
        }

        sqlite3_stmt *st;
        sqlite3_prepare_v2(g_db,
                           "DELETE FROM group_members WHERE group_id=? AND username=?;", -1, &st, 0);
        sqlite3_bind_int(st, 1, gid);
        sqlite3_bind_text(st, 2, user.c_str(), -1, SQLITE_STATIC);
        // bool ok = (sqlite3_step(st) == SQLITE_DONE && sqlite3_changes(g_db));
        bool ok = remove_group_member(gid, user);
        sqlite3_finalize(st);
        resp["message"] = ok ? "成员已移除" : "移除失败";
        queue_json(resp);
        for (auto &s : g_sessions)
            if (s->name() == user)
                s->push_meta();
    }
    void on_get_members(json const &j) {
        int gid = j.value("group_id", -1);
        if (gid < 0)
            return;
        // json resp = {{"type", "group_members"}, {"group_id", gid}, {"members", json::array()}};
        json resp = {{"type", "group_members"}, {"group_id", gid}, {"members", query_group_members(gid)}};
        sqlite3_stmt *st;
        sqlite3_prepare_v2(g_db,
                           "SELECT username,is_owner FROM group_members WHERE group_id=?;", -1, &st, 0);
        sqlite3_bind_int(st, 1, gid);
        while (sqlite3_step(st) == SQLITE_ROW) {
            resp["members"].push_back({{"username", reinterpret_cast<const char *>(sqlite3_column_text(st, 0))},
                                       {"is_owner", sqlite3_column_int(st, 1) != 0}});
        }
        sqlite3_finalize(st);
        queue_json(resp);
    }
    void on_get_group_msgs(json const &j) {
        int gid = j.value("group_id", -1);
        if (gid < 0 || !user_in_group(gid, username_))
            return;
        json resp = {{"type", "group_messages"}, {"group_id", gid}, {"messages", json::array()}};
        sqlite3_stmt *st;
        sqlite3_prepare_v2(g_db,
                           "SELECT sender,message,timestamp FROM group_messages "
                           "WHERE group_id=? ORDER BY id DESC LIMIT 50;",
                           -1, &st, 0);
        sqlite3_bind_int(st, 1, gid);
        while (sqlite3_step(st) == SQLITE_ROW) {
            resp["messages"].push_back({{"sender", reinterpret_cast<const char *>(sqlite3_column_text(st, 0))},
                                        {"message", reinterpret_cast<const char *>(sqlite3_column_text(st, 1))},
                                        {"timestamp", reinterpret_cast<const char *>(sqlite3_column_text(st, 2))}});
        }
        sqlite3_finalize(st);
        queue_json(resp);
    }
    void on_group_msg(json const &j) {
        /* 1. 基本合法性检查 */
        int gid = j.value("group_id", -1);
        std::string content = j.value("content", "");
        if (gid < 0 || content.empty())
            return;
        if (!user_in_group(gid, username_))
            return; // 非群成员直接忽略

        /* 2. 写入数据库并取 timestamp */
        int row_id = insert_group_message(gid, username_, content);

        std::string ts;
        sqlite3_stmt *ts_stmt = nullptr;
        sqlite3_prepare_v2(g_db,
                           "SELECT timestamp FROM group_messages WHERE id=?;",
                           -1, &ts_stmt, nullptr);
        sqlite3_bind_int(ts_stmt, 1, row_id);
        if (sqlite3_step(ts_stmt) == SQLITE_ROW) {
            ts = reinterpret_cast<const char *>(sqlite3_column_text(ts_stmt, 0));
        }
        sqlite3_finalize(ts_stmt);

        /* 3. 组装前端需要的 JSON */
        json gm = {
            {"type", "group_message"},
            {"group_id", gid},
            {"sender", username_},
            {"timestamp", ts},
            {"formatted_message",
             "[" + ts + "] " + username_ + ": " + content}};

        /* 4. 广播给群内所有在线成员 */
        for (auto &s : g_sessions) {
            if (user_in_group(gid, s->name()))
                s->push_json(gm); // push_json 是我们在 Session public 区域暴露的包装
        }
    }
}; // Session

// ── broadcast_json：调用每个 Session 的 queue_json
void broadcast_json(json const &j) {
    for (auto &s : g_sessions)
        s->push_json(j);
}

// ── 异步 accept
void do_accept(boost::asio::io_context &ioc, tcp::acceptor &acc) {
    acc.async_accept(
        [&](boost::system::error_code ec, tcp::socket sock) {
            if (!ec)
                std::make_shared<Session>(std::move(sock))->start();
            do_accept(ioc, acc);
        });
}

// ── main
int main() {
    if (!db_open())
        return 1;
    db_init();
    try {
        boost::asio::io_context ioc{1};
        tcp::acceptor acc{ioc, {tcp::v4(), 9002}};
        std::cout << "Chat server listening on :9002\n";
        do_accept(ioc, acc);
        ioc.run();
    } catch (std::exception const &e) {
        std::cerr << "Fatal: " << e.what() << '\n';
    }
    sqlite3_close(g_db);
    return 0;
}
