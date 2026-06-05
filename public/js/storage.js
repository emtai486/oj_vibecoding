const solvedStorage = {
  key: "oj_problem_status",

  all() {
    try {
      return JSON.parse(localStorage.getItem(this.key) || "{}");
    } catch {
      return {};
    }
  },

  isSolved(problemId) {
    return this.all()[problemId] === "passed";
  },

  markSolved(problemId) {
    const solved = this.all();
    solved[problemId] = "passed";
    localStorage.setItem(this.key, JSON.stringify(solved));
  },
};
