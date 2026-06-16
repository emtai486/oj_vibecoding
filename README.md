# 仿 LeetCode 在线判题系统 OJ

这是一个用于学习和实践的轻量级 Online Judge 系统。项目后端使用 C++20、cpp-httplib 和 MySQL 8，前端使用原生 HTML/CSS/JavaScript 与本地化 CodeMirror 编辑器，支持在线查看题目、编辑 C++ 代码、提交代码并获得判题结果。

项目目标是运行在单台 Ubuntu 22.04 LTS 服务器上，适合 1-20 人小规模受控使用，用于验证 C++ Web 服务、MySQL 持久化、在线编译运行、基础进程级资源限制和类 LeetCode 做题体验。

## 功能特性

- 题目浏览：未登录用户和登录用户都可以查看题目列表、题目详情和样例测试用例。
- 普通用户：支持注册、登录、退出、Cookie session、在线编辑 C++ 代码、提交代码和查看判题状态。
- 本地完成状态：普通用户的题目通过状态保存在当前浏览器 `localStorage` 中。
- 管理员后台：支持管理员登录、新增题目、删除题目，新增题目时可录入样例用例和隐藏用例。
- 判题流程：后端接收完整 C++ 程序，使用 `g++` 编译，在独立临时目录运行，并用隐藏测试用例校验输出。
- 资源限制：支持题目级时间限制、内存限制、输出大小限制，并限制同时判题进程数量。
- 输出比较：支持 `strict` 严格比较和 `float_1` 一位小数 token 比较。
- 本地静态资源：CodeMirror 已放入 `public/vendor/codemirror/`，前端运行不依赖公网 CDN。

## 技术栈

| 层级 | 技术 |
| --- | --- |
| 后端 | C++20、cpp-httplib |
| 数据库 | MySQL 8、MySQL C API |
| 判题 | g++、fork/exec、rlimit、临时目录 |
| 前端 | 原生 HTML、CSS、JavaScript、CodeMirror |
| 构建 | Makefile、Bash 脚本 |
| 测试 | C++ 测试、curl 接口测试、Python 接口测试、Playwright Web 回归脚本 |

## 系统架构

```text
Browser
  |
  | HTTP / JSON API
  v
C++ Web Server (cpp-httplib)
  |
  +-- Static Files: public/
  +-- MySQL: users/admins/problems/testcases
  +-- Judge Runner: g++ compile + process run + output compare
```

后端是单体服务，同一个进程负责静态资源服务、HTTP API、MySQL 访问、用户/管理员 session 和同步判题流程。

## 目录结构

```text
.
├── API.md                         # HTTP API 文档
├── DEPLOY.md                      # 部署文档
├── SPEC.md                        # 需求与架构规格
├── Makefile                       # 构建、测试、部署验收入口
├── config/
│   └── app.example.conf           # 配置模板，真实 app.conf 不提交
├── public/                        # 前端页面、样式、脚本和本地 CodeMirror
├── scripts/                       # 构建、初始化数据库、启动、验收脚本
├── sql/
│   ├── schema.sql                 # MySQL 建表脚本
│   └── seed.sql                   # 开发种子数据
├── src/                           # C++ 后端源码
│   ├── api/                       # HTTP API
│   ├── app/                       # 服务创建和路由注册
│   ├── auth/                      # 密码哈希和 session
│   ├── config/                    # 配置加载
│   ├── db/                        # MySQL 访问与 Repository
│   ├── judge/                     # 编译、运行、比较和并发限制
│   ├── model/                     # 数据模型
│   └── util/                      # JSON 与响应工具
├── tests/                         # C++ 测试
├── third_party/httplib/           # 本地 cpp-httplib 头文件
├── tools/                         # 辅助工具
└── var/judge_tmp/                 # 判题临时目录，运行时自动创建和清理
```

## 快速开始

以下命令以 Ubuntu 22.04 LTS 为例。

### 1. 安装依赖

```bash
sudo apt update
sudo apt install -y \
  build-essential gcc g++ make \
  mysql-server mysql-client libmysqlclient-dev \
  python3 curl
```

如需运行 GoogleTest 或 Web 自动化测试，可按 `dependence.md` 安装额外依赖。

