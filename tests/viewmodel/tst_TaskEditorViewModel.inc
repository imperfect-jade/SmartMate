// 编辑器 ViewModel 的类型化草稿、校验委托与原子创建测试。

void TaskViewModelsTest::editorCreatesACompleteTypedDraft()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    const QTimeZone timeZone = QTimeZone::fromSecondsAheadOfUtc(8 * 60 * 60);
    TaskEditorViewModel editor{service, timeZone};

    editor.beginCreate();
    QCOMPARE(editor.currentStatusText(), QStringLiteral("待办"));
    QCOMPARE(editor.priorityOptions().size(), 4);
    QCOMPARE(editor.metaObject()->indexOfProperty("statusIndex"), -1);
    QCOMPARE(editor.metaObject()->indexOfProperty("statusOptions"), -1);
    QCOMPARE(editor.metaObject()->indexOfMethod("setStatusIndex(int)"), -1);
    QVERIFY(!editor.canSave());

    editor.setTitle(QStringLiteral("  MVVM 大作业  "));
    editor.setDescription(QStringLiteral("实现任务编辑草稿"));
    editor.setPriorityIndex(static_cast<int>(TaskPriority::Urgent));
    QVERIFY(editor.setDeadlineSelection(2030, 6, 15, 8, 30));
    QVERIFY(editor.setEstimatedDuration(0, 1, 30));
    QVERIFY(editor.hasDeadline());
    QCOMPARE(editor.deadlineDisplayText(), QStringLiteral("2030-06-15 08:30"));
    QCOMPARE(editor.deadlineYear(), 2030);
    QCOMPARE(editor.deadlineMonth(), 6);
    QCOMPARE(editor.deadlineDay(), 15);
    QCOMPARE(editor.deadlineHour(), 8);
    QCOMPARE(editor.deadlineMinute(), 30);
    QVERIFY(editor.hasEstimatedDuration());
    QCOMPARE(editor.estimatedDurationDisplayText(), QStringLiteral("1小时 30分钟"));
    QVERIFY(editor.dirty());
    QVERIFY(editor.canSave());

    QSignalSpy savedSpy{&editor, &TaskEditorViewModel::saved};
    QVERIFY(editor.save());

    QCOMPARE(savedSpy.count(), 1);
    QCOMPARE(repository.tasks().size(), 1);
    const Task &created = repository.tasks().constFirst();
    QCOMPARE(created.title(), QStringLiteral("MVVM 大作业"));
    QCOMPARE(created.description(), QStringLiteral("实现任务编辑草稿"));
    QCOMPARE(created.status(), TaskStatus::Todo);
    QCOMPARE(created.priority(), TaskPriority::Urgent);
    QCOMPARE(created.estimatedMinutes(), std::optional<int>{90});
    QCOMPARE(created.deadline()->toTimeZone(timeZone).toString(
                 QStringLiteral("yyyy-MM-dd HH:mm")),
             QStringLiteral("2030-06-15 08:30"));
    QVERIFY(!editor.dirty());
    QVERIFY(!editor.canSave());
}


void TaskViewModelsTest::editorCancelLeavesTheStoredTaskUnchanged()
{
    // 取消时 Repository 保持原值，直接证明编辑草稿与领域实体相互隔离。
    const Task stored = task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                             QStringLiteral("original"), TaskStatus::Todo,
                             1700000001000);
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskEditorViewModel editor{service};

    QVERIFY(editor.beginEdit(stored.id().toString(QUuid::WithoutBraces)));
    editor.setTitle(QStringLiteral("unsaved change"));
    QVERIFY(editor.dirty());
    QSignalSpy cancelledSpy{&editor, &TaskEditorViewModel::cancelled};

    editor.cancel();

    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(repository.findById(stored.id())->title(), QStringLiteral("original"));
}


