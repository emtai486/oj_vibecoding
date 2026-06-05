# SPEC.md

## 1. 项目概述

### 1.1 项目名称

仿 LeetCode 在线判题系统 OJ

### 1.2 项目目标

搭建一个用于学习和实践的轻量级 Online Judge 系统，支持用户在线查看题目、编辑 C++ 代码、提交代码、编译运行并返回判题结果。

系统面向 1-20 人小规模在线使用，重点验证：

- C++ 后端 Web 服务开发能力
- MySQL 数据持久化能力
- 在线编译与运行判题流程
- 基础进程级隔离与资源限制
- 原生 HTML/CSS/JS 前端页面开发
- 类 LeetCode 的做题交互体验

### 1.3 成功标准

第一版系统完成后，应满足：

- 未登录用户可以访问题目列表和题目详情
- 未登录用户只能查看题目，不允许提交代码
- 登录普通用户可以在页面内编辑 C++ 代码
- 登录普通用户可以提交代码并获得“通过 / 失败”结果
- 登录普通用户的题目完成状态可在当前浏览器中保留
- 管理员可以登录后台
- 管理员可以新增题目
- 管理员可以删除题目
- 题目、样例测试用例、隐藏测试用例持久化存储到 MySQL
- 系统可在 Ubuntu 24.04 + g++ + MySQL 8 环境运行
- 前端第三方编辑器资源本地化，不依赖公网 CDN

---

## 2. 用户角色

### 2.1 未登录用户

未登录用户只允许查看题目，主要能力：

- 查看题目列表
- 查看题目详情
- 查看样例测试用例

限制：

- 不能提交代码
- 不能查看隐藏测试用例
- 不能新增、删除题目
- 不能访问管理员后台

### 2.2 普通用户

普通用户需要登录后才能提交代码，主要能力：

- 登录普通用户账号

- 查看题目列表
- 查看题目详情
- 查看样例测试用例
- 在线编辑 C++ 代码
- 提交代码进行判题
- 查看判题结果：通过 / 失败
- 在浏览器本地保存题目完成状态

限制：

- 不能查看隐藏测试用例
- 不能新增、删除题目
- 不保存提交记录到服务端
- 不保存用户代码到服务端

### 2.3 管理员

管理员需要登录。

管理员账号通过数据库初始化记录创建。

管理员能力：

- 登录管理后台
- 新增题目
- 删除题目
- 新增题目时同时录入：
  - 题目基础信息
  - 样例测试用例
  - 隐藏测试用例
  - 时间限制
  - 内存限制
  - 输出比较模式

第一版暂不支持：

- 编辑已有题目
- 查看用户提交记录
- 用户管理
- 题目软删除

---

## 3. 功能需求

### 3.1 题目列表

未登录用户和登录普通用户都可以查看题目列表。

题目列表字段：

- 题目 ID
- 标题
- 难度
- 完成状态

完成状态仅对登录普通用户有意义，存储在浏览器 `localStorage` 中。

未登录用户不展示可提交状态，也不能通过本地状态标记题目完成。

状态示例：

- 未完成
- 已通过

### 3.2 题目详情

题目详情页采用类 LeetCode 布局：

- 左侧：题目描述区
- 右侧：代码编辑区
- 底部：提交结果展示区

题目详情展示字段：

- 标题
- 难度
- 题目描述
- 输入格式
- 输出格式
- 样例输入
- 样例输出
- 时间限制
- 内存限制

普通用户不可见隐藏测试用例。

未登录用户可以查看题目详情和样例测试用例，但提交按钮应禁用或提示登录后提交。

### 3.3 在线代码编辑

前端允许引入 CodeMirror 或 Monaco Editor。

要求：

- 编辑器资源下载到本地静态资源目录
- 不依赖公网 CDN
- 支持 C++ 代码编辑
- 支持行号
- 支持基础语法高亮
- 页面默认提供半模板 ACM 代码

默认代码模板示例：

```cpp
#include <bits/stdc++.h>
using namespace std;

int main() {
    // write your code here
    return 0;
}
```

登录普通用户可以修改完整代码后提交。

未登录用户可以查看默认代码模板，但不能提交代码。

### 3.4 代码提交与判题

登录普通用户提交完整 C++ 程序。

未登录用户访问提交接口时应返回未登录或无权限结果，不进入编译和判题流程。

后端流程：

