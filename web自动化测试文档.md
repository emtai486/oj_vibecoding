# Web 自动化测试文档

## 1. 测试目标

本文档根据 `SPEC.md`、`API.md` 和当前前端/后端代码设计 Web 自动化测试用例，用于验证 OJ 项目第一版的核心浏览器交互流程。

测试重点：

- 静态页面和本地前端资源可访问。
- 未登录用户可以浏览题库和题目详情，但不能提交代码。
- 普通用户可以注册、登录、查看题目、编辑 C++ 代码、提交并查看判题结果。
- 普通用户通过题目后，浏览器 `localStorage` 会保存完成状态。
- 管理员可以登录后台、新增题目、删除题目。
- 管理员新增题目后，普通用户可在前台看到；删除后不可再访问。
- 判题结果覆盖通过、错误答案、编译错误、超时、内存超限、输出超限、`strict` 和 `float_1` 比较模式。
- 隐藏测试用例不在题目详情页暴露。

## 2. 测试范围

### 2.1 被测地址

```text
http://127.0.0.1:8080
```

### 2.2 页面范围

| 页面 | 地址 | 说明 |
| --- | --- | --- |
| 首页 | `/`、`/index.html` | 入口页、导航、进入题库 |
| 题目列表页 | `/problems.html` | 展示题目 ID、标题、难度、完成状态 |
| 题目详情页 | `/problem.html?id=1` | 展示题面、CodeMirror 编辑器、提交结果 |
| 普通用户登录页 | `/login.html` | 普通用户登录 |
| 普通用户注册页 | `/register.html` | 普通用户注册 |
| 管理员登录页 | `/admin/login.html` | 管理员登录 |
| 管理员后台页 | `/admin/index.html` | 题目管理、删除题目入口 |
| 新增题目页 | `/admin/new-problem.html` | 管理员新增题目 |

### 2.3 接口范围

Web 自动化主要从浏览器页面触发以下接口：

| 接口 | 触发页面/动作 |
| --- | --- |
| `GET /health` | 环境健康检查 |
| `GET /api/problems` | 题目列表页、管理员后台 |
| `GET /api/problems/{id}` | 题目详情页 |
| `GET /api/user/me` | 前台导航和提交状态判断 |
| `POST /api/user/register` | 普通用户注册页 |
| `POST /api/user/login` | 普通用户登录页 |
| `POST /api/user/logout` | 前台退出 |
| `GET /api/admin/me` | 管理员页面权限检查 |
| `POST /api/admin/login` | 管理员登录页 |
| `POST /api/admin/logout` | 管理员退出 |
| `POST /api/admin/problems` | 新增题目页 |
| `DELETE /api/admin/problems/{id}` | 管理员后台删除题目 |
| `POST /api/submit` | 题目详情页提交代码 |

## 3. 前置条件

1. 服务已启动并监听：

```text
http://127.0.0.1:8080
```

2. 数据库已完成初始化，至少包含种子题目：

| ID | 标题 | 比较模式 |
| --- | --- | --- |
| 1 | `A+B Problem` | `strict` |
| 2 | `Average Score` | `float_1` |

3. 可用账号：

| 角色 | 用户名 | 密码 |
| --- | --- | --- |
| 管理员 | `admin` | `password` |
| 普通用户 | `user1` | `password` |

4. 每轮完整自动化测试开始前，建议先执行数据库基线重置：

```bash
make reset-web-test-db
```

该命令会删除上轮测试可能残留的临时用户、临时题目和测试用例，并恢复自动化测试需要的基础数据：`user1/password`、`admin/password`、题目 1、题目 2。

5. 每条自动化用例建议使用独立浏览器上下文，避免 Cookie 和 `localStorage` 相互污染。

6. 每轮测试启动时生成全局唯一运行 ID，所有会写入数据库的数据都必须带唯一后缀，不使用固定名称。推荐格式：

```text
runId = ${yyyyMMddHHmmss}_${workerIndex}_${random}
dataId = ${runId}_${caseId}_${retryIndex}_${random}
```

示例：

```text
web_user_${dataId}
Web Auto Problem ${dataId}
```

