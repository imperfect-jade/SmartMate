#include "persistence/SqliteTaskRepository.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <QUuid>

using smartmate::model::RepositoryException;
using smartmate::model::Task;
using smartmate::model::TaskPriority;
using smartmate::model::TaskStatus;
using smartmate::model::persistence::SqliteTaskRepository;

namespace {

const QDateTime kCreatedAt =
    QDateTime::fromMSecsSinceEpoch(1'752'000'000'123LL, QTimeZone::UTC);
const QDateTime kUpdatedAt =
    QDateTime::fromMSecsSinceEpoch(1'752'000'123'456LL, QTimeZone::UTC);

Task makeTask(
    const QUuid &id,
    QString title = QStringLiteral("编写 SQLite 测试"),
    TaskPriority priority = TaskPriority::Normal,
    TaskStatus status = TaskStatus::Todo,
    std::optional<TaskStatus> statusBeforeArchive = std::nullopt,
    std::optional<QDateTime> deadline = std::nullopt,
    std::optional<int> estimatedMinutes = std::nullopt,
    QDateTime updatedAt = kUpdatedAt)
{
    return Task(
        id,
        std::move(title),
        QStringLiteral("覆盖中文、标点与 emoji：✅"),
        priority,
        status,
        statusBeforeArchive,
        deadline,
        estimatedMinutes,
        kCreatedAt,
        updatedAt);
}

void compareTasks(const Task &actual, const Task &expected)
{
    QCOMPARE(actual.id(), expected.id());
    QCOMPARE(actual.title(), expected.title());
    QCOMPARE(actual.description(), expected.description());
    QCOMPARE(actual.priority(), expected.priority());
    QCOMPARE(actual.status(), expected.status());
    QVERIFY(actual.statusBeforeArchive() == expected.statusBeforeArchive());
    QVERIFY(actual.deadline() == expected.deadline());
    QVERIFY(actual.estimatedMinutes() == expected.estimatedMinutes());
    QCOMPARE(actual.createdAtUtc(), expected.createdAtUtc());
    QCOMPARE(actual.updatedAtUtc(), expected.updatedAtUtc());
}

} // namespace

// 验证领域值与SQLite的双向映射、重复初始化安全性和数据库级约束。
class SqliteTaskRepositoryTest final : public QObject {
    Q_OBJECT

private slots:
    void initializesSchemaIdempotently();
    void roundTripsCompleteUnicodeTask();
    void roundTripsNullOptionals();
    void storesOmittedDescriptionAsEmptyText();
    void persistsAcrossRepositoryReopen();
    void updatesArchivedTaskAndOriginalStatus();
    void rejectsSecondInProgressTask();
    void returnsFalseWhenUpdatingMissingTask();
};

void SqliteTaskRepositoryTest::initializesSchemaIdempotently()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));

    {
        SqliteTaskRepository repository(databasePath);
        QVERIFY(repository.findAll().isEmpty());
    }
    {
        SqliteTaskRepository repository(databasePath);
        QVERIFY(repository.findAll().isEmpty());
    }

    const QString connectionName =
        QStringLiteral("schema_verification_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));

        QSqlQuery versionQuery(database);
        QVERIFY(versionQuery.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(versionQuery.next());
        QCOMPARE(versionQuery.value(0).toInt(), 1);

        QSqlQuery indexQuery(database);
        QVERIFY(indexQuery.exec(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master "
            "WHERE type = 'index' AND name IN ("
            "'idx_tasks_status', 'idx_tasks_deadline', 'idx_tasks_single_in_progress')")));
        QVERIFY(indexQuery.next());
        QCOMPARE(indexQuery.value(0).toInt(), 3);

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void SqliteTaskRepositoryTest::roundTripsCompleteUnicodeTask()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    const auto deadline =
        QDateTime::fromMSecsSinceEpoch(1'752'086'400'789LL, QTimeZone::UTC);
    const Task expected = makeTask(
        QUuid::createUuid(),
        QStringLiteral("完成课程大作业 🧭"),
        TaskPriority::Urgent,
        TaskStatus::Done,
        std::nullopt,
        deadline,
        180);

    repository.insert(expected);

    const auto actual = repository.findById(expected.id());
    QVERIFY(actual.has_value());
    compareTasks(*actual, expected);
}