void TaskViewModelsTest::editorRejectsInvalidSelectionsAndMapsServiceErrors()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskEditorViewModel editor{service};

    editor.beginCreate();
    editor.setTitle(QStringLiteral("second active"));
    QSignalSpy deadlineSpy{&editor, &TaskEditorViewModel::deadlineChanged};
    QSignalSpy durationSpy{&editor, &TaskEditorViewModel::estimatedDurationChanged};

    // 命令入口会防御无效选择；失败既不产生通知，也不污染原草稿。
    QVERIFY(!editor.setDeadlineSelection(2030, 2, 30, 8, 5));
    QVERIFY(!editor.hasDeadline());
    QCOMPARE(deadlineSpy.count(), 0);
    QVERIFY(!editor.setEstimatedDuration(0, 0, 0));
    QVERIFY(!editor.hasEstimatedDuration());
    QCOMPARE(durationSpy.count(), 0);

    QVERIFY(editor.setDeadlineSelection(2030, 2, 3, 8, 5));
    QVERIFY(editor.setEstimatedDuration(0, 0, 30));
    QVERIFY(editor.canSave());
    creationRepository.setWriteFailure(true);
    QVERIFY(!editor.save());
    QVERIFY(editor.errorMessage().contains(QStringLiteral("数据")));
    QVERIFY(editor.dirty());

    creationRepository.setWriteFailure(false);
    QVERIFY(editor.save());
    QCOMPARE(repository.tasks().constFirst().status(), TaskStatus::Todo);
}


void TaskViewModelsTest::editorSupportsDurationBoundariesAndClear()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskEditorViewModel editor{service};

    editor.beginCreate();
    QCOMPARE(editor.minimumEstimatedMinutes(), 1);
    QCOMPARE(editor.maximumEstimatedMinutes(), 525600);
    QCOMPARE(editor.estimatedDurationDisplayText(), QStringLiteral("未设置"));

    QSignalSpy durationSpy{&editor, &TaskEditorViewModel::estimatedDurationChanged};
    QVERIFY(editor.setEstimatedDuration(0, 0, 1));
    QCOMPARE(editor.estimatedDays(), 0);
    QCOMPARE(editor.estimatedHours(), 0);
    QCOMPARE(editor.estimatedMinutePart(), 1);

    QVERIFY(editor.setEstimatedDuration(2, 3, 4));
    QCOMPARE(editor.estimatedDays(), 2);
    QCOMPARE(editor.estimatedHours(), 3);
    QCOMPARE(editor.estimatedMinutePart(), 4);
    QCOMPARE(editor.estimatedDurationDisplayText(), QStringLiteral("2天 3小时 4分钟"));

    QVERIFY(editor.setEstimatedDuration(365, 0, 0));
    QCOMPARE(editor.estimatedDays(), 365);
    QCOMPARE(editor.estimatedHours(), 0);
    QCOMPARE(editor.estimatedMinutePart(), 0);
    QVERIFY(!editor.setEstimatedDuration(365, 0, 1));
    QCOMPARE(editor.estimatedDays(), 365);

    editor.clearEstimatedDuration();
    QVERIFY(!editor.hasEstimatedDuration());
    QCOMPARE(editor.estimatedDurationDisplayText(), QStringLiteral("未设置"));
    QCOMPARE(durationSpy.count(), 4);
}


void TaskViewModelsTest::editorConvertsInjectedTimeZoneAndRejectsDstTransitions()
{
    const QTimeZone newYork{QByteArray{"America/New_York"}};
    if (!newYork.isValid()) {
        QSKIP("The test platform does not provide the America/New_York time zone.");
    }

    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskEditorViewModel editor{service, newYork};
    editor.beginCreate();
    editor.setTitle(QStringLiteral("DST-safe deadline"));
    QSignalSpy deadlineSpy{&editor, &TaskEditorViewModel::deadlineChanged};

    QVERIFY(editor.setDeadlineSelection(2024, 2, 29, 23, 59));
    QCOMPARE(editor.deadlineDisplayText(), QStringLiteral("2024-02-29 23:59"));

    // 春季跳时不存在，秋季回拨时间有歧义；两者都不能替换已选截止时间。
    QVERIFY(!editor.setDeadlineSelection(2024, 3, 10, 2, 30));
    QVERIFY(!editor.setDeadlineSelection(2024, 11, 3, 1, 30));
    QCOMPARE(editor.deadlineDisplayText(), QStringLiteral("2024-02-29 23:59"));
    QCOMPARE(deadlineSpy.count(), 1);

    QVERIFY(editor.save());
    const QDateTime expectedUtc{QDate{2024, 3, 1}, QTime{4, 59}, QTimeZone::UTC};
    QCOMPARE(repository.tasks().constFirst().deadline(),
             std::optional<QDateTime>{expectedUtc});
}


