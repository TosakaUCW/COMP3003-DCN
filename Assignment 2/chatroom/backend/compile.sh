#!/bin/bash

# 确定 Boost 头文件路径
BOOST_INCLUDE=`brew --prefix boost`/include
echo "使用 Boost 头文件路径: $BOOST_INCLUDE"

# 确定 Boost 库文件路径
BOOST_LIB=`brew --prefix boost`/lib
echo "使用 Boost 库文件路径: $BOOST_LIB"

# 确定 SQLite 头文件路径
SQLITE_INCLUDE=`brew --prefix sqlite`/include
echo "使用 SQLite 头文件路径: $SQLITE_INCLUDE"

# 确定 SQLite 库文件路径
SQLITE_LIB=`brew --prefix sqlite`/lib
echo "使用 SQLite 库文件路径: $SQLITE_LIB"

OPENSSL_PREFIX=$(brew --prefix openssl@3)

# JSON 库路径在 server.cpp 中直接硬编码了，所以这里不需要特殊处理

# 编译
echo "开始编译..."
g++ -std=c++17 -o chatserver server.cpp \
  -I$BOOST_INCLUDE \
  -I$SQLITE_INCLUDE \
  -L$BOOST_LIB \
  -L$SQLITE_LIB \
  -I/opt/homebrew/include/ \
  -lboost_system -lsqlite3 -pthread \
  -I${OPENSSL_PREFIX}/include -L${OPENSSL_PREFIX}/lib \
  -lcrypto

# 检查编译是否成功
if [ $? -eq 0 ]; then
  echo "编译成功！"
  echo ""
  echo "使用方法："
  echo "1. 运行 ./chatserver 启动聊天服务器"
  echo "2. 在另一个终端窗口，进入前端目录并运行 python3 -m http.server 8000"
  echo "3. 在浏览器访问 http://localhost:8000"
else
  echo "编译失败，请检查错误信息"
fi

# 检查编译是否成功
if [ $? -eq 0 ]; then
  echo "编译成功！"
  echo ""
  echo "使用方法："
  echo "1. 运行 ./chatserver 启动聊天服务器"
  echo "2. 在另一个终端窗口，进入前端目录并运行 python3 -m http.server 8000"
  echo "3. 在浏览器访问 http://localhost:8000"
else
  echo "编译失败，请检查错误信息"
fi