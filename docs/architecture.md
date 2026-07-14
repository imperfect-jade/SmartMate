# SmartMate MVVM 架构说明

## 1. 架构目标

SmartMate 的 View 已完成纯 `.h/.cpp` Qt Widgets 迁移，旧 QML 前端、QuickTest、迁移 adapter 和 Qt Quick 构建依赖均已删除。切换只替换展示技术并收窄 View 与 ViewModel 的编译期依赖，不改变 Model 业务规则、Service API、Repository 事务语义、数据库格式或用户数据兼容性。

目标架构同时区分编译期依赖和运行时数据流：

```text
编译期：
Qt Widgets View → ViewModel Contracts ← ViewModel 实现 → Model Service
Persistence → Repository 接口

运行时：
Widget 事件 → Contract 命令 → ViewModel → Model Service → Repository 接口
Service 信号 → ViewModel 投影 → Contract 通知 → Widget 绑定
```

View 依赖 ViewModel 的抽象展示契约，而不是具体实现。该依赖倒置仍属于 MVVM：Contract 是 ViewModel 层公开给 View 的稳定边界，不是新的业务层、Controller 或消息总线。

`src/app` 是唯一组合根。它可以看见具体 Persistence、Service、ViewModel 和 View，负责对象创建、生命周期、依赖注入和主窗口启动；它不承担业务规则、展示投影或控件逻辑。

## 2. 最终实现约束

正式且唯一的 `SmartMate` 目标使用 Qt Widgets。仓库不得重新引入 `.qml`、`.ui`、Qt Quick、QuickTest、QML 注册 adapter 或旧迁移目标；完整迁移过程与清理结果记录在[Qt Widgets 迁移指南](widgets-migration.md)。

## 3. 各层职责

### 3.1 Common

`src/common` 只保存被多个非业务模块实际复用的 Qt Core 级通用类型。第一批允许的公开类型是 `UiSeverity` 与 `UiNotification`，用于表达展示级通知。

Common 不得包含：

- `Task`、`TaskId`、`TaskStatus`、`TaskCategoryId` 等业务类型；
- ViewModel Contract 或具体 ViewModel；
- QWidget、QDialog、QGraphicsItem 或其他 Qt Widgets 类型；
- SQL、QSettings、Repository、Service 或 Persistence；
- EventBus、Service Locator、全局业务单例或字符串路由。

`smartmate_common` 只链接 `Qt6::Core`。没有真实复用需求时不得为填充目录而增加工具类。

### 3.2 Model

Model 负责领域实体、输入校验、状态转换、任务依赖、规划算法、统计公式、服务编排与 Repository 接口。

- Domain 与 Service 可以使用 Qt Core 值类型。
- Domain、Service 和 Repository 接口不能依赖 View、ViewModel、ViewModel Contracts、Qt Widgets、Qt Quick、QML 运行时或 Qt SQL。
- `Task` 等领域对象保持普通 C++ 类型，不为了界面绑定继承 `QObject`。
- `TaskStateMachine` 集中定义开始、取消、完成、重做、归档和恢复；Service 执行附加约束，ViewModel 只投影命令资格。
- 逾期、阻塞、解锁和推荐顺序都由 Model 根据当前数据计算，不持久化派生结果。
- Finish-to-Start 依赖、循环检测、连接闭包、阻塞判断、自动解锁和拓扑整理均由纯 Model 算法完成。
- 业务规则必须能够在不启动任何图形界面的情况下测试。

SQLite 与 QSettings 的实现只位于 `src/model/persistence`。Persistence 只能实现 Repository 接口，不得被 ViewModel 或 View 直接调用。

### 3.3 ViewModel Contracts

`src/viewmodel/contracts` 定义 View 可以读取的状态、可以请求的命令以及可以观察的通知，不包含 Service 调用或具体实现。

- 列表契约继承抽象 `QAbstractListModel`，稳定定义 Role、状态 getter、强类型命令槽和通知信号。
- 其他契约继承抽象 `QObject`；具体 ViewModel 直接继承对应 Contract，禁止再多重继承另一个 `QObject`。
- Contract 边界使用 `QString` 表示稳定任务与类别 ID，不向 Widget 暴露领域实体或可变列表行号身份。
- 命令使用语义明确的强类型方法，例如 `startTask(const QString &taskId)`；禁止通用 `execute(QVariant)`、字符串命令名或反射路由。
- 命令资格通过属性或模型 Role 投影。View 只能绑定资格，Model Service 在执行时仍必须最终复核。
- 失败通过 `notificationRaised(const UiNotification &)` 等契约信号报告；成功通常由 Service 变化信号和投影刷新体现。
- 编辑器使用 `sessionActive` 等会话状态驱动显示与关闭，不得暴露 `openDialog`、`focusWidget` 等具体 View 控制命令。

