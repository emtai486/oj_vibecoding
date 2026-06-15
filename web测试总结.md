# Web 自动化测试总结

## 最新状态总览（2026-06-15）

### 当前结论

- Web 自动化用例总数：`38`
- 当前已确认通过：`37`
- 当前未完成或仍有问题：`1`
- 第一轮历史结果是 `24` 通过、`14` 失败；本轮新增确认通过：
  - `WEB-003`：未登录用户可访问题目列表。
  - `WEB-022`：不存在题目详情页显示错误状态。
  - `WEB-014`、`WEB-015`、`WEB-016`、`WEB-017`：失败类判题页面展示已通过。
  - `WEB-019`、`WEB-020`、`WEB-021`：超时、内存超限、输出超限页面展示已通过。
  - `WEB-030`：新增题目隐藏测试用例判题已通过。
  - `WEB-032`：删除后详情页不可访问已通过。
  - `WEB-035`：前台退出后详情页提交状态已通过。
  - `WEB-038`：浏览器后退/前进详情页稳定性已通过。

### 当前已确认通过用例

```text
WEB-001, WEB-002, WEB-003, WEB-004, WEB-005, WEB-006,
WEB-007, WEB-008, WEB-009, WEB-010,
WEB-011, WEB-012, WEB-013, WEB-014, WEB-015, WEB-016,
WEB-017, WEB-018, WEB-019, WEB-020, WEB-021, WEB-022,
WEB-023, WEB-024, WEB-025, WEB-026, WEB-027, WEB-028,
WEB-029, WEB-030, WEB-031, WEB-032,
WEB-033, WEB-034, WEB-035, WEB-036, WEB-038
```

### 当前未完成或仍需回归用例

```text
WEB-037
```

说明：

- `WEB-014` 到 `WEB-017`、`WEB-019` 到 `WEB-021`、`WEB-030`、`WEB-032`、`WEB-035`、`WEB-038` 已由 Ubuntu 侧目标回归脚本确认通过。
- `WEB-037` 本轮失败原因是回归脚本自身在 `playwright-cli run-code` 环境中使用了不可用的 `URL` 全局对象，失败信息为 `URL is not defined`。该脚本问题已在 Windows 工作区修复，需同步到 Ubuntu 后单独复跑 `WEB-037`。

### 2026-06-15 接手记录

- Ubuntu 侧已确认 `oj_server` 单进程监听 `0.0.0.0:8080`，且 `GET /health` 返回：

  ```json
  {"data":{"status":"ok"},"message":"ok","success":true}
  ```

- 当前 Windows 侧 Codex/Playwright 执行环境无法直连该 Ubuntu 服务：本机 `127.0.0.1:8080` 由 `Code.exe` 监听，请求 `/health` 连接后无响应；`localhost`、IPv6 loopback、已探测的 VS Code 转发端口和候选虚拟网卡地址均未返回 OJ 服务响应。因此本次不能在 Windows 侧给页面用例判定通过或失败。
- 已新增 Ubuntu 侧可直接执行的目标回归脚本：`scripts/web_targeted_regression.playwright.js`。该脚本覆盖以下待回归用例：

  ```text
  WEB-014, WEB-015, WEB-016, WEB-017,
  WEB-019, WEB-020, WEB-021, WEB-030,
  WEB-032, WEB-035, WEB-037, WEB-038
  ```

- Ubuntu 下建议执行：

  ```bash
  cd ~/project
  npx --no-install playwright-cli open about:blank
  npx --no-install playwright-cli run-code --filename=scripts/web_targeted_regression.playwright.js
  npx --no-install playwright-cli close
  ```

  如果 `playwright-cli` 是全局命令，也可以把 `npx --no-install playwright-cli` 替换为 `playwright-cli`。

- 脚本会先做轻量接口前置检查：`/health`、`/api/problems`、`/api/user/login`。如果前置检查失败，不应继续采信页面回归结果。

### 2026-06-15 Ubuntu 侧反馈

- Ubuntu 侧非沙箱接口确认通过：

  ```text
  GET /health -> 200
  GET /api/problems -> 200，包含 A+B Problem、Average Score
  POST /api/user/login -> 200，返回 user1
  ```

