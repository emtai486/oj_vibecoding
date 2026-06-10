# OJ Project

轻量级仿 LeetCode Online Judge，后端使用 C++20、cpp-httplib 和 MySQL 8，前端使用原生 HTML/CSS/JS 与本地 CodeMirror。

## 当前进度

- 已创建 SPEC 6.3 中的项目目录结构。
- 已引入本地 `third_party/httplib/httplib.h`。
- 已提供 MySQL 配置模板和基础连接封装。
- 已提供 MySQL 建表 SQL、开发种子数据、测试用户、管理员账号和示例题目。
- 已实现 cpp-httplib 服务启动、静态文件挂载、统一 JSON 响应和基础错误处理。
- 已准备前端静态资源目录，并将 CodeMirror 浏览器资源放入 `public/vendor/codemirror/`。
- 已实现普通用户题目列表、题目详情、注册、登录、退出、session/cookie、提交入口和本地完成状态。
- 已实现管理员登录、session/cookie、后台题目列表、新增题目和删除题目。
- 已实现一版同步判题流程：临时目录、g++ 编译、stdin/stdout、超时、内存限制、输出大小限制、strict/float_1 比较。
- 已提供部署脚本和验收脚本：构建、数据库初始化、启动、MySQL 连接检查、静态资源检查和主接口流程回归。

## 依赖

当前项目服务器 Ubuntu 22.04 LTS 上建议先安装：

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

也可以使用部署构建脚本：

```bash
bash scripts/build.sh
make deploy-build
```

## 测试

```bash
make test
```

如需运行 GoogleTest 版本的单元测试，先安装 `libgtest-dev`，再执行：

```bash
make test-gtest
```

接口级自动化测试：

```bash
make test-api-curl
make test-api-python
```

其中 Python 接口测试包含 12.6 判题回归用例，覆盖 `strict`、`float_1`、空代码、题目不存在、隐藏测试用例、超时、内存限制和输出限制。

## 启动

```bash
./build/oj_server config/app.conf
```

默认监听 `server.host` 和 `server.port`，并从 `public/` 提供静态页面。

部署启动脚本：

```bash
bash scripts/start_server.sh config/app.conf
make deploy-start
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

## 数据库初始化

建表和种子数据脚本位于 `sql/`：

```bash
mysql -u oj_user -p oj < sql/schema.sql
mysql -u oj_user -p oj < sql/seed.sql
```

部署初始化脚本会从配置文件读取 MySQL 连接参数，并导入 `schema.sql` 和 `seed.sql`：

```bash
bash scripts/init_db.sh config/app.conf
make deploy-init-db
```

开发种子账号：

```text
普通用户: user1 / password
管理员: admin / password
```

## 部署验收

基础验收不依赖业务数据库，覆盖构建、g++ C++17 编译运行和本地静态资源加载：

```bash
bash scripts/deploy_verify.sh --basic config/app.example.conf
make deploy-verify-basic
```

完整验收会初始化数据库、检查 MySQL 连接、启动临时服务，并运行普通用户提交、管理员新增/删除题目和判题接口回归：

```bash
bash scripts/deploy_verify.sh config/app.conf
make deploy-verify
```

当前项目服务器上的最终验收使用严格 OS 检查：

```bash
bash scripts/deploy_verify.sh --strict-os config/app.conf
make deploy-verify-strict
```

当前代码已完成 `SPEC.md` 12.1 初始化、12.2 数据库脚本、12.3 后端基础、12.4 普通用户功能、12.5 管理员功能、12.6 判题系统和 12.7 部署验收，并已在当前项目服务器 Ubuntu 22.04.5 LTS 上通过 `make deploy-verify-strict`。
