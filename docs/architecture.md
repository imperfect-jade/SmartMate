# SmartMate MVVM 架构说明

## 1. 架构目标

SmartMate 使用 MVVM 将业务行为与 Qt Quick 界面分离。只有源码依赖与运行时数据流都遵循以下方向时，项目才符合本课程要求：

```text
View（QML） → ViewModel → Model Service → Repository 接口
                                            ↑
                                      持久化实现
```

`src/app` 是唯一的对象组合根：它创建 Repository、Service 和 ViewModel 的具体对象，完成依赖注入并启动 QML 引擎。它不承担 Controller 或业务层职责。

外观偏好遵循同一依赖方向：`SettingsPage`只绑定`AppearanceSettingsViewModel`，设置 Service 校验有限枚举，Repository接口隐藏存储细节，`QSettingsAppearanceRepository`作为 persistence 实现由组合根注入。QML不得导入Settings模块或直接写`QSettings`。

## 2. 各层职责

### 2.1 Model

Model 负责领域实体、输入校验、状态转换、任务依赖、规划算法、统计公式、服务编排与 Repository 接口。

- Domain 与 Service 可以使用 Qt Core 的值类型。
- Domain、Service 和 Repository 接口不能依赖 Qt Quick、QML 运行时、ViewModel 或 View。
- 业务规则必须能够在不启动图形界面的情况下测试。
- `Task` 等领域对象保持为普通 C++ 类型，不为了界面绑定而继承 `QObject`。
- `TaskStateMachine`集中定义开始、取消、完成、重做、归档和恢复的合法来源与目标；Service负责执行附加约束，ViewModel只投影命令资格。
- `TaskDeadlinePolicy`根据任务状态、截止时间与调用方提供的UTC时间计算逾期；逾期可与进行中、阻塞等语义并存，且永不持久化。
- 普通字段编辑资格属于`Task`领域规则，只有待办任务可建立和保存编辑草稿；Service必须在真正写入前再次检查当前状态。
- 任务自动排序及其推荐理由属于 Model；即使主窗口或桌宠采用不同布局，也必须消费同一份领域顺序。
- Finish-to-Start依赖是独立领域关系，不嵌入`Task`实体。循环检测、阻塞判断、自动解锁与拓扑整理均由纯Model算法完成。

SQLite 持久化实现位于 `src/model/persistence`。该目录属于 Model 的基础设施实现，是生产代码中唯一允许使用 Qt SQL 或 `QSettings` 的位置。

### 2.2 ViewModel

ViewModel 负责：

- 适合界面绑定的可观察状态；
- 用户命令及其可用性；
- 表单草稿、选择、筛选等展示状态；
- 将 Model 的结构化错误映射为用户可理解的文本；
- 在 Model 状态变化后重新生成界面投影并发出属性通知。

所有 ViewModel 都由 C++ 创建并持有，QML 不得自行构造。`AppViewModel` 负责拥有和协调子 ViewModel；子 ViewModel 之间不能直接相互调用，而应通过同一个 Model 服务共享状态。

ViewModel 可以包含用于生成 QML 类型元数据的声明，但不能控制 QML 对象、调用具体持久化实现或包含 SQL、依赖算法和统计公式。

任务列表的搜索词、优先级和类别条件是 ViewModel 持有的会话级展示状态。它们只过滤 Model 已排序的结果，不写入 SQLite 或 `QSettings`，应用重启后恢复默认值。任务只保存可空的稳定`TaskCategoryId`，类别名称和固定颜色由独立`TaskCategoryService`提供；因此重命名或换色不会重写任务。只有待办任务可主动调整类别，删除类别则通过原子持久化端口把所有关联任务转为未分类，同时保留状态和依赖。

`TaskDependencyViewModel`维护独立的前置任务选择草稿。打开编辑器时只调用`TaskService::taskDependencyEditContext`，由Model一次性返回目标任务、当前选择、候选范围与`selectable`资格；ViewModel不得再次按任务状态筛选候选。保存时只向Service提交稳定`TaskId`集合，取消时丢弃草稿。它不调用任务列表或任务编辑器，三个子ViewModel只观察同一个`TaskService`。

`TaskEditorViewModel`在新建模式下同时维护任务字段和前置任务选择草稿。两者以一个`TaskCreationRequest`提交给Service，不能由两个ViewModel分步保存。编辑草稿不包含可写状态：新建时只展示固定的待办状态，所有转换必须经由任务列表命令。只有待办任务可通过`TaskService::findEditableTask`建立草稿，保存时Service还会重新读取状态以防止过期草稿绕过规则；ViewModel只保留草稿并映射结构化错误。类型化选择器可以检查日期和分段输入格式，但总分钟数等领域边界必须委托Model校验。