1. 接收题目 ID 和用户代码
2. 查询题目隐藏测试用例
3. 将代码写入临时文件
4. 使用 `g++` 编译
5. 编译成功后运行程序
6. 对每个隐藏测试用例执行：
   - 输入写入程序 stdin
   - 捕获 stdout
   - 与期望输出比较
7. 返回整体判题结果

第一版只返回：

- 通过
- 失败

以下情况统一返回失败：

- 编译错误
- 运行错误
- 超时
- 内存超限
- 输出不匹配
- 程序崩溃
- 空代码提交

### 3.5 输出比较规则

每道题支持配置比较模式。

第一版支持：

#### strict

严格字符串比较。

要求：

- 实际输出必须与期望输出完全一致
- 空格、换行都参与比较

#### float_1

小数比较模式。

适用于少量需要按小数点后一位比较的题目。

建议规则：

- 将实际输出和期望输出按 token 拆分
- token 数量必须一致
- 可解析为数字的 token 按保留 1 位比较
- 非数字 token 按字符串严格比较

### 3.6 普通用户登录

普通用户需要登录后才能提交题目。

第一版普通用户账号可以通过注册创建，或由数据库初始化脚本预置测试账号。

登录成功后使用 session/cookie 保持登录状态。

未登录用户只能查看题目列表、题目详情和样例测试用例。

第一版不需要：

- 找回密码
- 邮件验证
- 多设备登录管理
- 用户资料页

### 3.7 管理员登录

管理员账号来自数据库初始化记录。

登录成功后使用 session/cookie 保持登录状态。

第一版不需要：

- 管理员注册
- 找回密码
- 多管理员权限分级

### 3.8 管理员新增题目

新增题目字段：

- 标题
- 难度
- 题目描述
- 输入格式
- 输出格式
- 样例输入
- 样例输出
- 隐藏测试用例
- 时间限制
- 内存限制
- 输出比较模式

隐藏测试用例可以录入多组。

每组测试用例包含：

- 输入
- 期望输出

### 3.9 管理员删除题目

删除题目为物理删除。

删除题目时应同时删除关联测试用例。

---

## 4. 非功能需求

### 4.1 性能

目标使用规模：

- 1-20 人在线
- 同时判题进程限制为 2-4 个

默认资源限制：

- 时间限制：1 秒
- 内存限制：128 MB

每道题可配置自己的时间限制和内存限制。

未配置时使用默认值。

### 4.2 并发

系统允许并发提交，但必须限制同时运行的判题进程数量。

建议策略：

- 固定判题并发池，大小 2-4
- 超出并发上限时进入排队
- 第一版可简单实现为请求等待
- 若队列过长，可返回“失败”或“系统繁忙”

### 4.3 安全

第一版采用基础进程级隔离。

需要实现：

- 编译和运行使用临时目录
- 每次提交使用独立临时文件
- 限制运行时间
- 限制内存
- 限制输出大小
- 运行结束后清理临时文件
- 禁止服务进程以 root 身份运行
- 用户代码运行进程使用较低权限用户

明确不包含：

- Docker 沙箱
- Linux namespace 强隔离
- cgroup 完整资源控制
- 系统调用白名单
- 网络访问隔离

风险说明：

进程级隔离不能完全防御恶意代码。该方案适合学习项目和受控环境，不适合公网开放给不可信用户。

### 4.4 可扩展性

第一版采用单体服务。

同一个 C++ 服务负责：

- HTTP API
- 静态资源服务
- MySQL 访问
- 管理员 session
- 判题任务调度
- 编译与运行程序

后续可拆分：

- Web 服务
- Judge Worker
- 任务队列
- 独立沙箱服务
- Redis 缓存
- 用户系统
- 提交记录系统

### 4.5 成本

目标部署在单台云服务器。

推荐最低配置：

- 2 核 CPU
- 2 GB 内存
- 20 GB 磁盘
- Ubuntu 24.04
- MySQL 8
- g++

---

## 5. 技术栈

### 5.1 后端

- C++17 或 C++20
- cpp-httplib
- MySQL 8
- MySQL C API 或 C++ MySQL Connector
- g++ 作为用户代码编译器

### 5.2 前端

- 原生 HTML
- 原生 CSS
- 原生 JavaScript
- CodeMirror 或 Monaco Editor
- 本地静态资源

### 5.3 部署环境

验收环境：

- Ubuntu 24.04
- g++
- MySQL 8
- cpp-httplib
- 本地化前端静态资源

## 6. 架构设计

### 6.1 总体架构

