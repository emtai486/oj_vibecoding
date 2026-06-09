# 基于 curl 的接口自动化测试文档

本文档总结当前项目的接口自动化测试方案，覆盖第一批普通用户主链路、第二批管理员链路和第三批 12.6 判题接口回归。curl 脚本用于覆盖主链路基线，Python 脚本用于复用同一套 HTTP 流程并补充更复杂的判题 payload。

测试脚本位于：

```text
scripts/api_curl_test.sh
scripts/api_python_test.py
```

推荐通过 Makefile 执行：

```bash
make test-api-curl
```

运行 Python 版完整接口回归：

```bash
make test-api-python
```

## 1. 测试目标

接口自动化测试通过真实 HTTP 请求验证后端 API 合同、session/cookie 行为、数据库读写和关键业务链路，主要用于防止后续管理员功能、题目管理、判题逻辑调整时发生回归。

当前分两批覆盖：

- 第一批：普通用户主链路。
- 第二批：管理员题目管理链路。
- 第三批：12.6 判题接口回归，由 Python 脚本覆盖 strict、float_1、超时、资源限制、隐藏用例等提交行为。

## 2. 前置条件

### 2.1 依赖

本机需要具备：

- `curl`
- `make`
- `g++`
- MySQL 8
- 项目可正常编译生成 `build/oj_server`

提交相关测试会走判题逻辑，因此需要本机 C++ 编译环境可用。

### 2.2 配置文件

需要存在真实配置文件：

```text
config/app.conf
```

可从示例文件复制：

```bash
cp config/app.example.conf config/app.conf
```

然后按本机 MySQL 信息修改：

```text
mysql.host=127.0.0.1
mysql.port=3306
mysql.user=oj_user
mysql.password=change_me
mysql.database=oj
mysql.charset=utf8mb4
```

### 2.3 数据库初始化

完整接口测试依赖种子数据，执行前确保已导入：

```bash
mysql -u oj_user -p oj < sql/schema.sql
mysql -u oj_user -p oj < sql/seed.sql
```

种子账号：

```text
普通用户: user1 / password
管理员: admin / password
```

## 3. 执行方式

### 3.1 完整自动化测试

完整测试会临时启动本地服务，默认从 `18080` 开始寻找可用端口：

```bash
make
make test-api-curl
make test-api-python
```

等价于：

```bash
bash scripts/api_curl_test.sh config/app.conf
```

脚本行为：

- 读取 `config/app.conf`。
- 生成临时服务配置，把监听地址改为 `127.0.0.1`。
- 从 `18080` 到 `18120` 查找可用端口。
- 启动 `build/oj_server`。
- 使用 `curl` 发起真实 HTTP 请求。
- 使用临时 cookie jar 保存普通用户和管理员 session。
- 结束后自动停止临时服务并清理临时文件。

### 3.2 基础测试

基础测试不依赖数据库，只验证通用 API、JSON envelope 和未登录鉴权行为：

```bash
make
make test-api-curl-basic
make test-api-python-basic
```

等价于：

```bash
bash scripts/api_curl_test.sh --basic config/app.example.conf
```

### 3.3 对已启动服务测试

如果服务已经手动启动，可以不让脚本启动服务：

```bash
OJ_API_BASE_URL=http://127.0.0.1:8080 bash scripts/api_curl_test.sh --no-start
OJ_API_BASE_URL=http://127.0.0.1:8080 python3 scripts/api_python_test.py --no-start config/app.conf
```

也可以指定完整模式：

```bash
OJ_API_BASE_URL=http://127.0.0.1:8080 bash scripts/api_curl_test.sh --full --no-start
OJ_API_BASE_URL=http://127.0.0.1:8080 python3 scripts/api_python_test.py --full --no-start config/app.conf
```

## 4. 第一批：普通用户主链路覆盖

第一批重点验证普通用户可见题目、注册登录、session/cookie 和提交行为。

覆盖项：