`TaskGraphViewModel`只把Model提供的结构化图快照转换为自上而下的节点像素坐标、正交路径点、端口和箭头几何；拓扑层级、可见节点闭包及选中任务的前置/后继闭包仍由Model计算。列表与依赖图中的`TaskCommandAvailability`由同一个Model策略生成，ViewModel不得通过`TaskStatus`重新推导按钮资格。图的子列表模型与布局辅助类仍属于ViewModel层，不注册为可由QML创建的类型。

### 2.3 View

View 由 QML 布局、属性绑定、样式、动画和事件转发组成。View 可以根据 ViewModel 已提供的语义状态选择颜色或动画。主窗口主题对象只解释颜色、字体比例、间距和状态色等展示令牌；它不持久化设置，也不判断任务是否可开始。“现在做”任务槽消费 Task ViewModel 提供的独立焦点投影；拖放只携带稳定 TaskId并调用开始命令，不改变 Model 推荐顺序，也不在 QML 保存临时任务状态。

View不能：

- 直接调用 Repository 或 Model Service；
- 实现业务校验、状态转换、循环检测或规划评分；
- 遍历依赖图、计算阻塞原因或决定任务是否已经解锁；
- 直接访问数据库；
- 以列表行号代替稳定的任务 ID。

主窗口和桌宠都是 View。它们通过各自的 ViewModel 投影观察同一份 Model 状态，因此一处操作会自动反映到另一处，而不是互相操纵界面控件。

## 3. 构建目标与依赖边界

当前目标关系如下：

```text
smartmate_model       → Qt6::Core
smartmate_persistence → smartmate_model + Qt6::Sql
smartmate_viewmodel   → smartmate_model + Qt6::Core + QML 注册元数据
smartmate_ui          → Qt6::Quick + Qt6::QuickControls2
SmartMate             → persistence + viewmodel + ui 插件
```

`smartmate_viewmodel` 永远不能链接 `smartmate_persistence`。这样 ViewModel 只依赖抽象服务和领域结果，测试时可以替换为 Fake Repository。

## 4. 最小纵向链路

当前任务 CRUD 使用一条完整纵向链路验证 MVVM 和 QML 类型化注入：

1. `main.cpp` 创建 `QGuiApplication`、`AppBootstrapper` 和 `QQmlApplicationEngine`。
2. `AppBootstrapper` 按 SQLite Repository、`TaskService`与`TaskCategoryService`、`AppViewModel` 的顺序创建对象。
3. `AppViewModel` 拥有任务列表、任务编辑器、类别管理、依赖编辑器和依赖图子 ViewModel；它们可观察同一Model Service，但彼此不直接调用。
4. 静态 `SmartMate.ViewModel` 模块将所有 ViewModel 声明为 QML 不可创建类型。
5. `Main.qml` 接收 C++ 持有的 `AppViewModel`，任务页只绑定属性并转发任务 ID。

任务列表查询继续遵守相同边界：`TaskService` 请求 Model 生成带推荐理由的自动排序结果，`TaskListViewModel` 在该结果上应用实时搜索和优先级筛选，QML 只绑定条件、计数、角色与空状态。排序结果和筛选条件都不是持久化业务数据。

任务依赖采用同一条纵向链路：QML只提交任务ID与勾选集合，`TaskDependencyViewModel`维护可取消的选择草稿，`TaskService`调用纯C++依赖图验证端点、自依赖、重复边和循环，再通过独立Repository端口原子替换SQLite关系。Service通知后，任务列表重新投影Model给出的直接阻塞原因、解锁数量和依赖感知顺序。依赖解析具有待满足、已满足和已取消三态：完成及归档前完成永久满足关系；取消及归档前取消使关系动态失效且不阻塞；取消任务重做后原边重新进入待满足状态。边本身始终保存在SQLite中，因此这种变化只发送任务通知，不发送依赖变更通知。阻塞与解锁都是派生状态，不写入数据库。

带前置关系的新建命令使用独立的原子端口：`TaskEditorViewModel → TaskService → ITaskCreationRepository → SQLite事务`。Service先验证任务字段、候选端点和假想图，再由Persistence在一个事务中插入任务与全部边；任意一步失败都不得留下孤立任务或部分关系。

归档永久删除使用另一条显式命令链：`QML确认 → TaskListViewModel → TaskService → ITaskDeletionRepository → SQLite事务`。只有归档实体可删除；Persistence先删除目标任务的全部入边和出边，再以归档状态条件删除任务，任一步失败都必须回滚。Schema继续使用`ON DELETE RESTRICT`阻止绕过端口的直接删除。

批量管理沿用同一边界：`QML选择事件 → TaskListViewModel的QSet<TaskId> → TaskService批量校验 → 原子Repository端口 → SQLite事务`。批量归档与恢复通过独立状态写入端口一次提交全部条件更新，批量永久删除通过删除端口一次清理全部目标及关联边；任一任务缺失、资格变化、依赖冲突或写入失败都必须整批回滚。选择集、批量模式和全选状态只属于ViewModel会话投影，QML不得收集ID数组或循环调用单项命令。

