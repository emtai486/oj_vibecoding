const solvedStorage = {
  key: "oj.solvedProblems",

  all() {
    try {
      return JSON.parse(localStorage.getItem(this.key) || "{}");
    } catch {
      return {};
    }
  },

  isSolved(problemId) {
    return Boolean(this.all()[problemId]);
  },

  markSolved(problemId) {
    const solved = this.all();
    solved[problemId] = true;
    localStorage.setItem(this.key, JSON.stringify(solved));
  },
};