void SqliteTaskRepositoryTest::roundTripsNullOptionals()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    const Task expected = makeTask(QUuid::createUuid());
    repository.insert(expected);

    const auto actual = repository.findById(expected.id());
    QVERIFY(actual.has_value());
    QVERIFY(!actual->statusBeforeArchive().has_value());
    QVERIFY(!actual->deadline().has_value());
    QVERIFY(!actual->estimatedMinutes().has_value());
    compareTasks(*actual, expected);
}

void SqliteTaskRepositoryTest::storesOmittedDescriptionAsEmptyText()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    // 新建表单未填写可选描述时会产生 null QString；持久化层必须把它映射为
    // 空文本，而不是违反 description NOT NULL 约束的 SQL NULL。
    const Task taskWithoutDescription{
        QUuid::createUuid(),
        QStringLiteral("只填写标题的任务"),
        QString{},
        TaskPriority::Normal,
        TaskStatus::Todo,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        kCreatedAt,
        kUpdatedAt};

    repository.insert(taskWithoutDescription);

    const auto stored = repository.findById(taskWithoutDescription.id());
    QVERIFY(stored.has_value());
    QVERIFY(stored->description().isEmpty());
    QVERIFY(!stored->description().isNull());
}

void SqliteTaskRepositoryTest::persistsAcrossRepositoryReopen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString databasePath = directory.filePath(QStringLiteral("tasks.db"));
    const Task expected = makeTask(
        QUuid::createUuid(),
        QStringLiteral("关闭后仍需存在"),
        TaskPriority::High,
        TaskStatus::Cancelled);

    {
        SqliteTaskRepository repository(databasePath);
        repository.insert(expected);
    }
    {
        SqliteTaskRepository repository(databasePath);
        const auto actual = repository.findById(expected.id());
        QVERIFY(actual.has_value());
        compareTasks(*actual, expected);
    }
}

void SqliteTaskRepositoryTest::updatesArchivedTaskAndOriginalStatus()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    const auto id = QUuid::createUuid();
    repository.insert(makeTask(
        id,
        QStringLiteral("正在处理的任务"),
        TaskPriority::High,
        TaskStatus::InProgress));

    const Task archived = makeTask(
        id,
        QStringLiteral("已归档的任务"),
        TaskPriority::High,
        TaskStatus::Archived,
        TaskStatus::InProgress,
        std::nullopt,
        90,
        kUpdatedAt.addSecs(60));
    QVERIFY(repository.update(archived));

    const auto actual = repository.findById(id);
    QVERIFY(actual.has_value());
    compareTasks(*actual, archived);
}

void SqliteTaskRepositoryTest::rejectsSecondInProgressTask()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    repository.insert(makeTask(
        QUuid::createUuid(),
        QStringLiteral("第一个进行中任务"),
        TaskPriority::Normal,
        TaskStatus::InProgress));

    bool exceptionThrown = false;
    try {
        repository.insert(makeTask(
            QUuid::createUuid(),
            QStringLiteral("第二个进行中任务"),
            TaskPriority::Normal,
            TaskStatus::InProgress));
    } catch (const RepositoryException &) {
        exceptionThrown = true;
    }

    QVERIFY(exceptionThrown);
    QCOMPARE(repository.findAll().size(), 1);
}

void SqliteTaskRepositoryTest::returnsFalseWhenUpdatingMissingTask()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SqliteTaskRepository repository(directory.filePath(QStringLiteral("tasks.db")));

    QVERIFY(!repository.update(makeTask(QUuid::createUuid())));
}

QTEST_MAIN(SqliteTaskRepositoryTest)

#include "tst_SqliteTaskRepository.moc"
