# SmartMate 数据统计设计

> **实现状态**：Model 数据底座已经实现，包括 `TaskActivityEvent`、SQLite Schema v4、状态与事件原子写入、原始事件查询端口和 `StatisticsService`。Statistics ViewModel Contracts、具体 ViewModel、统计页面与 Qt Charts 依赖尚未实现，当前生产目标仍未链接 Qt Charts。

## 1. 目标与第一版边界

数据统计页用于让用户快速回答三个问题：今天和本周完成了多少、近期趋势如何、当前任务是否存在逾期或阻塞风险。第一版先完成任务统计，不以专注计时为前置条件。

第一版包含：

- 今日完成次数及较昨日同期变化；
- 本周完成次数及较上周同期变化；
- 本周按时完成率及明确的分子、分母；
- 当前逾期任务数及紧急逾期数；
- 近 7 天、近 30 天和近 12 周的完成趋势；
- 当前可执行、被阻塞、即将到期和已经逾期的任务健康快照；
- 所选趋势周期内按完成时类别划分的成果构成。

第一版不包含专注时长、预计用时与实际专注用时对比、自定义日期范围、日历热力图、自然语言洞察或复杂图表编辑。专注模块完成后，再沿用同一统计链路增加专注会话聚合。

## 2. 固定统计口径

### 2.1 完成与状态事件

一次合法的 `Complete` 状态转换记为一次完成。任务完成后若执行 `Redo`，随后再次 `Complete`，应累计为第二次完成；归档、恢复、编辑和查看不增加完成次数。取消次数来自 `Cancel` 事件，但第一版不把“取消减少”直接解释为积极或消极。

任务状态事件是历史统计的事实来源。当前 Schema v4 会为每次成功的 `Start`、`Cancel`、`Complete`、`Redo`、`Archive` 和 `Restore` 写入一条不可变事件；创建时间继续来自任务快照，第一版不额外记录 `Create` 事件。

### 2.2 时间与周期

持久化时间统一使用 UTC。`StatisticsService` 根据查询传入的 `QTimeZone` 将 UTC 事件划入用户本地自然日和自然周；自然周固定从周一 00:00 开始。测试必须显式传入 `nowUtc` 与时区，统计公式不得在内部隐式散布系统当前时间调用。

同期比较使用相同长度的已过去区间：

- 今日截至当前本地时刻，对比昨天同一时刻；
- 本周一截至当前本地时刻，对比上周一至上周同一时刻；
- 近 7 天对比此前 7 天；
- 近 30 天对比此前 30 天；
- 近 12 周对比此前 12 周。

近 7 天和近 30 天按本地自然日分桶，近 12 周按本地自然周分桶。没有完成事件的桶仍必须输出零值，保证图表横轴稳定。

### 2.3 按时完成与任务健康

按时完成只统计完成事件中存在截止时间快照的记录：

```text
按时完成率 = 完成时间不晚于截止时间快照的完成次数
             ÷ 具有截止时间快照的完成次数
```

没有截止时间的完成事件不进入分母；分母为零时展示“暂无截止任务”，禁止显示容易误解的 `0%`。

任务健康是当前快照而非历史事件：

- 可执行：Model 判定当前可开始的待办任务；
- 被阻塞：Finish-to-Start 前置关系尚未解析的待办任务；
- 已经逾期：当前待办或进行中任务的截止时间早于 `nowUtc`；
- 即将到期：尚未逾期，且截止时间位于当前时刻至第三个本地自然日结束之间的待办或进行中任务。

这些资格、依赖和截止时间规则必须复用 Model 的权威算法，ViewModel 与 Widget 不得重新推导。

### 2.4 类别与永久删除

类别成果按完成事件保存的类别快照统计。事件至少保存完成时的稳定类别 ID、名称和颜色；任务后续重做并改类、类别重命名或删除，都不得改写过去事件的类别归属。未分类任务进入“未分类”桶，展示时保留前五个类别，其余合并为“其他”。

只有归档任务允许永久删除。永久删除任务时，任务事件必须在同一事务内级联删除，统计结果随之变化；这是“永久删除全部相关任务数据”的组成部分。

### 2.5 旧数据

Schema v3 没有可靠的完成时间。迁移到 Schema v4 时不得用 `updatedAtUtc` 推测完成历史，也不得为已有完成任务生成伪造事件：

- 当前任务健康仍读取并统计全部现有任务；
- 完成趋势和完成次数从任务事件功能启用后开始累积；
- 没有事件时页面明确提示“完成新任务后开始生成趋势”。

## 3. 数据事实与持久化

### 3.1 普通领域类型

Model 新增普通 C++ `TaskActivityEvent`，至少包含：

```text
eventId
taskId
transition
fromStatus
toStatus
occurredAtUtc
deadlineSnapshotUtc
estimatedMinutesSnapshot
prioritySnapshot
categoryIdSnapshot
categoryNameSnapshot
categoryColorSnapshot
```

快照用于防止后续重做、编辑或类别变更改写历史统计。该类型不得继承 `QObject`，不得依赖 ViewModel Contracts、Qt Widgets、Qt Charts、Qt SQL 或 QML 运行时。

