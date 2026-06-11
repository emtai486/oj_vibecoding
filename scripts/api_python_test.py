#!/usr/bin/env python3
"""HTTP API automation tests implemented with the Python standard library."""

from __future__ import annotations

import argparse
import http.cookiejar
import json
import os
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import HTTPCookieProcessor, Request, build_opener


ROOT_DIR = Path(__file__).resolve().parents[1]


@dataclass
class HttpResult:
    status: int
    body: str
    data: Any


def pass_case(label: str) -> None:
    print(f"PASS: {label}")


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def config_value(config_path: Path, key: str, default: str) -> str:
    if not config_path.exists():
        return default

    prefix = f"{key}="
    for raw_line in config_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith(prefix):
            return line.split("=", 1)[1].strip()
    return default


def write_server_config(source_config: Path, target_config: Path, port: int) -> None:
    wrote_host = False
    wrote_port = False
    lines: list[str] = []

    for raw_line in source_config.read_text(encoding="utf-8").splitlines():
        stripped = raw_line.strip()
        if stripped.startswith("server.host="):
            lines.append("server.host=127.0.0.1")
            wrote_host = True
        elif stripped.startswith("server.port="):
            lines.append(f"server.port={port}")
            wrote_port = True
        else:
            lines.append(raw_line)

    if not wrote_host:
        lines.append("server.host=127.0.0.1")
    if not wrote_port:
        lines.append(f"server.port={port}")

    target_config.write_text("\n".join(lines) + "\n", encoding="utf-8")


def request_json(
    base_url: str,
    method: str,
    path: str,
    body: Any = None,
    cookie_jar: http.cookiejar.CookieJar | None = None,
    timeout: float = 10.0,
) -> HttpResult:
    if body is None:
        payload = None
    elif isinstance(body, str):
        payload = body.encode("utf-8")
    else:
        payload = json.dumps(body, separators=(",", ":")).encode("utf-8")

    request = Request(
        f"{base_url}{path}",
        data=payload,
        headers={"Content-Type": "application/json"},
        method=method,
    )
    opener = (
        build_opener(HTTPCookieProcessor(cookie_jar))
        if cookie_jar is not None
        else build_opener()
    )

    try:
        with opener.open(request, timeout=timeout) as response:
            status = response.status
            raw = response.read().decode("utf-8")
    except HTTPError as error:
        status = error.code
        raw = error.read().decode("utf-8")

    try:
        parsed = json.loads(raw)
    except json.JSONDecodeError:
        parsed = None

    return HttpResult(status=status, body=raw, data=parsed)


def assert_response(
    label: str,
    result: HttpResult,
    expected_status: int,
    expected_success: bool,
    expected_message: str,
) -> None:
    if result.status != expected_status:
        fail(
            f"{label} status expected {expected_status}, got {result.status}; "
            f"body: {result.body}"
        )
    if not isinstance(result.data, dict):
        fail(f"{label} expected JSON object response; body: {result.body}")
    if result.data.get("success") is not expected_success:
        fail(
            f"{label} success expected {expected_success}; body: {result.body}"
        )
    if result.data.get("message") != expected_message:
        fail(
            f"{label} message expected {expected_message}; body: {result.body}"
        )
    pass_case(label)


def assert_body_contains(label: str, result: HttpResult, needle: str) -> None:
    if needle not in result.body:
        fail(f"{label} expected body to contain {needle}; body: {result.body}")


def assert_body_not_contains(label: str, result: HttpResult, needle: str) -> None:
    if needle in result.body:
        fail(f"{label} expected body not to contain {needle}; body: {result.body}")


def assert_cookie(cookie_jar: http.cookiejar.CookieJar, name: str, label: str) -> None:
    if not any(cookie.name == name for cookie in cookie_jar):
        fail(f"{label} should store {name} cookie")


def assert_submit_result(
    base_url: str,
    cookie_jar: http.cookiejar.CookieJar,
    label: str,
    problem_id: int,
    code: str,
    expected_message: str,
    expected_result: str,
    expected_status: str | None = None,
    timeout: float = 10.0,
) -> None:
    result = request_json(
        base_url,
        "POST",
        "/api/submit",
        {"problem_id": problem_id, "code": code},
        cookie_jar,
        timeout=timeout,
    )
    assert_response(label, result, 200, True, expected_message)
    assert_body_contains(label, result, f'"result":"{expected_result}"')
    if expected_status is not None:
        assert_body_contains(label, result, f'"status":"{expected_status}"')


