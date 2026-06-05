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
    const result = await api.getCurrentUser();
    if (result.success && result.data?.logged_in) {
      return result.data.user;
    }
  } catch {
  }
  return null;
}

async function renderAuthNav() {
  const nav = document.querySelector("#auth-nav");
  if (!nav) {
    return;
  }

  const user = await currentUser();
  if (!user) {
    nav.innerHTML = `
      <a href="/login.html">登录</a>
      <a href="/register.html">注册</a>
      <a href="/admin/login.html">管理后台</a>
    `;
    return;
  }

  nav.innerHTML = `
    <span>${escapeAuthHtml(user.username)}</span>
    <button id="logout-button" class="link-button" type="button">退出</button>
    <a href="/admin/login.html">管理后台</a>
  `;

  document.querySelector("#logout-button").addEventListener("click", async () => {
    await api.logout();
    window.location.reload();
  });
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
        window.location.href = successUrl;
      }
    } catch {
      message.textContent = "请求失败";
    }
  });
}

bindJsonForm("#login-form", "#login-message", "/api/user/login", "/");
bindJsonForm("#register-form", "#register-message", "/api/user/register", "/login.html");
renderAuthNav();
