# SmartMate Qt Widgets 迁移指南

## 1. 目标与非目标

本次迁移把当前 Qt Quick/QML View 替换为纯 `.h/.cpp` Qt Widgets View，并通过 ViewModel Contracts 实现编译期依赖倒置。迁移以行为等价为第一目标。

本次包含：

- 引入 `smartmate_common`、`smartmate_viewmodel_contracts` 与 `smartmate_widgets`；
- 使用抽象 `QObject`/`QAbstractListModel` Contract 隔离 Widget 和具体 ViewModel；
- 使用强类型命令、Qt 信号槽通知和显式 Widget 数据绑定；
- 用 Qt Test、Fake Contract 和集成测试替换最终的 QML View 测试；
- 最终删除 QML、Qt Quick、QuickTest 与 QML 注册元数据。

本次不包含：

- 新业务功能、数据库迁移或 Repository API 重设计；
- 状态机、依赖语义、排序规则或原子事务行为变化；
- 大规模视觉改版；
- Controller、EventBus、Service Locator 或自动反射绑定框架。

## 2. 固定公共接口

### 2.1 Common

第一批通用类型固定为：

```cpp
enum class UiSeverity {
    Information,
    Warning,
    Error,
};

struct UiNotification {
    UiSeverity severity;
    QString title;
    QString message;
};
```

它们位于 `smartmate::common`，使用 `Q_DECLARE_METATYPE` 支持 `QSignalSpy` 与队列连接。Common 只依赖 Qt Core，不承载业务 ID、命令路由或 Widget 类型。

### 2.2 ViewModel Contracts

`TaskListContract` 继承 `QAbstractListModel`，固定列表 Role、筛选状态、批量状态、强类型任务命令和 `notificationRaised`。其他 Contract 继承 `QObject`。具体 ViewModel 直接继承 Contract，不能再继承另一个 `QObject`。

Contract 使用 `QString` 形式的稳定任务/类别 ID。命令采用 `startTask(const QString &taskId)`、`setSearchText(const QString &text)` 等强类型方法，禁止 `execute(QVariant)`。

编辑器通过 `sessionActive`、草稿属性和通知信号表达状态。View 根据状态显示或关闭 `QDialog`，Contract 不包含 `openDialog`、`focusWidget` 或任何控件类型。

### 2.3 注入边界

最终由 `AppBootstrapper` 构造：

```text
Repository → Service → AppViewModel/子ViewModel → MainWindowDependencies → MainWindow
```

`MainWindowDependencies` 只保存 Contract 引用。`MainWindow` 把每个页面需要的窄 Contract 注入对应 Widget；不向 Widget 传递具体 `AppViewModel`、Service 或 Repository。

## 3. 命令、通知与数据绑定

### 3.1 命令

Widget 读取 Contract 投影的稳定 ID 和命令资格，再调用强类型命令。View 不通过状态文本重新推导资格，Service 在执行时保留最终校验。

成功命令依靠现有 Service 变化信号触发全部 ViewModel 刷新。失败由 ViewModel 将结构化错误映射为 `UiNotification` 后发出。View 决定使用消息框、状态区或其他 Widget 呈现，不允许 ViewModel 调用 `QMessageBox`。

### 3.2 单向绑定

每个绑定必须先调用 getter 完成初始同步，再连接通知信号。连接使用 Widget 作为 context object，让 Qt 在 View 销毁时自动断开。

### 3.3 双向草稿绑定

用户输入使用 `QLineEdit::textEdited` 等只代表用户编辑的信号写入 Contract。ViewModel 的程序性更新使用通知信号回填控件，并用 `QSignalBlocker` 防止循环写回。

强类型绑定辅助函数放在 `src/view/widgets/binding`。禁止使用字符串属性名、动态调用、EventBus 或全局绑定注册表。

## 4. 实施阶段

当前进度：阶段 1、2、3、4、5 已完成。`SmartMateWidgets` 已具备主窗口、外观设置、任务主流程、类别/依赖编辑和完整依赖图；所有 Widget 通过独立 Contract 接入同一 Model Service。当前仅剩阶段 6 的正式入口切换与 QML 清理。

### 阶段 1：基础目标与契约

- 建立 `smartmate_common` 和 `smartmate_viewmodel_contracts`。
- 定义 `UiNotification`、列表 Role 和页面级 Contract。
- 让现有 ViewModel 实现 Contract，同时保留迁移期 QML 元数据与行为。
- 增加 Contract 通知测试和 CMake/源码架构守卫。

完成标准：现有 `SmartMate` 行为与测试不变，ViewModel 可以通过 Contract 指针独立使用。

### 阶段 2：Widgets 壳与设置页

- 建立 `smartmate_widgets` 静态库和临时 `SmartMateWidgets` 可执行目标。
- 实现纯 C++ `MainWindow`、导航、主题绑定和设置页。
- 使用 Fake `AppearanceSettingsContract` 测试控件事件与状态刷新。