## 4. 可重复运行设计原则

自动化用例必须支持连续多次执行。以下原则适用于本文档所有用例：

1. 完整套件执行前先运行 `make reset-web-test-db`，保证数据库回到基线状态。
2. 单条用例独立执行时，如涉及写数据库，必须在用例尝试开始时生成新的 `dataId`，自行构造唯一测试数据，并在用例结束时清理。
3. 成功注册、新增题目等“只能创建一次”的操作，不允许使用固定用户名或固定题目标题。
4. “重复操作失败”类用例必须在用例内部先创建唯一数据，再重复提交同一份数据触发失败；不能依赖上一次测试运行留下的数据。
5. 删除类用例必须记录本轮创建的数据 ID；如果数据不存在，应先创建再删除，不直接依赖历史数据。
6. 每个测试用例开始前清理浏览器 Cookie 和 `localStorage`，除非该用例明确验证登录态或本地完成状态。
7. 管理员新增题目的测试必须使用 `try/finally` 或等价机制删除本轮创建的题目；如果测试中断，下一轮执行前由 `make reset-web-test-db` 兜底清理。
8. 测试框架重试同一用例时，必须重新生成 `dataId`，不要复用上一次失败尝试的用户名或题目标题。

## 5. 自动化实现建议

### 5.1 推荐工具

推荐使用 Playwright，也可使用 Selenium。Playwright 对 CodeMirror、网络等待、浏览器上下文隔离和 `localStorage` 校验更方便。

### 5.2 通用配置

```text
baseURL = http://127.0.0.1:8080
defaultTimeout = 10000 ms
```

判题类用例建议单独提高超时时间：

```text
submitTimeout = 20000 ms
```

### 5.3 常用页面选择器

| 功能 | 选择器 |
| --- | --- |
| 前台导航 | `#auth-nav` |
| 普通用户登录表单 | `#login-form` |
| 普通用户登录消息 | `#login-message` |
| 普通用户注册表单 | `#register-form` |
| 普通用户注册消息 | `#register-message` |
| 题目列表表格 | `#problem-list` |
| 题目数量 | `#problem-count` |
| 题目标题 | `#problem-title` |
| 题目元信息 | `#problem-meta` |
| 题面内容 | `#problem-content` |
| 原始代码 textarea | `#code-editor` |
| CodeMirror 编辑区 | `.CodeMirror` |
| 提交按钮 | `#submit-code` |
| 提交结果 | `#submit-result` |
| 管理员登录表单 | `#admin-login-form` |
| 管理员登录消息 | `#admin-message` |
| 管理员题目列表 | `#admin-problem-list` |
| 管理员删除按钮 | `[data-delete-id]` |
| 新增题目表单 | `#new-problem-form` |
| 新增题目消息 | `#new-problem-message` |

### 5.4 CodeMirror 输入建议

题目详情页使用 CodeMirror。自动化脚本不要只填写隐藏的 `textarea#code-editor`，建议点击 `.CodeMirror` 后使用键盘输入，或通过页面脚本调用 CodeMirror 实例。

Playwright 可采用以下思路：

```js
await page.locator('.CodeMirror').click();
await page.keyboard.press(process.platform === 'darwin' ? 'Meta+A' : 'Control+A');
await page.keyboard.type(code);
```

如键盘输入过慢，可在测试框架中封装一个设置 CodeMirror 内容的工具函数。

## 6. 测试数据

### 6.1 正确 C++ 代码：A+B Problem

```cpp
#include <bits/stdc++.h>
using namespace std;
int main() {
    int a, b;
    if (cin >> a >> b) {
        cout << a + b << endl;
    }
    return 0;
}
```

### 6.2 错误答案代码

```cpp
#include <bits/stdc++.h>
using namespace std;
int main() {
    cout << 0 << endl;
    return 0;
}
```

### 6.3 编译错误代码

```cpp
int main( {
```

### 6.4 strict 换行不匹配代码

```cpp
#include <bits/stdc++.h>
using namespace std;
int main() {
    int a, b;
    if (cin >> a >> b) {
        cout << a + b;
    }
    return 0;
}
```

