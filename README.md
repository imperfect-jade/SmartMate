# SmartMate

SmartMate 是一个仅面向 Windows 的桌面任务规划应用，使用 C++20、Qt 6、SQLite 与严格 MVVM 架构开发。项目以“可执行的任务计划 + 陪伴式桌宠”为核心：用户管理任务与依赖关系，系统生成合理的执行顺序，桌宠则随时展示当前任务并提供快捷交互。

> **View 技术迁移状态**：纯 C++ Qt Widgets 已完成迁移并作为唯一正式前端。主窗口、外观设置、任务主流程、类别与依赖编辑及 `QGraphicsView/QGraphicsScene` 依赖图均只依赖窄 ViewModel Contracts；旧 QML、QuickTest、adapter 和 Qt Quick 构建依赖已删除。迁移记录见[Qt Widgets 迁移指南](docs/widgets-migration.md)。

> 当前版本已经完成基础任务 CRUD、用户自定义类别、Finish-to-Start依赖、原子新建依赖、有向依赖图、依赖感知排序、推荐理由、实时搜索、组合筛选、自由专注、首版任务统计仪表盘和三花猫桌宠。主窗口提供任务、依赖图、专注、统计、设置五个入口，以及“现在做”任务槽、逾期提醒、卡片详情和可持久化外观/桌宠设置。任务、类别、依赖、统计事件以及专注会话通过 SQLite Schema v5 保留；专注页面支持自由正计时、暂停、继续、完成、确认放弃、保存警告和最近 50 条记录。

主界面采用完整的柔和染色主题：青绿主题参考春晨、碧山与秩色一组自然绿，清蓝主题使用同等层级的浅蓝表面；背景、导航、卡片、输入区和边框会整体切换，正文始终保持深色高对比度。可执行任务可通过专用手柄真实拖入“现在做”槽开始，按钮操作仍作为键盘与无障碍替代入口。

新建与编辑任务采用响应式分区表单，在 900×620 窗口和 125% 字号下只进行纵向滚动；日期、时长和前置任务子弹窗同样限制在窗口可用范围内。

## 主要功能规划

- 任务管理（已实现）：新建和查看任务；只有待办任务可以编辑普通字段，其他状态通过“开始、取消、完成、重做、归档、恢复”命令转换。支持按当前搜索与筛选结果多选任务，原子批量归档、恢复或永久删除；归档永久删除会同时清理关联依赖。
- 查询与整理（已实现）：实时搜索标题或描述，按单个优先级筛选；Model 自动生成稳定顺序和推荐理由，ViewModel 只生成当前列表投影。搜索和筛选条件不会持久化。
- 任务类别（已实现）：创建、重命名、换色和删除“学习”“工作”“旅游”等自定义类别；任务可以保持未分类，列表可与关键字、优先级组合筛选。删除类别只会把关联任务转为未分类，不改变状态或依赖。
- 任务依赖（已实现）：新建或编辑待办任务时选择一个或多个前置任务，拒绝非法端点、重复依赖和循环依赖，并显示直接阻塞原因。完成前置任务永久满足关系；取消前置任务会让关系暂时失效并解锁后继，重做后原关系重新生效。
- 有向依赖图（已实现）：从左向右显示全部活动任务及相关归档节点，以绿色、橙色和灰色箭头区分完成、待满足和取消关系；也可按类别仅显示核心任务及其直接跨类别上下文，支持平移、缩放、适应窗口和节点详情。
- 智能规划（已实现）：可执行任务优先，被阻塞任务按拓扑关系整理；前置任务完成或取消后自动解锁后继并显示可解锁数量；待办和进行中任务超过截止时间后显示独立逾期提醒。
- 桌面宠物（已实现）：像素风“三花猫”在普通窗口时趴在右侧上边缘，主窗口最小化后切换为可拖动坐姿；复用当前焦点投影，可快捷开始、完成任务或恢复主窗口，并记忆跨屏悬浮位置。详细方案见[三花猫桌宠设计](docs/desktop-pet-design.md)。
- 专注计时（已实现）：Schema v5 和 `FocusService` 原子保存自由正计时会话、暂停时间段、检查点和任务快照，并在活动专注期间阻止任务完成或取消；纯 Widgets 专注页面只通过 `FocusContract` 执行开始、暂停、继续、完成和确认放弃，显示秒级计时、保存警告和最近 50 条记录。详细方案见[专注功能设计](docs/focus-design.md)。
- 数据统计（已实现）：Schema v4 任务事件、Model 聚合、Statistics Contracts/ViewModel 和纯 Widgets + Qt Charts 仪表盘已经接入正式导航；支持概览卡、近 7 天/30 天/12 周趋势、类别构成、任务健康、空状态、响应式滚动和任务状态变化后的自动刷新。详细方案见[数据统计设计](docs/statistics-design.md)。

更完整的功能范围、业务规则与开发优先级见[项目功能说明](docs/project-overview.md)。