### 3.2 Schema v4

SQLite Schema v4 新增 `task_activity_events` 表，列与上述领域事实一一对应，并至少建立以下索引：

- `occurred_at_utc_ms`；
- `transition, occurred_at_utc_ms`；
- `task_id, occurred_at_utc_ms`。

事件表通过任务外键使用 `ON DELETE CASCADE`。数据库不建立每日、每周、类别汇总或按时率汇总表，避免重复汇总值与原始事实失去一致性。

迁移必须在单个 Schema 事务内从 v3 升级到 v4，完整保留任务、类别和依赖关系；任何 DDL、索引或版本号更新失败都必须回滚。

### 3.3 原子状态转换

状态更新和事件插入必须属于同一个 Repository 事务：

```text
TaskService 计算合法状态变化和事件快照
  → Repository 按 expectedStatus 条件更新任务
  → Repository 插入对应任务事件
  → 全部成功后 COMMIT
```

单项开始、取消、完成和重做也必须使用单元素原子转换批次，不能继续先更新任务、再单独追加事件。批量归档和恢复对全部任务及全部事件整批提交或整批回滚。事务成功后 `TaskService` 只发送一次 `tasksChanged()`；失败路径不发送失效信号。

Repository 查询端口只按 UTC 范围返回普通 `TaskActivityEvent`，不得返回 SQL 类型、统计汇总、Chart 系列或展示字符串。永久删除端口继续负责任务和全部关联数据的原子删除。

## 4. MVVM 数据流与职责

### 4.1 查询链路

```text
StatisticsPage（Qt Widgets / Qt Charts）
  → StatisticsContract 与只读列表 Contract
  → StatisticsViewModel
  → StatisticsService
  → ITaskActivityRepository / 任务与依赖 Repository 接口
  → SQLite 实现
```

反向刷新链路为：

```text
任务或依赖写入成功
  → TaskService 失效信号
  → StatisticsViewModel 重新请求 StatisticsService 快照
  → Contract getter、Role 与通知更新
  → StatisticsPage 更新数字卡片和 Chart series
```

Statistics ViewModel 不得调用任务列表、焦点或图 ViewModel。`AppViewModel` 拥有统计 ViewModel，`src/app` 组合根创建 Repository、`StatisticsService`、具体 ViewModel，并把 Statistics Contract 引用注入主窗口。

### 4.2 Model

`StatisticsService` 负责本地日期边界、同期区间、分桶、完成次数、按时率、类别构成、任务健康和变化语义。它依赖原始事件、任务与依赖 Repository 接口，不调用具体 SQLite 实现，也不返回 Qt Charts 类型。

推荐的 Model 输出包括固定概览、所选周期趋势桶、类别桶、健康快照和结构化变化语义。积极、风险或中性的判断也由 Model 给出，例如完成增加为积极、逾期增加为风险；Widget 不能根据数字正负自行解释业务含义。

### 4.3 ViewModel Contracts 与具体 ViewModel

页面级 `StatisticsContract` 继承抽象 `QObject`，提供概览 getter、当前趋势范围、强类型范围切换/刷新命令和精确通知。趋势、类别与健康序列分别通过继承 `QAbstractListModel` 的窄列表 Contract 暴露稳定 Role，例如标签、数值、Tooltip、语义色索引和无障碍说明。

`StatisticsViewModel` 只负责：

- 调用 `StatisticsService` 并持有当前范围这一会话状态；
- 把领域结果投影为中文数字、百分比、变化文案和稳定 Role；
- 监听任务/依赖失效信号，在页面刷新或跨本地午夜时重新投影；
- 对无变化结果避免发送多余通知。

它不得计算日期桶、按时率、阻塞或逾期，不得访问 SQL，不得包含 `QChart`、`QBarSeries` 等 Qt Charts 类型。

### 4.4 Qt Widgets 与 Qt Charts

Qt Charts 只作为 `smartmate_widgets` 的私有 View 依赖使用，相关代码位于 `src/view/widgets/statistics`。Widgets 只读取 Contract getter/Role，并把已经聚合的数据转换成 Chart series；坐标、柱宽、悬停命中和动画插值属于展示计算。

固定组件映射如下：

| 信息 | 展示组件 |
| --- | --- |
| 今日完成、本周完成、当前逾期 | 普通 `QFrame`、`QLabel` 数字卡片 |
| 本周按时完成率 | `QPieSeries` 环形图与中心 `QLabel` |
| 完成趋势 | `QBarSeries`、`QBarCategoryAxis`、`QValueAxis` |
| 类别完成构成 | `QHorizontalBarSeries` 与分类/数值轴 |
| 当前任务健康 | 普通 Widgets 横向进度行 |

Qt Charts 在 Qt 6.10 已被标记为 deprecated，但 SmartMate 固定使用 Qt 6.10.2 且正式前端必须是纯 Qt Widgets，因此接受这一隔离依赖以降低第一版图表开发成本。禁止使用 Qt Graphs、`QQuickWidget`、QML 或 Qt Quick 替代；禁止向非 View 层泄漏 Charts API。未来替换图表实现时不得改变 Statistics Contract 或 Model 统计公式。

