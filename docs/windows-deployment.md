# Windows 部署与 Qt DLL 排障

> **当前实现**：`SmartMate` 仅使用 Qt Widgets。发布脚本会验证纯 Widgets 运行时闭包，并拒绝意外出现的 QML/Qt Quick 文件。

## 1. 为什么会提示缺少 DLL

CMake 构建得到的 `SmartMate.exe` 只包含 SmartMate 自身代码。Qt 默认使用动态链接，程序启动时还要加载：

- Qt 基础库，如 `Qt6Core.dll`、`Qt6Gui.dll`、`Qt6Widgets.dll` 和 `Qt6Sql.dll`；
- Qt 平台插件，如 `platforms/qwindows.dll`；
- SQLite 运行库与数据库驱动，如 `Qt6Sql.dll` 和 `sqldrivers/qsqlite.dll`；
- MinGW 运行库，如 `libgcc_s_seh-1.dll`、`libstdc++-6.dll` 和 `libwinpthread-1.dll`。

因此，把 `build/release/SmartMate.exe` 单独复制到桌面或另一台电脑运行，出现“找不到某个 DLL”是正常的部署问题，不代表源码或 MVVM 架构有错误。

## 2. 开发期间如何运行

推荐从使用 Qt 6.10.2 MinGW 13.1 Kit 的 Qt Creator 启动。若从 PowerShell 启动，应先把匹配的运行库加入当前会话：

```powershell
$env:QT_ROOT='D:/Qt/6.10.2/mingw_64'
$env:QT_MINGW_ROOT='D:/Qt/Tools/mingw1310_64'
$env:PATH="$env:QT_ROOT/bin;$env:QT_MINGW_ROOT/bin;$env:PATH"

.\build\debug\SmartMate.exe
```

这只适合本机开发，因为程序仍依赖 `D:/Qt` 中安装的文件。

## 3. 正确的 Release 发布方法

仓库中的部署脚本只构建正式 `SmartMate`，再调用同一 Qt 安装目录下的 `windeployqt.exe`，复制所需 Qt Widgets DLL、平台插件与 MinGW 运行库：

```powershell
$env:QT_ROOT='D:/Qt/6.10.2/mingw_64'
$env:QT_MINGW_ROOT='D:/Qt/Tools/mingw1310_64'

powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\deploy.ps1 -Configuration Release
```

`-ExecutionPolicy Bypass` 只作用于这一次新启动的 PowerShell 进程，不会修改系统或用户的永久执行策略。如果当前终端本来就允许本地脚本，也可以直接运行 `.\scripts\deploy.ps1`。

结果位于：

```text
dist/SmartMate/
  SmartMate.exe
  Qt6Core.dll
  Qt6Gui.dll
  Qt6Widgets.dll
  Qt6Sql.dll
  ...
  platforms/
    qwindows.dll
  sqldrivers/
    qsqlite.dll
```

分发时应压缩并复制整个 `dist/SmartMate` 目录，不能只发送 `SmartMate.exe`。`dist/` 是本地生成物，已被 `.gitignore` 排除，不应提交到源码仓库。

Debug 部署也可用于排障：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\deploy.ps1 -Configuration Debug
```

但给教师或其他电脑演示时应使用 Release 版本，因为 Debug 版本包含调试信息、体积更大。脚本会检测 Qt 调试运行库；当前 Qt MinGW 安装未提供带 `d` 后缀的 DLL，因此 Debug 构建会自动配套使用同一 Kit 的 Release Qt 运行库。

## 4. 为什么不能手工逐个复制 DLL

只按错误提示复制 `Qt6Core.dll`，下一次启动通常还会继续缺少其他库或插件；复制到错误版本的同名 DLL 还可能导致入口点不存在、程序崩溃或 ABI 不兼容。

`windeployqt` 会扫描正式可执行文件的链接依赖，按当前 Qt Kit 复制相互匹配的运行库。项目脚本还使用 `--compiler-runtime` 部署 MinGW 运行库，因此它比手工复制可靠，也便于在答辩前重复生成干净发布包。脚本不会传递 `--qmldir` 或 `--qmlimport`。

不要从互联网 DLL 下载网站获取文件，也不要把 DLL 复制到 `C:/Windows/System32`。这些做法会污染系统环境，并可能带来版本冲突和安全风险。

## 5. 常见错误与处理

### “找不到 Qt6Core.dll / Qt6Widgets.dll”

原因：运行了未部署的构建目录可执行文件，且 Qt 的 `bin` 不在 `PATH`。

处理：开发时设置本页第 2 节的 `PATH`；分发时运行 `deploy.ps1` 并从 `dist/SmartMate` 启动。

### “找不到 libgcc_s_seh-1.dll / libstdc++-6.dll / libwinpthread-1.dll”

原因：缺少 MinGW 运行库，或使用了错误的 MinGW 版本。

处理：确认 `QT_MINGW_ROOT` 为 `D:/Qt/Tools/mingw1310_64`，重新执行部署脚本。不要使用全局 MinGW 15.1 的 DLL。

### “no Qt platform plugin could be initialized”或找不到 `windows` 平台插件

原因：`platforms/qwindows.dll` 缺失、目录层级被破坏，或插件与 Qt DLL 版本不一致。

处理：确认以下路径存在，并保持脚本生成的目录结构：

```text
dist/SmartMate/platforms/qwindows.dll
```

删除旧的 `dist/SmartMate` 后重新运行部署脚本。脚本本身会安全地重建该目录。

### 发布目录意外出现 Qt6Qml、Qt6Quick 或 `qml/`

原因：部署了旧 QML 基线，或在未清理的目录上手工覆盖文件。

处理：重新运行项目脚本。脚本会安全重建 `dist/SmartMate`，并在发现任何 QML/Qt Quick 运行库时直接失败。

### “QSQLITE driver not loaded”

原因：发布目录缺少 SQLite 驱动，或 `sqldrivers` 目录层级被破坏。

处理：重新运行部署脚本，并确认 `dist/SmartMate/sqldrivers/qsqlite.dll` 与 `Qt6Sql.dll` 同时存在。不要把驱动 DLL 移到可执行文件同级目录。

### “The procedure entry point ... could not be located”或启动即崩溃

原因：同一目录中混入了其他 Qt 或 MinGW 版本的 DLL，或 Debug/Release 文件混用。

处理：不要在原目录上手工覆盖。重新执行对应配置的部署脚本，让它清理并完整生成 `dist/SmartMate`。

## 6. 发布前验证

1. 运行 Release 部署脚本，确认必需 DLL 检查和离屏冒烟测试均通过。
2. 确认发布目录不存在 `Qt6Qml*`、`Qt6Quick*`、`Qt6QuickControls2*` 或 `qml/`。
3. 从 `dist/SmartMate/SmartMate.exe` 启动，而不是从 `build/` 启动。
4. 最可靠的最终检查是在一台没有安装 Qt 的 Windows 10/11 机器或虚拟机中复制整个目录并运行。
5. 若程序可以启动但目标机器缺少 Microsoft 系统组件，应安装官方 Windows 更新或 Microsoft 运行时，不能使用第三方 DLL 下载站补文件。

Qt 的 LGPL 等许可要求也应在正式发布前单独核对。本课程演示包应保留 Qt 动态库，不要把 Qt 源码或来路不明的 DLL 混入仓库。
