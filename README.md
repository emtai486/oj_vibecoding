# OJ Project

轻量级仿 LeetCode Online Judge，后端使用 C++20、cpp-httplib 和 MySQL 8，前端使用原生 HTML/CSS/JS 与本地 CodeMirror。

## 当前进度

- 已创建 SPEC 6.3 中的项目目录结构。
- 已引入本地 `third_party/httplib/httplib.h`。
- 已提供 MySQL 配置模板和基础连接封装。
- 已提供 MySQL 建表 SQL、开发种子数据、测试用户、管理员账号和示例题目。
- 已实现 cpp-httplib 服务启动、静态文件挂载、统一 JSON 响应和基础错误处理。
- 已准备前端静态资源目录，并将 CodeMirror 浏览器资源放入 `public/vendor/codemirror/`。

## 依赖

Ubuntu 24.04 上建议先安装：

```bash
sudo apt update
sudo apt install -y build-essential g++ make mysql-server mysql-client libmysqlclient-dev
```

如需重新下载前端依赖，需要 Node.js 和 npm。

## 构建

```bash
make
```

构建产物位于：

```text
build/oj_server
```

## 测试

```bash
make test
```

如需运行 GoogleTest 版本的单元测试，先安装 `libgtest-dev`，再执行：

```bash
make test-gtest
```

## 启动

```bash
./build/oj_server config/app.conf
```

默认监听 `server.host` 和 `server.port`，并从 `public/` 提供静态页面。

## 配置

复制示例配置并修改数据库密码：

```bash
cp config/app.example.conf config/app.conf
```

示例配置字段：

```text
server.host=0.0.0.0
server.port=8080
mysql.host=127.0.0.1
mysql.port=3306
mysql.user=oj_user
mysql.password=change_me
mysql.database=oj
mysql.charset=utf8mb4
mysql.connect_timeout_seconds=5
```

真实配置文件 `config/*.conf` 默认不会提交，`config/app.example.conf` 除外。

## MySQL 连接检查

创建数据库和用户后可执行：

```bash
./build/oj_server --check-db config/app.conf
```

## 数据库初始化

建表和种子数据脚本位于 `sql/`：

```bash
mysql -u oj_user -p oj < sql/schema.sql
mysql -u oj_user -p oj < sql/seed.sql
```

开发种子账号：

```text
普通用户: user1 / password
管理员: admin / password
```

当前代码已完成 `SPEC.md` 12.1 初始化、12.2 数据库脚本和 12.3 后端基础。具体业务 API、登录态和判题流程会按后续 TODO 继续实现。