- 目标页面回归尚未执行，原因不是后端接口，而是自动化执行环境：
  - `playwright-cli` 全局命令不存在。
  - `npx --no-install playwright-cli --version`、`open about:blank`、`run-code --filename=...` 均只显示等待 spinner，10 秒后超时退出。
  - Ubuntu 侧 `~/project/scripts/web_targeted_regression.playwright.js` 文件不存在，说明 Windows 工作区新增脚本尚未同步到 Ubuntu 项目目录。

- 当前结论：不能更新 `WEB-014` 等 12 个待回归用例的通过状态。下一步需要先同步 `scripts/web_targeted_regression.playwright.js` 到 Ubuntu 的 `~/project/scripts/`，再解决 Ubuntu 下 `playwright-cli` 可执行性。

### 2026-06-15 Ubuntu 自动化环境修复反馈

- Ubuntu 侧 Playwright 自动化环境已恢复：

  ```text
  playwright-cli --version -> 0.1.14
  google-chrome --version -> Google Chrome 149.0.7827.114
  playwright-cli open about:blank -> 成功打开 Browser default
  ```

- `playwright-cli run-code --filename=scripts/web_targeted_regression.playwright.js` 仍未执行成功，唯一原因是脚本文件尚未同步到 Ubuntu：

  ```text
  Error: ENOENT: no such file or directory, open '/home/bzx/project/scripts/web_targeted_regression.playwright.js'
  ```

- 运行后健康检查仍正常：

  ```json
  {"data":{"status":"ok"},"message":"ok","success":true}
  ```

- 当前唯一剩余阻塞：将 Windows 工作区的 `scripts/web_targeted_regression.playwright.js` 同步到 Ubuntu 的 `~/project/scripts/web_targeted_regression.playwright.js`，然后重新执行目标回归脚本。

### 2026-06-15 目标回归脚本执行结果

- Ubuntu 侧已同步并执行 `scripts/web_targeted_regression.playwright.js`。
- 前置接口检查通过：`/health`、`/api/problems`、`/api/user/login` 均返回成功。
- 运行结果：

  ```text
  summary.total = 12
  summary.passed = 11
  summary.failed = 1
  ```

- 已通过：

  ```text
  WEB-014 Wrong Answer
  WEB-015 Compile Error
  WEB-016 Compile Error
  WEB-017 Wrong Answer
  WEB-019 Time Limit Exceeded
  WEB-020 Memory Limit Exceeded
  WEB-021 Output Limit Exceeded
  WEB-030 Wrong Answer
  WEB-032 题目不存在
  WEB-035 disabled=true, result=登录后提交
  WEB-038 back/forward kept list and detail renderable
  ```

- 唯一失败：

  ```text
  WEB-037 FAIL: URL is not defined
  ```

  该失败来自回归脚本自身，不是页面功能失败。`playwright-cli run-code` 环境中没有浏览器/Node 风格的全局 `URL`，脚本里 `new URL(page.url()).pathname` 抛错。Windows 工作区已修复为字符串方式解析当前路径，需同步新版脚本到 Ubuntu 后复跑 `WEB-037`。

- 运行后健康检查通过：

  ```json
  {"data":{"status":"ok"},"message":"ok","success":true}
  ```

- 临时题目清理结果：`/api/problems` 中无标题包含 `Web Auto Problem` 的残留题目。

### 本轮已经修复并验证

1. 未登录题库访问策略已修复并验证。

   Ubuntu 运行服务已经加载新版 `public/js/problem-list.js`。通过浏览器 DOM 检查确认：未登录访问 `/problems.html` 不再跳转登录页，页面显示 2 道题：

   ```text
   A+B Problem
   Average Score
   ```

   两题状态均为“未完成”。`WEB-003` 的核心预期已通过。

2. 不存在题目详情页错误状态已修复并验证。

   Ubuntu 运行服务已经加载新版 `public/js/problem-detail.js`。通过浏览器 DOM 检查确认：访问 `/problem.html?id=999999999` 时：

   ```text
   #problem-content = 题目不存在
   #submit-result = 登录后提交
   ```

   直接请求 `/api/problems/999999999` 返回 HTTP `404`。`WEB-022` 已通过。

3. 失败类判题接口响应已修复并验证。

   使用 `user1/password` 登录后，接口返回已经包含具体状态字段。例如错误答案：

   ```json
   {"data":{"result":"failed","status":"wrong_answer","status_text":"Wrong Answer","testcase":1},"message":"wrong_answer","success":true}
   ```

   已确认的后端响应：

   ```text
   Wrong Answer
   Compile Error
   strict 换行不匹配 -> Wrong Answer
   空代码 -> Compile Error
   ```

   注意：这只证明后端响应已恢复；页面级展示还需要在数据库 500 稳定后继续回归。

