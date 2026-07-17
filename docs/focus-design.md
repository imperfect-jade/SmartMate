# SmartMate 专注功能设计

> **实现状态**：首版自由专注纵向链路已经完成。当前代码包含专注领域事实、Schema v5、`IFocusSessionRepository`、SQLite 原子读写、`FocusService`、5 秒检查点、启动/退出恢复、任务完成和取消保护、`FocusContract`、`FocusViewModel`、纯 Widgets `FocusPage`、正式组合根注入、导航和最近记录投影；专注统计尚未实现。

## 1. 产品范围

首版专注采用自由正计时，不设置计划时长或番茄钟周期。产品规则固定为：

- 只有进行中的任务可以开始专注；
- 同一时刻全局最多一个活动会话，Running 和 Paused 都属于活动状态；
- 一个任务可以完成多次专注会话；
- Running 会话可以暂停、完成或放弃，Paused 会话可以继续、完成或放弃；
- 暂停期间不累计专注时间；
- 完成专注不会自动完成任务，放弃会物理删除会话及其时间段且不进入历史；
- 正常完成可以少于一分钟，但累计有效专注必须至少一秒；不足一秒仍允许暂停和继续，但不能完成；
- 专注页面首版显示当前会话和最近 50 条完成记录，Repository 接受显式数量上限。

首版只保存可靠的原始时间事实，不实现倒计时、提醒音、后台线程、自定义统计周期或预计/实际用时对比。

## 2. 领域事实

### 2.1 FocusSession

`FocusSession` 表示一次可暂停和继续的专注会话，包含：

- 稳定 `FocusSessionId` 与稳定任务 ID；
- `Running`、`Paused`、`Completed` 三种状态；
- 开始 UTC 和可空结束 UTC；
- 开始专注时的任务标题、预计用时和完整类别快照；
- 可空的任务 `Start` 事件 ID，用于在存在可靠事件时关联本次任务进行周期。

旧数据库没有历史 `Start` 事件时，该关联保持为空。类别 ID、名称和颜色作为同一份历史快照同时存在或同时为空，后续类别重命名不会篡改既有记录。

### 2.2 FocusInterval

`FocusInterval` 表示会话中一段连续 Running 时间，使用从零开始的稳定序号，保存开始 UTC、可空结束 UTC 和最后检查点 UTC。暂停或完成 Running 会话时关闭当前时间段，继续 Paused 会话时创建下一序号时间段。

数据库不保存重复的总专注时长。后续 Model 必须汇总所有已关闭时间段，并在跨本地午夜时按显式时区切分自然日。

## 3. Repository 边界

持久化依赖方向固定为：

```text
FocusService → IFocusSessionRepository ← SqliteTaskRepository
```

Repository 只暴露普通领域类型和结构化写入结果，不暴露 `QSqlQuery`、连接或 SQL 错误码。写入结果区分成功、任务不是进行中、已有活动会话、会话不存在和期望状态冲突；真正的数据库故障继续抛出 `RepositoryException`。

端口提供活动会话、指定会话、稳定排序时间段和最近完成会话查询，并提供原子开始、暂停、继续、完成、放弃、检查点和异常恢复操作。`ITaskActivityRepository` 同时支持查询指定任务在某一时刻前最近的 `Start` 事件。

## 4. Schema v5 与原子语义

Schema v5 新增 `focus_sessions` 与 `focus_intervals`：

- 会话随任务永久删除级联清理；可空任务事件关联在事件删除时置空；
- 受状态约束的唯一活动标记保证全库最多一个 Running 或 Paused 会话；
- Running/Paused 不允许结束时间，Completed 必须具有不早于开始时间的结束时间；
- 时间段以 `(session_id, sequence)` 为复合主键并随会话级联删除；
- 每个会话最多一个未关闭时间段，检查点不得早于开始时间，结束时间不得早于开始或检查点。

每项命令在一个 SQLite 事务内完成：

1. 开始重新确认任务仍为 InProgress 且不存在活动会话，再插入会话和第一个时间段；
2. 暂停关闭 Running 时间段并把会话切换为 Paused；
3. 继续把 Paused 切换为 Running 并插入下一序号时间段；
4. 完成关闭 Running 时间段，或直接结束 Paused 会话；
5. 放弃删除活动会话并依靠级联清理时间段；
6. 检查点只推进 Running 时间段的检查点，不改变会话状态；
7. 异常恢复以最后检查点关闭遗留 Running 时间段，并在同一事务中切换为 Paused。

v1、v2、v3 和 v4 数据库统一原子迁移到 v5，不回填专注记录，也不修改旧任务或任务事件。高于 v5 的数据库版本会被拒绝。

## 5. 后续阶段

### 阶段 1：持久化底座（已完成）

- 完成领域事实、Repository 端口、Schema v5、SQLite 原子操作和独立持久化测试；
- 保持应用正式入口和现有任务命令行为不变。

### 阶段 2：Model Service 与任务保护（已完成）

- `FocusService` 集中执行进行中资格、累计时长、至少一秒完成门槛、当前状态和 UTC 时钟规则；
- Running 会话每 5 秒写入检查点；持续故障保持 Running、自动重试且只发送一次后台错误，后续成功时清除故障状态；
- 启动时把遗留 Running 恢复到最后检查点，正常退出时自动暂停；恢复或退出后累计不足一秒的活动会话自动放弃；
- Running 与 Paused 都阻止关联任务完成或取消，`TaskService` 提供预检，SQLite 状态事务负责封堵并发窗口并整批回滚。

### 阶段 3：Contracts 与 ViewModel（已完成）

- `FocusService` 快照提供唯一进行中候选和权威命令资格，至少一秒完成门槛仍由 Model 计算；
- `FocusContract` 与历史列表 Contract 提供稳定 ID、当前会话、秒级 `H:MM:SS`、命令、保存可靠性和最近 50 条记录；
- `FocusViewModel` 每秒通过 `snapshot(0)`刷新 Running 展示，完整刷新读取最近记录；它只格式化本地时间与文案，不复制会话状态机或累计规则；
- 可选注入 `FocusService` 的 `AppViewModel` 拥有稳定专注子对象，并在专注变化后刷新共享任务计划源；正式组合根将在下一阶段切换到该构造。

### 阶段 4：专注页面与正式接入（已完成）

- 新增纯 Qt Widgets 专注页面，显示当前任务、正计时、暂停/继续/完成/放弃命令和最近记录；
- 通过组合根注入并加入正式导航，Widget 只调用 Contract 强类型命令；
- 主窗口顺序固定为任务、依赖图、专注、统计、设置；无进行中任务时可返回任务页，放弃专注必须确认；
- 任务变化通过 `AppViewModel` 协调刷新专注投影，专注变化通过共享计划源刷新任务资格，子 ViewModel 之间不直接调用。

### 阶段 5：专注统计（待实现）

- 在可靠会话事实稳定后，再把今日/本周专注时长、专注趋势及预计/实际对比接入 `StatisticsService`。

任何后续阶段都必须保持 `FocusPage → Focus Contract → FocusViewModel → FocusService → IFocusSessionRepository`，不得让 View 或 ViewModel 直接查询 SQLite 或自行计算业务时长。
