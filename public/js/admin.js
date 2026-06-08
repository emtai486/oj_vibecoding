function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (ch) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#39;",
  })[ch]);
}

function difficultyClass(value) {
  const difficulty = String(value || "").toLowerCase();
  return ["easy", "medium", "hard"].includes(difficulty) ? difficulty : "";
}

async function requireAdminSession() {
  const protectedPage = document.querySelector("[data-admin-page]");
  if (!protectedPage) {
    return true;
  }

  const result = await api.request("/api/admin/me");
  if (!result.success || !result.data.logged_in) {
    window.location.href = "/admin/login.html";
    return false;
  }
  return true;
}

async function loadAdminProblems() {
  const tbody = document.querySelector("#admin-problem-list");
  if (!tbody) {
    return;
  }

  try {
    const result = await api.getProblems();
    const problems = result.success ? result.data : [];
    tbody.innerHTML = problems
      .map((problem) => `<tr>
        <td>${problem.id}</td>
        <td><a href="/problem.html?id=${encodeURIComponent(problem.id)}">${escapeHtml(problem.title)}</a></td>
        <td><span class="difficulty ${difficultyClass(problem.difficulty)}">${escapeHtml(problem.difficulty)}</span></td>
        <td>
          <div class="table-actions">
            <a class="button-link button-secondary table-action" href="/problem.html?id=${encodeURIComponent(problem.id)}">练习</a>
            <button class="danger-button table-action" data-delete-id="${problem.id}" type="button">删除</button>
          </div>
        </td>
      </tr>`)
      .join("");

    if (problems.length === 0) {
      tbody.innerHTML = '<tr><td colspan="4" class="muted">暂无题目</td></tr>';
    }
  } catch {
    tbody.innerHTML = '<tr><td colspan="4" class="muted">加载失败</td></tr>';
  }
}

function bindAdminProblemList() {
  const tbody = document.querySelector("#admin-problem-list");
  if (!tbody) {
    return;
  }

  tbody.addEventListener("click", async (event) => {
    const button = event.target.closest("[data-delete-id]");
    if (!button) {
      return;
    }

    button.disabled = true;
    const id = button.dataset.deleteId;
    const result = await api.delete(`/api/admin/problems/${id}`);
    if (result.success) {
      await loadAdminProblems();
    } else if (result.message === "unauthorized") {
      window.location.href = "/admin/login.html";
    } else {
      button.disabled = false;
      button.textContent = result.message;
    }
  });
}

function bindAdminLogin() {
  const form = document.querySelector("#admin-login-form");
  if (!form) {
    return;
  }

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const result = await api.post("/api/admin/login", Object.fromEntries(new FormData(form).entries()));
    document.querySelector("#admin-message").textContent = result.message;
    if (result.success) {
      window.location.href = "/admin/index.html";
    }
  });
}

function parseTestcaseJson(raw) {
  const value = raw.trim();
  if (!value) {
    return [];
  }

  const parsed = JSON.parse(value);
  if (!Array.isArray(parsed)) {
    throw new Error("testcases should be an array");
  }
  return parsed.map((item) => ({
    input: String(item.input ?? ""),
    expected_output: String(item.expected_output ?? ""),
  }));
}

function bindNewProblem() {
  const form = document.querySelector("#new-problem-form");
  if (!form) {
    return;
  }

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const message = document.querySelector("#new-problem-message");
    const payload = Object.fromEntries(new FormData(form).entries());
    try {
      payload.time_limit_ms = Number(payload.time_limit_ms);
      payload.memory_limit_kb = Number(payload.memory_limit_kb);
      payload.samples = [{
        input: payload.sample_input,
        expected_output: payload.sample_output,
      }];
      payload.hidden_testcases = parseTestcaseJson(payload.hidden_testcases);
      const result = await api.post("/api/admin/problems", payload);
      message.textContent = result.message;
      if (result.success) {
        window.location.href = "/admin/index.html";
      } else if (result.message === "unauthorized") {
        window.location.href = "/admin/login.html";
      }
    } catch {
      message.textContent = "invalid problem";
    }
  });
}

async function bootAdminPage() {
  bindAdminLogin();
  const allowed = await requireAdminSession();
  if (!allowed) {
    return;
  }
  bindAdminProblemList();
  bindNewProblem();
  loadAdminProblems();
}

bootAdminPage();