void TaskViewModelsTest::editorPreservesUnchangedDeadlinePrecision()
{
    const QDateTime preciseDeadline{QDate{2032, 5, 6},
                                    QTime{7, 8, 45, 678},
                                    QTimeZone::UTC};
    const Task stored{QUuid::fromString(
                          QStringLiteral("{11111111-1111-1111-1111-111111111111}")),
                      QStringLiteral("preserve precision"),
                      QStringLiteral("description"),
                      TaskPriority::Normal,
                      TaskStatus::Todo,
                      std::nullopt,
                      preciseDeadline,
                      30,
                      utcTime(1700000000000),
                      utcTime(1700000001000)};
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskEditorViewModel editor{service, QTimeZone{QTimeZone::UTC}};

    QVERIFY(editor.beginEdit(stored.id().toString(QUuid::WithoutBraces)));
    editor.setTitle(QStringLiteral("title changed only"));
    QVERIFY(editor.save());
    QCOMPARE(repository.findById(stored.id())->deadline(),
             std::optional<QDateTime>{preciseDeadline});

    // 用户明确重新选择后，选择器精度为分钟，秒和毫秒应归零。
    QVERIFY(editor.setDeadlineSelection(2032, 5, 6, 7, 8));
    QVERIFY(editor.save());
    QCOMPARE(repository.findById(stored.id())->deadline()->time(), QTime(7, 8));
}


void TaskViewModelsTest::editorRejectsSaveWhenNothingChanged()
{
    // 即使绕过 QML 按钮直接调用命令，无改动草稿也不能触发持久化。
    const Task stored = task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                             QStringLiteral("unchanged"), TaskStatus::Todo,
                             1700000001000);
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskEditorViewModel editor{service};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    QSignalSpy savedSpy{&editor, &TaskEditorViewModel::saved};

    QVERIFY(editor.beginEdit(stored.id().toString(QUuid::WithoutBraces)));
    QVERIFY(!editor.dirty());
    QVERIFY(!editor.canSave());
    QVERIFY(!editor.save());

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(savedSpy.count(), 0);
    QCOMPARE(repository.findById(stored.id())->title(), QStringLiteral("unchanged"));
    QCOMPARE(editor.errorMessage(), QStringLiteral("没有需要保存的更改。"));
}


void TaskViewModelsTest::editorSuccessfullyUpdatesAStoredTask()
{
    const Task stored = task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                             QStringLiteral("before"), TaskStatus::Todo,
                             1700000001000);
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    const QTimeZone timeZone = QTimeZone::fromSecondsAheadOfUtc(8 * 60 * 60);
    TaskEditorViewModel editor{service, timeZone};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    QSignalSpy savedSpy{&editor, &TaskEditorViewModel::saved};

    QVERIFY(editor.beginEdit(stored.id().toString(QUuid::WithoutBraces)));
    QCOMPARE(editor.currentStatusText(), QStringLiteral("待办"));
    editor.setTitle(QStringLiteral("after"));
    editor.setDescription(QStringLiteral("updated description"));
    editor.setPriorityIndex(static_cast<int>(TaskPriority::High));
    QVERIFY(editor.setDeadlineSelection(2031, 3, 4, 9, 45));
    QVERIFY(editor.setEstimatedDuration(0, 2, 0));
    QVERIFY(editor.canSave());

    QVERIFY(editor.save());

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(savedSpy.count(), 1);
    const auto updated = repository.findById(stored.id());
    QVERIFY(updated.has_value());
    QCOMPARE(updated->title(), QStringLiteral("after"));
    QCOMPARE(updated->description(), QStringLiteral("updated description"));
    QCOMPARE(updated->status(), TaskStatus::Todo);
    QCOMPARE(updated->priority(), TaskPriority::High);
    QCOMPARE(updated->estimatedMinutes(), std::optional<int>{120});
    QCOMPARE(updated->deadline()->toTimeZone(timeZone).toString(
                 QStringLiteral("yyyy-MM-dd HH:mm")),
             QStringLiteral("2031-03-04 09:45"));
    QVERIFY(!editor.dirty());
    QVERIFY(!editor.canSave());
    QVERIFY(editor.errorMessage().isEmpty());
}


void TaskViewModelsTest::editorRejectsStaleDraftAfterTaskLeavesTodo()
{
    const Task stored = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("保存前改变状态"), TaskStatus::Todo, 1700000001000);
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskEditorViewModel editor{service};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    QSignalSpy savedSpy{&editor, &TaskEditorViewModel::saved};
    const QString stableId = stored.id().toString(QUuid::WithoutBraces);

    QVERIFY(editor.beginEdit(stableId));
    editor.setTitle(QStringLiteral("仍保留在草稿中的标题"));
    QVERIFY(editor.dirty());
    QVERIFY(service.startTask(stored.id()).ok());
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(repository.updateCount(), 1);

    // Service在写入前重新读取实体，防止旧Todo草稿覆盖已经开始的任务。
    QVERIFY(!editor.save());
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(savedSpy.count(), 0);
    QCOMPARE(repository.updateCount(), 1);
    QVERIFY(editor.dirty());
    QCOMPARE(editor.title(), QStringLiteral("仍保留在草稿中的标题"));
    QVERIFY(editor.errorMessage().contains(QStringLiteral("只有待办")));
    QCOMPARE(repository.findById(stored.id())->title(),
             QStringLiteral("保存前改变状态"));
    QCOMPARE(repository.findById(stored.id())->status(), TaskStatus::InProgress);
}