```text
+---------------------+
|      Browser        |
| HTML/CSS/JS Editor  |
+----------+----------+
           |
           | HTTP JSON API
           |
+----------v----------+
|   C++ Web Server    |
|   cpp-httplib       |
+----------+----------+
           |
           +----------------------+
           |                      |
+----------v----------+  +--------v---------+
|      MySQL 8        |  |   Judge Runner   |
| problems/testcases  |  | g++ compile/run  |
+---------------------+  +--------+---------+
                                  |
                         +--------v---------+
                         | temporary files  |
                         | process limits   |
                         +------------------+
```

### 6.2 后端模块

```text
server
├── http_api
│   ├── problem api
│   ├── judge api
│   └── admin api
├── auth
│   ├── user login
│   ├── admin login
│   └── session cookie
├── db
│   ├── mysql connection
│   ├── problem repository
│   └── testcase repository
├── judge
│   ├── compile
│   ├── run
│   ├── compare output
│   └── concurrency limit
└── static
    ├── html
    ├── css
    ├── js
    └── editor assets
```

### 6.3 项目目录结构

第一版项目建议采用单体仓库结构，后端、前端静态资源、数据库脚本和部署脚本放在同一仓库内。

```text
oj-project/
├── SPEC.md
├── README.md
├── Makefile
├── config/
│   └── app.example.conf
├── scripts/
│   ├── build.sh
│   ├── run.sh
│   └── init_db.sh
├── sql/
│   ├── schema.sql
│   └── seed.sql
├── third_party/
│   ├── httplib/
│   │   └── httplib.h
│   └── json/
│       └── json.hpp
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── server.cpp
│   │   └── server.h
│   ├── api/
│   │   ├── problem_api.cpp
│   │   ├── problem_api.h
│   │   ├── submit_api.cpp
│   │   ├── submit_api.h
│   │   ├── user_api.cpp
│   │   ├── user_api.h
│   │   ├── admin_api.cpp
│   │   └── admin_api.h
│   ├── auth/
│   │   ├── session.cpp
│   │   ├── session.h
│   │   ├── password.cpp
│   │   └── password.h
│   ├── config/
│   │   ├── config.cpp
│   │   └── config.h
│   ├── db/
│   │   ├── mysql_client.cpp
│   │   ├── mysql_client.h
│   │   ├── user_repository.cpp
│   │   ├── user_repository.h
│   │   ├── admin_repository.cpp
│   │   ├── admin_repository.h
│   │   ├── problem_repository.cpp
│   │   ├── problem_repository.h
│   │   ├── testcase_repository.cpp
│   │   └── testcase_repository.h
│   ├── judge/
│   │   ├── judge_service.cpp
│   │   ├── judge_service.h
│   │   ├── compiler.cpp
│   │   ├── compiler.h
│   │   ├── runner.cpp
│   │   ├── runner.h
│   │   ├── comparator.cpp
│   │   ├── comparator.h
│   │   ├── judge_queue.cpp
│   │   └── judge_queue.h
│   ├── model/
│   │   ├── user.h
│   │   ├── admin.h
│   │   ├── problem.h
│   │   └── testcase.h
│   └── util/
│       ├── json_response.cpp
│       ├── json_response.h
│       ├── file_util.cpp
│       ├── file_util.h
│       ├── process_util.cpp
│       └── process_util.h
├── public/
│   ├── index.html
│   ├── problem.html
│   ├── login.html
│   ├── register.html
│   ├── admin/
│   │   ├── login.html
│   │   ├── index.html
│   │   └── new-problem.html
│   ├── css/
│   │   ├── base.css
│   │   ├── layout.css
│   │   └── admin.css
│   ├── js/
│   │   ├── api.js
│   │   ├── auth.js
│   │   ├── problem-list.js
│   │   ├── problem-detail.js
│   │   ├── editor.js
│   │   ├── storage.js
│   │   └── admin.js
│   └── vendor/
│       └── codemirror/
├── var/
│   └── judge_tmp/
└── tests/
    ├── judge/
    ├── db/
    └── api/
```

目录职责说明：

- `config/`: 存放配置文件模板，不提交真实数据库密码。
- `scripts/`: 存放构建、启动、初始化数据库等脚本。
- `sql/`: 存放建表 SQL 和初始化数据 SQL。
- `third_party/`: 存放 cpp-httplib、JSON 库等头文件依赖。
- `src/api/`: HTTP API 路由与请求处理。
- `src/auth/`: 普通用户、管理员登录态与密码处理。
- `src/db/`: MySQL 访问封装和 Repository。
- `src/judge/`: 编译、运行、比较、并发限制等判题逻辑。
- `src/model/`: 数据模型定义。
- `src/util/`: JSON 响应、文件、进程等通用工具。
- `public/`: 前端 HTML/CSS/JS 和本地化编辑器资源。
- `public/vendor/`: 本地第三方前端依赖，例如 CodeMirror。
- `var/judge_tmp/`: 判题临时目录，运行时创建并定期清理。
- `tests/`: 后续测试代码目录，第一版可按风险逐步补充。