Contracts 是 ViewModel 层的公开端口，不放入 Common。`smartmate_viewmodel_contracts` 只能依赖 `smartmate_common` 与 Qt Core，不得依赖领域 Model、具体 ViewModel、Model Service、Persistence 或 Qt Widgets。

### 3.4 ViewModel 实现

ViewModel 负责可观察展示状态、语义命令及其可用性、输入草稿、会话选择、搜索筛选和结构化错误的中文展示映射。

- 具体 ViewModel 实现对应 Contract，并只调用 Model Service。
- ViewModel 不得包含 QWidget、QDialog、QGraphicsScene、QGraphicsItem 等 View 类型，不得控制焦点、窗口、对话框或控件。
- ViewModel 不得调用具体 Persistence，不得包含 SQL、状态机、依赖算法、统计公式或重复业务校验。
- 所有 ViewModel 由 C++ 创建并持有；`AppViewModel` 可以拥有和协调子 ViewModel。
- 子 ViewModel 禁止直接调用。它们通过相同 Service 的变化信号独立刷新；只有确实跨投影的会话协调可以由 `AppViewModel` 完成。
- 搜索、筛选、批量选择、图选中和编辑草稿属于会话级展示状态，不写入 SQLite 或 QSettings。

`TaskDependencyViewModel` 只消费 `TaskService::taskDependencyEditContext` 给出的目标、现有选择、候选范围和资格，不从原始任务列表重新推导业务上下文。`TaskEditorViewModel` 保存任务字段与新建前置集合的同一原子草稿。`TaskGraphViewModel` 只把 Model 图快照转换为像素坐标、正交路径、端口、箭头几何和详情投影。

任务主流程按展示职责拆分为三个彼此独立的投影：`TaskListViewModel` 只负责列表、筛选、状态命令和批量选择，`TaskFocusViewModel` 负责不受列表筛选影响的“现在做”投影，`TaskDetailsViewModel` 负责稳定 `TaskId` 驱动的详情会话。三者不互相调用，分别监听同一个 `TaskService` 的变化信号，禁止把焦点或详情状态重新塞回列表 Contract。

### 3.5 Qt Widgets View

View 全部位于 `src/view/widgets`，只使用 `.h/.cpp` 构建，不使用 `.ui`。View 负责控件创建、布局、绘制、样式、动画、焦点、对话框显示、缩放平移和用户事件转发。

- Widget 只依赖 ViewModel Contracts、Common 和 Qt Widgets，不包含具体 ViewModel、Model、Service、Repository 或 Persistence 头文件。
- `QListView` 直接消费抽象列表 Contract；Delegate 只读取 Contract 定义的稳定 Role。
- Widget 使用稳定任务/类别 ID 调用强类型命令，不以行号、名称或显示顺序作为身份。
- 按钮资格、阻塞原因、逾期、排序理由和类别语义来自 ViewModel 投影；View 不得根据状态再次推导业务资格。
- View 可以执行纯展示计算，例如控件尺寸、缩放比例和滚动位置，但不得遍历依赖图或计算拓扑、闭包和边满足状态。
- ViewModel→View 使用 getter 初始同步加 Qt 通知信号；View→ViewModel 使用控件事件到 Contract 命令的显式连接。
- 双向草稿绑定必须区分用户编辑与程序性更新，并使用 `QSignalBlocker` 或等价机制防止回写循环。
- Widget 绑定辅助代码位于 `src/view/widgets/binding`，因为它依赖 Qt Widgets，不能放入 Common。
- View 决定使用状态栏、消息框或其他控件呈现 `UiNotification`；ViewModel 不得直接调用 `QMessageBox`。

任务 Widgets 页面由 `TaskListContract`、`TaskFocusContract`、`TaskDetailsContract`、`TaskEditorContract`、`TaskCategoryContract` 和 `TaskDependencyContract` 六个窄端口组成。焦点面板读取焦点投影，但状态命令仍转发给任务列表命令端口；详情只维护稳定 ID 选择，任务编辑器由 `sessionActive` 和草稿通知驱动。类别筛选、管理与依赖候选都直接读取 Contract Role，创建前置只修改编辑草稿，已有任务依赖只调用一次原子保存命令；Widget 不按行号、名称或状态重新推导业务身份与资格。

依赖图 Widgets 页面只依赖 `TaskGraphContract` 及复用详情/依赖编辑所需的窄 Contract。`QGraphicsScene` 按节点 Role 放置 `QGraphicsObject`，按边 Role 给出的正交路径点和箭头顶点绘制连线；选中、悬停、筛选降暗、缩放、平移和详情展开属于 View 会话状态。Widget 禁止访问原始依赖边重建邻接关系，也不得计算拓扑、闭包、类别裁剪、阻塞状态或路径几何。

