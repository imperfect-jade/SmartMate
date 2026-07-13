# SmartMate

SmartMate 是一个仅面向 Windows 的桌面任务规划应用，使用 C++20、Qt 6、Qt Quick/QML、SQLite 与严格 MVVM 架构开发。项目以“可执行的任务计划 + 陪伴式桌宠”为核心：用户管理任务与依赖关系，系统生成合理的执行顺序，桌宠则随时展示当前任务并提供快捷交互。

> 当前版本已经完成基础任务 CRUD、用户自定义类别、Finish-to-Start依赖、原子新建依赖、有向依赖图、依赖感知排序、推荐理由、实时搜索和组合筛选。主窗口提供可折叠左侧导航、“现在做”任务槽、逾期提醒、卡片详情以及可持久化的青绿/清蓝与字体设置。任务、类别与依赖通过 SQLite 保留，外观偏好通过 persistence 层的 QSettings 保留；桌宠、专注计时和统计仍按后续阶段逐步实现。

主界面当前采用完整的柔和染色主题：青绿主题参考春晨、碧山与秩色一组自然绿，清蓝主题使用同等层级的浅蓝表面；背景、导航、卡片、输入区和边框会整体切换，正文始终保持深色高对比度。可执行任务可通过专用手柄真实拖入“现在做”槽开始，按钮操作仍作为键盘与无障碍替代入口。

新建与编辑任务采用响应式分区表单，在 900×620 窗口和 110% 字号下只进行纵向滚动；日期、时长和前置任务子弹窗同样限制在窗口可用范围内。

## 主要功能规划

- 任务管理（已实现）：新建和查看任务；只有待办任务可以编辑普通字段，其他状态通过“开始、取消、完成、重做、归档、恢复”命令转换。支持按当前搜索与筛选结果多选任务，原子批量归档、恢复或永久删除；归档永久删除会同时清理关联依赖。
- 查询与整理（已实现）：实时搜索标题或描述，按单个优先级筛选；Model 自动生成稳定顺序和推荐理由，ViewModel 只生成当前列表投影。搜索和筛选条件不会持久化。
- 任务类别（已实现）：创建、重命名、换色和删除“学习”“工作”“旅游”等自定义类别；任务可以保持未分类，列表可与关键字、优先级组合筛选。删除类别只会把关联任务转为未分类，不改变状态或依赖。
- 任务依赖（已实现）：新建或编辑待办任务时选择一个或多个前置任务，拒绝非法端点、重复依赖和循环依赖，并显示直接阻塞原因。完成前置任务永久满足关系；取消前置任务会让关系暂时失效并解锁后继，重做后原关系重新生效。
- 有向依赖图（已实现）：从左向右显示全部活动任务及相关归档节点，以绿色、橙色和灰色箭头区分完成、待满足和取消关系；也可按类别仅显示核心任务及其直接跨类别上下文，支持平移、缩放、适应窗口和节点详情。
- 智能规划（已实现）：可执行任务优先，被阻塞任务按拓扑关系整理；前置任务完成或取消后自动解锁后继并显示可解锁数量；待办和进行中任务超过截止时间后显示独立逾期提醒。
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

这样可以让任务列表、依赖图、桌宠和统计页面以不同方式展示同一份业务状态，同时保持算法和数据库代码可独立测试。依赖图尤其体现分层：Model计算拓扑语义，ViewModel计算像素布局和箭头几何，QML只用`Shape`绘制。详细约束见[架构说明](docs/architecture.md)与[仓库开发规则](AGENTS.md)。

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
