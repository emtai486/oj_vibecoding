# 项目部署文档

本文档说明如何在 Ubuntu 22.04 LTS 服务器上部署本项目。项目是单体 C++ 服务：同一进程负责静态页面、HTTP API、MySQL 访问和同步判题。

## 1. 部署目标

- 操作系统：Ubuntu 22.04 LTS
- 后端服务：`build/oj_server`
- 数据库：MySQL 8
- 默认端口：`8080`
- 静态资源目录：`public/`
- 判题临时目录：`var/judge_tmp/`
- 配置文件：`config/app.conf`

部署后可访问：

```text
http://服务器地址:8080/index.html
http://服务器地址:8080/problems.html
http://服务器地址:8080/admin/login.html
```

## 2. 安全边界说明

当前判题系统使用进程级隔离和资源限制，包括独立临时目录、运行时间限制、内存限制和输出大小限制。它不包含 Docker、namespace、cgroup、seccomp 或系统调用白名单等强沙箱能力。

因此，本项目适合学习、课程设计、内网或受控小规模场景，不建议直接开放给不可信公网用户提交任意代码。部署时不要用 root 运行 OJ 服务。

## 3. 安装系统依赖

最小部署依赖：

```bash
sudo apt update
sudo apt install -y \
  ca-certificates curl git make \
  build-essential gcc g++ \
  mysql-server mysql-client libmysqlclient-dev \
  python3
```

推荐调试工具：

```bash
sudo apt install -y lsof net-tools jq
```

启动 MySQL：

```bash
sudo systemctl enable --now mysql
sudo systemctl status mysql
```

如果需要更完整的依赖说明，参考 `dependence.md`。

## 4. 创建数据库和数据库用户

以下示例创建数据库 `oj` 和用户 `oj_user`：

```bash
sudo mysql -e "CREATE DATABASE oj DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
sudo mysql -e "CREATE USER 'oj_user'@'localhost' IDENTIFIED BY 'change_me';"
sudo mysql -e "GRANT ALL PRIVILEGES ON oj.* TO 'oj_user'@'localhost';"
sudo mysql -e "FLUSH PRIVILEGES;"
```

请把 `change_me` 替换为真实强密码。若 MySQL 与服务不在同一台机器，请同步调整授权主机和 `mysql.host`。

## 5. 准备项目配置

复制配置模板：

```bash
cp config/app.example.conf config/app.conf
```

编辑 `config/app.conf`：

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

字段说明：

| 配置项 | 说明 |
| --- | --- |
| `server.host` | 服务监听地址。部署到服务器通常使用 `0.0.0.0`，本机测试可用 `127.0.0.1` |
| `server.port` | 服务监听端口，默认 `8080` |
| `mysql.host` | MySQL 地址 |
| `mysql.port` | MySQL 端口 |
| `mysql.user` | MySQL 用户 |
| `mysql.password` | MySQL 密码 |
| `mysql.database` | 项目数据库名 |
| `mysql.charset` | MySQL 字符集，默认 `utf8mb4` |
| `mysql.connect_timeout_seconds` | 数据库连接超时时间 |
| `mysql.pool_size` | MySQL 连接池大小 |

真实配置文件 `config/*.conf` 已被 `.gitignore` 忽略，不要把生产密码提交到仓库。

## 6. 构建服务

执行：

```bash
bash scripts/build.sh
```

等价 Makefile 命令：

```bash
make
```

构建产物：

```text
build/oj_server
```

如果构建失败，优先检查：

- `g++ --version`
- `make --version`
- `mysql_config --version`
- 是否已安装 `libmysqlclient-dev`

## 7. 初始化数据库

推荐使用项目脚本初始化：

```bash
bash scripts/init_db.sh config/app.conf
```

脚本会读取 `config/app.conf` 中的 MySQL 配置，执行：

- `CREATE DATABASE IF NOT EXISTS`
- 导入 `sql/schema.sql`
- 导入 `sql/seed.sql`

也可以手动导入：

```bash
mysql -u oj_user -p oj < sql/schema.sql
mysql -u oj_user -p oj < sql/seed.sql
```

种子数据包含：

```text
普通用户: user1 / password
管理员: admin / password
示例题目: A+B Problem、Average Score
```

`seed.sql` 使用 `ON DUPLICATE KEY UPDATE`，可重复执行；但生产环境上线后建议及时修改或删除默认账号。

## 8. 检查数据库连接

```bash
./build/oj_server --check-db config/app.conf
```

成功时会输出类似：

```text
mysql connection ok: 127.0.0.1:3306/oj
```

如果失败，检查 `config/app.conf`、MySQL 用户授权、数据库是否存在，以及 MySQL 服务是否运行。

## 9. 启动服务

前台启动：

```bash
bash scripts/start_server.sh config/app.conf
```