### 6.5 float_1 通过代码：Average Score

```cpp
#include <bits/stdc++.h>
using namespace std;
int main() {
    int n;
    if (!(cin >> n)) return 0;
    double sum = 0, x;
    for (int i = 0; i < n; ++i) {
        cin >> x;
        sum += x;
    }
    cout << fixed << setprecision(2) << sum / n << '\n';
    return 0;
}
```

### 6.6 超时代码

```cpp
#include <cstdint>
int main() {
    volatile std::uint64_t x = 0;
    while (true) {
        ++x;
    }
    return 0;
}
```

### 6.7 内存超限代码

```cpp
#include <iostream>
#include <vector>
int main() {
    std::vector<char> data(300 * 1024 * 1024);
    std::cout << data.size() << '\n';
    return 0;
}
```

### 6.8 输出超限代码

```cpp
#include <iostream>
#include <string>
int main() {
    std::cout << std::string(1100000, 'x');
    return 0;
}
```

## 7. Web 自动化测试用例

### WEB-001 首页可访问并展示核心入口

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 服务已启动 |
| 操作步骤 | 1. 打开 `/`。<br>2. 检查页面标题区域。<br>3. 检查导航中存在“登录”“注册”“管理后台”。<br>4. 检查“登录进入题库”或同等入口可见。 |
| 预期结果 | 首页加载成功；品牌 `CodeJudge` 可见；导航入口完整；页面无控制台致命错误。 |

### WEB-002 静态资源本地加载

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 服务已启动 |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 等待 `.CodeMirror` 出现。<br>3. 检查网络请求中 CodeMirror、CSS、JS 均来自 `127.0.0.1:8080`。 |
| 预期结果 | CodeMirror 编辑器显示成功；无 CDN 请求；无静态资源 404。 |

### WEB-003 未登录用户访问题目列表

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 清空 Cookie 和 `localStorage` |
| 操作步骤 | 1. 打开 `/problems.html`。<br>2. 等待 `#problem-list` 加载。<br>3. 检查列表中存在 `A+B Problem` 和难度 `easy`。 |
| 预期结果 | 未登录用户可以看到题目列表；页面显示题目标题和难度；未登录状态不应显示“已通过”。 |

### WEB-004 未登录用户访问题目详情

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 清空 Cookie 和 `localStorage` |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 等待 `#problem-title` 显示 `A+B Problem`。<br>3. 检查 `#problem-content` 包含题目描述、输入格式、输出格式、样例输入、样例输出。<br>4. 检查页面不包含隐藏测试输入 `10 20`。<br>5. 检查 `#submit-code` 为禁用状态。 |
| 预期结果 | 未登录用户能查看题目详情和样例；看不到隐藏测试用例；提交按钮禁用或提示登录后提交。 |

### WEB-005 未登录用户不能进入管理员后台

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 清空 Cookie |
| 操作步骤 | 1. 直接打开 `/admin/index.html`。<br>2. 等待页面跳转。 |
| 预期结果 | 浏览器被重定向到 `/admin/login.html`；未登录用户不能看到管理员题目管理列表。 |

### WEB-006 普通用户登录失败提示

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 清空 Cookie |
| 操作步骤 | 1. 打开 `/login.html`。<br>2. 在 `#login-form` 输入用户名 `user1`、密码 `wrong-password`。<br>3. 提交表单。 |
| 预期结果 | 页面停留在登录页；`#login-message` 显示 `invalid username or password`。 |

### WEB-007 普通用户登录成功

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 存在普通用户 `user1/password` |
| 操作步骤 | 1. 打开 `/login.html`。<br>2. 输入用户名 `user1`、密码 `password`。<br>3. 提交表单。<br>4. 等待跳转到 `/problems.html`。 |
| 预期结果 | 登录成功；跳转到题目列表；导航展示 `user1` 和“退出”。 |

### WEB-008 普通用户注册成功并可登录

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 使用本次用例尝试唯一用户名，例如 `web_user_${dataId}_register`；测试前不要求该用户存在。 |
| 操作步骤 | 1. 打开 `/register.html`。<br>2. 输入本轮唯一用户名和密码 `password`。<br>3. 提交表单。<br>4. 等待跳转到 `/login.html`。<br>5. 使用新账号登录。 |
| 预期结果 | 注册接口返回成功；新账号可登录并进入题库。 |

