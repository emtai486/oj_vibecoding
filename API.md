# API.md

本文档基于 `SPEC.md` 和当前项目代码整理，描述第一版已实现和规划中的 HTTP API 合同。

## 1. 通用约定

### 1.1 基础地址

本地开发默认地址：

```text
http://127.0.0.1:8080
```

实际监听地址由配置文件决定：

```text
server.host=0.0.0.0
server.port=8080
```

### 1.2 请求格式

除静态资源外，业务接口使用 JSON：

```http
Content-Type: application/json
```

### 1.3 统一响应格式

成功和失败都返回统一 JSON envelope：

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
  "message": "unauthorized",
  "data": null
}
```

### 1.4 登录态

普通用户登录成功后，服务端通过 Cookie 保存 session：

```text
oj_user_session=<session_id>; Path=/; HttpOnly; SameSite=Lax
```

需要普通用户登录的接口必须携带该 Cookie。

管理员登录态在 `SPEC.md` 中规划，但当前后端尚未实现管理员 API 和管理员 session。

## 2. 基础 API

### 2.1 健康检查

```http
GET /health
```

认证要求：无。

成功响应：

```json
{
  "success": true,
  "message": "ok",
  "data": {
    "status": "ok"
  }
}
```

### 2.2 JSON 回显

```http
POST /api/_echo
```

认证要求：无。

用途：开发和测试 JSON 请求解析、统一响应格式。

请求示例：

```json
{
  "hello": "world"
}
```

成功响应：

```json
{
  "success": true,
  "message": "ok",
  "data": {
    "hello": "world"
  }
}
```

无效 JSON：

```http
HTTP/1.1 400 Bad Request
```

```json
{
  "success": false,
  "message": "invalid json",
  "data": null
}
```

## 3. 题目 API

### 3.1 获取题目列表

```http
GET /api/problems
```

认证要求：无。未登录用户和登录用户均可访问。

成功响应：

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

字段说明：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | number | 题目 ID |
| `title` | string | 题目标题 |
| `difficulty` | string | 难度，取值 `easy` / `medium` / `hard` |

可能错误：

| HTTP 状态码 | message | 说明 |
| --- | --- | --- |
| 500 | `database error` | 数据库连接或查询失败 |

### 3.2 获取题目详情

```http
GET /api/problems/{id}
```

认证要求：无。未登录用户和登录用户均可访问。

成功响应：

```json
{
  "success": true,
  "message": "ok",
  "data": {
    "id": 1,
    "title": "A+B Problem",
    "difficulty": "easy",
    "description": "Read two integers a and b and output their sum.",
    "input_format": "Two integers a and b separated by spaces.",
    "output_format": "Print one integer: a + b.",
    "sample_input": "1 2\n",
    "sample_output": "3\n",
    "time_limit_ms": 1000,
    "memory_limit_kb": 131072,
    "compare_mode": "strict",
    "samples": [
      {
        "id": 1,
        "input": "1 2\n",
        "expected_output": "3\n",
        "is_sample": true
      }
    ]
  }
}
```

注意：

- 详情接口只返回样例测试用例。
- 隐藏测试用例不返回给普通用户或未登录用户。

可能错误：

| HTTP 状态码 | message | 说明 |
| --- | --- | --- |
| 404 | `not found` | 题目不存在 |
| 500 | `database error` | 数据库连接或查询失败 |

## 4. 普通用户 API

### 4.1 查询当前用户

```http
GET /api/user/me
```

认证要求：无。未登录时也返回 200。

未登录响应：

```json
{
  "success": true,
  "message": "ok",
  "data": {
    "logged_in": false
  }
}
```

已登录响应：

```json
{
  "success": true,
  "message": "ok",
  "data": {
    "logged_in": true,
    "user": {
      "id": 1,
      "username": "user1"
    }
  }
}
```

### 4.2 注册普通用户

```http
POST /api/user/register
```

认证要求：无。

请求：

```json
{
  "username": "new_user",
  "password": "password"
}
```

校验规则：

- `username` 长度为 3-64。
- `username` 只能包含字母、数字、下划线和短横线。
- `password` 长度至少为 6。

成功响应：

```http
HTTP/1.1 201 Created
```

```json
{
  "success": true,
  "message": "registered",
  "data": {
    "id": 2,
    "username": "new_user"
  }
}
```

可能错误：

| HTTP 状态码 | message | 说明 |
| --- | --- | --- |
| 400 | `invalid json` | 请求体不是合法 JSON |
| 400 | `invalid username or password` | 用户名或密码不符合规则 |
| 409 | `username exists` | 用户名已存在 |
| 500 | `database error` | 数据库连接或查询失败 |

### 4.3 普通用户登录

```http
POST /api/user/login
```

认证要求：无。

请求：

```json
{
  "username": "user1",
  "password": "password"
}
```

成功响应：

```json
{
  "success": true,
  "message": "logged in",
  "data": {
    "id": 1,
    "username": "user1"
  }
}
```

响应头包含：

```http
Set-Cookie: oj_user_session=<session_id>; Path=/; HttpOnly; SameSite=Lax
```

可能错误：

| HTTP 状态码 | message | 说明 |
| --- | --- | --- |
| 400 | `invalid json` | 请求体不是合法 JSON |
| 400 | `invalid username or password` | 缺少用户名或密码字段 |
| 401 | `invalid username or password` | 用户不存在或密码错误 |
| 500 | `database error` | 数据库连接或查询失败 |

### 4.4 普通用户退出

```http
POST /api/user/logout
```

认证要求：无。已登录时会销毁当前 session；未登录时也返回成功。

成功响应：

```json
{
  "success": true,
  "message": "logged out",
  "data": null
}
```

响应头包含清理 Cookie：

```http
Set-Cookie: oj_user_session=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax
```

## 5. 提交 API

### 5.1 提交代码

```http
POST /api/submit
```

认证要求：普通用户登录。

请求：

```json
{
  "problem_id": 1,
  "code": "#include <bits/stdc++.h>\nusing namespace std;\nint main(){int a,b;cin>>a>>b;cout<<a+b<<'\\n';return 0;}"
}
```

通过响应：

```json
{
  "success": true,
  "message": "accepted",
  "data": {
    "result": "passed"
  }
}
```

失败响应：

```json
{
  "success": true,
  "message": "failed",
  "data": {
    "result": "failed"
  }
}
```

当前第一版会将以下情况统一返回为失败：

- 空代码。
- 题目不存在。
- 隐藏测试用例不存在。
- 编译错误。
- 运行错误。
- 超时。
- 内存超限。
- 输出超限。
- 输出不匹配。

未登录响应：

```http
HTTP/1.1 401 Unauthorized
```

```json
{
  "success": false,
  "message": "unauthorized",
  "data": null
}
```

可能错误：

| HTTP 状态码 | message | 说明 |
| --- | --- | --- |
| 400 | `invalid json` | 请求体不是合法 JSON |
| 401 | `unauthorized` | 未登录 |
| 500 | `database error` | 数据库连接或查询失败 |

## 6. 管理员 API

`SPEC.md` 规划了以下管理员接口，但当前后端尚未实现：

```http
POST /api/admin/login
POST /api/admin/logout
POST /api/admin/problems
DELETE /api/admin/problems/{id}
```

当前调用这些接口会命中 API 404 错误处理：

```json
{
  "success": false,
  "message": "not found",
  "data": null
}
```

## 7. curl 请求示例

以下示例与当前 API 合同一致，可用于手工验证。完整自动化执行方式见下一节。

先设置基础地址：

```bash
BASE_URL=http://127.0.0.1:8080
```

基础接口：

```bash
curl -sS "$BASE_URL/health"

