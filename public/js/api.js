const api = {
  async request(path, options = {}) {
    const response = await fetch(path, {
      credentials: "same-origin",
      headers: {
        "Content-Type": "application/json",
        ...(options.headers || {}),
      },
      ...options,
    });
    return response.json();
  },

  getProblems() {
    return this.request("/api/problems");
  },

  getProblem(id) {
    return this.request(`/api/problems/${id}`);
  },

  getCurrentUser() {
    return this.request("/api/user/me");
  },

  post(path, body) {
    return this.request(path, {
      method: "POST",
      body: JSON.stringify(body),
    });
  },

  delete(path) {
    return this.request(path, {
      method: "DELETE",
    });
  },

  logout() {
    return this.post("/api/user/logout", {});
  },
};