不建立 EventBus。直接 Qt 信号槽连接具有明确生产者、消费者、连接上下文和类型检查；全局总线会隐藏命令目标、状态来源、处理顺序和生命周期，并可能让 View 绕过 ViewModel。

### 3.6 App 组合根

`AppBootstrapper` 创建并按依赖顺序持有 Repository、Service、具体 ViewModel 和主窗口。它把 Contract 引用组成 `MainWindowDependencies` 后显式注入 View。Widget 不接收具体 `AppViewModel`、Service 或 Repository，也不通过 Service Locator 获取依赖。

`src/app` 可以包含进程配置、数据库路径选择、Qt 应用对象配置和启动失败处理，但不能包含任务规则、展示格式或控件事件逻辑。

## 4. 构建目标与边界

正式入口切换后的 CMake 关系为：

```text
smartmate_common               → Qt6::Core
smartmate_model                → Qt6::Core
smartmate_persistence          → smartmate_model + Qt6::Sql
smartmate_viewmodel_contracts  → smartmate_common + Qt6::Core
smartmate_viewmodel            → contracts + smartmate_model + Qt6::Core
smartmate_widgets              → contracts + smartmate_common + Qt6::Widgets
SmartMate                      → app core + smartmate_widgets + Qt6::Widgets
```

`smartmate_widgets` 永远不能链接具体 `smartmate_viewmodel`；`smartmate_viewmodel` 永远不能链接 `smartmate_widgets` 或 `smartmate_persistence`。只有组合根可同时链接具体实现和 View。正式目标与发布目录不得包含 Qt QML、Qt Quick 或旧迁移插件。

## 5. 关键纵向链路

### 5.1 任务命令

```text
用户点击“开始/取消/完成/重做/归档/恢复”
  → Widget 从 Contract Role 获取稳定 TaskId 与命令资格
  → Widget 调用 Contract 强类型命令
  → 具体 ViewModel 调用 TaskService
  → TaskService 复用状态机并校验依赖与单进行中约束
  → Repository 原子保存
  → Service 发出变化信号
  → 各 ViewModel 独立重新投影
  → Contract 通知驱动 Widget 更新
```

View 的确认对话框只收集用户确认，不循环调用单项命令制造批量操作。批量选择仍由 ViewModel 使用稳定 ID 维护，Service 和独立 Repository 端口保证整批原子成功或失败。

### 5.2 编辑与依赖

带前置关系的新建继续使用 `TaskEditorViewModel → TaskService → ITaskCreationRepository → SQLite 事务`。Service 校验字段、端点和假想图，Persistence 在一个事务中写入任务与全部边。编辑、恢复、归档和永久删除继续遵守现有原子端口与状态资格，不因 View 技术迁移改变。

### 5.3 依赖图

```text
SQLite Repository
  → TaskService 结构化图快照
  → TaskGraphViewModel 节点布局与正交路径
  → QGraphicsView/QGraphicsScene 绘制、平移、缩放与点击
```

Model 负责拓扑层级、连接闭包、边满足状态和按类别裁剪；ViewModel 负责像素坐标、端口和箭头几何；Widget 只按投影创建 `QGraphicsItem` 并转发稳定 ID。View 不得重新遍历图或推导阻塞状态。

## 6. 目标目录结构

目录只有在包含真实代码时才创建，不提交空目录或 `.gitkeep`。

```text
src/
  common/
    presentation/             # UiSeverity、UiNotification
  app/                        # 唯一组合根
  model/
    domain/
    dependencies/
    planner/
    repositories/
    services/
    persistence/
  viewmodel/
    contracts/                # ViewModel 抽象展示端口
    AppViewModel.*
    TaskListViewModel.*
    TaskFocusViewModel.*
    TaskDetailsViewModel.*
    TaskEditorViewModel.*
    TaskDependencyViewModel.*
    TaskGraphViewModel.*
  view/
    widgets/
      MainWindow.*
      binding/
      task/
      graph/
      settings/
```

## 7. 架构守卫与验收证明

项目使用四道防线：

1. CMake target 限制链接方向，特别锁定 `smartmate_widgets` 只能依赖 Contracts，而不是具体 ViewModel。
2. `tests/architecture/check_mvvm_boundaries.cmake` 检查禁用 include、链接、Controller、EventBus、Service Locator、SQL 和 View API 泄漏。
3. ViewModel 测试使用 Fake Repository 验证投影、命令和通知；Widget 测试只注入 Fake Contract，证明 View 可脱离具体 ViewModel、Service 和数据库测试。
4. `AGENTS.md` 要求每次修改先声明所属层、目标数据流和测试方式。

最能体现 MVVM 的验收场景是：Widget 测试通过 Fake Contract 记录命令；具体 ViewModel 测试通过 Service/Fake Repository 验证投影；完整集成测试再验证 Widget→Contract→ViewModel→Service→内存 SQLite→通知刷新。任何阶段都不得为了迁移速度绕过这些边界。