### WEB-009 普通用户重复注册失败

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 使用本次用例尝试唯一用户名，例如 `web_user_${dataId}_duplicate`；测试前不要求该用户存在。 |
| 操作步骤 | 1. 打开 `/register.html`。<br>2. 输入本轮唯一用户名和密码 `password`。<br>3. 提交表单并等待跳转到 `/login.html`。<br>4. 清理 Cookie 或新开浏览器上下文。<br>5. 再次打开 `/register.html`。<br>6. 使用同一个本轮唯一用户名和密码 `password` 再次提交。 |
| 预期结果 | 第一次注册成功；第二次注册在同一轮用例内部触发重复用户名，页面显示 `username exists`；不依赖历史数据库中是否已有该用户名。 |

### WEB-010 登录用户查看题目列表完成状态

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 已登录普通用户；清空 `localStorage.oj_problem_status` |
| 操作步骤 | 1. 打开 `/problems.html`。<br>2. 检查 `A+B Problem` 所在行完成状态。 |
| 预期结果 | 题目显示“未完成”或等价状态；不会被误判为“已通过”。 |

### WEB-011 登录用户提交正确代码并通过

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 已登录普通用户 |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 将编辑器内容替换为“正确 C++ 代码：A+B Problem”。<br>3. 点击 `#submit-code`。<br>4. 等待 `#submit-result` 更新。 |
| 预期结果 | 提交过程中显示“提交中”；最终显示 `accepted` 或通过状态；接口返回 `result=passed`。 |

### WEB-012 通过后 localStorage 保存完成状态

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 新建浏览器上下文并登录普通用户；清空 `localStorage.oj_problem_status`。 |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 输入“正确 C++ 代码：A+B Problem”。<br>3. 点击提交并等待通过。<br>4. 在同一浏览器上下文中读取 `localStorage.getItem('oj_problem_status')`。<br>5. 返回 `/problems.html`。<br>6. 查找题目 1 的完成状态。 |
| 预期结果 | `localStorage` 中包含 `"1":"passed"`；题目列表显示“已通过”。 |

### WEB-013 登出后不展示本地完成状态

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 新建浏览器上下文并登录普通用户；清空 `localStorage.oj_problem_status`。 |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 输入“正确 C++ 代码：A+B Problem”并提交通过，使当前浏览器写入通过状态。<br>3. 点击导航中的“退出”。<br>4. 等待页面刷新或回到未登录状态。<br>5. 打开 `/problems.html`。 |
| 预期结果 | 导航显示登录/注册入口；题目列表不应以登录用户状态展示“已通过”。 |

### WEB-014 登录用户提交错误答案返回失败

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 已登录普通用户 |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 输入“错误答案代码”。<br>3. 点击提交。 |
| 预期结果 | `#submit-result` 最终显示 `failed` 或失败状态；不会写入新的通过状态。 |

### WEB-015 登录用户提交编译错误返回失败

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 已登录普通用户 |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 输入“编译错误代码”。<br>3. 点击提交。 |
| 预期结果 | 判题返回失败；页面展示失败结果。 |

### WEB-016 空代码提交返回失败

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 已登录普通用户 |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 清空编辑器。<br>3. 点击提交。 |
| 预期结果 | 页面展示失败结果；不进入通过状态。 |

### WEB-017 strict 比较模式对换行敏感

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 已登录普通用户；题目 1 为 `strict` 模式 |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 输入“strict 换行不匹配代码”。<br>3. 点击提交。 |
| 预期结果 | 即使数值正确，因为输出缺少期望换行，结果仍为失败。 |

### WEB-018 float_1 比较模式通过

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 已登录普通用户；题目 2 为 `float_1` 模式 |
| 操作步骤 | 1. 打开 `/problem.html?id=2`。<br>2. 检查 `#problem-meta` 包含 `float_1`。<br>3. 输入“float_1 通过代码：Average Score”。<br>4. 点击提交。 |
| 预期结果 | 判题通过；页面显示 `accepted`。 |