统计功能实现完成后，构建目标才加入：

```text
smartmate_widgets → smartmate_common + smartmate_viewmodel_contracts
                  + Qt6::Widgets + Qt6::Charts
```

当前只完成 Model 数据底座；Qt Charts 仍不是现有运行依赖，必须等 Widgets 统计页实现时再加入。

## 5. 页面与交互

### 5.1 信息布局

正常宽度下按以下层级展示：

1. 顶部四张概览卡：今日完成、本周完成、本周按时完成率、当前逾期；
2. 中部完成趋势和周期切换；
3. 底部当前任务健康与所选周期类别构成。

页面放入纵向 `QScrollArea`。宽度小于约 1050 像素时，概览卡改为 2×2，趋势、健康和类别构成改为单列；不得通过压缩坐标标签破坏可读性。

### 5.2 图表交互

- 周期按钮固定为“近 7 天”“近 30 天”“近 12 周”，只调用 Contract 强类型命令；
- 柱体悬停显示本地日期范围和完成次数；
- 当前日期或当前周使用主题强调色，其他柱使用柔和强调色；
- 数字变化可使用 150～250ms 轻量动画，不启用自由缩放、拖动或复杂 Chart 动画；
- 类别超过五项时只展示前五项和“其他”，排序由 Model 输出；
- 没有事件时保留稳定页面结构并显示引导文案，不呈现误导性的全零仪表盘。

### 5.3 主题与无障碍

Chart 背景、绘图区、坐标文字、网格线和系列颜色由纯 View `WidgetTheme` 适配，不使用会覆盖现有青绿/清蓝主题的内置 Chart 主题。颜色必须同时配合数字、箭头或文字；变化不能只用红绿表示。

每张数字卡和图表都必须设置可访问名称与简短说明。图表旁保留可由辅助技术读取的文字摘要，例如“最近 7 天完成 14 项，比此前 7 天增加 3 项”；Tooltip 不能成为读取精确数值的唯一方式。

## 6. 实施阶段与验收

### 阶段 1：构建决策与架构守卫

- 在统计实现分支中为 `smartmate_widgets` 私有加入 `Qt6::Charts`；
- 守卫 Qt Charts 头文件和链接不能进入 Contracts、ViewModel、Model 或 Persistence；
- 部署检查要求存在 `Qt6Charts.dll`，同时继续拒绝 QML/Quick 运行库和 `qml/` 目录。

完成标准：纯 Widgets 离屏测试能构造空 `QChartView`，正式目标与发布目录不包含 QML/Quick。

### 阶段 2：事件与持久化（已完成）

- 实现 `TaskActivityEvent`、Schema v4、事件查询端口及原子状态/事件写入；
- 覆盖 v3 迁移、失败回滚、批量原子性和永久删除级联测试。

完成标准：任何成功状态转换都有且只有一条事件，任何失败转换都不改变任务或事件。

### 阶段 3：Model 统计（已完成）

- 实现 `StatisticsService`、周期查询、分桶、同期比较、按时率、类别构成和任务健康；
- 使用固定 UTC 时间与时区验证自然日、周一周界、零桶、重做再次完成和零分母。

完成标准：全部公式无需启动 GUI 或 SQLite 即可通过 Fake Repository 独立测试。

### 阶段 4：Contracts 与 ViewModel

- 实现 Statistics 页面/列表 Contracts、具体 ViewModel、范围会话状态和失效刷新；
- 使用 `QSignalSpy`、`QAbstractItemModelTester` 验证初始 getter、稳定 Role 与精确通知。

完成标准：ViewModel 不包含业务公式或 Charts 类型，Fake Repository 数据能够完整投影。

### 阶段 5：Widgets 图表

- 实现统计页面、数字卡、环形图、趋势柱状图、类别横条图和响应式布局；
- 使用 Fake Contract 验证初始同步、范围命令、模型刷新、空状态、主题和无障碍文本。

完成标准：Widget 不依赖具体 ViewModel/Service/Repository，900×620 最小窗口可完整滚动访问。

### 阶段 6：集成与部署

- 在组合根注入统计纵向链路并加入主窗口统计导航；
- 使用内存 SQLite 验证完成、重做再完成、归档和永久删除后的图表刷新；
- 完成 Debug 全量测试和 Release 部署验证。

完成标准：状态和事件始终一致，图表自动刷新，发布目录包含 Qt Charts 且不包含任何 QML/Quick 文件。

## 7. 后续专注统计

专注模块完成后新增原始 `FocusSession` 记录，由 Repository 返回已经结束且扣除暂停时长的会话事实，`StatisticsService` 再计算今日/本周专注时长和每日趋势。跨本地午夜的会话应按自然日切分。

预计与实际对比只纳入同时具有预计用时快照和有效专注记录的完成周期，分别展示预计总时长、实际专注时长、绝对偏差和实际/预计比例。完成数量与专注时长单位不同，必须使用独立图表，禁止双 Y 轴混合。
