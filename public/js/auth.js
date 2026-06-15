function escapeAuthHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

async function currentUser() {
  try {
    const [userResult, adminResult] = await Promise.allSettled([
      api.getCurrentUser(),
      api.getCurrentAdmin(),
    ]);

    const admin = adminResult.status === "fulfilled" ? adminResult.value : null;
    if (admin?.success && admin.data?.logged_in) {
      return {
        ...admin.data.admin,
        role: "admin",
        roleLabel: "管理员",
      };
    }

    const user = userResult.status === "fulfilled" ? userResult.value : null;
    if (user?.success && user.data?.logged_in) {
      return {
        ...user.data.user,
        role: "user",
        roleLabel: "普通用户",
      };
    }
  } catch {
  }
  return null;
}

async function renderAuthNav() {
  const nav = document.querySelector("#auth-nav");
  const user = await currentUser();

  if (!nav) {
    return user;
  }

  if (!user) {
    nav.innerHTML = `
      <a href="/login.html">登录</a>
      <a href="/register.html">注册</a>
      <a href="/admin/login.html">管理后台</a>
    `;
    return null;
  }

  nav.innerHTML = `
    <a href="/problems.html">题库</a>
    <span>${escapeAuthHtml(user.username)}${user.role === "admin" ? " · 管理员" : ""}</span>
    <button id="logout-button" class="link-button" type="button">退出</button>
    <a href="${user.role === "admin" ? "/admin/index.html" : "/admin/login.html"}">管理后台</a>
  `;

  document.querySelector("#logout-button").addEventListener("click", async () => {
    await Promise.allSettled([api.logout(), api.adminLogout()]);
    window.location.reload();
  });

  return user;
}

function bindJsonForm(formId, messageId, path, successUrl) {
  const form = document.querySelector(formId);
  if (!form) {
    return;
  }

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const message = document.querySelector(messageId);
    const data = Object.fromEntries(new FormData(form).entries());

    try {
      const result = await api.post(path, data);
      message.textContent = result.message;
      if (result.success && successUrl) {
        window.location.href = typeof successUrl === "function" ? successUrl() : successUrl;
      }
    } catch {
      message.textContent = "请求失败";
    }
  });
}

function safeNextUrl(defaultUrl) {
  const next = new URLSearchParams(window.location.search).get("next");
  if (next && next.startsWith("/") && !next.startsWith("//")) {
    return next;
  }
  return defaultUrl;
}

function renderLandingAccess(user) {
  const links = [
    document.querySelector("#landing-problems-link"),
    document.querySelector("#landing-flow-link"),
  ].filter(Boolean);
  if (links.length === 0) {
    return;
  }

  links.forEach((link) => {
    if (user) {
      link.href = "/problems.html";
      link.textContent = user.role === "admin" ? "管理员进入题库" : "进入题库";
    } else {
      link.href = "/problems.html";
      link.textContent = "进入题库";
    }
  });
}

bindJsonForm("#login-form", "#login-message", "/api/user/login", () => safeNextUrl("/problems.html"));
bindJsonForm("#register-form", "#register-message", "/api/user/register", "/login.html");
renderAuthNav().then(renderLandingAccess);