### WEB-019 超时代码返回失败

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 已登录普通用户 |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 输入“超时代码”。<br>3. 点击提交。<br>4. 等待最多 20 秒。 |
| 预期结果 | 页面最终显示失败；服务没有卡死；之后仍可继续请求 `/health` 或打开题目列表。 |

### WEB-020 内存超限代码返回失败

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 已登录普通用户 |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 输入“内存超限代码”。<br>3. 点击提交。 |
| 预期结果 | 页面最终显示失败；服务仍可继续响应。 |

### WEB-021 输出超限代码返回失败

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 已登录普通用户 |
| 操作步骤 | 1. 打开 `/problem.html?id=1`。<br>2. 输入“输出超限代码”。<br>3. 点击提交。 |
| 预期结果 | 页面最终显示失败；服务没有返回超大页面内容。 |

### WEB-022 题目不存在时详情页展示加载失败或错误状态

| 项 | 内容 |
| --- | --- |
| 优先级 | P2 |
| 前置条件 | 无 |
| 操作步骤 | 1. 打开 `/problem.html?id=999999999`。<br>2. 等待请求结束。 |
| 预期结果 | 页面不崩溃；题面区域显示加载失败、缺少题目或等价错误状态。 |

### WEB-023 缺少题目 ID 时详情页提示错误

| 项 | 内容 |
| --- | --- |
| 优先级 | P2 |
| 前置条件 | 无 |
| 操作步骤 | 1. 打开 `/problem.html`。 |
| 预期结果 | `#problem-content` 显示“缺少题目 ID”。 |

### WEB-024 管理员登录失败提示

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 清空 Cookie |
| 操作步骤 | 1. 打开 `/admin/login.html`。<br>2. 输入用户名 `admin`、密码 `wrong-password`。<br>3. 提交表单。 |
| 预期结果 | 页面停留在管理员登录页；`#admin-message` 显示 `invalid username or password`。 |

### WEB-025 管理员登录成功并进入后台

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 管理员账号 `admin/password` 可用 |
| 操作步骤 | 1. 打开 `/admin/login.html`。<br>2. 输入用户名 `admin`、密码 `password`。<br>3. 提交表单。<br>4. 等待跳转到 `/admin/index.html`。 |
| 预期结果 | 管理员登录成功；后台题目列表加载；可看到“新增题目”和至少一个题目。 |

### WEB-026 管理员新增题目成功

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 已登录管理员；使用本次用例尝试唯一题目标题，例如 `Web Auto Problem ${dataId}_create`。 |
| 操作步骤 | 1. 打开 `/admin/new-problem.html`。<br>2. 填写本轮唯一标题。<br>3. 选择难度 `easy`。<br>4. 选择比较模式 `strict`。<br>5. 填写题目描述、输入格式、输出格式。<br>6. 填写样例输入 `2 3\n`、样例输出 `5\n`。<br>7. 时间限制填写 `1000`，内存限制填写 `131072`。<br>8. 隐藏测试用例 JSON 填写 `[{"input":"7 8\n","expected_output":"15\n"},{"input":"-2 5\n","expected_output":"3\n"}]`。<br>9. 点击“保存题目”。<br>10. 从管理员列表中读取新题目 ID。<br>11. 用 `try/finally` 在断言结束后删除该题目。 |
| 预期结果 | 表单提交成功；跳转到 `/admin/index.html`；管理员题目列表出现新题目；清理后该题目不再留在数据库中。 |

### WEB-027 管理员新增题目时隐藏测试用例必填

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 已登录管理员；使用本次用例尝试唯一题目标题，例如 `Web Auto Problem ${dataId}_no_hidden`。 |
| 操作步骤 | 1. 打开 `/admin/new-problem.html`。<br>2. 使用本轮唯一标题填写其他必填字段。<br>3. 将隐藏测试用例 JSON 填写为 `[]`。<br>4. 提交表单。<br>5. 打开 `/admin/index.html`，确认该唯一标题未出现在列表中。 |
| 预期结果 | 页面显示 `invalid problem`；不会新增题目。 |