### 本地已修复但 Ubuntu 仍需同步

当前工作区已进一步修复 `public/js/problem-detail.js` 的提交按钮竞态：

- 问题：页面初始 HTML 中 `#submit-code` 是可点击状态。如果用户或自动化脚本在 `currentUser()` 完成前点击，会误显示“请先登录”。
- 修复：脚本启动时立即禁用按钮，等待登录态检查完成后再由 `configureSubmitState()` 启用。

需要在 Ubuntu 的 `~/project/public/js/problem-detail.js` 同步以下改动。

在文件顶部：

```js
const params = new URLSearchParams(window.location.search);
const problemId = params.get("id");
const editor = createCppEditor(document.querySelector("#code-editor"));
let loggedInUser = null;
```

后面增加：

```js
const submitButton = document.querySelector("#submit-code");
submitButton.disabled = true;
```

然后在 `configureSubmitState()` 中删除局部重复声明：

```js
const submitButton = document.querySelector("#submit-code");
```

Ubuntu 操作命令：

```bash
cd ~/project
nano public/js/problem-detail.js
```

保存后重启服务：

```bash
# 在服务运行终端按 Ctrl+C
cd ~/project
bash scripts/start_server.sh config/app.conf
```

### 当前新增阻塞：后端数据库接口偶发 500

多次回归时发现后端接口间歇性返回：

```json
{"data":null,"message":"database error","success":false}
```

受影响接口包括：

```text
POST /api/user/login
GET /api/problems
GET /api/problems/1
```

特点：

- 不是稳定复现；同一接口重试后可能返回 200。
- `/health` 通常仍返回正常。
- 该问题会阻塞页面级登录、提交、判题展示回归。

在 Ubuntu 终端执行以下命令复现和定位：

```bash
cd ~/project
for i in {1..20}; do
  echo "---- problems $i ----"
  curl -s -w "\nHTTP %{http_code}\n" http://127.0.0.1:8080/api/problems
done
```

```bash
cd ~/project
for i in {1..20}; do
  echo "---- login $i ----"
  curl -s -w "\nHTTP %{http_code}\n" -X POST http://127.0.0.1:8080/api/user/login \
    -H 'Content-Type: application/json' \
    -d '{"username":"user1","password":"password"}'
done
```

同时观察服务运行终端是否打印数据库错误。如果有输出，下次优先根据该错误定位 MySQL 连接、权限、连接数或查询异常。

### 下一次继续测试步骤

1. 在 Ubuntu 同步 `public/js/problem-detail.js` 的提交按钮初始禁用修复。
2. 重启服务。
3. 确认服务健康：

```bash
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/api/problems
curl -i -X POST http://127.0.0.1:8080/api/user/login \
  -H 'Content-Type: application/json' \
  -d '{"username":"user1","password":"password"}'
```

4. 如果仍出现 `database error`，先解决数据库 500，不要继续跑页面判题用例。
5. 数据库接口稳定后，优先重跑：

```text
WEB-014, WEB-015, WEB-016, WEB-017,
WEB-019, WEB-020, WEB-021, WEB-030
```

6. 然后补跑：

```text
WEB-032, WEB-035, WEB-037, WEB-038
```

### Playwright 备注

- 后续不再强制加 `--headed`。
- `playwright-cli open` 当前可生成快照，但快照可能早于异步接口渲染完成；验证异步 DOM 时，需要等待页面内容稳定后再判断。
- CodeMirror 内容设置应取 `.CodeMirror.CodeMirror` 实例，不要只填隐藏的 `textarea#code-editor`。

## 测试时间与环境

- 测试日期：2026-06-12
- 被测地址：`http://127.0.0.1:8080`
- 测试依据：`web自动化测试文档.md`
- 自动化工具：`playwright-cli 0.1.13`
- 浏览器模式：第一轮曾按要求使用 `--headed`；后续因会话保留问题，不再强制使用 `--headed`
- 操作节奏：自动化脚本中每个页面操作之间加入了约 `1s` 等待

## 环境限制

