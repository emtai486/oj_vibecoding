async page => {
  const BASE =
    (typeof process !== "undefined" && process.env.OJ_WEB_BASE_URL) ||
    "http://127.0.0.1:8080";
  const CASE_FILTER = new Set(
    ((typeof process !== "undefined" && process.env.OJ_WEB_CASES) || "")
      .split(",")
      .map((value) => value.trim())
      .filter(Boolean),
  );
  const request = page.context().request;
  const results = [];
  const pageErrors = [];
  const consoleErrors = [];

  page.setDefaultTimeout(15000);
  page.on("pageerror", (error) => pageErrors.push(String(error)));
  page.on("console", (message) => {
    if (message.type() === "error") {
      consoleErrors.push(message.text());
    }
  });

  function assert(condition, message) {
    if (!condition) {
      throw new Error(message);
    }
  }

  function textMatches(text, expected) {
    const haystack = String(text || "");
    return Array.isArray(expected)
      ? expected.some((value) => haystack.includes(value))
      : haystack.includes(expected);
  }

  function currentPath() {
    const withoutHash = page.url().split("#", 1)[0];
    const withoutQuery = withoutHash.split("?", 1)[0];
    const origin = BASE.replace(/\/$/, "");
    return withoutQuery.startsWith(origin)
      ? withoutQuery.slice(origin.length) || "/"
      : withoutQuery;
  }

  async function jsonResponse(response) {
    let body = null;
    try {
      body = await response.json();
    } catch {
      body = await response.text();
    }
    return { status: response.status(), body };
  }

  async function apiGet(path) {
    return jsonResponse(await request.get(`${BASE}${path}`, { timeout: 5000 }));
  }

  async function apiPost(path, data) {
    return jsonResponse(await request.post(`${BASE}${path}`, { data, timeout: 5000 }));
  }

  async function apiDelete(path) {
    return jsonResponse(await request.delete(`${BASE}${path}`, { timeout: 5000 }));
  }

  async function clearSession() {
    await page.context().clearCookies();
    await page.goto(`${BASE}/`, { waitUntil: "domcontentloaded" });
    await page.evaluate(() => {
      localStorage.clear();
      sessionStorage.clear();
    });
  }

  async function loginUser() {
    await clearSession();
    const response = await apiPost("/api/user/login", {
      username: "user1",
      password: "password",
    });
    assert(response.status === 200, `user login HTTP ${response.status}`);
    assert(response.body?.success === true, `user login failed: ${JSON.stringify(response.body)}`);
  }

  async function loginAdmin() {
    await clearSession();
    const response = await apiPost("/api/admin/login", {
      username: "admin",
      password: "password",
    });
    assert(response.status === 200, `admin login HTTP ${response.status}`);
    assert(response.body?.success === true, `admin login failed: ${JSON.stringify(response.body)}`);
  }

  async function gotoPath(path) {
    await page.goto(`${BASE}${path}`, { waitUntil: "domcontentloaded" });
  }

  async function waitForText(selector, expected, timeout = 30000) {
    await page.waitForFunction(
      ({ selector, expected }) => {
        const text = document.querySelector(selector)?.textContent || "";
        const values = Array.isArray(expected) ? expected : [expected];
        return values.some((value) => text.includes(value));
      },
      { selector, expected },
      { timeout },
    );
    return (await page.locator(selector).textContent()) || "";
  }

  async function openProblem(problemId, title = null) {
    await gotoPath(`/problem.html?id=${problemId}`);
    await page.locator(".CodeMirror").waitFor({ state: "visible", timeout: 15000 });
    if (title) {
      await waitForText("#problem-title", title, 15000);
    }
    await page.waitForFunction(() => !document.querySelector("#submit-code")?.disabled, null, {
      timeout: 15000,
    });
  }

  async function setCodeMirror(code) {
    await page.evaluate((value) => {
      const mirror = document.querySelector(".CodeMirror");
      if (mirror?.CodeMirror) {
        mirror.CodeMirror.setValue(value);
        return;
      }
      const textarea = document.querySelector("#code-editor");
      if (!textarea) {
        throw new Error("CodeMirror textarea not found");
      }
      textarea.value = value;
      textarea.dispatchEvent(new Event("input", { bubbles: true }));
    }, code);
  }

  async function submitCodeAndExpect(problemId, code, expectedText, timeout = 30000) {
    await openProblem(problemId, problemId === 1 ? "A+B Problem" : null);
    await setCodeMirror(code);
    await page.locator("#submit-code").click();
    const text = await waitForText("#submit-result", expectedText, timeout);
    assert(textMatches(text, expectedText), `submit result ${JSON.stringify(text)} did not contain ${expectedText}`);
    return text;
  }

  async function assertNoPassedStatus(problemId) {
    const stored = await page.evaluate(() => localStorage.getItem("oj_problem_status"));
    assert(
      !stored || !stored.includes(`"${problemId}":"passed"`),
      `localStorage should not mark problem ${problemId} as passed: ${stored}`,
    );
  }

  async function assertHealthOk() {
    const health = await apiGet("/health");
    assert(health.status === 200, `/health HTTP ${health.status}`);
    assert(health.body?.success === true, `/health failed: ${JSON.stringify(health.body)}`);
  }

  async function createTempProblem(suffix) {
    await loginAdmin();
    const dataId = `${Date.now()}_${Math.random().toString(16).slice(2)}`;
    const title = `Web Auto Problem ${dataId}_${suffix}`;
    const payload = {
      title,
      difficulty: "easy",
      compare_mode: "strict",
      description: "Temporary problem for targeted web regression.",
      input_format: "Two integers.",
      output_format: "Their sum.",
      sample_input: "2 3\n",
      sample_output: "5\n",
      time_limit_ms: 1000,
      memory_limit_kb: 131072,
      samples: [{ input: "2 3\n", expected_output: "5\n" }],
      hidden_testcases: [
        { input: "7 8\n", expected_output: "15\n" },
        { input: "-2 5\n", expected_output: "3\n" },
      ],
    };
    const created = await apiPost("/api/admin/problems", payload);
    assert(created.status === 201, `create problem HTTP ${created.status}: ${JSON.stringify(created.body)}`);
    assert(created.body?.success === true, `create problem failed: ${JSON.stringify(created.body)}`);
    const id = created.body?.data?.id;
    assert(Number.isInteger(id), `create problem returned invalid id: ${JSON.stringify(created.body)}`);
    return { id, title };
  }

  async function deleteProblem(id) {
    const deleted = await apiDelete(`/api/admin/problems/${id}`);
    assert([200, 404].includes(deleted.status), `delete problem HTTP ${deleted.status}: ${JSON.stringify(deleted.body)}`);
    if (deleted.status === 200) {
      assert(deleted.body?.success === true, `delete problem failed: ${JSON.stringify(deleted.body)}`);
    }
  }

  async function runCase(id, fn) {
    if (CASE_FILTER.size > 0 && !CASE_FILTER.has(id)) {
      return;
    }

    const startedAt = Date.now();
    try {
      const detail = await fn();
      results.push({ id, status: "PASS", ms: Date.now() - startedAt, detail: detail || "" });
    } catch (error) {
      results.push({ id, status: "FAIL", ms: Date.now() - startedAt, detail: error.message });
    }
  }

  const codes = {
    wrongAnswer: `#include <bits/stdc++.h>
using namespace std;
int main() {
    cout << 0 << endl;
    return 0;
}
`,
    compileError: "int main( {\n",
    strictNoNewline: `#include <bits/stdc++.h>
using namespace std;
int main() {
    int a, b;
    if (cin >> a >> b) {
        cout << a + b;
    }
    return 0;
}
`,
    timeLimit: `#include <cstdint>
int main() {
    volatile std::uint64_t x = 0;
    while (true) {
        ++x;
    }
    return 0;
}
`,
    memoryLimit: `#include <iostream>
#include <vector>
int main() {
    std::vector<char> data(300 * 1024 * 1024);
    std::cout << data.size() << '\\n';
    return 0;
}
`,
    outputLimit: `#include <iostream>
#include <string>
int main() {
    std::cout << std::string(1100000, 'x');
    return 0;
}
`,
    sampleOnly: `#include <iostream>
int main() {
    std::cout << "5\\n";
    return 0;
}
`,
  };

  const preflight = {
    health: await apiGet("/health"),
    problems: await apiGet("/api/problems"),
    login: await apiPost("/api/user/login", {
      username: "user1",
      password: "password",
    }),
  };
  await page.context().clearCookies();

  assert(preflight.health.status === 200, `preflight health HTTP ${preflight.health.status}`);
  assert(preflight.health.body?.success === true, `preflight health failed: ${JSON.stringify(preflight.health.body)}`);
  assert(preflight.problems.status === 200, `preflight problems HTTP ${preflight.problems.status}`);
  assert(preflight.problems.body?.success === true, `preflight problems failed: ${JSON.stringify(preflight.problems.body)}`);
  assert(
    JSON.stringify(preflight.problems.body).includes("A+B Problem") &&
      JSON.stringify(preflight.problems.body).includes("Average Score"),
    `preflight problems missing seed titles: ${JSON.stringify(preflight.problems.body)}`,
  );
  assert(preflight.login.status === 200, `preflight login HTTP ${preflight.login.status}`);
  assert(preflight.login.body?.success === true, `preflight login failed: ${JSON.stringify(preflight.login.body)}`);

  await runCase("WEB-014", async () => {
    await loginUser();
    const text = await submitCodeAndExpect(1, codes.wrongAnswer, "Wrong Answer");
    await assertNoPassedStatus(1);
    return text;
  });

  await runCase("WEB-015", async () => {
    await loginUser();
    return submitCodeAndExpect(1, codes.compileError, "Compile Error");
  });

  await runCase("WEB-016", async () => {
    await loginUser();
    return submitCodeAndExpect(1, "", "Compile Error");
  });

  await runCase("WEB-017", async () => {
    await loginUser();
    const text = await submitCodeAndExpect(1, codes.strictNoNewline, "Wrong Answer");
    await assertNoPassedStatus(1);
    return text;
  });

  await runCase("WEB-019", async () => {
    await loginUser();
    const text = await submitCodeAndExpect(1, codes.timeLimit, "Time Limit Exceeded", 35000);
    await assertHealthOk();
    return text;
  });

  await runCase("WEB-020", async () => {
    await loginUser();
    const text = await submitCodeAndExpect(1, codes.memoryLimit, "Memory Limit Exceeded", 35000);
    await assertHealthOk();
    return text;
  });

  await runCase("WEB-021", async () => {
    await loginUser();
    const text = await submitCodeAndExpect(1, codes.outputLimit, "Output Limit Exceeded", 35000);
    await assertHealthOk();
    return text;
  });

  await runCase("WEB-030", async () => {
    const problem = await createTempProblem("hidden_judge");
    try {
      await openProblem(problem.id, problem.title);
      await setCodeMirror(codes.sampleOnly);
      await page.locator("#submit-code").click();
      return await waitForText("#submit-result", "Wrong Answer", 30000);
    } finally {
      await deleteProblem(problem.id);
    }
  });

  await runCase("WEB-032", async () => {
    const problem = await createTempProblem("deleted_detail");
    await deleteProblem(problem.id);
    const apiResult = await apiGet(`/api/problems/${problem.id}`);
    assert(apiResult.status === 404, `deleted detail API HTTP ${apiResult.status}`);
    await gotoPath(`/problem.html?id=${problem.id}`);
    const content = await waitForText("#problem-content", ["题目不存在", "加载失败"], 15000);
    assert(!content.includes(problem.title), `deleted detail still shows title ${problem.title}`);
    return content.trim();
  });

  await runCase("WEB-035", async () => {
    await loginUser();
    await gotoPath("/problems.html");
    await page.locator("#logout-button").waitFor({ state: "visible", timeout: 15000 });
    await page.locator("#logout-button").click();
    await page.waitForFunction(() => {
      const nav = document.querySelector("#auth-nav")?.textContent || "";
      return nav.includes("登录") && nav.includes("注册");
    });
    await gotoPath("/problem.html?id=1");
    await waitForText("#problem-title", "A+B Problem", 15000);
    const disabled = await page.locator("#submit-code").isDisabled();
    const resultText = (await page.locator("#submit-result").textContent()) || "";
    assert(disabled || resultText.includes("登录后提交"), `submit state after logout: disabled=${disabled}, text=${resultText}`);
    return `disabled=${disabled}, result=${resultText}`;
  });

  await runCase("WEB-037", async () => {
    await clearSession();
    await gotoPath("/");
    await page.locator("#landing-problems-link").click();
    await page.waitForLoadState("domcontentloaded");
    assert(currentPath() === "/problems.html", `home problem entry went to ${page.url()}`);
    await waitForText("#problem-list", "A+B Problem", 15000);
    await page.locator('a[href="/problem.html?id=1"]').click();
    await waitForText("#problem-title", "A+B Problem", 15000);
    await page.locator(".brand").click();
    await page.waitForURL(`${BASE}/`);
    await page.locator('a[href="/login.html"]').first().click();
    await page.waitForURL(`${BASE}/login.html`);
    await gotoPath("/");
    await page.locator('a[href="/register.html"]').first().click();
    await page.waitForURL(`${BASE}/register.html`);
    return "home -> problems -> detail -> home -> login/register";
  });

  await runCase("WEB-038", async () => {
    const errorStart = pageErrors.length + consoleErrors.length;
    await clearSession();
    await gotoPath("/problems.html");
    await waitForText("#problem-list", "A+B Problem", 15000);
    await page.locator('a[href="/problem.html?id=1"]').click();
    await waitForText("#problem-title", "A+B Problem", 15000);
    await page.locator(".CodeMirror").waitFor({ state: "visible", timeout: 15000 });
    await page.goBack({ waitUntil: "domcontentloaded" });
    await waitForText("#problem-list", "A+B Problem", 15000);
    await page.goForward({ waitUntil: "domcontentloaded" });
    await waitForText("#problem-title", "A+B Problem", 15000);
    await page.locator(".CodeMirror").waitFor({ state: "visible", timeout: 15000 });
    const errorEnd = pageErrors.length + consoleErrors.length;
    assert(errorEnd === errorStart, `new browser errors: ${JSON.stringify({ pageErrors, consoleErrors })}`);
    return "back/forward kept list and detail renderable";
  });

  const passed = results.filter((item) => item.status === "PASS").length;
  const failed = results.filter((item) => item.status === "FAIL").length;
  await assertHealthOk();

  return {
    base: BASE,
    summary: { total: results.length, passed, failed },
    results,
    pageErrors,
    consoleErrors,
  };
}
