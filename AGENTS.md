# SmartMate 开发规则

本文件约束整个仓库。SmartMate 正在把当前 Qt Quick/QML View 迁移为纯 `.h/.cpp` Qt Widgets View；后续任何修改都必须保持以下 MVVM 边界。

## 固定依赖方向

编译期依赖固定为：

`Qt Widgets View → ViewModel Contracts ← ViewModel实现 → Model Service → Repository接口`

运行时数据流固定为：

`Widget事件 → Contract命令 → ViewModel → Model Service → Repository接口`

`Service信号 → ViewModel投影 → Contract通知 → Widget绑定`

Persistence 只实现 Repository 接口。`src/app` 是唯一组合根，可以看见具体实现。禁止反向依赖和跨层捷径。

## 迁移期规则

- 当前 `SmartMate` Qt Quick/QML 程序只是行为基线；迁移期新增临时 `SmartMateWidgets`，最终由 Widgets 版本接管正式目标。
- 禁止新增 QML 页面、QML ViewModel API 或 Qt Quick 功能。现有 QML 只允许行为保持、回归修复和迁移阻断修复。
- 新 View 必须位于 `src/view/widgets`，全部使用 `.h/.cpp` 构建，禁止 `.ui`。
- 两个前端必须观察相同 Model Service；禁止为 Widgets 复制业务规则或持久化逻辑。
- 在正式切换前继续运行当前 QML 测试和 `all_qmllint`；切换完成后才删除 QML、QuickTest、Qt Quick 依赖和 QML 注册元数据。
- 迁移以行为等价为先，不同时增加新功能或大规模重做交互。

## 各层职责

- `src/common` 只允许业务无关、Qt Core 级通用类型；禁止放入 Task/Category 业务类型、ViewModel Contract、Widget、Service、Repository、SQL、QSettings 或全局总线。
- `src/model` 必须负责实体、状态转换、业务校验、依赖与规划算法、统计、服务编排和 Repository 接口。
- 领域实体和算法必须是普通 C++ 类型；禁止依赖 View、ViewModel、Contracts、Qt Widgets、Qt Quick、QML 运行时或 Qt SQL。
- SQL 与 `QSettings` 只能位于 `src/model/persistence`。
- `src/viewmodel/contracts` 只定义状态 getter、稳定 Role、强类型命令槽和通知信号；禁止调用 Service 或包含具体实现。
- `src/viewmodel` 具体实现只负责可观察展示状态、命令、输入草稿和错误映射；禁止 SQL、业务规则、具体持久化代码或操纵 View。
- ViewModel 禁止使用 `QWidget`、`QDialog`、`QGraphicsScene`、`QGraphicsItem`、`QQmlEngine`、`QQmlContext`、`QQuickItem` 等 View API。
- ViewModel 之间禁止直接调用；`AppViewModel` 可以拥有并协调确实跨投影的会话流程。
- `src/view/widgets` 只负责控件、布局、绘制、样式、绑定和事件转发；禁止调用 Service/Repository，禁止实现校验、状态转换、排序和规划。
- `src/app` 只负责进程配置、对象创建、依赖注入和启动窗口。

## Contract、命令、通知与绑定

- 列表 Contract 继承抽象 `QAbstractListModel`；其他 Contract 继承抽象 `QObject`。具体 ViewModel 直接继承 Contract，禁止多重继承另一个 `QObject`。
- View 边界使用 `QString` 形式的稳定 `TaskId`/`TaskCategoryId`，不得暴露领域实体或以行号、名称作为身份。
- 命令必须是强类型语义方法；禁止统一 `execute(QVariant)`、字符串命令名、反射路由或 EventBus。
- 命令资格必须由 Model 计算、ViewModel 投影；Widget 只能绑定资格，Service 在执行时最终复核。
- ViewModel 通过 Contract 信号报告展示通知，禁止调用 `QMessageBox`、打开窗口、切换焦点或操纵控件。
- 成功命令主要依靠 Service 变化信号触发投影刷新；禁止用全局事件总线同步 ViewModel。
- ViewModel→Widget 绑定必须先同步当前 getter，再监听通知信号；Widget→ViewModel 使用显式控件信号到 Contract 命令的连接。
- 双向草稿绑定必须区分用户编辑与程序性更新，并使用 `QSignalBlocker` 或等价机制防止回写循环。
- Widget 绑定辅助代码属于 `src/view/widgets/binding`，不得因“通用”而放入 Common。
- `smartmate_widgets` 不得链接具体 `smartmate_viewmodel`；只有 `src/app` 可以同时看见 View、具体 ViewModel 和 Persistence。

## 明确禁止

- 禁止 Controller 层和 `*Controller` 类型。
- 禁止 EventBus、Service Locator、全局业务单例和字符串反射绑定。
- 禁止 View 或 ViewModel 绕过 Model Service 直接写 Repository。
- 禁止 ViewModel 控制 Widget 或 QML 对象。
- 禁止向 View 暴露 `QSqlTableModel`。
- 禁止用列表行号作为任务身份，必须使用稳定 `TaskId`。
- 禁止由 View 创建具体 ViewModel；其生命周期必须由 C++ 组合根管理。

## 任务模块规则