- 当前 Windows 环境没有 `make` 命令。
- 工作区只有 `config/app.example.conf`，没有 `config/app.conf`。
- 因此本轮没有执行文档建议的 `make reset-web-test-db`。
- 本轮通过唯一用户名、唯一题目标题规避重复数据冲突。
- 测试后已检查管理员题库，没有残留标题包含 `Web Auto Problem` 的临时题目。
- 最终健康检查通过：`GET /health` 返回 `{"data":{"status":"ok"},"message":"ok","success":true}`。

## 执行方式

- 全量 38 个用例如果一次性跑完，会因为外层命令超时而丢失报告。
- 本轮采用分批执行：
  - `WEB-001` 到 `WEB-010`
  - `WEB-011` 到 `WEB-018`
  - `WEB-019` 到 `WEB-023`
  - `WEB-024` 到 `WEB-032`
  - `WEB-033` 到 `WEB-038`
- 中途发现并修正过测试脚本自身问题：
  - `URL is not defined`：浏览器执行环境里没有 Node 风格全局 `URL`，后来改为字符串解析路径。
  - CodeMirror 内容设置位置错误：实例挂在 `.CodeMirror.CodeMirror`，不是 `textarea#code-editor.CodeMirror`。
  - 通过结果大小写差异：页面实际显示 `accepted`，文档允许 `accepted` 或通过状态，因此后续按大小写不敏感处理。

## 总体结果

- 总用例数：38
- 第一轮通过：24
- 第一轮失败：14
- 当前已确认通过：26
- 当前未完成或仍有问题：12

## 已通过用例

```text
WEB-001, WEB-002, WEB-003, WEB-004, WEB-005, WEB-006,
WEB-007, WEB-008, WEB-009, WEB-010,
WEB-011, WEB-012, WEB-013, WEB-018,
WEB-022, WEB-023, WEB-024, WEB-025, WEB-026, WEB-027,
WEB-028, WEB-029, WEB-031,
WEB-033, WEB-034, WEB-036
```

## 历史问题与当前状态

### 1. 未登录用户无法访问题目列表（已解决，WEB-003 已通过）

影响用例：

```text
WEB-003, WEB-037, WEB-038
```

第一轮实际情况：

- 未登录访问 `/problems.html` 会被前端重定向到 `/login.html?next=%2Fproblems.html`。
- 文档预期是未登录用户可以浏览题库列表。
- 相关代码位置：`public/js/problem-list.js` 中 `loadProblems()` 会在 `!user` 时直接跳转登录页。

当前处理结果：

- 已按 `web自动化测试文档.md` 的产品预期处理为“未登录可浏览题库”。
- `public/js/problem-list.js` 已修改：未登录时仍加载 `GET /api/problems`，只是不展示登录用户的完成状态。
- Ubuntu 运行服务已加载该修改；浏览器 DOM 检查确认未登录可看到 `A+B Problem` 和 `Average Score`。
- `WEB-003` 已标记为通过。
- `WEB-037`、`WEB-038` 仍需补完整导航/前进后退流程回归。

### 2. 失败类判题只显示通用 `failed`（后端已解决，页面仍需回归）

影响用例：

```text
WEB-014, WEB-015, WEB-016, WEB-017,
WEB-019, WEB-020, WEB-021, WEB-030
```

第一轮实际情况：

- 错误答案、编译错误、空代码、strict 换行不匹配、超时、内存超限、输出超限、隐藏用例失败等场景，页面最终都只显示 `failed`。
- 文档预期页面显示：
  - `Wrong Answer`
  - `Compile Error`
  - `Time Limit Exceeded`
  - `Memory Limit Exceeded`
  - `Output Limit Exceeded`
- 诊断时接口响应也只看到类似：

```json
{"data":{"result":"failed"},"message":"failed","success":true}
```

补充观察：

- 源码 `src/judge/judge_service.cpp` 中存在 `judge_result_text()`，理论上可返回具体文本。
- `src/api/submit_api.cpp` 中 `submit_result_json()` 也构造了 `status_text`。
- 但浏览器实际收到的失败响应未包含 `status`、`status_text` 等字段，只包含 `data.result=failed`。

当前处理结果：

- 重新构建/重启后，当前运行服务的 `POST /api/submit` 已返回 `status` 和 `status_text`。
- 已确认 `Wrong Answer`、`Compile Error`、空代码、strict 换行不匹配的后端响应。
- 页面代码 `submitStatusText(result)` 已优先读取 `result.data.status_text`。
- 由于后端接口目前存在间歇性 `database error`，页面级提交展示尚未稳定回归完成；`WEB-014` 到 `WEB-017`、`WEB-019` 到 `WEB-021`、`WEB-030` 仍列为未完成。

