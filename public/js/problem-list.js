async function loadProblems() {
  const tbody = document.querySelector("#problem-list");
  const count = document.querySelector("#problem-count");

  try {
    const result = await api.getProblems();
    const problems = result.success ? result.data : [];
    count.textContent = `${problems.length} 题`;
    tbody.innerHTML = problems
      .map((problem) => {
        const status = solvedStorage.isSolved(problem.id) ? "已通过" : "未完成";
        return `<tr>
          <td>${problem.id}</td>
          <td><a href="/problem.html?id=${problem.id}">${problem.title}</a></td>
          <td>${problem.difficulty}</td>
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
