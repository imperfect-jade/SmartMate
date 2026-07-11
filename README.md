# SmartMate

SmartMate 是一个仅面向 Windows 的桌面任务规划应用，使用 C++20、Qt 6、Qt Quick/QML、SQLite 与严格 MVVM 架构开发。项目以“可执行的任务计划 + 陪伴式桌宠”为核心：用户管理任务与依赖关系，系统生成合理的执行顺序，桌宠则随时展示当前任务并提供快捷交互。

> 当前版本已经完成基础任务 CRUD、自动排序、推荐理由、实时搜索和优先级筛选。任务可以创建、查看、编辑、软归档和恢复，并通过 SQLite 在重启后保留；依赖规划、桌宠、专注计时和统计仍按后续阶段逐步实现。

## 主要功能规划

- 任务管理（已实现）：新建、查看、编辑、归档、恢复任务，并维护状态、优先级、截止时间和预计用时；日期时间与时长通过类型化选择器填写，避免格式错误。
- 查询与整理（已实现）：实时搜索标题或描述，按单个优先级筛选；Model 自动生成稳定顺序和推荐理由，ViewModel 只生成当前列表投影。搜索和筛选条件不会持久化。
- 任务依赖：建立任务的前后置关系，拒绝自依赖、重复依赖和循环依赖，自动判断任务是否可执行。
- 智能规划（后续增强）：加入任务依赖后，在不破坏依赖顺序的前提下综合逾期情况、优先级和截止时间生成推荐执行顺序。
- 桌面宠物：以第二个 View 展示当前任务，可快捷开始或完成任务，并与主窗口实时同步。
- 专注计时：针对当前任务开始、暂停、继续和结束专注会话，同一时刻最多一个活动会话。
- 数据统计：汇总今日/本周完成任务数、专注时长，以及预计用时与实际用时。

更完整的功能范围、业务规则与开发优先级见[项目功能说明](docs/project-overview.md)。

## 为什么采用 MVVM

本项目的主要学习目标是理解 MVVM，因此依赖方向被严格固定为：

```text
View（QML） → ViewModel → Model Service → Repository 接口
                                            ↑
                                      持久化实现
```

- View 只负责布局、绑定、动画和转发用户事件。
- ViewModel 负责可观察的展示状态、命令和错误信息映射。
- Model 负责业务规则、状态转换、依赖算法、规划算法和统计公式。
- `src/app` 是唯一的对象组合根，负责创建具体实现并完成依赖注入。

这样可以让任务列表、桌宠和统计页面以不同方式展示同一份业务状态，同时保持算法和数据库代码可独立测试。详细约束见[架构说明](docs/architecture.md)与[仓库开发规则](AGENTS.md)。

## 技术环境

- C++20
- Qt 6.10.2（Qt Quick/QML）
- Qt 自带的 MinGW 13.1：`D:/Qt/Tools/mingw1310_64`
- CMake 3.24 或更高版本
- Windows 10/11

必须使用 Qt 配套的 MinGW 13.1，不能使用系统中无关的 MinGW 15.1，否则可能发生 ABI 不兼容。首次打开 PowerShell 后设置：

```powershell
$env:QT_ROOT='D:/Qt/6.10.2/mingw_64'
$env:QT_MINGW_ROOT='D:/Qt/Tools/mingw1310_64'
```

## 构建与验证

Debug 构建：

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
cmake --build --preset debug --target all_qmllint
```

Release 构建使用对应的 `release` preset：

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

所有构建产物都位于 `build/`，不会被 Git 跟踪。CTest 包含一个离屏启动测试，用于确认静态 QML 模块能够加载、C++ 创建的 `AppViewModel` 能够注入根 QML 对象，并且必需属性已正确初始化。

正式运行时，任务数据库保存在 Windows 当前用户的本地应用数据目录。测试与离屏启动使用内存数据库，不会写入真实任务数据。SQLite 只由 Repository 实现访问，ViewModel 和 QML 都不能直接执行 SQL。

## 运行与发布

构建目录中的 `SmartMate.exe` 依赖 Qt DLL、QML 插件和 MinGW 运行库。在开发环境运行时，Qt 与 MinGW 的 `bin` 目录必须在 `PATH` 中；向其他电脑分发时，应使用项目脚本生成完整发布目录：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\deploy.ps1 -Configuration Release
```

部署结果位于 `dist/SmartMate/`。请分发整个目录，不能只复制 `SmartMate.exe`。详细说明和常见错误见[Windows 部署与 DLL 排障](docs/windows-deployment.md)。

## 文档索引

- [项目功能说明](docs/project-overview.md)：产品定位、主要功能、业务规则、开发阶段与演示场景。
- [MVVM 架构说明](docs/architecture.md)：各层职责、目标依赖与对象创建流程。
- [Windows 部署与 DLL 排障](docs/windows-deployment.md)：开发期运行、发布打包和缺失 DLL 的解决方法。
- [仓库开发规则](AGENTS.md)：所有后续修改必须遵守的强制边界。