### WEB-028 管理员新增题目时隐藏测试用例 JSON 格式错误

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 已登录管理员；使用本次用例尝试唯一题目标题，例如 `Web Auto Problem ${dataId}_bad_json`。 |
| 操作步骤 | 1. 打开 `/admin/new-problem.html`。<br>2. 使用本轮唯一标题填写其他必填字段。<br>3. 将隐藏测试用例 JSON 填写为 `{"broken":`。<br>4. 提交表单。<br>5. 打开 `/admin/index.html`，确认该唯一标题未出现在列表中。 |
| 预期结果 | 前端捕获解析错误；`#new-problem-message` 显示 `invalid problem`；不会发送有效新增请求。 |

### WEB-029 新增题目前台可见且详情只展示样例

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 本用例内先以管理员创建唯一临时题目，例如 `Web Auto Problem ${dataId}_visible`，并记录题目 ID；用例结束后删除该题目。 |
| 操作步骤 | 1. 打开 `/problems.html`。<br>2. 查找本用例创建的新题目标题。<br>3. 点击进入详情页。<br>4. 检查题面展示样例 `2 3` 和 `5`。<br>5. 检查页面不包含隐藏测试输入 `7 8`。 |
| 预期结果 | 新题目前台可见；详情页只展示样例，不暴露隐藏测试用例；用例清理后下次运行不会受该题影响。 |

### WEB-030 新增题目使用隐藏测试用例判题

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 本用例内先以管理员创建唯一临时题目，例如 `Web Auto Problem ${dataId}_hidden_judge`，隐藏用例包含 `7 8 -> 15`；随后登录普通用户或使用管理员会话提交；用例结束后删除该题目。 |
| 操作步骤 | 1. 打开本用例创建的题目详情页。<br>2. 输入只输出样例答案 `5` 的代码。<br>3. 点击提交。 |
| 预期结果 | 判题失败，证明服务使用隐藏测试用例而不是只使用样例；临时题目最终被删除。 |

### WEB-031 管理员删除题目成功

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 本用例内先以管理员创建唯一临时题目，例如 `Web Auto Problem ${dataId}_delete`，并记录题目 ID。 |
| 操作步骤 | 1. 打开 `/admin/index.html`。<br>2. 找到本用例创建的题目所在行。<br>3. 点击该行的“删除”按钮。<br>4. 等待列表刷新。<br>5. 打开 `/problems.html` 查找该唯一标题。 |
| 预期结果 | 新增题目从管理员列表消失；前台 `/problems.html` 不再显示该题目。 |

### WEB-032 删除题目后详情不可访问

| 项 | 内容 |
| --- | --- |
| 优先级 | P0 |
| 前置条件 | 本用例内先以管理员创建唯一临时题目，例如 `Web Auto Problem ${dataId}_deleted_detail`，记录题目 ID 后立即删除。 |
| 操作步骤 | 1. 打开 `/problem.html?id=${deletedProblemId}`。<br>2. 等待详情请求完成。 |
| 预期结果 | 页面不展示原题目内容；接口返回 404；页面显示加载失败或等价错误状态；用例不依赖其他删除用例是否执行。 |

### WEB-033 管理员退出后不能继续访问后台

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 已登录管理员 |
| 操作步骤 | 1. 在前台导航或测试脚本中触发 `/api/admin/logout`。<br>2. 打开 `/admin/index.html`。 |
| 预期结果 | 被重定向到 `/admin/login.html`；后台列表不可见。 |

### WEB-034 管理员可以进入题库并提交代码

| 项 | 内容 |
| --- | --- |
| 优先级 | P2 |
| 前置条件 | 已登录管理员 |
| 操作步骤 | 1. 打开 `/problems.html`。<br>2. 进入 `/problem.html?id=1`。<br>3. 输入 A+B 正确代码。<br>4. 点击提交。 |
| 预期结果 | 管理员作为已认证会话也可提交代码；判题通过。 |