### 3. 不存在题目的详情页错误状态为空（WEB-022 已解决；WEB-032 待完整回归）

影响用例：

```text
WEB-022, WEB-032
```

第一轮实际情况：

- 访问 `/problem.html?id=999999999` 时，接口返回 404，浏览器控制台出现 404 资源/请求错误，但 `#problem-content` 为空。
- 创建题目后删除，再访问 `/problem.html?id=<deletedId>`，接口也返回 404，但详情页内容仍为空。
- 文档预期页面显示“加载失败”“题目不存在”或等价错误状态。

当前处理结果：

- `public/js/problem-detail.js` 已在 `!result.success` 时显示“题目不存在”。
- Ubuntu 运行服务已加载该修改；访问 `/problem.html?id=999999999` 时，页面显示“题目不存在”。
- 直接请求 `/api/problems/999999999` 返回 HTTP 404。
- `WEB-022` 已通过。
- `WEB-032` 还需要创建临时题目、删除后访问详情页，尚未完成完整回归。

### 4. 前台退出后验证详情页按钮状态的流程不符合文档（待回归）

影响用例：

```text
WEB-035
```

实际情况：

- 普通用户退出后，再打开 `/problems.html` 或经题库链路访问详情，会被重定向到 `/login.html?next=%2Fproblems.html`。
- 测试未能进入题目详情页检查 `#submit-code` 是否禁用或提示登录。
- 根因与“未登录无法访问题库”一致。

当前处理方向：

- 已确认产品预期按 `web自动化测试文档.md`：未登录可浏览题库和详情，但不能提交。
- 题库未登录访问已经修复。
- 详情页未登录访问之前已能显示题面和“登录后提交”；还需要在本轮修复后重新完整跑 `WEB-035`。

## 重要通过点

- 首页和本地静态资源加载正常，CodeMirror 来自本地资源，没有发现 CDN 请求。
- 未登录访问题目详情 `/problem.html?id=1` 可以查看题面，隐藏测试输入未暴露，提交按钮禁用或提示登录。
- 未登录访问管理员后台会重定向到管理员登录页。
- 普通用户登录、注册、重复注册提示都符合预期。
- 正确 A+B 代码提交后通过，页面显示 `accepted`。
- 通过后 `localStorage.oj_problem_status` 写入 `"1":"passed"`，题目列表显示“已通过”。
- 退出后导航恢复未登录状态。
- `float_1` 题目 `Average Score` 正确代码可通过。
- 管理员登录、新增题目、隐藏测试用例必填校验、JSON 格式错误校验、新增题目前台可见、删除题目均通过。
- 管理员也可进入题库提交代码并通过。
- 页面刷新后普通用户登录态保持。

## 下次继续测试建议

> 本节为第一轮测试后的历史建议，已被文档顶部“最新状态总览（2026-06-13）”更新。下次继续时优先按顶部步骤执行。

1. 未登录题库访问策略已经修复，`WEB-003` 已通过。
2. `submit_api.cpp` 相关后端响应已经通过重新构建/重启确认生效，失败类提交接口已返回 `status_text`。
3. 当前仍需在数据库接口稳定后，重点重跑页面级展示：

```text
WEB-014, WEB-015, WEB-016, WEB-017,
WEB-019, WEB-020, WEB-021, WEB-030
```

4. 不存在题目详情错误状态已经修复，`WEB-022` 已通过；删除后详情页仍需重跑：

```text
WEB-032
```

5. 根据已修复的题库访问策略继续补跑：

```text
WEB-035, WEB-037, WEB-038
```

6. 如果下次仍使用当前 Windows 环境，建议继续分批跑，避免全量一次性执行被外层超时截断。

## 2026-06-13 继续处理记录

### 已完成并确认

- 已确认当前运行服务的 `POST /api/submit` 失败类响应已包含具体判题字段。使用 `user1/password` 登录后，错误答案接口返回：

```json
{"data":{"result":"failed","status":"wrong_answer","status_text":"Wrong Answer","testcase":1},"message":"wrong_answer","success":true}
```

