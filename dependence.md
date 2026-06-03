# dependence.md

## 1. 环境假设

当前目标环境是一台空白的 Ubuntu 24.04 云服务器。

该文档只梳理项目依赖和安装命令，具体需求与架构见 `SPEC.md`。

---

## 2. 系统基础工具

用于拉取依赖、解压资源、执行构建脚本和排查问题。

```bash
sudo apt update
sudo apt install -y \
  ca-certificates \
  curl \
  wget \
  git \
  unzip \
  pkg-config \
  make \
  cmake
```

---

## 3. C++ 编译与构建依赖

用于编译 OJ 后端服务，以及编译用户提交的 C++ 代码。

```bash
sudo apt install -y \
  build-essential \
  g++ \
  gcc
```

验证命令：

```bash
g++ --version
make --version
```

---

## 4. MySQL 8 与开发库

用于持久化存储用户、管理员、题目和测试用例。

```bash
sudo apt install -y \
  mysql-server \
  mysql-client \
  libmysqlclient-dev
```

启动并设置开机自启：

```bash
sudo systemctl enable --now mysql
sudo systemctl status mysql
```

可选安全初始化：

```bash
sudo mysql_secure_installation
```

项目数据库和账号示例：

```bash
sudo mysql -e "CREATE DATABASE oj DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
sudo mysql -e "CREATE USER 'oj_user'@'localhost' IDENTIFIED BY 'change_me';"
sudo mysql -e "GRANT ALL PRIVILEGES ON oj.* TO 'oj_user'@'localhost';"
sudo mysql -e "FLUSH PRIVILEGES;"
```

实际部署时应替换 `change_me`，并将真实密码写入本机配置文件或环境变量，不提交到 Git。

---

## 5. 密码与会话相关依赖

用于密码哈希、随机 token、会话签名等能力。

```bash
sudo apt install -y libssl-dev
```

如果第一版暂时不接入 OpenSSL，也可以先用系统随机源生成 session token，但仍建议安装 `libssl-dev`，便于后续实现更规范的密码和会话处理。

---

## 6. 后端头文件依赖

后端需要：

- cpp-httplib
- JSON 库，建议使用 nlohmann/json

推荐方式一：使用 Ubuntu 包管理安装。

```bash
sudo apt install -y \
  libcpp-httplib-dev \
  nlohmann-json3-dev
```

推荐方式二：下载到项目 `third_party/` 目录，和源码一起管理。

```bash
mkdir -p third_party/httplib third_party/json
curl -L -o third_party/httplib/httplib.h \
  https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
curl -L -o third_party/json/json.hpp \
  https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
```

如果选择 `third_party/` 方式，构建参数需要包含：

```bash
-Ithird_party/httplib -Ithird_party/json
```

---

## 7. 前端本地静态资源依赖

前端不依赖公网 CDN，CodeMirror 或 Monaco Editor 需要下载到 `public/vendor/`。

使用 npm 下载前端依赖：

```bash
sudo apt install -y nodejs npm
mkdir -p public/vendor
npm install codemirror @codemirror/lang-cpp @codemirror/theme-one-dark
```

实现时可将需要的文件从 `node_modules/` 复制或打包到：

```text
public/vendor/codemirror/
```

如果选择 Monaco Editor，可替换为：

```bash
npm install monaco-editor
```

第一版建议优先使用 CodeMirror，因为静态页面集成成本更低。

---

## 8. 进程级资源限制相关工具

判题运行需要基础进程级隔离和资源限制。Ubuntu 默认已提供 `timeout`、`ulimit`、`nice` 等能力，建议确认 coreutils 已安装。

```bash
sudo apt install -y coreutils procps
```

验证命令：

```bash
timeout --version
ulimit -a
```

---

## 9. 可选调试与测试工具

用于开发阶段排查 API、进程、端口和数据库连接问题。

```bash
sudo apt install -y \
  gdb \
  valgrind \
  lsof \
  net-tools \
  jq
```

API 调试：

```bash
curl http://127.0.0.1:8080/api/problems
```

---

## 10. 最小完整安装命令

空白 Ubuntu 24.04 上可先执行以下命令安装第一版所需依赖：

```bash
sudo apt update
sudo apt install -y \
  ca-certificates curl wget git unzip pkg-config make cmake \
  build-essential gcc g++ \
  mysql-server mysql-client libmysqlclient-dev \
  libssl-dev \
  libcpp-httplib-dev nlohmann-json3-dev \
  nodejs npm \
  coreutils procps \
  gdb valgrind lsof net-tools jq
```

安装后启动 MySQL：

```bash
sudo systemctl enable --now mysql
```