- `GET /health` 健康检查。
- `POST /api/_echo` JSON 回显。
- 无效 JSON 返回 `400 invalid json`。
- 未注册 `/api/*` 路由返回 JSON 格式 `404 not found`。
- `GET /api/problems` 获取题目列表。
- `GET /api/problems/1` 获取题目详情。
- 题目详情只暴露样例测试用例，不暴露隐藏用例。
- `GET /api/problems/{missing_id}` 返回 `404 not found`。
- `GET /api/user/me` 未登录返回 `logged_in=false`。
- `POST /api/user/register` 注册随机测试用户。
- 重复注册同名用户返回 `409 username exists`。
- 非法用户名或密码返回 `400 invalid username or password`。
- 普通用户错误密码登录返回 `401 invalid username or password`。
- 普通用户正确登录并写入 `oj_user_session` Cookie。
- 登录后 `GET /api/user/me` 返回 `logged_in=true`。
- 未登录 `POST /api/submit` 返回 `401 unauthorized`。
- 登录后提交正确代码返回 `passed`。
- 登录后提交错误答案代码返回 `failed`。
- 登录后提交编译错误代码返回 `failed`。
- 登录后提交空代码返回 `failed`。
- 登录后提交不存在题目返回 `failed`。
- `POST /api/user/logout` 退出后 `GET /api/user/me` 返回 `logged_in=false`。

## 5. 第二批：管理员链路覆盖

第二批在完成 `SPEC.md` 12.5 管理员功能后补充，重点验证管理员登录、新增题目、删除题目和普通用户侧可见性变化。

覆盖项：

- `GET /api/admin/me` 未登录返回 `logged_in=false`。
- 未登录 `POST /api/admin/problems` 返回 `401 unauthorized`。
- 未登录 `DELETE /api/admin/problems/{id}` 返回 `401 unauthorized`。
- 管理员错误密码登录返回 `401 invalid username or password`。
- 管理员正确登录并写入 `oj_admin_session` Cookie。
- 登录后 `GET /api/admin/me` 返回 `logged_in=true`。
- 管理员新增题目缺少必填字段返回 `400 invalid problem`。
- 管理员可以新增包含样例用例和隐藏用例的题目。
- 新增题目后，普通用户通过 `GET /api/problems` 可以看到该题目。
- 新增题目后，普通用户通过 `GET /api/problems/{id}` 可以访问详情。
- 普通用户访问题目详情时只能看到样例用例，看不到隐藏用例。
- 管理员可以删除刚创建的题目。
- 删除后，普通用户访问该题目详情返回 `404 not found`。
- 删除后，普通用户题目列表不再显示该题目。
- 重复删除同一题目返回 `404 not found`。
- `POST /api/admin/logout` 退出后 `GET /api/admin/me` 返回 `logged_in=false`。

## 6. 第三批：12.6 判题接口回归覆盖

第三批在 `SPEC.md` 12.6 判题系统完成后补充，重点不再只依赖判题服务单元测试，而是通过真实 `/api/submit` HTTP 请求验证接口层、数据库题目配置、隐藏测试用例和判题服务之间的协作。

当前第三批由 `scripts/api_python_test.py` 覆盖，原因是这些用例包含较长 C++ 源码 payload，Python 标准库脚本比 shell 字符串更稳定、更容易维护。

覆盖项：

- 未登录 `POST /api/submit` 返回 `401 unauthorized`，不进入判题。
- 登录后提交 A+B 正确代码，`strict` 完全匹配输出，返回 `passed`。
- 登录后提交 A+B 错误答案，返回 `failed`。
- 登录后提交编译错误代码，返回 `failed`。
- 登录后提交空代码，返回 `failed`。
- 登录后提交不存在题目，返回 `failed`。
- `strict` 模式下缺少末尾换行，返回 `failed`。
- `float_1` 模式下输出按一位小数比较，通过用例返回 `passed`。
- `float_1` 模式下一位小数不匹配，返回 `failed`。
- 运行超过题目时间限制时返回 `failed`。
- 申请超过题目内存限制时返回 `failed`。
- 输出超过大小限制时返回 `failed`。
- 管理员创建题目时隐藏测试用例为空，返回 `400 invalid problem`。
- 样例输出正确但隐藏测试用例不匹配时，提交返回 `failed`。

