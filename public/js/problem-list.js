function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function difficultyClass(value) {
  const difficulty = String(value || "").toLowerCase();
  return ["easy", "medium", "hard"].includes(difficulty) ? difficulty : "";
}
async function loadProblems() {
  const tbody = document.querySelector("#problem-list");
  const count = document.querySelector("#problem-count");

  try {
    const user = await currentUser();


    const result = await api.getProblems();
    const problems = result.success ? result.data : [];
    count.textContent = `${problems.length} 题`;
    tbody.innerHTML = problems
      .map((problem) => {
        const status = user && solvedStorage.isSolved(problem.id)
          ? '<span class="status-pill solved">已通过</span>'
          : '<span class="status-pill unsolved">未完成</span>';
        const difficulty = escapeHtml(problem.difficulty);
        return `<tr>
          <td>${escapeHtml(problem.id)}</td>
          <td><a href="/problem.html?id=${encodeURIComponent(problem.id)}">${escapeHtml(problem.title)}</a></td>
          <td><span class="difficulty ${difficultyClass(problem.difficulty)}">${difficulty}</span></td>
          <td>${status}</td>
        </tr>`;
      })
      .join("");

    if (problems.length === 0) {
      tbody.innerHTML = '<tr><td colspan="4" class="muted">暂无题目</td></tr>';
    }
  } catch {
    tbody.innerHTML = '<tr><td colspan="4" class="muted">加载失败</td></tr>';
  }
}

loadProblems();