void TaskViewModelsTest::editorCreationPredecessorPickerUsesIsolatedCheckpoints()
{
    const Task active = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("需求确认"), TaskStatus::Todo, 1700000001000);
    const Task archived = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("已归档候选"), TaskStatus::Archived, 1700000002000);
    const Task cancelled = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("已取消候选"), TaskStatus::Cancelled, 1700000003000);
    FakeTaskRepository repository{{archived, cancelled, active}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskEditorViewModel editor{service};

    QVERIFY(editor.beginCreate());
    QCOMPARE(editor.predecessorCandidateCount(), 1);
    QCOMPARE(editor.data(editor.index(0), TaskEditorViewModel::CandidateTaskIdRole)
                 .toString(),
             active.id().toString(QUuid::WithoutBraces));
    QVERIFY(editor.canConfigurePredecessors());

    // 弹窗勾选只改变工作副本，清空后取消必须恢复打开前的主草稿选择。
    editor.beginPredecessorSelection();
    QVERIFY(editor.setCreationPredecessorSelected(
        active.id().toString(QUuid::WithoutBraces), true));
    QCOMPARE(editor.selectedPredecessorCount(), 1);
    QVERIFY(editor.data(editor.index(0), TaskEditorViewModel::CandidateSelectedRole)
                .toBool());
    editor.clearCreationPredecessors();
    QCOMPARE(editor.selectedPredecessorCount(), 0);
    editor.cancelPredecessorSelection();
    QCOMPARE(editor.selectedPredecessorCount(), 0);

    editor.beginPredecessorSelection();
    QVERIFY(editor.setCreationPredecessorSelected(
        active.id().toString(QUuid::WithoutBraces), true));
    editor.acceptPredecessorSelection();
    QCOMPARE(editor.selectedPredecessorCount(), 1);
    QCOMPARE(editor.predecessorSummaryText(), QStringLiteral("已选择 1 项"));
    editor.setTitle(QStringLiteral("实现任务页面"));
    QVERIFY(editor.canSave());
    QCOMPARE(editor.selectedPredecessorCount(), 1);
    QCOMPARE(editor.currentStatusText(), QStringLiteral("待办"));

    editor.cancel();
    QCOMPARE(editor.predecessorCandidateCount(), 0);
    QCOMPARE(editor.selectedPredecessorCount(), 0);
    QVERIFY(editor.title().isEmpty());
    QVERIFY(!editor.dirty());
}


void TaskViewModelsTest::editorAtomicCreationFailurePreservesTheWholeDraft()
{
    const Task predecessor = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("接口设计"), TaskStatus::Todo, 1700000001000);
    FakeTaskRepository repository{{predecessor}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskEditorViewModel editor{service};
    QSignalSpy savedSpy{&editor, &TaskEditorViewModel::saved};

    QVERIFY(editor.beginCreate());
    editor.setTitle(QStringLiteral("实现接口"));
    editor.beginPredecessorSelection();
    QVERIFY(editor.setCreationPredecessorSelected(
        predecessor.id().toString(QUuid::WithoutBraces), true));
    editor.acceptPredecessorSelection();
    creationRepository.setWriteFailure(true);

    QVERIFY(!editor.save());
    QCOMPARE(repository.tasks().size(), 1);
    QVERIFY(dependencyRepository.dependencies().isEmpty());
    QCOMPARE(editor.title(), QStringLiteral("实现接口"));
    QCOMPARE(editor.selectedPredecessorCount(), 1);
    QVERIFY(editor.dirty());
    QCOMPARE(savedSpy.count(), 0);

    creationRepository.setWriteFailure(false);
    QVERIFY(editor.save());
    QCOMPARE(repository.tasks().size(), 2);
    QCOMPARE(dependencyRepository.dependencies().size(), 1);
    QCOMPARE(savedSpy.count(), 1);
}
