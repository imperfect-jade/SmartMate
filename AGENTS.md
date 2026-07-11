# SmartMate 开发规则

本文件约束整个仓库。后续任何修改都必须保持以下MVVM边界。

## 固定依赖方向

`View（QML）→ ViewModel → Model Service → Repository接口`

Persistence只实现Repository接口。`src/app`是唯一组合根，可以看见具体实现。禁止反向依赖和跨层捷径。

## 各层职责

- `src/model`必须负责实体、状态转换、业务校验、依赖与规划算法、统计、服务编排和Repository接口。
- 领域实体和算法必须是普通C++类型；禁止依赖View、ViewModel、Qt Quick、QML运行时或Qt SQL。
- 引入持久化后，SQL与`QSettings`只能位于`src/model/persistence`。
- `src/viewmodel`只负责可观察展示状态、命令、输入草稿和错误映射；禁止编写SQL、业务规则、具体持久化代码或操纵View。
- ViewModel只可使用QML类型注册元数据；禁止使用`QQmlEngine`、`QQmlContext`、`QQuickItem`等视图控制API。
- ViewModel之间禁止直接调用；`AppViewModel`可以拥有并协调子ViewModel。
- `src/view`中的QML只负责布局、绑定、动画和转发事件；禁止调用Service/Repository，禁止实现校验、状态转换、排序和规划。
- `src/app`只负责进程配置、对象创建、依赖注入和启动QML引擎。

## 明确禁止

- 禁止Controller层和`*Controller`类型。
- 禁止Service Locator和全局业务单例。
- 禁止C++通过`findChild`、`setProperty`等方式操纵QML控件。
- 禁止向QML暴露`QSqlTableModel`。
- 禁止用列表行号作为任务身份，必须使用`TaskId`。
- 禁止由QML创建ViewModel；其生命周期必须由C++管理。
- 禁止View或ViewModel绕过Model Service直接写Repository。

## 任务模块规则

- `Task`必须是普通C++类型；标题、时间、状态和优先级校验必须位于Model。
- 任意时刻最多一个进行中任务；该规则必须由`TaskService`执行并测试。
- 删除任务表示归档，不做物理删除；归档时保存原状态，恢复时恢复原状态。
- ViewModel只能维护编辑草稿、列表投影、命令和中文错误映射，不能复制业务校验。
- QML只能使用稳定的`TaskId`提交创建、编辑、归档和恢复操作。
- 截止时间与预计用时必须通过类型化选择器进入ViewModel；禁止恢复自由文本格式解析。
- 自动排序规则和推荐理由必须由Model产生；ViewModel只能执行搜索、优先级筛选和文案投影，QML不得自行筛选或排序。
- 第一版任务依赖只允许Finish-to-Start；多个前置任务采用AND语义，全部满足后后继任务才能开始或完成。
- 依赖端点、自依赖、重复关系、循环路径、阻塞状态、自动解锁和依赖感知排序必须由Model计算并测试。
- 只有活动待办任务可以修改自己的前置集合；任务归档不得删除已有关系，归档前已完成的任务继续满足依赖。
- 阻塞原因、解锁数量和排序位置是派生状态，不得写入SQLite；QML不得遍历依赖图或自行判断任务是否可执行。
- 新建任务携带前置关系时，任务与全部依赖边必须通过独立Repository端口在同一事务中原子写入；禁止先建任务再逐条补边。
- 有前置任务的新任务只能创建为待办；创建候选范围、端点有效性和图一致性必须由Model最终校验。
- 依赖图的拓扑层级、连接闭包和边满足状态属于Model；节点像素坐标和箭头几何属于ViewModel；QML只负责Shape绘制、平移缩放和稳定TaskId事件转发。
- 依赖图布局、缩放、选中状态、阻塞状态和解锁数量均为派生展示状态，不得写入SQLite或`QSettings`。
- 搜索词与优先级筛选属于会话级展示状态，不得写入SQLite或`QSettings`；重新启动应用后恢复默认条件。
- Repository接口不得包含Qt SQL类型；SQLite和SQL语句只能位于`src/model/persistence`。
- Task Service、SQLite Repository和Task ViewModel必须分别添加独立测试。

## 代码注释要求

- 注释以中文为主，类型名、API、SQL、UTC、MVVM等技术术语保留英文。
- 公共类、接口和含义不明显的函数使用`///`说明职责、输入输出、错误语义与边界；无需注释简单getter/setter。
- 重要成员变量必须说明用途、单位、所有权、生命周期或可空语义；无需逐个注释临时变量和自解释变量。
- 实现注释必须解释设计原因、业务约束和不可破坏的架构边界，不得逐行翻译代码。
- QML注释只说明界面分区、数据绑定和事件转发；测试注释只说明关键场景验证的规则与回归风险。
- 禁止保留与实现不一致的过期注释。修改行为、接口或约束时，必须同步更新附近注释。

## 修改流程

修改前必须说明所属层、目标数据流和测试方式。新增业务规则必须放入Model并添加Model测试；新增可绑定状态必须放入ViewModel并验证通知信号。改变层边界前必须先更新`docs/architecture.md`。

修改后运行：

```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
cmake --build --preset debug --target all_qmllint
```

禁止提交构建产物、运行数据库、IDE用户配置、凭据、生成缓存或无关改动。
