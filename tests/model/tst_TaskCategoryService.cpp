#include "fakes/FakeTaskCategoryRepository.h"

#include "domain/TaskCategory.h"
#include "services/TaskCategoryService.h"

#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

using smartmate::model::TaskCategory;
using smartmate::model::TaskCategoryColor;
using smartmate::model::TaskCategoryDraft;
using smartmate::model::TaskCategoryError;
using smartmate::model::TaskCategoryService;
using smartmate::model::taskCategoryColorFromStorageText;
using smartmate::model::taskCategoryColorToStorageText;
using smartmate::model::taskCategoryNameKey;
using smartmate::tests::FakeTaskCategoryRepository;

namespace {

[[nodiscard]] QDateTime timestamp()
{
    return QDateTime::fromMSecsSinceEpoch(1700000000000, QTimeZone::UTC);
}

[[nodiscard]] TaskCategory category(QString name,
                                    TaskCategoryColor color = TaskCategoryColor::Blue)
{
    return {QUuid::createUuid(), std::move(name), color, timestamp(), timestamp()};
}

} // namespace

/// 验证类别名称唯一性、固定颜色和Service通知语义均由Model集中负责。
class TaskCategoryServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void normalizesNamesAndColors();
    void createsAndSortsCategories();
    void rejectsInvalidAndDuplicateNames();
    void updatesWithoutNotifyingForNoChange();
    void deletesAndReportsUnassignedTasks();
    void mapsRepositoryFailures();
};

void TaskCategoryServiceTest::normalizesNamesAndColors()
{
    QCOMPARE(taskCategoryNameKey(QStringLiteral("  Ｗork  ")),
             taskCategoryNameKey(QStringLiteral("work")));
    const QList<TaskCategoryColor> colors{
        TaskCategoryColor::Blue, TaskCategoryColor::Teal,
        TaskCategoryColor::Green, TaskCategoryColor::Amber,
        TaskCategoryColor::Orange, TaskCategoryColor::Rose,
        TaskCategoryColor::Violet, TaskCategoryColor::Slate};
    for (const TaskCategoryColor color : colors) {
        const QString text = taskCategoryColorToStorageText(color);
        QVERIFY(!text.isEmpty());
        QCOMPARE(taskCategoryColorFromStorageText(text),
                 std::optional<TaskCategoryColor>{color});
    }
    QVERIFY(!taskCategoryColorFromStorageText(QStringLiteral("unknown")).has_value());
}

void TaskCategoryServiceTest::createsAndSortsCategories()
{
    const TaskCategory work = category(QStringLiteral("工作"));
    FakeTaskCategoryRepository repository{{work}};
    TaskCategoryService service{repository};
    QSignalSpy changedSpy{&service, &TaskCategoryService::categoriesChanged};

    const auto created = service.createCategory(
        {QStringLiteral("  学习  "), TaskCategoryColor::Green});
    QVERIFY(created.ok());
    QCOMPARE(created.value->name, QStringLiteral("学习"));
    QCOMPARE(created.value->color, TaskCategoryColor::Green);
    QVERIFY(created.value->createdAtUtc.isValid());
    QCOMPARE(created.value->createdAtUtc.timeSpec(), Qt::UTC);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(repository.insertCount(), 1);

    const auto listed = service.listCategories();
    QVERIFY(listed.ok());
    QCOMPARE(listed.value->size(), 2);
}

void TaskCategoryServiceTest::rejectsInvalidAndDuplicateNames()
{
    FakeTaskCategoryRepository repository{{category(QStringLiteral("Work"))}};
    TaskCategoryService service{repository};

    QCOMPARE(service.createCategory({QStringLiteral("   "), TaskCategoryColor::Blue}).error,
             TaskCategoryError::EmptyName);
    QCOMPARE(service.createCategory({QString(51, QLatin1Char('a')),
                                     TaskCategoryColor::Blue}).error,
             TaskCategoryError::NameTooLong);
    const QString emoji = QString::fromUcs4(U"😀");
    QVERIFY(service.createCategory({emoji.repeated(50),
                                    TaskCategoryColor::Teal}).ok());
    QCOMPARE(service.createCategory({emoji.repeated(51),
                                     TaskCategoryColor::Teal}).error,
             TaskCategoryError::NameTooLong);
    QCOMPARE(service.createCategory({QStringLiteral("ｗＯＲＫ"),
                                     TaskCategoryColor::Blue}).error,
             TaskCategoryError::DuplicateName);
    QCOMPARE(service.createCategory({QStringLiteral("Travel"),
                                     static_cast<TaskCategoryColor>(999)}).error,
             TaskCategoryError::InvalidColor);
    // 50个补充平面字符是合法边界，只有这一笔写入应当成功。
    QCOMPARE(repository.insertCount(), 1);
}

void TaskCategoryServiceTest::updatesWithoutNotifyingForNoChange()
{
    const TaskCategory stored = category(QStringLiteral("学习"),
                                         TaskCategoryColor::Blue);
    FakeTaskCategoryRepository repository{{stored}};
    TaskCategoryService service{repository};
    QSignalSpy changedSpy{&service, &TaskCategoryService::categoriesChanged};

    QVERIFY(service.updateCategory(
        stored.id, {stored.name, stored.color}).ok());
    QCOMPARE(repository.updateCount(), 0);
    QCOMPARE(changedSpy.count(), 0);

    const auto updated = service.updateCategory(
        stored.id, {QStringLiteral("课程"), TaskCategoryColor::Violet});
    QVERIFY(updated.ok());
    QCOMPARE(repository.updateCount(), 1);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(updated.value->createdAtUtc, stored.createdAtUtc);
    QCOMPARE(updated.value->name, QStringLiteral("课程"));
}

void TaskCategoryServiceTest::deletesAndReportsUnassignedTasks()
{
    const TaskCategory stored = category(QStringLiteral("旅游"));
    FakeTaskCategoryRepository repository{{stored}};
    repository.setUnassignedTaskCount(3);
    TaskCategoryService service{repository};
    QSignalSpy changedSpy{&service, &TaskCategoryService::categoriesChanged};
    QSignalSpy assignmentSpy{
        &service, &TaskCategoryService::taskCategoryAssignmentsChanged};

    const auto deleted = service.deleteCategory(stored.id);
    QVERIFY(deleted.ok());
    QCOMPARE(deleted.value->category, stored);
    QCOMPARE(deleted.value->unassignedTaskCount, 3);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(assignmentSpy.count(), 1);
    QVERIFY(repository.lastDeletionTimeUtc().isValid());
    QCOMPARE(repository.lastDeletionTimeUtc().timeSpec(), Qt::UTC);
}

void TaskCategoryServiceTest::mapsRepositoryFailures()
{
    FakeTaskCategoryRepository repository;
    TaskCategoryService service{repository};
    repository.setReadFailure(true);
    QCOMPARE(service.listCategories().error,
             TaskCategoryError::PersistenceFailure);
    QCOMPARE(service.createCategory({QStringLiteral("学习"),
                                     TaskCategoryColor::Blue}).error,
             TaskCategoryError::PersistenceFailure);
}

QTEST_GUILESS_MAIN(TaskCategoryServiceTest)

#include "tst_TaskCategoryService.moc"