---

## 7. 数据模型

### 7.1 users

普通用户表。

```sql
users
- id
- username
- password_hash
- created_at
```

说明：

- 普通用户登录后才允许提交代码
- 密码不应明文存储
- 第一版可支持注册，或通过初始化 SQL 创建测试用户

### 7.2 admins

管理员表。

```sql
admins
- id
- username
- password_hash
- created_at
```

说明：

- 管理员账号通过初始化 SQL 创建
- 密码不应明文存储
- 第一版可使用固定初始化账号

### 7.3 problems

题目表。

```sql
problems
- id
- title
- difficulty
- description
- input_format
- output_format
- sample_input
- sample_output
- time_limit_ms
- memory_limit_kb
- compare_mode
- created_at
```

字段说明：

- `difficulty`: easy / medium / hard
- `time_limit_ms`: 默认 1000
- `memory_limit_kb`: 默认 131072
- `compare_mode`: strict / float_1

### 7.4 testcases

测试用例表。

```sql
testcases
- id
- problem_id
- input
- expected_output
- is_sample
- created_at
```

字段说明：

- `is_sample = true`: 样例测试用例，普通用户可见
- `is_sample = false`: 隐藏测试用例，普通用户不可见

---

## 8. API 设计

### 8.1 统一返回格式

```json
{
  "success": true,
  "message": "ok",
  "data": {}
}
```

失败示例：

```json
{
  "success": false,
  "message": "failed",
  "data": null
}
```

### 8.2 公开 API

#### GET /api/problems

获取题目列表。

未登录用户和登录用户均可访问。

返回：

```json
{
  "success": true,
  "message": "ok",
  "data": [
    {
      "id": 1,
      "title": "A+B Problem",
      "difficulty": "easy"
    }
  ]
}
```

#### GET /api/problems/{id}

获取题目详情。

返回内容包含样例测试用例，不包含隐藏测试用例。

未登录用户和登录用户均可访问。

### 8.3 普通用户 API

#### POST /api/user/register

普通用户注册。

第一版可以实现注册，也可以通过初始化 SQL 预置测试用户后不开放注册入口。

请求：

```json
{
  "username": "user1",
  "password": "password"
}
```

#### POST /api/user/login

普通用户登录。

请求：

```json
{
  "username": "user1",
  "password": "password"
}
```

登录成功后通过 session/cookie 保持登录状态。

#### POST /api/user/logout

普通用户退出登录。

#### POST /api/submit

提交代码。

需要普通用户登录。

未登录用户调用该接口时不进入判题流程，应返回：

```json
{
  "success": false,
  "message": "unauthorized",
  "data": null
}
```

请求：

```json
{
  "problem_id": 1,
  "code": "#include <bits/stdc++.h>\nusing namespace std;\nint main(){return 0;}"
}
```

返回：

```json
{
  "success": true,
  "message": "accepted",
  "data": {
    "result": "passed"
  }
}
```

失败返回：

```json
{
  "success": true,
  "message": "failed",
  "data": {
    "result": "failed"
  }
}
```

### 8.4 管理员 API

#### POST /api/admin/login

管理员登录。

请求：

```json
{
  "username": "admin",
  "password": "password"
}
```

#### POST /api/admin/problems

新增题目。

需要管理员登录。

#### DELETE /api/admin/problems/{id}

删除题目。

需要管理员登录。

#### POST /api/admin/logout

管理员退出登录。

---

## 9. 前端页面

### 9.1 页面列表

```text
/
题目列表页

/problem.html?id=1
题目详情与代码提交页

/login.html
普通用户登录页

/register.html
普通用户注册页，可选

/admin/login.html
管理员登录页

/admin/index.html
管理员后台首页

/admin/new-problem.html
新增题目页
```

### 9.2 题目详情页布局

```text
+--------------------------------------------------+
| Header                                           |
+------------------------+-------------------------+
| Problem Description    | Code Editor             |
|                        |                         |
| - title                | #include <bits/stdc++.h>|
| - difficulty           | using namespace std;    |
| - description          |                         |
| - input/output format  |                         |
| - samples              |                         |
+------------------------+-------------------------+
| Result Panel                                     |
| [Submit]   Result: passed / failed              |
+--------------------------------------------------+
```