### WEB-035 前台退出清理会话

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 已登录普通用户或管理员 |
| 操作步骤 | 1. 点击 `#logout-button`。<br>2. 等待页面刷新。<br>3. 检查导航。<br>4. 打开题目详情页。 |
| 预期结果 | 导航恢复“登录/注册/管理后台”；题目详情页提交按钮不可用或提示登录后提交。 |

### WEB-036 页面直接刷新后登录态保持

| 项 | 内容 |
| --- | --- |
| 优先级 | P1 |
| 前置条件 | 已登录普通用户 |
| 操作步骤 | 1. 打开 `/problems.html`。<br>2. 刷新页面。<br>3. 检查导航。 |
| 预期结果 | Cookie 会话仍有效；导航仍显示当前用户名和“退出”。 |

### WEB-037 多页面导航链路

| 项 | 内容 |
| --- | --- |
| 优先级 | P2 |
| 前置条件 | 无 |
| 操作步骤 | 1. 从首页点击进入题库入口。<br>2. 从题库点击题目标题。<br>3. 从题目详情点击品牌返回首页。<br>4. 从首页进入登录页和注册页。 |
| 预期结果 | 所有导航链接可用；页面路径符合预期；无 404。 |

### WEB-038 浏览器后退不破坏题目详情页

| 项 | 内容 |
| --- | --- |
| 优先级 | P2 |
| 前置条件 | 无 |
| 操作步骤 | 1. 打开 `/problems.html`。<br>2. 点击 `A+B Problem`。<br>3. 浏览器后退回题目列表。<br>4. 再前进回题目详情。 |
| 预期结果 | 题目列表和详情都能重新渲染；编辑器可用；无 JS 错误。 |

## 8. 建议自动化执行顺序

为了减少用例互相影响，建议按以下顺序执行：

1. 环境和静态资源：WEB-001、WEB-002。
2. 未登录访问和权限：WEB-003、WEB-004、WEB-005。
3. 普通用户认证：WEB-006、WEB-007、WEB-008、WEB-009。
4. 普通用户题目和判题：WEB-010 到 WEB-023。
5. 管理员认证和后台：WEB-024、WEB-025。
6. 管理员新增/删除题目：WEB-026 到 WEB-032。
7. 退出、登录态和导航：WEB-033 到 WEB-038。

判题压力较大的用例 WEB-019、WEB-020、WEB-021 建议串行执行，避免和其他判题用例并发导致结果不稳定。

## 9. 数据清理策略

1. 整套测试开始前执行 `make reset-web-test-db`，恢复基础账号、基础题目和基础测试用例。
2. 每个普通用户注册用例使用 `dataId` 生成唯一用户名，避免重复数据冲突。
3. 管理员新增题目用例必须使用 `dataId` 生成唯一标题，并记录新题目 ID。
4. 管理员新增题目的测试结束后必须删除该题目。
5. 每个浏览器上下文开始前清理：

```js
await context.clearCookies();
await page.evaluate(() => localStorage.clear());
```

6. 如果测试中断导致残留用户、题目或测试用例，可重新运行数据库重置工具恢复自动化测试基线：

```bash
make reset-web-test-db
```

`scripts/init_db.sh config/app.conf` 只用于首次建表和导入种子数据；重复执行自动化测试前应优先使用 `make reset-web-test-db`，因为它会删除上一轮测试产生的冗余数据。

## 10. 通过标准

本套 Web 自动化测试全部通过时，应满足：

- 所有 P0 用例 100% 通过。
- P1 用例无功能阻塞型失败。
- 管理员新增的临时题目最终被删除。
- 浏览器控制台无影响核心流程的 JavaScript 错误。
- 判题失败类用例不会导致服务不可用。
- 测试完成后 `/health` 仍返回：

```json
{"data":{"status":"ok"},"message":"ok","success":true}
```

## 11. 不纳入第一版 Web 自动化范围

根据 `SPEC.md` 第一版明确不做，以下内容不纳入本轮 Web 自动化测试：

- 服务端提交记录查询。
- 服务端保存用户代码。
- 完整用户资料系统。
- 找回密码。
- 邮件验证。
- 排行榜。
- 题解。
- 评论区。
- 竞赛模式。
- 多语言提交。
- Special Judge。
- 题目编辑。
- 用户管理。