curl -sS -X POST "$BASE_URL/api/_echo" \
  -H "Content-Type: application/json" \
  --data '{"hello":"world"}'

curl -sS -X POST "$BASE_URL/api/_echo" \
  -H "Content-Type: application/json" \
  --data '{"broken":'
```

题目接口：

```bash
curl -sS "$BASE_URL/api/problems"

curl -sS "$BASE_URL/api/problems/1"

curl -sS "$BASE_URL/api/problems/999999999"
```

普通用户接口：

```bash
curl -sS "$BASE_URL/api/user/me"

curl -sS -X POST "$BASE_URL/api/user/register" \
  -H "Content-Type: application/json" \
  --data '{"username":"new_user","password":"password"}'

curl -sS -X POST "$BASE_URL/api/user/login" \
  -H "Content-Type: application/json" \
  -c /tmp/oj_cookie.txt \
  --data '{"username":"user1","password":"password"}'

curl -sS "$BASE_URL/api/user/me" \
  -b /tmp/oj_cookie.txt

curl -sS -X POST "$BASE_URL/api/user/logout" \
  -H "Content-Type: application/json" \
  -b /tmp/oj_cookie.txt \
  -c /tmp/oj_cookie.txt \
  --data '{}'
```

提交接口：

```bash
curl -sS -X POST "$BASE_URL/api/submit" \
  -H "Content-Type: application/json" \
  --data '{"problem_id":1,"code":"int main(){return 0;}"}'

