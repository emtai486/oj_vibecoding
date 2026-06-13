const params = new URLSearchParams(window.location.search);
const problemId = params.get("id");
const editor = createCppEditor(document.querySelector("#code-editor"));
let loggedInUser = null;

const submitButton = document.querySelector("#submit-code");
submitButton.disabled = true;

function escapeProblemHtml(value) {
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

function setSubmitResult(kind, text) {
  const resultView = document.querySelector("#submit-result");
  resultView.className = kind ? `result-pill ${kind}` : "muted";
  resultView.textContent = text;
}

function submitStatusText(result) {
  return result?.data?.status_text || result?.message || "提交失败";
}

function renderProblem(problem) {
  document.querySelector("#problem-title").textContent = problem.title;
  document.querySelector("#problem-meta").innerHTML = `
    <span class="difficulty ${difficultyClass(problem.difficulty)}">${escapeProblemHtml(problem.difficulty)}</span>
    <span class="pill">${escapeProblemHtml(problem.time_limit_ms)} ms</span>
    <span class="pill">${escapeProblemHtml(problem.memory_limit_kb)} KB</span>
    <span class="pill">${escapeProblemHtml(problem.compare_mode || "strict")}</span>
  `;
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

function configureSubmitState() {
  if (!loggedInUser) {
    submitButton.disabled = true;
    setSubmitResult("failed", "登录后提交");
    return;
  }

  submitButton.disabled = false;
  if (solvedStorage.isSolved(problemId)) {
    setSubmitResult("accepted", "已通过");
  } else {
    setSubmitResult("pending", "等待提交");
  }
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
    } else {
      document.querySelector("#problem-content").textContent = "题目不存在";
    }
  } catch {
    document.querySelector("#problem-content").textContent = "加载失败";
  }
}

document.querySelector("#submit-code").addEventListener("click", async () => {
  if (!loggedInUser) {
    setSubmitResult("failed", "请先登录");
    return;
  }

  setSubmitResult("pending", "提交中");

  try {
    const result = await api.post("/api/submit", {
      problem_id: Number(problemId),
      code: editor.getValue(),
    });

    if (result.success && result.data?.result === "passed") {
      solvedStorage.markSolved(problemId);
      setSubmitResult("accepted", submitStatusText(result));
    } else if (!result.success && result.message === "unauthorized") {
      setSubmitResult("failed", "请先登录");
    } else {
      setSubmitResult("failed", submitStatusText(result));
    }
  } catch {
    setSubmitResult("failed", "提交失败");
  }
});

async function initProblemPage() {
  loggedInUser = await currentUser();
  configureSubmitState();
  await loadProblem();
}

initProblemPage();
