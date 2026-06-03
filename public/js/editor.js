const defaultCppCode = `#include <bits/stdc++.h>
using namespace std;

int main() {
    // write your code here
    return 0;
}
`;

function createCppEditor(textarea) {
  textarea.value = defaultCppCode;
  return CodeMirror.fromTextArea(textarea, {
    mode: "text/x-c++src",
    theme: "material-darker",
    lineNumbers: true,
    indentUnit: 4,
    tabSize: 4,
  });
}