### 9.3 localStorage 设计

用于保存登录普通用户在当前浏览器中的完成状态。

未登录用户不写入完成状态。

key 示例：

```text
oj_problem_status
```

value 示例：

```json
{
  "1": "passed",
  "2": "passed"
}
```

---

## 10. 判题流程

### 10.1 编译流程

```text
receive code
    |
create temp directory
    |
write main.cpp
    |
run g++ main.cpp -std=c++17 -O2 -o main
    |
compile success?
    |
yes -> run testcases
no  -> failed
```

### 10.2 运行流程

```text
for each hidden testcase:
    start executable process
    pass testcase input to stdin
    capture stdout
    enforce time limit
    enforce memory limit
    enforce output size limit
    compare stdout with expected output
    if mismatch -> failed

all passed -> passed
```

### 10.3 异常处理

统一返回失败：

- 编译失败
- 执行超时
- 内存超限
- 输出过大
- 程序异常退出
- 输出错误
- 测试用例不存在
- 题目不存在
- 空代码

---

## 11. 主要风险与权衡

### 11.1 安全风险

用户提交代码本质上是不可信代码。

第一版只做进程级隔离，不能完全防止恶意行为。

风险包括：

- 读取服务器文件
- 创建大量进程
- 消耗 CPU / 内存 / 磁盘
- 尝试访问网络
- 利用系统漏洞逃逸限制

权衡：

- 第一版目标是学习和小规模受控使用
- 暂不引入 Docker / namespace / cgroup
- 后续如公网部署，应升级沙箱方案

### 11.2 并发风险

多人同时提交可能压垮服务器。

第一版限制 2-4 个并发判题进程。

权衡：

- 实现简单
- 适合学习项目
- 高并发场景需要任务队列和独立 Judge Worker

### 11.3 数据模型权衡

提交记录和用户代码暂不持久化。

优点：

- 系统复杂度低
- 只需要最小登录系统，不需要完整用户资料和提交历史
- 适合第一版

缺点：

- 不能查看历史提交
- 不能跨设备同步完成状态
- 登录用户的完成状态仍只保存在当前浏览器

### 11.4 输出比较权衡

第一版支持严格字符串比较和小数点后一位比较。

优点：

- 实现简单
- 覆盖基础题目

缺点：

- 对格式较敏感
- 暂不支持 Special Judge
- 暂不支持误差范围配置

---

## 12. TODO 清单

### 12.1 初始化
code
- [x] 创建项目目录结构
- [x] 引入 cpp-httplib
- [x] 配置 MySQL 连接
- [x] 准备静态资源目录
- [x] 下载 CodeMirror 或 Monaco 到本地
- [x] 编写 README

### 12.2 数据库

- [x] 编写 MySQL 建表 SQL
- [x] 创建 users 表
- [x] 创建 admins 表
- [x] 创建 problems 表
- [x] 创建 testcases 表
- [x] 编写初始化普通用户测试账号 SQL，或实现普通用户注册
- [x] 编写初始化管理员账号 SQL
- [x] 准备示例题目数据

### 12.3 后端基础

- [x] 启动 cpp-httplib HTTP 服务
- [x] 提供静态文件访问
- [x] 实现 JSON 请求解析
- [x] 实现统一 JSON 响应
- [x] 实现 MySQL 查询封装
- [x] 实现错误处理

### 12.4 普通用户功能

- [x] 实现 GET /api/problems
- [x] 实现 GET /api/problems/{id}
- [x] 实现 POST /api/user/register，或提供初始化测试用户
- [x] 实现 POST /api/user/login
- [x] 实现 POST /api/user/logout
- [x] 实现普通用户 session/cookie
- [x] 实现 POST /api/submit
- [x] 实现未登录用户禁止提交
- [x] 实现题目列表页面
- [x] 实现题目详情页面
- [x] 实现普通用户登录页
- [x] 实现普通用户注册页，可选
- [x] 实现代码编辑器
- [x] 实现提交结果展示
- [x] 实现 localStorage 完成状态

### 12.5 管理员功能

- [x] 实现 POST /api/admin/login
- [x] 实现 POST /api/admin/logout
- [x] 实现管理员 session/cookie
- [x] 实现 POST /api/admin/problems
- [x] 实现 DELETE /api/admin/problems/{id}
- [x] 实现管理员登录页
- [x] 实现管理员后台页
- [x] 实现新增题目页
- [x] 实现删除题目功能

