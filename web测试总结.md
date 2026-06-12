# Web 自动化测试总结

## 测试时间与环境

- 测试日期：2026-06-12
- 被测地址：`http://127.0.0.1:8080`
- 测试依据：`web自动化测试文档.md`
- 自动化工具：`playwright-cli 0.1.13`
- 浏览器模式：已按要求使用 `playwright-cli ... open ... --headed` 启动有头浏览器
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
- 通过：24
- 失败：14

## 已通过用例

```text
WEB-001, WEB-002, WEB-004, WEB-005, WEB-006,
WEB-007, WEB-008, WEB-009, WEB-010,
WEB-011, WEB-012, WEB-013, WEB-018,
WEB-023, WEB-024, WEB-025, WEB-026, WEB-027,
WEB-028, WEB-029, WEB-031,
WEB-033, WEB-034, WEB-036
```

## 未解决问题

### 1. 未登录用户无法访问题目列表

影响用例：

```text
WEB-003, WEB-037, WEB-038
```

实际情况：

- 未登录访问 `/problems.html` 会被前端重定向到 `/login.html?next=%2Fproblems.html`。
- 文档预期是未登录用户可以浏览题库列表。
- 相关代码位置：`public/js/problem-list.js` 中 `loadProblems()` 会在 `!user` 时直接跳转登录页。

待确认方向：

- 如果产品预期是“未登录可浏览题库”，需要修改 `problem-list.js`，未登录时仍加载 `GET /api/problems`，只是不展示登录用户的完成状态。
- 如果产品预期是“题库必须登录后访问”，需要同步更新 `web自动化测试文档.md` 中 WEB-003、WEB-037、WEB-038 的预期。

### 2. 失败类判题只显示通用 `failed`

影响用例：

```text
WEB-014, WEB-015, WEB-016, WEB-017,
WEB-019, WEB-020, WEB-021, WEB-030
```

实际情况：

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

待确认方向：

- 排查当前运行服务是否是最新构建产物。
- 如果服务已是最新，应检查 JSON 序列化或 `submit_result_json()` 调用链，确保失败响应包含 `status` 和 `status_text`。
- 前端 `public/js/problem-detail.js` 当前使用 `submitStatusText(result)`，会优先取 `result.data.status_text`，否则退回 `result.message`；只要后端返回 `status_text`，页面应能显示具体结果。

### 3. 不存在或已删除题目的详情页错误状态为空

影响用例：

```text
WEB-022, WEB-032
```

实际情况：

- 访问 `/problem.html?id=999999999` 时，接口返回 404，浏览器控制台出现 404 资源/请求错误，但 `#problem-content` 为空。
- 创建题目后删除，再访问 `/problem.html?id=<deletedId>`，接口也返回 404，但详情页内容仍为空。
- 文档预期页面显示“加载失败”“题目不存在”或等价错误状态。

待确认方向：

- 检查 `public/js/problem-detail.js` 的 `loadProblem()`：
  - 当前只有 `catch` 时写入 `加载失败`。
  - 如果 `api.getProblem()` 返回的是正常 JSON 响应但 `success=false`，当前代码不会写错误文案。
- 建议在 `!result.success` 时显式设置 `#problem-content` 为 `加载失败` 或 `题目不存在`。

### 4. 前台退出后验证详情页按钮状态的流程不符合文档

影响用例：

```text
WEB-035
```

实际情况：

- 普通用户退出后，再打开 `/problems.html` 或经题库链路访问详情，会被重定向到 `/login.html?next=%2Fproblems.html`。
- 测试未能进入题目详情页检查 `#submit-code` 是否禁用或提示登录。
- 根因与“未登录无法访问题库”一致。

待确认方向：

- 若允许未登录访问详情页，则需要让退出后仍可打开 `/problem.html?id=1` 并显示“登录后提交”。
- 若要求题库/详情都必须登录，则需要修改 WEB-035 的预期。

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

1. 优先修复或确认未登录题库访问策略。
2. 重新构建并确认正在运行的服务是否包含最新 `submit_api.cpp` 逻辑。
3. 修复失败类判题响应或前端显示后，重点重跑：

```text
WEB-014, WEB-015, WEB-016, WEB-017,
WEB-019, WEB-020, WEB-021, WEB-030
```

4. 修复不存在题目详情错误状态后，重跑：

```text
WEB-022, WEB-032
```

5. 根据题库访问策略重跑：

```text
WEB-003, WEB-035, WEB-037, WEB-038
```

6. 如果下次仍使用当前 Windows 环境，建议继续分批跑，避免全量一次性执行被外层超时截断。