- 进一步确认了以下代表场景的后端响应：
  - 编译错误：`status=compile_error`，`status_text=Compile Error`
  - 空代码：`status=compile_error`，`status_text=Compile Error`
  - strict 换行不匹配：`status=wrong_answer`，`status_text=Wrong Answer`

这说明“失败类判题只显示通用 `failed`”的后端响应问题已经通过重新构建/重启服务得到修复。前端 `problem-detail.js` 已经优先读取 `result.data.status_text`，因此在服务和静态文件一致时，页面应显示具体判题结果。

### 本地已修改，待同步到 Ubuntu 运行目录后验证（历史状态，已被后续记录覆盖）

- 已在当前工作区修改 `public/js/problem-list.js`：未登录用户不再被 `/problems.html` 重定向到登录页；未登录时题目状态统一显示“未完成”，不读取本地通过状态。
- 已在当前工作区修改 `public/js/problem-detail.js`：`api.getProblem()` 返回 `success=false` 时，`#problem-content` 显示“题目不存在”，避免不存在或已删除题目详情页空白。

后续已经确认 Ubuntu 运行服务加载了这两个前端修改；见“2026-06-13 前端同步后回归记录”。

### 当前阻塞（历史状态，已被后续记录部分解决）

- 旧阻塞：运行中的服务返回旧版前端 JS。该问题已解决，`WEB-003` 和 `WEB-022` 已通过。
- 后续验证过程中，`POST /api/user/login` 一度返回 HTTP 500 且响应体为空；同时 `/health` 和 `/api/problems` 正常。需要在 Ubuntu 服务终端查看日志，并确认 MySQL 连接与 `user1/password` 基线数据仍正常。

### 下一步重跑重点

同步前端文件并重启服务后，优先重跑：

```text
WEB-003, WEB-022, WEB-032, WEB-035, WEB-037, WEB-038
```

登录接口恢复后，继续重跑失败类判题页面展示：

```text
WEB-014, WEB-015, WEB-016, WEB-017,
WEB-019, WEB-020, WEB-021, WEB-030
```

## 2026-06-13 前端同步后回归记录

### 已完成并确认

- Ubuntu 运行服务已经加载新版 `public/js/problem-list.js`。通过浏览器 DOM 检查确认：未登录访问 `/problems.html` 不再跳转登录页，页面显示 2 道题：
  - `A+B Problem`
  - `Average Score`
  - 两题状态均为“未完成”

  对应 `WEB-003` 的核心预期已通过。

- Ubuntu 运行服务已经加载新版 `public/js/problem-detail.js`。通过浏览器 DOM 检查确认：访问 `/problem.html?id=999999999` 时，`#problem-content` 显示“题目不存在”，`#submit-result` 显示“登录后提交”。

  对应 `WEB-022` 的页面错误状态预期已通过。

- 直接请求 `/api/problems/999999999` 返回 HTTP 404，符合不存在题目的接口预期。

### 本地新增修复，待同步到 Ubuntu 后验证

- 已在当前工作区进一步修改 `public/js/problem-detail.js`：页面脚本启动时先禁用 `#submit-code`，等待 `currentUser()` 完成后再按登录态启用，避免自动化或用户在登录态检查完成前点击提交按钮导致误显示“请先登录”。

该修复尚未同步到 Ubuntu `~/project/public/js/problem-detail.js`，因此页面级提交回归仍可能遇到登录态竞态。

### 当前新增阻塞

- 多次回归时发现后端接口出现间歇性 HTTP 500，响应为：

```json
{"data":null,"message":"database error","success":false}
```

- 受影响接口包括：
  - `POST /api/user/login`
  - `GET /api/problems`
  - `GET /api/problems/1`

- 这些 500 不是稳定复现；同一接口重试后可能返回 200。因此目前无法稳定完成页面级提交回归，也不能把 `WEB-014` 到 `WEB-021` 的页面展示全部标记为通过。

### 下一步

1. 将当前工作区的 `public/js/problem-detail.js` 再同步一次到 Ubuntu 运行目录，确保提交按钮初始禁用竞态修复生效。
2. 在 Ubuntu 服务终端观察 `database error` 出现时的标准错误输出。
3. 检查 MySQL 连接稳定性和服务端配置后，重跑页面级提交回归：

```text
WEB-014, WEB-015, WEB-016, WEB-017,
WEB-019, WEB-020, WEB-021, WEB-030
```
