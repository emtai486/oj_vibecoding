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