这些用例共同覆盖 `SPEC.md` 12.6 中的编译、运行、stdin/stdout、严格比较、小数比较、时间限制、内存限制、输出限制、隐藏用例和失败收敛逻辑。

## 7. 测试数据说明

脚本会使用以下数据：

- 普通用户登录：`user1 / password`。
- 管理员登录：`admin / password`。
- 注册用户：动态生成，格式类似 `api_test_<timestamp>_<pid>`。
- 管理员新增题目：动态生成标题，格式类似 `Curl Admin Problem <test_user>`。
- Python 判题回归使用种子题目 `1` A+B Problem 和 `2` Average Score。

注意：

- 注册用户会保留在数据库中，因为普通用户注册接口没有删除用户功能。
- 管理员测试创建的题目会在测试流程中删除。
- 如果测试中途被强制终止，可能残留 `Curl Admin Problem ...` 测试题目，可以在数据库中手动清理。

清理残留测试题目的示例 SQL：

```sql
DELETE FROM problems WHERE title LIKE 'Curl Admin Problem api_test_%';
```

`testcases` 表通过外键 `ON DELETE CASCADE` 自动删除关联测试用例。

## 8. 成功输出

成功时会看到类似输出：

```text
PASS: GET /health
PASS: POST /api/_echo
PASS: GET /api/problems
PASS: POST /api/user/login
PASS: POST /api/submit accepted code
PASS: POST /api/submit strict newline mismatch
PASS: POST /api/submit float_1 accepted
PASS: POST /api/submit timeout
PASS: POST /api/submit memory limit
PASS: POST /api/submit output limit
PASS: POST /api/admin/login
PASS: POST /api/admin/problems
PASS: POST /api/admin/problems empty hidden testcases
PASS: POST /api/submit hidden testcase mismatch
PASS: DELETE /api/admin/problems/{created}
PASS: GET /api/admin/me after logout
```

所有用例均显示 `PASS` 且命令退出码为 `0`，表示测试通过。

## 9. 常见失败排查

### 9.1 `build/oj_server not found`

先执行构建：

```bash
make
```

### 9.2 `config not found: config/app.conf`

复制并修改配置：

```bash
cp config/app.example.conf config/app.conf
```

### 9.3 `database error`

检查：

- MySQL 是否启动。
- `config/app.conf` 中账号、密码、数据库名是否正确。
- 是否已导入 `sql/schema.sql` 和 `sql/seed.sql`。
- `admins` 表是否存在 `admin`。
- `users` 表是否存在 `user1`。

可手动检查：

```bash
mysql -u oj_user -p oj -e "SELECT id, username FROM users; SELECT id, username FROM admins;"
```

### 9.4 管理员或普通用户登录失败

重新导入种子数据：

```bash
mysql -u oj_user -p oj < sql/seed.sql
```

种子脚本会通过 `ON DUPLICATE KEY UPDATE` 重置 `user1` 和 `admin` 的密码 hash。

### 9.5 服务端口无法监听

脚本默认尝试 `18080-18120`。如果这些端口被占用，可以指定起始端口：

```bash
OJ_API_TEST_PORT=19080 make test-api-curl
```

如果运行环境限制本机端口监听，需要在允许监听本机端口的环境中执行。

### 9.6 提交测试失败

检查：

- 本机 `g++` 是否可用。
- 判题临时目录是否可写。
- `problem_id=1` 的 A+B 种子题目是否存在。
- `testcases` 表中该题目的样例和隐藏用例是否存在。

## 10. 后续扩展建议

当前已经覆盖 12.6 的主要接口级回归。后续仍可继续扩展：

- 并发提交限制的接口级压力测试。
- 更细粒度的错误分类，例如区分 compile error、time limit exceeded、wrong answer。
- 管理员创建自定义时间限制和内存限制题目后，再提交对应边界代码。
- 判题临时目录残留检查的端到端用例。

新增接口时建议同步更新：

- `API.md`：先写清 API 合同、请求、响应、错误码。
- `scripts/api_curl_test.sh`：再补 curl 自动化用例。
- `scripts/api_python_test.py`：复杂 payload 或判题场景优先补 Python 自动化用例。
- 本文档：同步补充覆盖项和执行说明。