依赖图读取链路为：`SQLite Repository → TaskService结构化图快照 → TaskGraphViewModel纵向布局与正交路由 → QML Shape渲染`。Model负责最长前置链层级、活动任务及其相连归档节点闭包、上下游闭包、边解析状态和异常图拒绝；ViewModel负责交叉最小化、虚拟路由点、端口、层间通道、像素坐标和详情投影；View只绘制路径、主题与动画，并转发稳定任务ID。按类别查看时，Model保留匹配类别的核心节点及其直接跨类别前置、后继上下文，拒绝递归扩张或保留外部节点之间的边；ViewModel只负责重新布局并投影类别颜色和上下文标记。布局、筛选、缩放、响应式详情面板和选中状态不持久化。

`view.qml_bootstrap` CTest 使用内存 SQLite 和 Qt 离屏平台执行整条链路，并在根对象创建成功后自动退出。它可以发现数据库驱动、QML 模块、属性注入和运行库加载问题，同时不会写入用户数据。

后续任务功能的数据流将保持一致：

```text
用户点击“开始/取消/完成/重做/归档/恢复”
  → QML 转发任务 ID
  → TaskListViewModel 执行命令
  → TaskService 复用 TaskStateMachine 并校验依赖与单进行中约束
  → Repository 保存
  → 服务发出状态变化
  → 任务列表与 PetViewModel 分别重新投影
  → 主窗口和桌宠通过绑定自动更新
```

## 5. 目标目录结构

模块只有在包含真实代码时才创建；不提交空目录或 `.gitkeep`。

```text
src/
  app/                       # 应用生命周期和依赖注入
  model/
    domain/                  # 实体、值对象、领域规则
    dependencies/            # Finish-to-Start图校验、阻塞与解锁算法
    repositories/            # 持久化端口
    services/                # 应用/领域服务
    persistence/             # 当前 SQLite Repository 实现
    planner/                 # 依赖感知的稳定推荐排序
  viewmodel/
    AppViewModel.*           # 子ViewModel组合与QML入口
    TaskListViewModel.*      # 当前活动/归档列表投影
    TaskEditorViewModel.*    # 当前编辑草稿与命令
    TaskDependencyViewModel.* # 前置任务关系草稿与错误投影
    TaskGraphViewModel.*     # 图快照的节点布局、箭头几何与选择投影
    planner/                 # 推荐计划投影
    pet/                     # 桌宠展示状态与命令
    focus/                   # 专注会话状态
    statistics/              # 统计结果投影
  view/
    qml/
      Main.qml
      TaskPage.qml           # 当前简单任务页
      TaskEditorDialog.qml   # 当前任务编辑对话框
      TaskDependencyDialog.qml # 当前任务的前置关系编辑器
      TaskCreationPredecessorDialog.qml # 新建任务内的前置选择草稿
      DependencyGraphPage.qml # 只读有向依赖图与任务详情
      components/
      pet/
      focus/
      theme/
```

依赖图已通过独立图投影ViewModel和QML页面实现，边仍只能通过现有依赖编辑流程修改。Windows通知应先在Model定义网关接口，再由Windows适配器实现；更丰富的桌宠动画只改变QML与素材，继续消费`PetViewModel`提供的Idle、Working、Urgent、Celebrate等语义状态。

## 6. 如何体现 MVVM 的优势

本项目会用以下场景展示架构价值：

1. **算法可独立测试**：循环检测和任务排序不启动 QML 即可验证。
2. **持久化可替换**：`TaskService` 依赖 `ITaskRepository`，单元测试使用 Fake，正式程序使用 SQLite。
3. **多个 View 自动同步**：主窗口和桌宠不互相调用，而是观察同一 Model 状态的不同投影。
4. **界面可替换**：增加依赖图或统计页面时不修改领域规则。
5. **职责清晰**：QML 不会逐渐堆积数据库、校验和排序代码，问题更容易定位。

## 7. 架构守卫

项目组合使用三道防线：

1. CMake target 的链接和可见性限制每一层能够依赖的库。
2. `tests/architecture/check_mvvm_boundaries.cmake` 自动检查生产代码中的禁用包含、链接、QML 直接访问和 Controller 类型。
3. `AGENTS.md` 要求每次修改先说明所属层、数据流和测试方式。

静态守卫只能发现结构性越界；“候选资格与命令资格必须来自Model”还由Model契约测试和ViewModel投影测试共同保证，禁止用简单搜索`TaskStatus`的正则替代语义测试。

任何需要改变上述边界的修改，必须先更新本文档并解释原因；不能为了快速实现功能绕过分层。