def start_server(config_path: Path, tmp_dir: Path) -> tuple[str, subprocess.Popen[bytes]]:
    if not config_path.exists():
        fail(f"config not found: {config_path}")

    server_path = ROOT_DIR / "build" / "oj_server"
    if not server_path.exists() or not os.access(server_path, os.X_OK):
        fail("build/oj_server not found; run make first")

    start_port = int(os.environ.get("OJ_API_TEST_PORT", "18080"))
    last_port = start_port + 40
    logs: list[Path] = []

    for port in range(start_port, last_port + 1):
        server_config = tmp_dir / f"app.{port}.conf"
        server_log = tmp_dir / f"server.{port}.log"
        logs.append(server_log)
        write_server_config(config_path, server_config, port)
        base_url = f"http://127.0.0.1:{port}"

        log_handle = server_log.open("wb")
        process = subprocess.Popen(
            [str(server_path), str(server_config)],
            cwd=str(ROOT_DIR),
            stdout=log_handle,
            stderr=subprocess.STDOUT,
        )
        log_handle.close()

        ready = False
        for _ in range(30):
            try:
                result = request_json(base_url, "GET", "/health", timeout=1.0)
                if result.status == 200:
                    ready = True
                    break
            except URLError:
                pass

            if process.poll() is not None:
                break
            time.sleep(0.1)

        if ready:
            return base_url, process

        stop_server(process)

        if port == last_port:
            for log in logs:
                if log.exists():
                    sys.stderr.write(log.read_text(encoding="utf-8", errors="replace"))
            fail(
                f"server exited before becoming ready on ports {start_port}-{last_port}"
            )

    fail(f"server exited before becoming ready on ports {start_port}-{last_port}")


def stop_server(process: subprocess.Popen[bytes] | None) -> None:
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)


def run_basic_checks(base_url: str) -> None:
    result = request_json(base_url, "GET", "/health")
    assert_response("GET /health", result, 200, True, "ok")
    assert_body_contains("GET /health", result, '"status":"ok"')

    result = request_json(base_url, "POST", "/api/_echo", {"hello": "world", "n": 1})
    assert_response("POST /api/_echo", result, 200, True, "ok")
    assert_body_contains("POST /api/_echo", result, '"hello":"world"')

    result = request_json(base_url, "POST", "/api/_echo", '{"broken":')
    assert_response("POST /api/_echo invalid JSON", result, 400, False, "invalid json")

    result = request_json(base_url, "GET", "/api/not-found")
    assert_response("GET /api/not-found", result, 404, False, "not found")

    result = request_json(base_url, "GET", "/api/user/me")
    assert_response("GET /api/user/me anonymous", result, 200, True, "ok")
    assert_body_contains("GET /api/user/me anonymous", result, '"logged_in":false')

    result = request_json(
        base_url, "POST", "/api/submit", {"problem_id": 1, "code": "int main(){return 0;}"}
    )
    assert_response("POST /api/submit anonymous", result, 401, False, "unauthorized")

    result = request_json(base_url, "POST", "/api/user/logout", {})
    assert_response("POST /api/user/logout anonymous", result, 200, True, "logged out")

    result = request_json(base_url, "GET", "/api/admin/me")
    assert_response("GET /api/admin/me anonymous", result, 200, True, "ok")
    assert_body_contains("GET /api/admin/me anonymous", result, '"logged_in":false')

    result = request_json(base_url, "POST", "/api/admin/logout", {})
    assert_response("POST /api/admin/logout anonymous", result, 200, True, "logged out")

    result = request_json(base_url, "POST", "/api/admin/problems", {"title": "Example"})
    assert_response("POST /api/admin/problems anonymous", result, 401, False, "unauthorized")

    result = request_json(base_url, "DELETE", "/api/admin/problems/1")
    assert_response("DELETE /api/admin/problems/1 anonymous", result, 401, False, "unauthorized")


