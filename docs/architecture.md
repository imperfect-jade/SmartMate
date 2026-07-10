# SmartMate MVVM 架构说明

## 1. 架构目标

SmartMate 使用 MVVM 将业务行为与 Qt Quick 界面分离。只有源码依赖与运行时数据流都遵循以下方向时，项目才符合本课程要求：

```text
View（QML） → ViewModel → Model Service → Repository 接口
                                            ↑
                                      持久化实现
```

`src/app` 是唯一的对象组合根：它创建 Repository、Service 和 ViewModel 的具体对象，完成依赖注入并启动 QML 引擎。它不承担 Controller 或业务层职责。

## 2. 各层职责

### 2.1 Model

Model 负责领域实体、输入校验、状态转换、任务依赖、规划算法、统计公式、服务编排与 Repository 接口。

- Domain 与 Service 可以使用 Qt Core 的值类型。
- Domain、Service 和 Repository 接口不能依赖 Qt Quick、QML 运行时、ViewModel 或 View。
- 业务规则必须能够在不启动图形界面的情况下测试。
- `Task` 等领域对象保持为普通 C++ 类型，不为了界面绑定而继承 `QObject`。

持久化实现将在需要 SQLite 与 `QSettings` 时加入 `src/model/persistence`。该目录属于 Model 的基础设施实现，是生产代码中唯一允许使用 Qt SQL 或 `QSettings` 的位置。

### 2.2 ViewModel

ViewModel 负责：

- 适合界面绑定的可观察状态；
- 用户命令及其可用性；
- 表单草稿、选择、筛选等展示状态；
- 将 Model 的结构化错误映射为用户可理解的文本；
- 在 Model 状态变化后重新生成界面投影并发出属性通知。

所有 ViewModel 都由 C++ 创建并持有，QML 不得自行构造。`AppViewModel` 负责拥有和协调子 ViewModel；子 ViewModel 之间不能直接相互调用，而应通过同一个 Model 服务共享状态。

ViewModel 可以包含用于生成 QML 类型元数据的声明，但不能控制 QML 对象、调用具体持久化实现或包含 SQL、依赖算法和统计公式。

### 2.3 View

View 由 QML 布局、属性绑定、样式、动画和事件转发组成。View 可以根据 ViewModel 已提供的语义状态选择颜色或动画，但不能：

- 直接调用 Repository 或 Model Service；
- 实现业务校验、状态转换、循环检测或规划评分；
- 直接访问数据库；
- 以列表行号代替稳定的任务 ID。

主窗口和桌宠都是 View。它们通过各自的 ViewModel 投影观察同一份 Model 状态，因此一处操作会自动反映到另一处，而不是互相操纵界面控件。

## 3. 构建目标与依赖边界

当前目标关系如下：

```text
smartmate_model       → Qt6::Core
smartmate_viewmodel   → smartmate_model + Qt6::Core + QML 注册元数据
smartmate_ui          → Qt6::Quick + Qt6::QuickControls2
SmartMate             → smartmate_viewmodel + smartmate_ui 插件
```

加入持久化后：

```text
smartmate_persistence → smartmate_model + Qt6::Sql
SmartMate             → smartmate_persistence（仅在组合根中使用）
```

`smartmate_viewmodel` 永远不能链接 `smartmate_persistence`。这样 ViewModel 只依赖抽象服务和领域结果，测试时可以替换为 Fake Repository。

## 4. 最小纵向链路

当前骨架用一条最小链路验证 MVVM 和 QML 类型化注入：

1. `main.cpp` 创建 `QGuiApplication`、`AppBootstrapper` 和 `QQmlApplicationEngine`。
2. `AppBootstrapper` 拥有 `AppViewModel`，并将其作为 QML 根对象的初始属性提供。
3. 静态 `SmartMate.ViewModel` 模块将 `AppViewModel` 声明为 QML 不可创建类型。
4. `Main.qml` 声明 `required property AppViewModel appViewModel`。
5. View 将窗口标题绑定到 `applicationName`，不复制或主动查询该状态。

`view.qml_bootstrap` CTest 使用 Qt 的离屏平台执行整条链路，并在根对象创建成功后自动退出。它可以发现 QML 模块缺失、属性没有注入以及运行库加载失败等集成问题。

后续任务功能的数据流将保持一致：

```text
用户点击“完成”
  → QML 转发任务 ID
  → TaskListViewModel 执行命令
  → TaskService 校验并改变领域状态
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
    repositories/            # 持久化端口
    services/                # 应用/领域服务
    persistence/             # SQLite、QSettings；实现时再创建
    planner/                 # 依赖与规划算法；实现时再创建
  viewmodel/
    tasks/                   # 列表与编辑器投影
    planner/                 # 推荐计划投影
    pet/                     # 桌宠展示状态与命令
    focus/                   # 专注会话状态
    statistics/              # 统计结果投影
  view/
    qml/
      pages/
      dialogs/
      components/
      pet/
      focus/
      theme/
```

未来的依赖图应新增图投影 ViewModel 和 QML 页面；Windows 通知应先在 Model 定义网关接口，再由 Windows 适配器实现；更丰富的桌宠动画只改变 QML 与素材，继续消费 `PetViewModel` 提供的 Idle、Working、Urgent、Celebrate 等语义状态。

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

任何需要改变上述边界的修改，必须先更新本文档并解释原因；不能为了快速实现功能绕过分层。
