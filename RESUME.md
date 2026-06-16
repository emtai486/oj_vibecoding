# 项目名称

仿 LeetCode 在线判题系统 OJ

# 项目简介

基于 C++20、cpp-httplib、MySQL 8 和原生 HTML/CSS/JavaScript 实现的轻量级 Online Judge 系统，支持用户浏览题目、在线编辑 C++ 代码、提交代码并获得判题结果，同时提供管理员后台进行题目新增和删除。项目覆盖后端 API、数据库持久化、用户/管理员认证、在线判题、前端交互、自动化测试和部署验收完整流程。

# 技术栈

**技术栈：** C++20、cpp-httplib、MySQL 8、HTML/CSS/JavaScript、CodeMirror、Linux 进程控制、Makefile/Bash

# 具体罗列

- 使用 C++20 和 cpp-httplib 搭建单体 Web 服务，提供题目、用户、管理员和提交判题 API，并挂载原生 HTML/CSS/JS 前端页面。
- 设计 MySQL 表结构和 Repository 层，持久化用户、管理员、题目、样例测试用例和隐藏测试用例，并封装连接池提升数据库访问稳定性。
- 实现基于 Cookie session 的普通用户/管理员认证体系，密码使用 PBKDF2-SHA256 哈希存储。
- 实现 C++ 在线判题引擎，支持代码落盘、`g++` 编译、进程运行、stdin/stdout 交互、隐藏用例判题、并发限制和临时目录清理。
- 通过 Linux `fork/exec/rlimit` 实现时间、内存和输出大小限制，返回 Accepted、Wrong Answer、Compile Error、TLE、MLE、OLE 等细分状态。
- 建立 Makefile、Bash 部署脚本、curl/Python 接口回归和 Playwright Web 回归测试，完成 Ubuntu 22.04 环境部署验收。