- `Task` 必须是普通 C++ 类型；标题、时间、状态和优先级校验必须位于 Model。
- 任务状态只能通过 Model 的显式状态机转换；创建固定为待办，编辑草稿不得包含状态，View 与 ViewModel 不得直接指定目标状态。
- 状态机固定为：待办可开始或取消，进行中可完成或取消，已完成/已取消可重做为待办或归档；禁止待办直接完成，其他转换一律拒绝。
- 任意时刻最多一个进行中任务；开始任务时必须由 `TaskService` 同时检查前置依赖与单进行中约束。
- 活动任务的删除表示归档；只有已完成或已取消任务可以归档。正常数据恢复为归档前的完成/取消状态，旧数据中的其他恢复目标统一安全恢复为待办。
- 只有待办任务允许修改标题、描述、优先级、截止时间和预计用时；其他状态只能查看或执行合法状态命令，Service 必须在保存时重新检查资格。
- 只有归档任务允许确认后永久删除；任务及其全部入边、出边必须通过独立 Repository 端口在同一事务中删除，禁止 ViewModel 绕过 Service 物理删除。
- 批量归档、恢复和永久删除必须整批原子成功或整批失败；禁止 ViewModel 或 View 循环调用单项命令制造部分成功状态。
- 批量选择只能由 ViewModel 使用稳定 `TaskId` 维护，属于会话级展示状态；View 只能切换选择并触发一次批量命令。
- ViewModel 只能维护编辑草稿、列表投影、命令和中文错误映射，不能复制业务校验。
- 截止时间与预计用时必须通过类型化控件进入 ViewModel；禁止恢复自由文本格式解析。
- 自动排序规则和推荐理由必须由 Model 产生；ViewModel 只能执行搜索、优先级筛选和文案投影，View 不得自行筛选或排序。
- 第一版任务依赖只允许 Finish-to-Start；多个前置任务采用 AND 语义，所有关系必须处于已完成或已取消解析状态，后继任务才能开始或完成。
- 依赖端点、自依赖、重复关系、循环路径、阻塞状态、自动解锁和依赖感知排序必须由 Model 计算并测试。
- 只有活动待办任务可以修改自己的前置集合；已取消任务不能新增为前置候选，已有取消关系必须保留显示并允许移除。
- 任务取消只让其后继依赖动态失效并解除阻塞，不删除依赖边；重做为待办后原关系重新生效。归档不得删除已有关系。
- 逾期、阻塞原因、解锁数量和排序位置是派生状态，不得写入 SQLite；逾期只由 Model 根据任务状态、截止时间和当前 UTC 时间计算。
- 新建任务携带前置关系时，任务与全部依赖边必须通过独立 Repository 端口在同一事务中原子写入。
- 有前置任务的新任务只能创建为待办；创建候选范围、端点有效性和图一致性必须由 Model 最终校验。
- 依赖图拓扑层级、连接闭包和边满足状态属于 Model；节点像素坐标和箭头几何属于 ViewModel；Widget 只负责 `QGraphicsView/QGraphicsScene` 绘制、平移缩放和稳定 ID 事件转发。
- 图布局、缩放、选中状态、阻塞状态和解锁数量均为派生展示状态，不得写入 SQLite 或 `QSettings`。
- 搜索词、优先级和类别筛选属于 ViewModel 会话状态，不得持久化；列表筛选只能过滤 Model 已有顺序。
- 任务最多关联一个稳定 `TaskCategoryId`，也可保持未分类；任务实体不得嵌入类别名称。
- 类别名称、颜色、唯一性和存在性校验必须位于 Model。删除类别必须通过原子 Repository 端口把关联任务转为未分类，且不得删除依赖边。
- 跨类别依赖继续合法，类别不得参与状态转换、依赖满足、优先级或推荐排序。
- 依赖图按类别裁剪时，核心节点和直接跨类别上下文节点的选择属于 Model；ViewModel 只布局并投影上下文语义，View 不得遍历依赖图或自行裁边。
- Repository 接口不得包含 Qt SQL 类型；SQLite 和 SQL 语句只能位于 `src/model/persistence`。
- Task Service、SQLite Repository、ViewModel Contract 实现和 Widget 必须分别添加独立测试。

## 代码注释要求

- 注释以中文为主，类型名、API、SQL、UTC、MVVM等技术术语保留英文。
- 公共类、接口和含义不明显的函数使用 `///` 说明职责、输入输出、错误语义与边界；无需注释简单 getter/setter。
- 重要成员变量必须说明用途、单位、所有权、生命周期或可空语义。
- 实现注释必须解释设计原因、业务约束和不可破坏的架构边界，不得逐行翻译代码。
- Widget 注释只说明界面分区、数据绑定和事件转发；测试注释只说明关键场景验证的规则与回归风险。
- 禁止保留与实现不一致的过期注释。修改行为、接口或约束时必须同步更新附近注释。

## 修改流程

修改前必须说明所属层、目标数据流和测试方式。新增业务规则必须放入 Model 并添加 Model 测试；新增可观察状态必须放入 ViewModel 并验证通知信号；新增 Widget 绑定必须使用 Fake Contract 验证命令转发。改变层边界前必须先更新 `docs/architecture.md`。

迁移期修改后运行：

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
cmake --build --preset debug --target all_qmllint
```

创建 `SmartMateWidgets` 目标后，还必须构建该目标并运行 Widget 测试；具体命令随目标落地同步加入本节。最终删除 QML 后才移除 `all_qmllint`。

禁止提交构建产物、运行数据库、IDE用户配置、凭据、生成缓存或无关改动。