def run_full_checks(base_url: str) -> None:
    run_basic_checks(base_url)

    result = request_json(base_url, "GET", "/api/problems")
    assert_response("GET /api/problems", result, 200, True, "ok")
    assert_body_contains("GET /api/problems", result, '"title":"A+B Problem"')

    result = request_json(base_url, "GET", "/api/problems/1")
    assert_response("GET /api/problems/1", result, 200, True, "ok")
    assert_body_contains("GET /api/problems/1", result, '"title":"A+B Problem"')
    assert_body_contains("GET /api/problems/1", result, '"samples":')
    assert_body_not_contains("GET /api/problems/1 hidden testcase", result, "10 20")

    result = request_json(base_url, "GET", "/api/problems/999999999")
    assert_response("GET /api/problems/999999999", result, 404, False, "not found")

    test_user = f"api_test_{int(time.time())}_{os.getpid()}"
    user_cookies = http.cookiejar.CookieJar()
    admin_cookies = http.cookiejar.CookieJar()

    register_body = {"username": test_user, "password": "password"}
    result = request_json(base_url, "POST", "/api/user/register", register_body)
    assert_response("POST /api/user/register", result, 201, True, "registered")
    assert_body_contains("POST /api/user/register", result, f'"username":"{test_user}"')

    result = request_json(base_url, "POST", "/api/user/register", register_body)
    assert_response("POST /api/user/register duplicate", result, 409, False, "username exists")

    result = request_json(
        base_url, "POST", "/api/user/register", {"username": "x", "password": "123"}
    )
    assert_response(
        "POST /api/user/register invalid",
        result,
        400,
        False,
        "invalid username or password",
    )

    result = request_json(
        base_url,
        "POST",
        "/api/user/login",
        {"username": "user1", "password": "wrong-password"},
    )
    assert_response(
        "POST /api/user/login wrong password",
        result,
        401,
        False,
        "invalid username or password",
    )

    result = request_json(
        base_url,
        "POST",
        "/api/user/login",
        {"username": "user1", "password": "password"},
        user_cookies,
    )
    assert_response("POST /api/user/login", result, 200, True, "logged in")
    assert_body_contains("POST /api/user/login", result, '"username":"user1"')
    assert_cookie(user_cookies, "oj_user_session", "POST /api/user/login")

    result = request_json(base_url, "GET", "/api/user/me", cookie_jar=user_cookies)
    assert_response("GET /api/user/me logged in", result, 200, True, "ok")
    assert_body_contains("GET /api/user/me logged in", result, '"logged_in":true')

    accepted_code = (
        "#include <bits/stdc++.h>\n"
        "using namespace std;\n"
        "int main(){int a,b;if(cin>>a>>b){cout<<a+b<<endl;}return 0;}\n"
    )
    assert_submit_result(
        base_url,
        user_cookies,
        "POST /api/submit accepted code",
        1,
        accepted_code,
        "accepted",
        "passed",
        "accepted",
    )

    wrong_code = (
        "#include <bits/stdc++.h>\n"
        "using namespace std;\n"
        "int main(){cout<<0<<endl;return 0;}\n"
    )
    assert_submit_result(
        base_url,
        user_cookies,
        "POST /api/submit wrong answer",
        1,
        wrong_code,
        "wrong_answer",
        "failed",
        "wrong_answer",
    )

    assert_submit_result(
        base_url,
        user_cookies,
        "POST /api/submit compile error",
        1,
        "int main( {",
        "compile_error",
        "failed",
        "compile_error",
    )

    assert_submit_result(
        base_url,
        user_cookies,
        "POST /api/submit empty code",
        1,
        "",
        "compile_error",
        "failed",
        "compile_error",
    )

    assert_submit_result(
        base_url,
        user_cookies,
        "POST /api/submit missing problem",
        999999999,
        "int main(){return 0;}",
        "system_error",
        "failed",
        "system_error",
    )

    strict_missing_newline_code = (
        "#include <bits/stdc++.h>\n"
        "using namespace std;\n"
        "int main(){int a,b;if(cin>>a>>b){cout<<a+b;}return 0;}\n"
    )
    assert_submit_result(
        base_url,
        user_cookies,
        "POST /api/submit strict newline mismatch",
        1,
        strict_missing_newline_code,
        "wrong_answer",
        "failed",
        "wrong_answer",
    )

    float_one_accepted_code = (
        "#include <bits/stdc++.h>\n"
        "using namespace std;\n"
        "int main(){int n;if(!(cin>>n))return 0;double sum=0,x;"
        "for(int i=0;i<n;i++){cin>>x;sum+=x;}"
        "cout<<fixed<<setprecision(2)<<sum/n<<'\\n';return 0;}\n"
    )
    assert_submit_result(
        base_url,
        user_cookies,
        "POST /api/submit float_1 accepted",
        2,
        float_one_accepted_code,
        "accepted",
        "passed",
        "accepted",
    )

    float_one_rejected_code = (
        "#include <bits/stdc++.h>\n"
        "using namespace std;\n"
        "int main(){int n;if(!(cin>>n))return 0;double sum=0,x;"
        "for(int i=0;i<n;i++){cin>>x;sum+=x;}"
        "cout<<fixed<<setprecision(2)<<(sum/n+0.06)<<'\\n';return 0;}\n"
    )
    assert_submit_result(
        base_url,
        user_cookies,
        "POST /api/submit float_1 mismatch",
        2,
        float_one_rejected_code,
        "wrong_answer",
        "failed",
        "wrong_answer",
    )

    timeout_code = (
        "#include <cstdint>\n"
        "int main(){volatile std::uint64_t x=0;while(true){++x;}return 0;}\n"
    )
    assert_submit_result(
        base_url,
        user_cookies,
        "POST /api/submit timeout",
        1,
        timeout_code,
        "time_limit_exceeded",
        "failed",
        "time_limit_exceeded",
        timeout=15.0,
    )

    memory_limit_code = (
        "#include <iostream>\n"
        "#include <vector>\n"
        "int main(){std::vector<char> data(300*1024*1024);"
        "std::cout<<data.size()<<'\\n';return 0;}\n"
    )
    assert_submit_result(
        base_url,
        user_cookies,
        "POST /api/submit memory limit",
        1,
        memory_limit_code,
        "memory_limit_exceeded",
        "failed",
        "memory_limit_exceeded",
        timeout=15.0,
    )

    output_limit_code = (
        "#include <iostream>\n"
        "#include <string>\n"
        "int main(){std::cout<<std::string(1100000,'x');return 0;}\n"
    )
    assert_submit_result(
        base_url,
        user_cookies,
        "POST /api/submit output limit",
        1,
        output_limit_code,
        "output_limit_exceeded",
        "failed",
        "output_limit_exceeded",
        timeout=15.0,
    )

    result = request_json(base_url, "POST", "/api/user/logout", {}, user_cookies)
    assert_response("POST /api/user/logout logged in", result, 200, True, "logged out")

    result = request_json(base_url, "GET", "/api/user/me", cookie_jar=user_cookies)
    assert_response("GET /api/user/me after logout", result, 200, True, "ok")
    assert_body_contains("GET /api/user/me after logout", result, '"logged_in":false')

    result = request_json(
        base_url,
        "POST",
        "/api/admin/login",
        {"username": "admin", "password": "wrong-password"},
    )
    assert_response(
        "POST /api/admin/login wrong password",
        result,
        401,
        False,
        "invalid username or password",
    )

    result = request_json(
        base_url,
        "POST",
        "/api/admin/login",
        {"username": "admin", "password": "password"},
        admin_cookies,
    )
    assert_response("POST /api/admin/login", result, 200, True, "logged in")
    assert_body_contains("POST /api/admin/login", result, '"username":"admin"')
    assert_cookie(admin_cookies, "oj_admin_session", "POST /api/admin/login")

    result = request_json(base_url, "GET", "/api/admin/me", cookie_jar=admin_cookies)
    assert_response("GET /api/admin/me logged in", result, 200, True, "ok")
    assert_body_contains("GET /api/admin/me logged in", result, '"logged_in":true')

    result = request_json(
        base_url, "POST", "/api/admin/problems", {"title": "Missing fields"}, admin_cookies
    )
    assert_response("POST /api/admin/problems invalid", result, 400, False, "invalid problem")

    no_hidden_body = {
        "title": f"No Hidden Problem {test_user}",
        "difficulty": "easy",
        "description": "Invalid fixture without hidden testcases.",
        "input_format": "No input.",
        "output_format": "One integer.",
        "sample_input": "",
        "sample_output": "0\n",
        "time_limit_ms": 1000,
        "memory_limit_kb": 131072,
        "compare_mode": "strict",
        "samples": [{"input": "", "expected_output": "0\n"}],
        "hidden_testcases": [],
    }
    result = request_json(
        base_url, "POST", "/api/admin/problems", no_hidden_body, admin_cookies
    )
    assert_response(
        "POST /api/admin/problems empty hidden testcases",
        result,
        400,
        False,
        "invalid problem",
    )

    admin_title = f"Curl Admin Problem {test_user}"
    create_body = {
        "title": admin_title,
        "difficulty": "easy",
        "description": "Read two integers and output their sum.",
        "input_format": "Two integers.",
        "output_format": "One integer.",
        "sample_input": "2 3\n",
        "sample_output": "5\n",
        "time_limit_ms": 1000,
        "memory_limit_kb": 131072,
        "compare_mode": "strict",
        "samples": [{"input": "2 3\n", "expected_output": "5\n"}],
        "hidden_testcases": [
            {"input": "7 8\n", "expected_output": "15\n"},
            {"input": "-2 5\n", "expected_output": "3\n"},
        ],
    }
    result = request_json(base_url, "POST", "/api/admin/problems", create_body, admin_cookies)
    assert_response("POST /api/admin/problems", result, 201, True, "created")
    assert_body_contains("POST /api/admin/problems", result, f'"title":"{admin_title}"')
    created_problem_id = None
    if isinstance(result.data, dict) and isinstance(result.data.get("data"), dict):
        created_problem_id = result.data["data"].get("id")
    if not isinstance(created_problem_id, int):
        fail(f"POST /api/admin/problems should return created problem id; body: {result.body}")

    result = request_json(base_url, "GET", "/api/problems")
    assert_response("GET /api/problems after admin create", result, 200, True, "ok")
    assert_body_contains(
        "GET /api/problems after admin create", result, f'"title":"{admin_title}"'
    )

    result = request_json(base_url, "GET", f"/api/problems/{created_problem_id}")
    assert_response("GET /api/problems/{created}", result, 200, True, "ok")
    assert_body_contains("GET /api/problems/{created}", result, f'"title":"{admin_title}"')
    assert_body_contains("GET /api/problems/{created}", result, '"input":"2 3\\n"')
    assert_body_not_contains("GET /api/problems/{created} hidden testcase", result, "7 8")

    sample_only_code = (
        "#include <iostream>\n"
        "int main(){std::cout << 5 << '\\n'; return 0;}\n"
    )
    assert_submit_result(
        base_url,
        admin_cookies,
        "POST /api/submit hidden testcase mismatch",
        created_problem_id,
        sample_only_code,
        "wrong_answer",
        "failed",
        "wrong_answer",
    )

    result = request_json(
        base_url, "DELETE", f"/api/admin/problems/{created_problem_id}", cookie_jar=admin_cookies
    )
    assert_response("DELETE /api/admin/problems/{created}", result, 200, True, "deleted")
    assert_body_contains("DELETE /api/admin/problems/{created}", result, '"deleted":true')

    result = request_json(base_url, "GET", f"/api/problems/{created_problem_id}")
    assert_response(
        "GET /api/problems/{created} after delete", result, 404, False, "not found"
    )

    result = request_json(base_url, "GET", "/api/problems")
    assert_response("GET /api/problems after admin delete", result, 200, True, "ok")
    assert_body_not_contains(
        "GET /api/problems after admin delete", result, f'"title":"{admin_title}"'
    )

    result = request_json(
        base_url, "DELETE", f"/api/admin/problems/{created_problem_id}", cookie_jar=admin_cookies
    )
    assert_response(
        "DELETE /api/admin/problems/{created} again", result, 404, False, "not found"
    )

    result = request_json(base_url, "POST", "/api/admin/logout", {}, admin_cookies)
    assert_response("POST /api/admin/logout logged in", result, 200, True, "logged out")

    result = request_json(base_url, "GET", "/api/admin/me", cookie_jar=admin_cookies)
    assert_response("GET /api/admin/me after logout", result, 200, True, "ok")
    assert_body_contains("GET /api/admin/me after logout", result, '"logged_in":false')


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run HTTP API automation tests with the Python standard library."
    )
    parser.add_argument("config_path", nargs="?", default="config/app.conf")
    parser.add_argument("--basic", action="store_true", help="run non-DB checks only")
    parser.add_argument("--full", action="store_true", help="run full API checks")
    parser.add_argument(
        "--no-start",
        action="store_true",
        help="use OJ_API_BASE_URL or config-derived URL instead of starting a server",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    mode = "basic" if args.basic else "full"
    if args.full:
        mode = "full"

    config_path = (ROOT_DIR / args.config_path).resolve()
    base_url = os.environ.get("OJ_API_BASE_URL", "")
    process: subprocess.Popen[bytes] | None = None

    if not base_url:
        host = config_value(config_path, "server.host", "127.0.0.1")
        port = config_value(config_path, "server.port", "8080")
        if host == "0.0.0.0":
            host = "127.0.0.1"
        base_url = f"http://{host}:{port}"

    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = Path(tmp)
        try:
            if not args.no_start:
                base_url, process = start_server(config_path, tmp_dir)

            if mode == "basic":
                run_basic_checks(base_url)
                print("SKIP: DB-backed API checks were not run in --basic mode")
            else:
                run_full_checks(base_url)
        finally:
            stop_server(process)


if __name__ == "__main__":
    main()
