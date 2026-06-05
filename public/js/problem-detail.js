const params = new URLSearchParams(window.location.search);
const problemId = params.get("id");
const editor = createCppEditor(document.querySelector("#code-editor"));
let loggedInUser = null;

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

function configureSubmitState() {
  const submitButton = document.querySelector("#submit-code");
  const resultView = document.querySelector("#submit-result");
  if (!loggedInUser) {
    submitButton.disabled = true;
    resultView.textContent = "登录后提交";
    return;
  }

  submitButton.disabled = false;
  resultView.textContent = solvedStorage.isSolved(problemId) ? "已通过" : "";
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
  if (!loggedInUser) {
    resultView.textContent = "请先登录";
    return;
  }

  resultView.textContent = "提交中";

  try {
    const result = await api.post("/api/submit", {
      problem_id: Number(problemId),
      code: editor.getValue(),
    });

    resultView.textContent = result.message;
    if (result.success && result.data?.result === "passed") {
      solvedStorage.markSolved(problemId);
      resultView.textContent = "accepted";
    } else if (!result.success && result.message === "unauthorized") {
      resultView.textContent = "请先登录";
    }
  } catch {
    resultView.textContent = "提交失败";
  }
});

async function initProblemPage() {
  loggedInUser = await currentUser();
  configureSubmitState();
  await loadProblem();
}

initProblemPage();
