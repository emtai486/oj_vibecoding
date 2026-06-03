const params = new URLSearchParams(window.location.search);
const problemId = params.get("id");
const editor = createCppEditor(document.querySelector("#code-editor"));

function renderProblem(problem) {
  document.querySelector("#problem-title").textContent = problem.title;
  document.querySelector("#problem-meta").textContent =
    `${problem.difficulty} · ${problem.time_limit_ms} ms · ${problem.memory_limit_kb} KB`;
  document.querySelector("#problem-content").textContent = [
    problem.description,
    "",
    "输入格式",
    problem.input_format,
    "",
    "输出格式",
    problem.output_format,
    "",
    "样例输入",
    problem.sample_input,
    "",
    "样例输出",
    problem.sample_output,
  ].join("\n");
}

async function loadProblem() {
  if (!problemId) {
    document.querySelector("#problem-content").textContent = "缺少题目 ID";
    return;
  }

  try {
    const result = await api.getProblem(problemId);
    if (result.success) {
      renderProblem(result.data);
    }
  } catch {
    document.querySelector("#problem-content").textContent = "加载失败";
  }
}

document.querySelector("#submit-code").addEventListener("click", async () => {
  const resultView = document.querySelector("#submit-result");
  resultView.textContent = "提交中";

  try {
    const result = await api.post("/api/submit", {
      problem_id: Number(problemId),
      code: editor.getValue(),
    });

    resultView.textContent = result.message;
    if (result.success && result.data?.result === "passed") {
      solvedStorage.markSolved(problemId);
    }
  } catch {
    resultView.textContent = "提交失败";
  }
});

loadProblem();
