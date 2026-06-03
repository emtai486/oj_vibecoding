# OJ Project

轻量级仿 LeetCode Online Judge，后端使用 C++20、cpp-httplib 和 MySQL 8，前端使用原生 HTML/CSS/JS 与本地 CodeMirror。

## 当前进度

- 已创建 SPEC 6.3 中的项目目录结构。
- 已引入本地 `third_party/httplib/httplib.h`。
- 已提供 MySQL 配置模板和基础连接封装。
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

当前代码只完成初始化阶段的依赖、配置和静态资源准备，HTTP API、数据库表结构、判题流程会按 `SPEC.md` 后续 TODO 继续实现。
