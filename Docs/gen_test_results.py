# -*- coding: utf-8 -*-
"""从 AutoTest.log 重新生成 Docs/TestResults.md 的结果部分。

这个文件自称「由 AutoTest.log 生成，非手写」——那就让它名副其实。
手改数字的问题是：改完之后没人能确认表里那一百多行还对不对得上。

⚠️ 只覆盖「最近一次运行」与结果表，前后的说明文字原样保留。

用法（在仓库根目录）：
    python Docs/gen_test_results.py            # 预览
    python Docs/gen_test_results.py --write
"""
import io
import os
import re
import sys

LOG = r"UnrealProject\Saved\Logs\AutoTest.log"
DOC = r"Docs\TestResults.md"

BEGIN = "## 最近一次运行"
END = "## 前端测试（WebReplay）"


def say(msg):
    enc = sys.stdout.encoding or "utf-8"
    sys.stdout.write(msg.encode(enc, errors="replace").decode(enc) + "\n")


def parse_log(path):
    """返回 (测试列表, 通过数, 失败数, 退出码, 耗时秒)。"""
    raw = io.open(path, encoding="utf-8", errors="replace").read()

    # Result={成功} / Result={Fail} —— 语言随编辑器区域设置变，故只判断「是不是失败」
    pattern = re.compile(
        r"Test Completed\. Result=\{(?P<result>[^}]*)\} Name=\{[^}]*\} Path=\{(?P<path>[^}]*)\}")
    tests = []
    for m in pattern.finditer(raw):
        result = m.group("result")
        failed = ("Fail" in result) or ("失败" in result)
        tests.append((m.group("path"), not failed))

    exit_code = None
    m = re.search(r"TEST COMPLETE\. EXIT CODE: (-?\d+)", raw)
    if m:
        exit_code = int(m.group(1))

    stamps = re.findall(r"^\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2})", raw, re.M)
    span = None
    if len(stamps) >= 2:
        span = (stamps[0], stamps[-1])

    passed = sum(1 for _, ok in tests if ok)
    return tests, passed, len(tests) - passed, exit_code, span


def build_section(tests, passed, failed, exit_code, mtime):
    lines = [BEGIN, "", "| 项 | 值 |", "|---|---|"]
    lines.append("| 运行时间 | %s |" % mtime)
    verdict = "**%d / %d 通过**" % (passed, len(tests))
    if failed:
        verdict += "　⚠️ **%d 个失败**" % failed
    lines.append("| 结果 | %s |" % verdict)
    lines.append("| 退出码 | %s |" % exit_code)
    lines.append("| 运行依赖 | 无需 World / PIE / 网络（被测对象为纯函数/纯逻辑）|")
    lines.append("")
    lines.append("覆盖范围：画像分析器（链路②）· 规则解析器（链路⑥）· 四道护栏（链路⑤）·")
    lines.append("本地 Provider（链路④）· **三级降级（链路④的失败路径）**。")
    lines.append("")
    lines.append("| 结果 | 测试（路径已去掉公共前缀 `SHM.`） |")
    lines.append("|---|---|")
    for path, ok in sorted(tests):
        short = path[4:] if path.startswith("SHM.") else path
        lines.append("| %s | `%s` |" % ("PASS" if ok else "**FAIL**", short))
    lines.append("")
    return "\n".join(lines)


def main():
    write = "--write" in sys.argv
    if not os.path.exists(LOG):
        say("找不到日志：%s —— 先跑一次 headless 测试" % LOG)
        return 1

    tests, passed, failed, exit_code, span = parse_log(LOG)
    if not tests:
        say("日志里一个 Test Completed 都没有 —— 这次测试根本没跑起来（见台账 #23/#35）")
        return 1

    import datetime
    mtime = datetime.datetime.fromtimestamp(os.path.getmtime(LOG)).strftime("%Y-%m-%d %H:%M")

    say("解析到 %d 个测试：%d 通过 / %d 失败，退出码 %s" % (len(tests), passed, failed, exit_code))
    say("日志时间：%s（日志范围 %s）" % (mtime, span))

    doc = io.open(DOC, encoding="utf-8").read()
    i, j = doc.find(BEGIN), doc.find(END)
    if i < 0 or j < 0 or j < i:
        say("TestResults.md 结构对不上（找不到起止锚点），未做任何修改")
        return 1

    new_doc = doc[:i] + build_section(tests, passed, failed, exit_code, mtime) + "\n" + doc[j:]

    if not write:
        say("\n（预览模式，加 --write 才写回）")
        return 0

    io.open(DOC, "w", encoding="utf-8").write(new_doc)
    say("已写回 %s" % DOC)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