### 2. 创建数据库和账号

```bash
sudo systemctl enable --now mysql
sudo mysql -e "CREATE DATABASE oj DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
sudo mysql -e "CREATE USER 'oj_user'@'localhost' IDENTIFIED BY 'change_me';"
sudo mysql -e "GRANT ALL PRIVILEGES ON oj.* TO 'oj_user'@'localhost';"
sudo mysql -e "FLUSH PRIVILEGES;"
```

实际部署时请替换 `change_me`。

### 3. 准备配置

```bash
cp config/app.example.conf config/app.conf
```

编辑 `config/app.conf`，至少修改数据库密码：

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
mysql.pool_size=8
```

`config/*.conf` 默认不会提交到 Git，避免泄露真实密码。

### 4. 构建和初始化

```bash
bash scripts/build.sh
bash scripts/init_db.sh config/app.conf
./build/oj_server --check-db config/app.conf
```

也可以直接使用 Makefile：

```bash
make
make deploy-init-db
```

### 5. 启动服务

```bash
bash scripts/start_server.sh config/app.conf
```

默认访问地址：

```text
http://127.0.0.1:8080/index.html
http://127.0.0.1:8080/problems.html
http://127.0.0.1:8080/admin/login.html
```

种子账号：

```text
普通用户: user1 / password
管理员: admin / password
```

## 常用命令

| 命令 | 说明 |
| --- | --- |
| `make` | 构建 `build/oj_server` |
| `make clean` | 删除构建产物 |
| `make check-db` | 使用 `config/app.example.conf` 执行数据库连接检查 |
| `make deploy-build` | 通过 `scripts/build.sh` 构建服务 |
| `make deploy-init-db` | 使用 `config/app.conf` 初始化数据库 |
| `make deploy-start` | 使用 `config/app.conf` 启动服务 |
| `make test` | 运行 C++ 基础测试 |
| `make test-gtest` | 运行 GoogleTest 测试，需要本机安装 gtest |
| `make test-api-curl` | 运行 curl 接口回归，需要可用 MySQL 配置 |
| `make test-api-curl-basic` | 运行不依赖业务数据的 curl 基础接口回归 |
| `make test-api-python` | 运行 Python 接口回归，需要可用 MySQL 配置 |
| `make test-api-python-basic` | 运行不依赖业务数据的 Python 基础接口回归 |
| `make deploy-verify-basic` | 基础部署验收，不依赖业务数据库 |
| `make deploy-verify` | 完整部署验收，包含数据库、静态资源和 API 流程 |
| `make deploy-verify-strict` | 严格 Ubuntu 22.04 环境验收 |

## API 与页面

主要 API 文档见 `API.md`。核心入口包括：

- `GET /health`
- `GET /api/problems`
- `GET /api/problems/{id}`
- `POST /api/user/register`
- `POST /api/user/login`
- `POST /api/submit`
- `POST /api/admin/login`
- `POST /api/admin/problems`
- `DELETE /api/admin/problems/{id}`

前端页面位于 `public/`：

- `index.html`：首页
- `problems.html`：题目列表
- `problem.html?id=1`：题目详情与代码提交
- `login.html` / `register.html`：普通用户登录和注册
- `admin/login.html`、`admin/index.html`、`admin/new-problem.html`：管理员后台

## 判题说明

判题服务会把用户代码写入 `var/judge_tmp/` 下的独立临时目录，使用以下方式编译：

```bash
g++ main.cpp -std=c++17 -O2 -pipe -o main
```

运行阶段会将隐藏测试用例输入写入程序 stdin，捕获 stdout，并按题目配置的比较模式校验输出。当前支持的判题状态包括：

- `accepted`
- `wrong_answer`
- `compile_error`
- `time_limit_exceeded`
- `memory_limit_exceeded`
- `output_limit_exceeded`
- `runtime_error`
- `system_error`

当前实现是基础进程级隔离，适合学习项目和受控环境。它不是完整安全沙箱，不建议直接开放给不可信公网用户。

## 部署文档

完整部署、验收、后台运行和故障排查步骤见 `DEPLOY.md`。
