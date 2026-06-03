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
        <td>${problem.title}</td>
        <td>${problem.difficulty}</td>
        <td><button data-delete-id="${problem.id}" type="button">删除</button></td>
      </tr>`)
      .join("");

    if (problems.length === 0) {
      tbody.innerHTML = '<tr><td colspan="4" class="muted">暂无题目</td></tr>';
    }
  } catch {
    tbody.innerHTML = '<tr><td colspan="4" class="muted">加载失败</td></tr>';
  }
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

function bindNewProblem() {
  const form = document.querySelector("#new-problem-form");
  if (!form) {
    return;
  }

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const payload = Object.fromEntries(new FormData(form).entries());
    payload.time_limit_ms = Number(payload.time_limit_ms);
    payload.memory_limit_kb = Number(payload.memory_limit_kb);
    const result = await api.post("/api/admin/problems", payload);
    document.querySelector("#new-problem-message").textContent = result.message;
  });
}

bindAdminLogin();
bindNewProblem();
loadAdminProblems();