### 12.6 判题系统

- [ ] 创建临时工作目录
- [ ] 写入用户代码文件
- [ ] 调用 g++ 编译
- [ ] 捕获编译结果
- [ ] 执行用户程序
- [ ] 传入测试用例 stdin
- [ ] 捕获 stdout
- [ ] 限制运行时间
- [ ] 限制内存
- [ ] 限制输出大小
- [ ] 实现 strict 比较
- [ ] 实现 float_1 比较
- [ ] 实现并发判题限制
- [ ] 清理临时文件

### 12.7 部署与验收

- [ ] 编写构建脚本
- [ ] 编写启动脚本
- [ ] 编写数据库初始化脚本
- [ ] 在 Ubuntu 24.04 验证
- [ ] 验证 g++ 编译运行
- [ ] 验证 MySQL 连接
- [ ] 验证前端静态资源本地加载
- [ ] 验证普通用户登录后才能提交代码
- [ ] 验证管理员新增/删除题目
- [ ] 验证普通用户提交代码

---

## 13. 验收标准

### 13.1 环境验收

在以下环境中可以运行：

- Ubuntu 24.04
- g++
- MySQL 8
- cpp-httplib
- 本地静态前端资源

验收项：

- [ ] 可以初始化数据库
- [ ] 可以创建管理员账号
- [ ] 可以编译 C++ 后端服务
- [ ] 可以启动服务
- [ ] 浏览器可以访问首页

### 13.2 普通用户验收

- [ ] 用户无需登录即可进入题目列表
- [ ] 用户可以看到题目标题和难度
- [ ] 用户可以进入题目详情页
- [ ] 用户只能看到样例测试用例
- [ ] 用户看不到隐藏测试用例
- [ ] 未登录用户不能提交代码
- [ ] 未登录用户调用提交接口时返回未登录或无权限
- [ ] 普通用户可以登录
- [ ] 登录普通用户可以编辑 C++ 代码
- [ ] 登录普通用户可以提交代码
- [ ] 正确代码返回通过
- [ ] 错误代码返回失败
- [ ] 编译错误返回失败
- [ ] 超时代码返回失败
- [ ] 登录普通用户通过后的题目状态保存到 localStorage

### 13.3 管理员验收

- [ ] 管理员可以登录
- [ ] 管理员登录状态通过 session/cookie 保持
- [ ] 管理员可以新增题目
- [ ] 新增题目可以包含样例测试用例
- [ ] 新增题目可以包含隐藏测试用例
- [ ] 新增题目后普通用户可以看到
- [ ] 管理员可以删除题目
- [ ] 删除题目后普通用户不可访问该题目

### 13.4 判题验收

- [ ] 系统可以编译 C++17 代码
- [ ] 系统可以运行用户程序
- [ ] 系统可以将测试输入传给 stdin
- [ ] 系统可以捕获 stdout
- [ ] strict 模式可以严格比较输出
- [ ] float_1 模式可以按小数点后一位比较
- [ ] 超过 1 秒默认时间限制时返回失败
- [ ] 超过 128 MB 默认内存限制时返回失败
- [ ] 多人同时提交时最多运行 2-4 个判题进程

### 13.5 安全验收

- [ ] 用户代码在临时目录中编译和运行
- [ ] 每次提交使用独立临时目录
- [ ] 判题结束后清理临时文件
- [ ] 服务进程不以 root 身份运行
- [ ] 用户程序受到时间限制
- [ ] 用户程序受到内存限制
- [ ] 用户程序受到输出大小限制

---

## 14. 第一版明确不做

- 服务端保存提交记录
- 服务端保存用户代码
- 完整用户资料系统
- 找回密码
- 邮件验证
- 排行榜
- 题解
- 评论区
- 竞赛模式
- 多语言支持
- Special Judge
- Docker 沙箱
- 完整 namespace/cgroup 隔离
- 管理员编辑题目
- 题目软删除
- 代码查重
- 邮件通知
- 移动端深度适配

---

## 15. 后续演进方向

后续可逐步扩展：

- 提交记录持久化
- 用户代码历史版本
- 完整用户资料系统
- 题目编辑功能
- 排行榜
- 题解系统
- 竞赛模式
- 多语言支持
- Docker 沙箱
- Judge Worker 独立服务
- Redis 任务队列
- 更严格资源隔离
- Special Judge
- 题目标签和搜索
- 用户通过率统计