完成标准：两个前端可独立启动并观察相同外观 Service；Widgets 目标不链接具体 ViewModel。

当前实现额外把具体 ViewModel 核心目标与迁移期 QML 注册 adapter 分离，因此 `SmartMateWidgets` 的链接闭包不包含 Qt QML、Qt Quick、QML 插件或 `smartmate_ui`。Widgets 单元测试只注入 Fake `AppearanceSettingsContract`；纵向集成测试再组合具体 ViewModel 与 Service。

### 阶段 3：任务主流程

- 拆分过宽的列表投影为列表、焦点和详情 Contract/ViewModel。
- 使用 `QListView + QStyledItemDelegate` 迁移列表、搜索、筛选、焦点任务和详情。
- 使用 `QDialog` 与类型化控件迁移新建、编辑、日期和时长草稿。
- 迁移单项状态命令、确认流程和原子批量操作。
- 类别只迁移任务卡片展示和编辑器中的稳定 ID 选择；类别管理、类别筛选、新建前置集合与依赖编辑仍属于阶段 4。

完成标准：任务 CRUD、筛选、拖放开始、详情、状态机命令和批量行为与 QML 基线等价。

当前结果：已实现纯 C++ `QListView/QStyledItemDelegate`、焦点与详情投影、`sessionActive` 编辑会话、类型化日期/时长选择器、稳定 ID 拖放、确认流程及单次原子批量命令调用；Fake Contract、内存 SQLite 纵向集成和现有 QML 基线测试共同作为行为等价证据。

### 阶段 4：类别与依赖编辑

- 迁移类别管理、类别筛选、新建前置集合和依赖编辑对话框。
- 保持 `taskDependencyEditContext` 和原子 Repository 端口不变。
- 使用 Fake Contract 验证 Widget 只提交稳定 ID，不自行筛选候选或循环保存。

完成标准：非法端点、重复边、循环、取消依赖和类别删除行为与基线等价。

当前结果：类别目录、草稿、删除确认和筛选直接消费 `TaskCategoryContract/TaskListContract`；创建前置集合复用 `TaskEditorContract` 局部草稿，已有任务依赖通过 `TaskDependencyContract` 一次原子保存。Fake Contract、内存 SQLite 纵向集成及原 QML QuickTest 共同验证稳定 ID、取消语义、循环失败无部分写入和类别删除不改写依赖。

### 阶段 5：依赖图

- 使用 `QGraphicsView/QGraphicsScene`、节点 Item 和边 Item 绘制现有 ViewModel 布局。
- Widget 只实现绘制、Hover、选择、平移、缩放、适应窗口和详情联动。
- Model 继续计算拓扑、闭包和类别裁剪；ViewModel 继续计算节点坐标、端口与箭头路径。

完成标准：节点、边状态、类别上下文、选择链、高亮、缩放和详情入口与基线等价，View 中没有图遍历算法。

当前结果：纯 C++ `QGraphicsView/QGraphicsScene` 直接消费 `TaskGraphContract` 节点、边和关系 Role，复刻纵向节点、正交箭头、状态线型、选择链、类别上下文、筛选定位、缩放和详情侧栏。Fake Contract 与内存 SQLite 图集成测试证明 View 不计算拓扑或路径，原 QML 图 QuickTest 继续作为行为与视觉语义基线。

### 阶段 6：切换与清理

- 完整回归通过后，让 Widgets 版本接管正式 `SmartMate`。
- 删除临时 `SmartMateWidgets` 名称、`smartmate_ui`、QML 插件、QML 文件和 QuickTest。
- 删除 Qt Quick、QML、QuickControls2、QuickTest 依赖及所有 QML 注册元数据。
- 更新 README、项目概览、部署文档和 `AGENTS.md`，移除迁移期例外与 `all_qmllint`。

完成标准：仓库不含 `.qml`、`.ui`、QML 注册元数据或 Qt Quick 链接；正式程序和发布目录只依赖 Qt Core/Gui/Widgets/Sql 及必要平台插件。

## 5. 每阶段测试与退出条件

每阶段必须运行当前完整 Model、Persistence、ViewModel、集成和架构测试。新增代码还必须满足：

- Contract/具体 ViewModel：使用 `QSignalSpy` 验证初始状态、命令、精确通知和 Service 变化后的刷新。
- 列表：使用 `QAbstractItemModelTester` 验证模型协议与稳定 Role。
- Widget：只注入 Fake Contract，使用 `QTest` 操作控件并验证命令参数、资格绑定、初始同步、错误通知和防回写循环。
- 集成：使用内存 SQLite 验证 Widget→Contract→ViewModel→Service→Repository→通知刷新链路。
- 架构：View 不链接具体 ViewModel，ViewModel 不包含 Widgets，Common 不包含业务或 View 类型。

只有当一个纵向切片的 Widgets 测试、集成测试和现有 QML 基线测试同时通过时，才可标记为已迁移。迁移期间不得通过放宽 Model 规则、复制算法或跳过原子端口来追求界面进度。