curl -sS -X POST "$BASE_URL/api/submit" \
  -H "Content-Type: application/json" \
  -b /tmp/oj_cookie.txt \
  --data '{"problem_id":1,"code":"#include <bits/stdc++.h>\nusing namespace std;\nint main(){int a,b;if(cin>>a>>b){cout<<a+b<<endl;}return 0;}\n"}'
```

当前未实现的管理员接口：

```bash
curl -sS -X POST "$BASE_URL/api/admin/login" \
  -H "Content-Type: application/json" \
  --data '{"username":"admin","password":"password"}'

curl -sS -X POST "$BASE_URL/api/admin/logout" \
  -H "Content-Type: application/json" \
  --data '{}'

curl -sS -X POST "$BASE_URL/api/admin/problems" \
  -H "Content-Type: application/json" \
  --data '{"title":"Example"}'

curl -sS -X DELETE "$BASE_URL/api/admin/problems/1"
```

## 8. curl 接口自动化测试

测试文件：

```text
scripts/api_curl_test.sh
```

测试脚本严格通过 `curl` 构造 HTTP 请求并校验 HTTP 状态码、统一 JSON envelope 和关键响应字段。

基础接口测试不依赖数据库：

```bash
make
make test-api-curl-basic
```

基础用例覆盖：

- `GET /health`
- `POST /api/_echo`
- 无效 JSON 返回 `400 invalid json`
- 未注册 `/api/*` 路由返回 JSON 格式 `404 not found`
- `GET /api/user/me` 未登录返回 `logged_in=false`
- 未登录 `POST /api/submit` 返回 `401 unauthorized`
- 未登录 `POST /api/user/logout` 返回成功
- 当前未实现的管理员 API 返回 `404 not found`

完整接口测试需要先准备测试数据库并导入：

```bash
mysql -u oj_user -p oj < sql/schema.sql
mysql -u oj_user -p oj < sql/seed.sql
```

然后执行：

```bash
make
make test-api-curl
```

也可以对已启动的服务直接运行：

```bash
OJ_API_BASE_URL=http://127.0.0.1:8080 bash scripts/api_curl_test.sh --no-start
```

完整用例覆盖：

- `GET /api/problems`
- `GET /api/problems/1`
- `GET /api/problems/{missing_id}`
- `GET /api/user/me`
- `POST /api/user/register`
- `POST /api/user/login`
- `POST /api/user/logout`
- 未登录 `POST /api/submit` 返回 `401 unauthorized`
- 登录后正确代码提交返回 `passed`
- 登录后错误代码提交返回 `failed`
- 登录后编译错误代码提交返回 `failed`
- 登录后空代码提交返回 `failed`
