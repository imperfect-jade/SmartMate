#include "AppViewModel.h"
#include "TaskEditorViewModel.h"
#include "TaskListViewModel.h"
#include "persistence/SqliteTaskRepository.h"
#include "services/TaskService.h"

#include <QSignalSpy>
#include <QTest>
#include <QUuid>

using smartmate::model::TaskService;
using smartmate::model::persistence::SqliteTaskRepository;
using smartmate::viewmodel::AppViewModel;
using smartmate::viewmodel::TaskListViewModel;

/// 覆盖真实 SQLite 纵向链路，防止各层独立测试遗漏默认表单值的存储差异。
class TaskCreationFlowTest final : public QObject {
    Q_OBJECT

private slots:
    void createsAndReopensTaskWhenOptionalDescriptionIsUntouched();
};

void TaskCreationFlowTest::createsAndReopensTaskWhenOptionalDescriptionIsUntouched()
{
    SqliteTaskRepository repository{QStringLiteral(":memory:")};
    TaskService service{repository};
    AppViewModel appViewModel{service};
    auto *editor = appViewModel.taskEditor();
    auto *taskList = appViewModel.taskList();
    QSignalSpy listErrorSpy{taskList, &TaskListViewModel::errorOccurred};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};

    // 精确模拟用户只填写标题、从未操作可选描述框的创建路径。
    editor->beginCreate();
    editor->setTitle(QStringLiteral("只填写标题也能保存"));

    QVERIFY2(editor->save(), qPrintable(editor->errorMessage()));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(listErrorSpy.count(), 0);
    QCOMPARE(taskList->count(), 1);

    const QModelIndex firstTask = taskList->index(0);
    const QString taskId =
        taskList->data(firstTask, TaskListViewModel::TaskIdRole).toString();
    QVERIFY(!QUuid::fromString(taskId).isNull());
    QVERIFY(editor->beginEdit(taskId));
    QCOMPARE(editor->title(), QStringLiteral("只填写标题也能保存"));
    QVERIFY(editor->description().isEmpty());

    const auto stored = repository.findById(QUuid::fromString(taskId));
    QVERIFY(stored.has_value());
    QVERIFY(stored->description().isEmpty());
    QVERIFY(!stored->description().isNull());
}

// Qt SQL 连接依赖 QCoreApplication 生命周期，测试无需 GUI 事件环境。
QTEST_GUILESS_MAIN(TaskCreationFlowTest)

#include "tst_TaskCreationFlow.moc"