也可以直接执行：

```bash
./build/oj_server config/app.conf
```

启动后检查健康接口：

```bash
curl -sS http://127.0.0.1:8080/health
```

预期返回：

```json
{"success":true,"message":"ok","data":{"status":"ok"}}
```

## 10. 后台运行示例

简单后台运行可使用 `nohup`：

```bash
mkdir -p var/log
nohup ./build/oj_server config/app.conf > var/log/oj_server.log 2>&1 &
```

查看进程：

```bash
pgrep -af oj_server
```

停止服务：

```bash
pkill -f "build/oj_server config/app.conf"
```

生产环境建议使用 systemd 管理进程。示例：

```ini
[Unit]
Description=OJ Server
After=network.target mysql.service

[Service]
Type=simple
WorkingDirectory=/home/bzx/project
ExecStart=/home/bzx/project/build/oj_server /home/bzx/project/config/app.conf
Restart=on-failure
RestartSec=3
User=bzx
Group=bzx

[Install]
WantedBy=multi-user.target
```

保存为 `/etc/systemd/system/oj-server.service` 后执行：

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now oj-server
sudo systemctl status oj-server
```

请按实际部署路径和运行用户修改 `WorkingDirectory`、`ExecStart`、`User` 和 `Group`。服务运行用户必须对项目目录下的 `var/` 有写权限，因为判题会创建 `var/judge_tmp/`。

## 11. 部署验收

基础验收不依赖业务数据库，覆盖构建、C++ 编译运行和静态资源加载：

```bash
bash scripts/deploy_verify.sh --basic config/app.example.conf
```

等价 Makefile 命令：

```bash
make deploy-verify-basic
```

完整验收会初始化数据库、检查 MySQL 连接、启动临时服务，并运行用户、管理员和判题接口回归：

```bash
bash scripts/deploy_verify.sh config/app.conf
```

等价 Makefile 命令：

```bash
make deploy-verify
```

严格 OS 验收要求当前环境为 Ubuntu 22.04：

```bash
bash scripts/deploy_verify.sh --strict-os config/app.conf
```

等价 Makefile 命令：

```bash
make deploy-verify-strict
```

注意：部署验收脚本会检查不要以 root 身份运行验证流程。

## 12. 接口回归测试

服务构建完成并配置好数据库后，可运行：

```bash
make test-api-curl
make test-api-python
```

Python 接口测试覆盖：

- 健康检查和统一 JSON 响应
- 普通用户注册、登录、退出
- 管理员登录、新增题目、删除题目
- 题目列表和题目详情
- 未登录提交拦截
- `strict` 和 `float_1` 判题
- 空代码、题目不存在、隐藏测试用例、超时、内存限制和输出限制等判题状态

## 13. 防火墙和访问

如果服务器启用了 UFW，并且需要从外部访问 `8080` 端口：

```bash
sudo ufw allow 8080/tcp
sudo ufw status
```

如果使用云服务器，还需要在云厂商安全组中放行对应端口。

## 14. 常见问题

### 构建时报 `mysql_config not found`

安装 MySQL 开发包：

```bash
sudo apt install -y libmysqlclient-dev
```

### 启动时报配置文件不存在

确认已经创建真实配置：

```bash
cp config/app.example.conf config/app.conf
```

### 数据库连接失败

检查：

```bash
sudo systemctl status mysql
mysql -h 127.0.0.1 -P 3306 -u oj_user -p oj
./build/oj_server --check-db config/app.conf
```

同时确认 `mysql.password` 与创建用户时设置的密码一致。

### 端口被占用

查看占用进程：

```bash
lsof -i :8080
```

可修改 `config/app.conf` 中的 `server.port` 后重启服务。

### 提交代码一直失败或返回系统错误

检查：

- 服务运行用户是否有项目目录写权限。
- `var/judge_tmp/` 是否可以创建临时目录。
- 系统是否可以执行 `g++`。
- 题目是否至少包含一条隐藏测试用例。
- 服务是否以 root 运行；部署建议使用普通用户运行。

### 静态页面能打开但 API 失败

先检查健康接口：

```bash
curl -sS http://127.0.0.1:8080/health
```

再检查浏览器请求地址是否与服务监听端口一致，并查看服务日志。

## 15. 上线前检查清单

- 已修改 `config/app.conf` 中的数据库密码。
- 已删除或修改种子默认账号密码。
- OJ 服务不以 root 身份运行。
- `var/` 目录对服务运行用户可写。
- MySQL 已启动并设置开机自启。
- `./build/oj_server --check-db config/app.conf` 通过。
- `bash scripts/deploy_verify.sh config/app.conf` 通过。
- 防火墙和安全组只开放必要端口。
- 已明确当前判题隔离不是强安全沙箱。