## 为什么采用 MVVM

本项目的主要学习目标是理解 MVVM。目标架构在 View 与具体 ViewModel 之间增加抽象展示契约，依赖方向固定为：

```text
编译期：Qt Widgets View → ViewModel Contracts ← ViewModel实现 → Model Service
        Persistence → Repository接口

运行时：Widget事件 → Contract命令 → ViewModel → Model Service → Repository接口
        Service信号 → ViewModel投影 → Contract通知 → Widget绑定
```

- View 只依赖抽象 Contract，负责控件、布局、绘制、绑定和转发用户事件。
- ViewModel Contracts 定义稳定状态、Role、强类型命令和通知信号。
- 具体 ViewModel 负责可观察展示投影、输入草稿、命令转发和错误信息映射。
- Model 负责业务规则、状态转换、依赖算法、规划算法和统计公式。
- `src/app` 是唯一的对象组合根，负责创建具体实现并完成依赖注入。

这样可以让任务列表、依赖图、专注、桌宠和统计页面以不同方式展示同一份业务状态，同时保持算法和数据库代码可独立测试。依赖图继续遵循 Model 计算拓扑语义、ViewModel 计算像素布局和箭头几何、View 只绘制并转发稳定 ID；桌宠复用焦点与任务命令 Contracts，只为开关和位置建立独立设置切片；统计页面同样只消费 Contract 已聚合的数据，Qt Charts 不进入 Model 或 ViewModel；专注页面也只消费 `FocusContract`，不接触 Service 或 SQLite。详细约束见[架构说明](docs/architecture.md)、[三花猫桌宠设计](docs/desktop-pet-design.md)、[数据统计设计](docs/statistics-design.md)、[专注功能设计](docs/focus-design.md)与[仓库开发规则](AGENTS.md)。

## 技术环境

- C++20
- Qt 6.10.2（Core、Gui、Widgets、Charts、Sql、Test）
- Qt 自带的 MinGW 13.1：`D:/Qt/Tools/mingw1310_64`
- CMake 3.24 或更高版本
- Windows 10/11

`Qt6::Charts` 仅作为 `smartmate_widgets` 的私有 View 依赖，用于正式 StatisticsPage；项目没有引入 Qt Graphs、QML、Qt Quick、`QQuickWidget` 或 Charts QML 模块。

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
```

构建和运行正式 Widgets 前端：

```powershell
cmake --build --preset debug --target SmartMate
.\build\debug\SmartMate.exe
```

正式 `SmartMate` 不链接 Qt QML、Qt Quick 或任何 QML 插件。

Release 构建使用对应的 `release` preset：

```powershell
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

所有构建产物都位于 `build/`，不会被 Git 跟踪。默认 CTest 包含正式 `SmartMate` 的离屏启动测试、只注入 Fake Contract 的 Widget 测试，以及外观设置、任务主流程、类别/依赖编辑和依赖图的 Widgets 纵向集成测试。

正式运行时，任务数据库保存在 Windows 当前用户的本地应用数据目录。测试与离屏启动使用内存数据库，不会写入真实任务数据。SQLite 只由 Repository 实现访问，ViewModel 和任何 View 都不能直接执行 SQL。

## 运行与发布

正式 `SmartMate.exe` 依赖 Qt Core、Gui、Widgets、Sql、平台插件和 MinGW 运行库，但不依赖 QML 或 Qt Quick。在开发环境运行时，Qt 与 MinGW 的 `bin` 目录必须在 `PATH` 中；向其他电脑分发时，应使用项目脚本生成完整发布目录：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\deploy.ps1 -Configuration Release
```

部署结果位于 `dist/SmartMate/`。请分发整个目录，不能只复制 `SmartMate.exe`。详细说明和常见错误见[Windows 部署与 DLL 排障](docs/windows-deployment.md)。

## 文档索引

- [项目功能说明](docs/project-overview.md)：产品定位、主要功能、业务规则、开发阶段与演示场景。
- [MVVM 架构说明](docs/architecture.md)：各层职责、目标依赖与对象创建流程。
- [三花猫桌宠设计](docs/desktop-pet-design.md)：首版窗口状态、交互、位置持久化、Contract 边界、像素动画与验收要求。
- [数据统计设计](docs/statistics-design.md)：任务事件、统计口径、Qt Charts 展示边界与分阶段验收标准。
- [专注功能设计](docs/focus-design.md)：自由正计时规则、Schema v5 持久化事实、Repository 原子语义与后续阶段。
- [Qt Widgets 迁移指南](docs/widgets-migration.md)：已完成的迁移阶段、Contract 接口和最终清理记录。
- [Windows 部署与 DLL 排障](docs/windows-deployment.md)：开发期运行、发布打包和缺失 DLL 的解决方法。
- [仓库开发规则](AGENTS.md)：所有后续修改必须遵守的强制边界。
