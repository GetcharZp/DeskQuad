# SCons 在运行时注入 Import 与 env，静态检查看不到这两个名字
# ruff: noqa: F821
# pyright: reportUndefinedVariable=false
"""构建 LittleFS 之前自动重建前端。

web/dist 是 Vite 的构建产物，同时作为 LittleFS 根目录。这里比较
dist/index.html 与前端源码的时间戳，过期时才执行 npm run build，
避免 pio run -t uploadfs 把旧页面刷进设备。

设置 DESKQUAD_SKIP_WEB_BUILD=1 可跳过本次自动构建。
"""

import locale
import os
import shutil
import subprocess
import sys

Import("env")

WEB_DIR = os.path.join(env.subst("$PROJECT_DIR"), "web")
DIST_INDEX = os.path.join(WEB_DIR, "dist", "index.html")

# 参与新旧判断的源码与配置
WATCHED = (
    "src",
    "index.html",
    "vite.config.js",
    "package.json",
    "package-lock.json",
    "scripts",
)

# 只有这些目标会把 web/dist 打进 LittleFS
FS_TARGETS = {"buildfs", "uploadfs", "uploadfsota"}


def newest_mtime(path):
    """返回文件或目录树中最新修改时间。"""
    if os.path.isfile(path):
        return os.path.getmtime(path)
    newest = 0.0
    for root, dirs, files in os.walk(path):
        dirs[:] = [name for name in dirs if name not in ("node_modules", "dist")]
        for name in files:
            newest = max(newest, os.path.getmtime(os.path.join(root, name)))
    return newest


def build_web():
    if os.environ.get("DESKQUAD_SKIP_WEB_BUILD") == "1":
        print("[web] DESKQUAD_SKIP_WEB_BUILD=1, skip frontend build")
        return

    sources = [os.path.join(WEB_DIR, item) for item in WATCHED]
    if os.path.isfile(DIST_INDEX) and os.path.getmtime(DIST_INDEX) >= max(
        newest_mtime(p) for p in sources
    ):
        print("[web] dist is up to date, skip build")
        return

    if not os.path.isdir(os.path.join(WEB_DIR, "node_modules")):
        sys.stderr.write(
            "[web] web/node_modules missing, run `cd web && npm ci` first\n"
        )
        raise SystemExit(1)

    if shutil.which("npm") is None:
        sys.stderr.write("[web] npm not found in PATH\n")
        raise SystemExit(1)

    print("[web] building frontend ...")
    if os.name == "nt":
        command = [os.environ.get("COMSPEC", "cmd.exe"), "/c", "npm", "run", "build"]
    else:
        command = ["npm", "run", "build"]

    # 失败时自行回显日志，不用 check 抛异常
    completed = subprocess.run(
        command,
        check=False,
        cwd=WEB_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if completed.returncode == 0:
        print("[web] frontend build done")
        return

    # 失败时才回显构建日志；按控制台编码转一次，避免 Vite 的 ✓ 等字符
    # 在 Windows GBK 控制台上触发 UnicodeEncodeError
    text = completed.stdout.decode("utf-8", errors="replace")
    encoding = locale.getpreferredencoding(False)
    sys.stdout.write(text.encode(encoding, errors="replace").decode(encoding))
    sys.stdout.flush()
    raise SystemExit(completed.returncode)


# extra_scripts 在目标解析阶段执行，此时 COMMAND_LINE_TARGETS 已经可用。
# 直接在这里构建可以保证镜像打包时 dist 是最新的；如果把动作挂到 buildfs
# 别名上，它会在镜像生成之后才执行。
if FS_TARGETS & set(globals().get("COMMAND_LINE_TARGETS") or []):
    build_web()
