// 列表 ViewModel 的投影、筛选、焦点与命令测试。

void TaskViewModelsTest::listProjectsActiveAndArchivedTasksInPlanOrder()
{
    // Repository 返回顺序不是 UI 契约；ViewModel 必须保留Model给出的计划顺序。
    const Task older = task(QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
                            QStringLiteral("older"), TaskStatus::Todo, 1700000001000);
    const Task secondAtSameTime =
        task(QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
             QStringLiteral("same time second"), TaskStatus::Done, 1700000003000,
             TaskPriority::High);
    const Task firstAtSameTime =
        task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
             QStringLiteral("same time first"), TaskStatus::InProgress, 1700000003000,
             TaskPriority::Urgent);
    const Task archived = task(QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
                               QStringLiteral("archived"), TaskStatus::Archived,
                               1700000004000);
    FakeTaskRepository repository{{older, secondAtSameTime, archived, firstAtSameTime}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};

    QCOMPARE(viewModel.count(), 3);
    QCOMPARE(idAt(viewModel, 0), QStringLiteral("11111111-1111-1111-1111-111111111111"));
    QCOMPARE(idAt(viewModel, 1), QStringLiteral("33333333-3333-3333-3333-333333333333"));
    QCOMPARE(idAt(viewModel, 2), QStringLiteral("22222222-2222-2222-2222-222222222222"));
    QCOMPARE(viewModel.data(viewModel.index(0), TaskListViewModel::PriorityTextRole).toString(),
             QStringLiteral("紧急"));
    QCOMPARE(viewModel.data(viewModel.index(0), TaskListViewModel::StatusRole).toInt(),
             static_cast<int>(TaskStatusVisual::InProgress));
    QCOMPARE(viewModel.data(viewModel.index(0), TaskListViewModel::PriorityRole).toInt(),
             static_cast<int>(TaskPriorityVisual::Urgent));
    QCOMPARE(viewModel.data(viewModel.index(0),
                            TaskListViewModel::OrderReasonTextRole).toString(),
             QStringLiteral("正在进行"));

    QSignalSpy countSpy{&viewModel, &TaskListViewModel::countChanged};
    viewModel.setShowArchived(true);

    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0), QStringLiteral("44444444-4444-4444-4444-444444444444"));
    QCOMPARE(viewModel.data(viewModel.index(0),
                            TaskListViewModel::OrderReasonTextRole).toString(),
             QStringLiteral("已归档"));
}


void TaskViewModelsTest::listMapsEveryRecommendedOrderReason()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const Task inProgress = task(
        QStringLiteral("{10000000-0000-0000-0000-000000000001}"),
        QStringLiteral("in progress"), TaskStatus::InProgress, 1700000001000,
        TaskPriority::Urgent);
    const Task overdue = task(
        QStringLiteral("{20000000-0000-0000-0000-000000000002}"),
        QStringLiteral("overdue"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent, QStringLiteral("description"), now.addDays(-1));
    const Task urgent = task(
        QStringLiteral("{30000000-0000-0000-0000-000000000003}"),
        QStringLiteral("urgent"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent);
    const Task high = task(
        QStringLiteral("{40000000-0000-0000-0000-000000000004}"),
        QStringLiteral("high"), TaskStatus::Todo, 1700000001000,
        TaskPriority::High);
    const Task upcoming = task(
        QStringLiteral("{50000000-0000-0000-0000-000000000005}"),
        QStringLiteral("upcoming"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal, QStringLiteral("description"), now.addDays(1));
    const Task todo = task(
        QStringLiteral("{60000000-0000-0000-0000-000000000006}"),
        QStringLiteral("todo"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal);
    const Task completed = task(
        QStringLiteral("{70000000-0000-0000-0000-000000000007}"),
        QStringLiteral("completed"), TaskStatus::Done, 1700000001000);
    const Task cancelled = task(
        QStringLiteral("{80000000-0000-0000-0000-000000000008}"),
        QStringLiteral("cancelled"), TaskStatus::Cancelled, 1700000001000);
    const Task archived = task(
        QStringLiteral("{90000000-0000-0000-0000-000000000009}"),
        QStringLiteral("archived"), TaskStatus::Archived, 1700000001000);
    FakeTaskRepository repository{{archived, todo, upcoming, cancelled, high,
                                   overdue, completed, urgent, inProgress}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};

    QCOMPARE(reasonForId(viewModel, inProgress.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("正在进行"));
    QCOMPARE(reasonForId(viewModel, overdue.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("已逾期"));
    QCOMPARE(reasonForId(viewModel, urgent.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("紧急优先"));
    QCOMPARE(reasonForId(viewModel, high.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("高优先"));
    QCOMPARE(reasonForId(viewModel, upcoming.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("截止较近"));
    QCOMPARE(reasonForId(viewModel, todo.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("待办"));
    QCOMPARE(reasonForId(viewModel, completed.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("已完成"));
    QCOMPARE(reasonForId(viewModel, cancelled.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("已取消"));

    viewModel.setShowArchived(true);
    QCOMPARE(reasonForId(viewModel, archived.id().toString(QUuid::WithoutBraces)),
             QStringLiteral("已归档"));
}


void TaskViewModelsTest::listSearchesTrimmedKeywordsInTitleAndDescription()
{
    const Task titleMatch = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("Write MVVM Report"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Low, QStringLiteral("other"));
    const Task descriptionMatch = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("整理资料"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal, QStringLiteral("REPORT 中包含中文说明"));
    const Task noMatch = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("unrelated"), TaskStatus::Todo, 1700000001000,
        TaskPriority::High, QStringLiteral("nothing here"));
    const Task archivedMatch = task(
        QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
        QStringLiteral("archived report"), TaskStatus::Archived, 1700000001000,
        TaskPriority::Urgent);
    FakeTaskRepository repository{{noMatch, archivedMatch, descriptionMatch, titleMatch}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};

    viewModel.setSearchText(QStringLiteral("  report  "));
    QCOMPARE(viewModel.searchText(), QStringLiteral("  report  "));
    QVERIFY(viewModel.hasActiveFilters());
    QCOMPARE(viewModel.count(), 2);
    const QSet<QString> matchingIds{idAt(viewModel, 0), idAt(viewModel, 1)};
    const QSet<QString> expectedIds{
        titleMatch.id().toString(QUuid::WithoutBraces),
        descriptionMatch.id().toString(QUuid::WithoutBraces)};
    QCOMPARE(matchingIds, expectedIds);

    viewModel.setShowArchived(true);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0),
             archivedMatch.id().toString(QUuid::WithoutBraces));

    viewModel.setSearchText(QStringLiteral("   "));
    QCOMPARE(viewModel.count(), 1);
    QVERIFY(!viewModel.hasActiveFilters());
}


void TaskViewModelsTest::listCombinesPrioritySearchAndArchiveFilters()
{
    const Task lowReport = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("report low"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Low);
    const Task urgentReport = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("report urgent"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent);
    const Task urgentOther = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("other urgent"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent);
    const Task archivedUrgentReport = task(
        QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
        QStringLiteral("archived report"), TaskStatus::Archived, 1700000001000,
        TaskPriority::Urgent);
    FakeTaskRepository repository{{urgentOther, archivedUrgentReport,
                                   lowReport, urgentReport}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};

    QCOMPARE(viewModel.priorityFilterOptions(),
             QStringList({QStringLiteral("全部优先级"), QStringLiteral("低"),
                          QStringLiteral("普通"), QStringLiteral("高"),
                          QStringLiteral("紧急")}));
    viewModel.setSearchText(QStringLiteral("report"));
    viewModel.setPriorityFilterIndex(4);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0), urgentReport.id().toString(QUuid::WithoutBraces));

    viewModel.setShowArchived(true);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0),
             archivedUrgentReport.id().toString(QUuid::WithoutBraces));

    viewModel.clearFilters();
    QVERIFY(viewModel.showArchived());
    QCOMPARE(viewModel.searchText(), QString{});
    QCOMPARE(viewModel.priorityFilterIndex(), 0);
    QVERIFY(!viewModel.hasActiveFilters());
    QCOMPARE(viewModel.count(), 1);
}


void TaskViewModelsTest::listFilterPropertiesNotifyOnceAndRejectInvalidIndexes()
{
    const Task stored = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("report"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent);
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};
    QSignalSpy searchSpy{&viewModel, &TaskListViewModel::searchTextChanged};
    QSignalSpy prioritySpy{&viewModel,
                           &TaskListViewModel::priorityFilterIndexChanged};
    QSignalSpy activeSpy{&viewModel, &TaskListViewModel::hasActiveFiltersChanged};
    QSignalSpy resetSpy{&viewModel, &QAbstractItemModel::modelReset};
    QSignalSpy countSpy{&viewModel, &TaskListViewModel::countChanged};

    viewModel.setSearchText(QStringLiteral("report"));
    viewModel.setSearchText(QStringLiteral("report"));
    viewModel.setPriorityFilterIndex(4);
    viewModel.setPriorityFilterIndex(4);
    viewModel.setPriorityFilterIndex(-1);
    viewModel.setPriorityFilterIndex(5);

    QCOMPARE(viewModel.priorityFilterIndex(), 4);
    QCOMPARE(searchSpy.count(), 1);
    QCOMPARE(prioritySpy.count(), 1);
    QCOMPARE(activeSpy.count(), 1);
    QCOMPARE(resetSpy.count(), 2);
    QCOMPARE(countSpy.count(), 2);

    // 分钟刷新读取到相同计划时不得无意义重置列表。
    viewModel.reload();
    QCOMPARE(resetSpy.count(), 2);
    QCOMPARE(countSpy.count(), 2);

    viewModel.clearFilters();
    viewModel.clearFilters();
    QCOMPARE(searchSpy.count(), 2);
    QCOMPARE(prioritySpy.count(), 2);
    QCOMPARE(activeSpy.count(), 2);
    QCOMPARE(resetSpy.count(), 3);
    QCOMPARE(countSpy.count(), 3);
}


void TaskViewModelsTest::listRetainsFiltersAndStableTaskIdAcrossServiceReloads()
{
    const Task stored = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("needle task"), TaskStatus::Done, 1700000001000,
        TaskPriority::Urgent);
    const Task other = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("other"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent);
    FakeTaskRepository repository{{other, stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};

    viewModel.setSearchText(QStringLiteral("needle"));
    viewModel.setPriorityFilterIndex(4);
    QCOMPARE(viewModel.count(), 1);
    const QString stableId = idAt(viewModel, 0);

    QVERIFY(viewModel.archiveTask(stableId));
    QCOMPARE(viewModel.searchText(), QStringLiteral("needle"));
    QCOMPARE(viewModel.priorityFilterIndex(), 4);
    QCOMPARE(viewModel.count(), 0);

    viewModel.setShowArchived(true);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0), stableId);
    QCOMPARE(reasonForId(viewModel, stableId), QStringLiteral("已归档"));
    QVERIFY(viewModel.restoreTask(stableId));
    QCOMPARE(viewModel.count(), 0);

    viewModel.setShowArchived(false);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0), stableId);
}


void TaskViewModelsTest::listSchedulesMinuteRefreshForTimeSensitiveReasons()
{
    const QDateTime deadline = QDateTime::currentDateTimeUtc().addSecs(1);
    const Task soonOverdue = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("等待定时刷新的任务"), TaskStatus::Todo,
        1700000001000, TaskPriority::Normal, QString{}, deadline);
    FakeTaskRepository repository{{soonOverdue}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};
    TaskFocusViewModel focusViewModel{sources.plan, sources.categories};

    const auto timers = sources.plan.findChildren<QTimer *>(
        QString{}, Qt::FindDirectChildrenOnly);
    QCOMPARE(timers.size(), 1);
    QCOMPARE(timers.constFirst()->interval(), 60'000);
    QVERIFY(timers.constFirst()->isActive());

    const auto projectedOverdue = [&viewModel]() {
        return viewModel.data(viewModel.index(0),
                              TaskListViewModel::OverdueRole).toBool();
    };
    QVERIFY(!projectedOverdue());
    QVERIFY(!focusViewModel.focusOverdue());

    // 不必真实等待一分钟：跨过截止时刻后触发共享源的唯一 timeout 通道，
    // 验证一次计划查询会同步更新列表与焦点投影。
    QTRY_VERIFY_WITH_TIMEOUT(QDateTime::currentDateTimeUtc() > deadline, 3'000);
    const int readsBeforeRefresh = repository.findAllCount();
    QVERIFY(QMetaObject::invokeMethod(timers.constFirst(), "timeout",
                                      Qt::DirectConnection));
    QCOMPARE(repository.findAllCount(), readsBeforeRefresh + 1);
    QVERIFY(projectedOverdue());
    QVERIFY(focusViewModel.focusOverdue());
}


void TaskViewModelsTest::listProjectsOverdueStateIndependentlyFromOrderReason()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const Task overdueTodo = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("逾期待办"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal, QString{}, now.addSecs(-60));
    const Task overdueInProgress = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("逾期进行中"), TaskStatus::InProgress, 1700000002000,
        TaskPriority::Normal, QString{}, now.addSecs(-60));
    const Task completed = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("完成后不逾期"), TaskStatus::Done, 1700000003000,
        TaskPriority::Normal, QString{}, now.addSecs(-60));
    const Task upcoming = task(
        QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
        QStringLiteral("尚未逾期"), TaskStatus::Todo, 1700000004000,
        TaskPriority::Normal, QString{}, now.addDays(1));
    FakeTaskRepository repository{{completed, upcoming, overdueTodo,
                                   overdueInProgress}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};
    TaskFocusViewModel focusViewModel{sources.plan, sources.categories};

    const auto overdueFor = [&viewModel](const Task &stored) {
        const int row = rowForId(
            viewModel, stored.id().toString(QUuid::WithoutBraces));
        return row >= 0
            && viewModel.data(viewModel.index(row),
                              TaskListViewModel::OverdueRole).toBool();
    };
    QVERIFY(overdueFor(overdueTodo));
    QVERIFY(overdueFor(overdueInProgress));
    QVERIFY(!overdueFor(completed));
    QVERIFY(!overdueFor(upcoming));
    QCOMPARE(focusViewModel.focusTaskId(),
             overdueInProgress.id().toString(QUuid::WithoutBraces));
    QVERIFY(focusViewModel.focusOverdue());
    QCOMPARE(focusViewModel.focusReasonText(), QStringLiteral("正在进行"));

    const QString inProgressId =
        overdueInProgress.id().toString(QUuid::WithoutBraces);
    QVERIFY(viewModel.completeTask(inProgressId));
    QVERIFY(!overdueFor(overdueInProgress));
    QVERIFY(viewModel.redoTask(inProgressId));
    QVERIFY(overdueFor(overdueInProgress));

    const QString todoId = overdueTodo.id().toString(QUuid::WithoutBraces);
    QVERIFY(viewModel.cancelTask(todoId));
    QVERIFY(!overdueFor(overdueTodo));
    QVERIFY(viewModel.redoTask(todoId));
    QVERIFY(overdueFor(overdueTodo));
}


void TaskViewModelsTest::listProjectsAndExecutesStateActionsByStableTaskId()
{
    // 每次刷新都可能改变行号；全部状态命令必须始终使用稳定 TaskId。
    const Task stored = task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                             QStringLiteral("archive me"), TaskStatus::Todo,
                             1700000001000);
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};
    QSignalSpy changedSpy{&service, &TaskService::tasksChanged};
    const QString stableId = idAt(viewModel, 0);

    const auto action = [&viewModel](const TaskListViewModel::Role role) {
        return viewModel.data(viewModel.index(0), role).toBool();
    };
    const auto verifyActions = [&action](const bool canStart,
                                         const bool canCancel,
                                         const bool canComplete,
                                         const bool canRedo,
                                         const bool canArchive,
                                         const bool canRestore) {
        QCOMPARE(action(TaskListViewModel::CanStartRole), canStart);
        QCOMPARE(action(TaskListViewModel::CanCancelRole), canCancel);
        QCOMPARE(action(TaskListViewModel::CanCompleteRole), canComplete);
        QCOMPARE(action(TaskListViewModel::CanRedoRole), canRedo);
        QCOMPARE(action(TaskListViewModel::CanArchiveRole), canArchive);
        QCOMPARE(action(TaskListViewModel::CanRestoreRole), canRestore);
    };

    verifyActions(true, true, false, false, false, false);

    // Todo 不得跳过 InProgress 直接完成；拒绝时不写入也不通知。
    QVERIFY(!viewModel.completeTask(stableId));
    QCOMPARE(repository.findById(stored.id())->status(), TaskStatus::Todo);
    QCOMPARE(changedSpy.count(), 0);
    viewModel.clearError();

    QVERIFY(viewModel.startTask(stableId));
    QCOMPARE(repository.findById(stored.id())->status(), TaskStatus::InProgress);
    verifyActions(false, true, true, false, false, false);

    QVERIFY(viewModel.completeTask(stableId));
    QCOMPARE(repository.findById(stored.id())->status(), TaskStatus::Done);
    verifyActions(false, false, false, true, true, false);

    QVERIFY(viewModel.redoTask(stableId));
    QCOMPARE(repository.findById(stored.id())->status(), TaskStatus::Todo);
    QVERIFY(viewModel.cancelTask(stableId));
    QCOMPARE(repository.findById(stored.id())->status(), TaskStatus::Cancelled);
    verifyActions(false, false, false, true, true, false);

    QVERIFY(viewModel.archiveTask(stableId));
    QCOMPARE(viewModel.count(), 0);
    QCOMPARE(repository.findById(stored.id())->status(), TaskStatus::Archived);

    viewModel.setShowArchived(true);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0), stableId);
    verifyActions(false, false, false, false, false, true);
    QVERIFY(viewModel.restoreTask(stableId));
    QCOMPARE(viewModel.count(), 0);

    viewModel.setShowArchived(false);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(idAt(viewModel, 0), stableId);
    QCOMPARE(repository.findById(stored.id())->status(), TaskStatus::Cancelled);
    QCOMPARE(changedSpy.count(), 6);
}


void TaskViewModelsTest::onlyTodoTasksCanOpenEditor()
{
    const Task todo = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("可编辑待办"), TaskStatus::Todo,
        1700000001000);
    const Task inProgress = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("进行中不可编辑"), TaskStatus::InProgress,
        1700000002000);
    const Task done = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("完成不可编辑"), TaskStatus::Done,
        1700000003000);
    const Task cancelled = task(
        QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
        QStringLiteral("取消不可编辑"), TaskStatus::Cancelled,
        1700000004000);
    const Task archived = task(
        QStringLiteral("{55555555-5555-5555-5555-555555555555}"),
        QStringLiteral("归档不可编辑"), TaskStatus::Archived,
        1700000005000);
    FakeTaskRepository repository{{todo, inProgress, done, cancelled, archived}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel list{service, sources.plan, sources.categories};
    TaskEditorViewModel editor{service, sources.categories};
    const auto canEdit = [&list](const Task &stored) {
        const int row = rowForId(
            list, stored.id().toString(QUuid::WithoutBraces));
        return row >= 0
            && list.data(list.index(row),
                         TaskListViewModel::CanEditTaskRole).toBool();
    };
    QVERIFY(canEdit(todo));
    QVERIFY(!canEdit(inProgress));
    QVERIFY(!canEdit(done));
    QVERIFY(!canEdit(cancelled));

    const QString todoId = todo.id().toString(QUuid::WithoutBraces);
    QVERIFY(editor.beginEdit(todoId));
    QVERIFY(editor.editMode());
    QCOMPARE(editor.taskId(), todoId);
    editor.cancel();

    for (const Task *stored : {&inProgress, &done, &cancelled, &archived}) {
        QVERIFY(!editor.beginEdit(
            stored->id().toString(QUuid::WithoutBraces)));
        QVERIFY(editor.errorMessage().contains(QStringLiteral("只有待办")));
        QVERIFY(!editor.editMode());
        QVERIFY(editor.taskId().isEmpty());
    }

    list.setShowArchived(true);
    QCOMPARE(list.count(), 1);
    QVERIFY(!list.data(list.index(0),
                       TaskListViewModel::CanEditTaskRole).toBool());
}


void TaskViewModelsTest::listProjectsAndForwardsPermanentDeletionByStableTaskId()
{
    const Task archived = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("永久删除"), TaskStatus::Archived, 1700000001000);
    const Task todo = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("保留待办"), TaskStatus::Todo, 1700000002000);
    FakeTaskRepository repository{{todo, archived}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskDeletionRepository localDeletionRepository{repository,
                                                        dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), localDeletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};
    TaskDetailsViewModel detailsViewModel{sources.plan, sources.categories};

    const int todoRow = rowForId(
        viewModel, todo.id().toString(QUuid::WithoutBraces));
    QVERIFY(todoRow >= 0);
    QVERIFY(!viewModel.data(viewModel.index(todoRow),
                            TaskListViewModel::CanDeletePermanentlyRole).toBool());

    viewModel.setShowArchived(true);
    QCOMPARE(viewModel.count(), 1);
    QVERIFY(viewModel.data(viewModel.index(0),
                           TaskListViewModel::CanDeletePermanentlyRole).toBool());
    const QString archivedId = archived.id().toString(QUuid::WithoutBraces);
    QVERIFY(detailsViewModel.selectTask(archivedId));
    QCOMPARE(detailsViewModel.selectedTaskId(), archivedId);
    QVERIFY(viewModel.deleteArchivedTask(archivedId));
    QCOMPARE(viewModel.count(), 0);
    QVERIFY(detailsViewModel.selectedTaskId().isEmpty());
    QCOMPARE(localDeletionRepository.deletedTaskIds().size(), 1);
    QCOMPARE(localDeletionRepository.deletedTaskIds().constFirst(), archived.id());

    QVERIFY(!viewModel.deleteArchivedTask(QStringLiteral("invalid-id")));
    QVERIFY(viewModel.errorMessage().contains(QStringLiteral("不存在")));
    QCOMPARE(localDeletionRepository.deletedTaskIds().size(), 1);
}

void TaskViewModelsTest::listMaintainsBulkSelectionByStableIdAndResetsAtDisplayBoundaries()
{
    const Task todo = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("不可批量的待办"), TaskStatus::Todo, 1700000001000);
    const Task done = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("可归档完成项"), TaskStatus::Done, 1700000002000);
    const Task cancelled = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("可归档取消项"), TaskStatus::Cancelled, 1700000003000);
    const Task archived = task(
        QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
        QStringLiteral("归档视图任务"), TaskStatus::Archived, 1700000004000);
    FakeTaskRepository repository{{cancelled, archived, todo, done}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchRepository{repository};
    FakeTaskDeletionRepository localDeletionRepository{repository,
                                                        dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchRepository, localDeletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};
    QSignalSpy bulkSpy{&viewModel, &TaskListViewModel::bulkSelectionChanged};

    QCOMPARE(viewModel.bulkSelectableVisibleCount(), 2);
    viewModel.beginBulkSelection();
    QVERIFY(viewModel.bulkSelectionMode());
    const QString todoId = todo.id().toString(QUuid::WithoutBraces);
    const QString doneId = done.id().toString(QUuid::WithoutBraces);
    const QString cancelledId = cancelled.id().toString(QUuid::WithoutBraces);
    QVERIFY(!viewModel.toggleBulkSelection(todoId));
    QVERIFY(viewModel.toggleBulkSelection(doneId));
    QCOMPARE(viewModel.bulkSelectedCount(), 1);
    QVERIFY(viewModel.data(viewModel.index(rowForId(viewModel, doneId)),
                           TaskListViewModel::BulkSelectedRole).toBool());
    QVERIFY(!viewModel.data(viewModel.index(rowForId(viewModel, todoId)),
                            TaskListViewModel::BulkSelectableRole).toBool());

    // Model刷新改变行号后，选择仍由TaskId保留，而不是绑定旧行号。
    const Task reordered{done.id(), done.title(), done.description(),
                         done.priority(), done.status(),
                         done.statusBeforeArchive(), done.deadline(),
                         done.estimatedMinutes(), done.createdAtUtc(),
                         utcTime(1800000000000)};
    QVERIFY(repository.update(reordered));
    viewModel.reload();
    QVERIFY(viewModel.data(viewModel.index(rowForId(viewModel, doneId)),
                           TaskListViewModel::BulkSelectedRole).toBool());

    // 筛选条件变化清空选择但保留模式；恢复结果后可以重新选择。
    viewModel.setSearchText(QStringLiteral("无匹配结果"));
    QCOMPARE(viewModel.bulkSelectedCount(), 0);
    QVERIFY(viewModel.bulkSelectionMode());
    QCOMPARE(viewModel.bulkSelectableVisibleCount(), 0);
    viewModel.setSearchText({});
    QVERIFY(viewModel.toggleBulkSelection(cancelledId));
    QCOMPARE(viewModel.bulkSelectedCount(), 1);

    // 活动/归档边界改变时必须同时清空并退出，避免跨视图误操作。
    viewModel.setShowArchived(true);
    QVERIFY(!viewModel.bulkSelectionMode());
    QCOMPARE(viewModel.bulkSelectedCount(), 0);
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(viewModel.bulkSelectableVisibleCount(), 1);
    QVERIFY(bulkSpy.count() > 0);
}

void TaskViewModelsTest::listExecutesAtomicBulkCommandsAndPreservesFailedSelection()
{
    const Task done = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("批量完成项"), TaskStatus::Done, 1700000001000);
    const Task cancelled = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("批量取消项"), TaskStatus::Cancelled, 1700000002000);
    FakeTaskRepository repository{{cancelled, done}};
    FakeTaskDependencyRepository dependencyRepository{{
        TaskDependency{done.id(), cancelled.id()},
    }};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchRepository{repository};
    FakeTaskDeletionRepository localDeletionRepository{repository,
                                                        dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchRepository, localDeletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};
    QSignalSpy tasksSpy{&service, &TaskService::tasksChanged};
    QSignalSpy dependenciesSpy{&service, &TaskService::dependenciesChanged};

    viewModel.beginBulkSelection();
    viewModel.toggleSelectAllVisible();
    QCOMPARE(viewModel.bulkSelectedCount(), 2);
    QVERIFY(viewModel.allVisibleSelected());
    QVERIFY(viewModel.canBulkArchive());
    QVERIFY(viewModel.archiveSelectedTasks());
    QVERIFY(!viewModel.bulkSelectionMode());
    QCOMPARE(batchRepository.callCount(), 1);
    QCOMPARE(tasksSpy.count(), 1);

    viewModel.setShowArchived(true);
    QCOMPARE(viewModel.count(), 2);
    viewModel.beginBulkSelection();
    viewModel.toggleSelectAllVisible();
    QVERIFY(viewModel.canBulkRestore());
    QVERIFY(viewModel.restoreSelectedTasks());
    QCOMPARE(repository.findById(done.id())->status(), TaskStatus::Done);
    QCOMPARE(repository.findById(cancelled.id())->status(), TaskStatus::Cancelled);
    QCOMPARE(tasksSpy.count(), 2);

    // 再次归档后模拟删除事务失败：选择与模式均保留，便于用户重试。
    QVERIFY(service.archiveTasks({done.id(), cancelled.id()}).ok());
    viewModel.setShowArchived(true);
    viewModel.beginBulkSelection();
    viewModel.toggleSelectAllVisible();
    localDeletionRepository.setWriteFailure(true);
    QVERIFY(!viewModel.deleteSelectedArchivedTasks());
    QVERIFY(viewModel.bulkSelectionMode());
    QCOMPARE(viewModel.bulkSelectedCount(), 2);
    QVERIFY(viewModel.errorMessage().contains(QStringLiteral("批量完成项")));
    QVERIFY(viewModel.errorMessage().contains(
        done.id().toString(QUuid::WithoutBraces).left(8)));
    QCOMPARE(repository.tasks().size(), 2);
    QCOMPARE(dependencyRepository.findAllDependencies().size(), 1);

    localDeletionRepository.setWriteFailure(false);
    QVERIFY(viewModel.deleteSelectedArchivedTasks());
    QVERIFY(!viewModel.bulkSelectionMode());
    QCOMPARE(viewModel.count(), 0);
    QCOMPARE(repository.tasks().size(), 0);
    QCOMPARE(dependencyRepository.findAllDependencies().size(), 0);
    QCOMPARE(dependenciesSpy.count(), 1);

    viewModel.beginBulkSelection();
    QVERIFY(!viewModel.deleteSelectedArchivedTasks());
    QVERIFY(viewModel.bulkSelectionMode());
    QVERIFY(viewModel.errorMessage().contains(QStringLiteral("至少选择")));
    viewModel.cancelBulkSelection();
}

void TaskViewModelsTest::listMapsBatchConflictIdsToTaskTitles()
{
    const Task conflicted = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("发生竞争的任务"), TaskStatus::Done, 1700000001000);
    const Task unaffected = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("未冲突任务"), TaskStatus::Cancelled, 1700000002000);
    FakeTaskRepository repository{{unaffected, conflicted}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskBatchTransitionRepository batchRepository{repository};
    batchRepository.setResult({0, {conflicted.id()}});
    FakeTaskDeletionRepository localDeletionRepository{repository,
                                                        dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchRepository, localDeletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};

    viewModel.beginBulkSelection();
    viewModel.toggleSelectAllVisible();
    QVERIFY(!viewModel.archiveSelectedTasks());
    QCOMPARE(viewModel.bulkSelectedCount(), 2);
    QVERIFY(viewModel.bulkSelectionMode());
    QVERIFY(viewModel.errorMessage().contains(QStringLiteral("发生竞争的任务")));
    QVERIFY(viewModel.errorMessage().contains(
        conflicted.id().toString(QUuid::WithoutBraces).left(8)));
    QVERIFY(!viewModel.errorMessage().contains(QStringLiteral("未冲突任务")));
}


void TaskViewModelsTest::listExposesAndClearsChineseErrors()
{
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};
    QSignalSpy errorSpy{&viewModel, &TaskListViewModel::errorOccurred};

    repository.setReadFailure(true);
    viewModel.reload();

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(!viewModel.errorMessage().isEmpty());
    QVERIFY(viewModel.errorMessage().contains(QStringLiteral("数据")));

    viewModel.clearError();
    QVERIFY(viewModel.errorMessage().isEmpty());
}


void TaskViewModelsTest::listProjectsBlockingAndUnlockInformation()
{
    const Task predecessor = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("需求分析"), TaskStatus::Todo, 1700000001000);
    const Task successor = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("编码"), TaskStatus::Todo, 1700000002000);
    const Task completed = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("已完成"), TaskStatus::Done, 1700000003000);
    FakeTaskRepository repository{{successor, completed, predecessor}};
    FakeTaskDependencyRepository dependencyRepository{{
        TaskDependency{predecessor.id(), successor.id()},
    }};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};

    const int predecessorRow = rowForId(
        viewModel, predecessor.id().toString(QUuid::WithoutBraces));
    const int successorRow = rowForId(
        viewModel, successor.id().toString(QUuid::WithoutBraces));
    const int completedRow = rowForId(
        viewModel, completed.id().toString(QUuid::WithoutBraces));
    QVERIFY(predecessorRow >= 0);
    QVERIFY(successorRow >= 0);
    QVERIFY(completedRow >= 0);

    QCOMPARE(viewModel.data(viewModel.index(successorRow),
                            TaskListViewModel::BlockedRole).toBool(), true);
    QVERIFY(viewModel.data(viewModel.index(successorRow),
                           TaskListViewModel::BlockingReasonTextRole).toString()
                .contains(QStringLiteral("需求分析")));
    QCOMPARE(viewModel.data(viewModel.index(successorRow),
                            TaskListViewModel::PredecessorCountRole).toInt(), 1);
    QCOMPARE(viewModel.data(viewModel.index(predecessorRow),
                            TaskListViewModel::UnlockCountRole).toInt(), 1);
    QCOMPARE(viewModel.data(viewModel.index(successorRow),
                            TaskListViewModel::CanEditDependenciesRole).toBool(), true);
    QCOMPARE(viewModel.data(viewModel.index(completedRow),
                            TaskListViewModel::CanEditDependenciesRole).toBool(), false);

    QVERIFY(service.replaceTaskPredecessors(successor.id(), {}).ok());
    const int refreshedRow = rowForId(
        viewModel, successor.id().toString(QUuid::WithoutBraces));
    QCOMPARE(viewModel.data(viewModel.index(refreshedRow),
                            TaskListViewModel::BlockedRole).toBool(), false);
    QCOMPARE(viewModel.data(viewModel.index(refreshedRow),
                            TaskListViewModel::PredecessorCountRole).toInt(), 0);
}


void TaskViewModelsTest::listFocusProjectionIgnoresFiltersAndTracksInProgress()
{
    const Task urgent = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("现在最值得做"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Urgent, QStringLiteral("description"),
        std::nullopt, std::nullopt, std::nullopt);
    const Task normal = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("普通任务"), TaskStatus::Todo, 1700000002000);
    FakeTaskRepository repository{{normal, urgent}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskListViewModel viewModel{service, sources.plan, sources.categories};
    TaskFocusViewModel focusViewModel{sources.plan, sources.categories};
    QSignalSpy focusChangedSpy{&focusViewModel,
                               &TaskFocusViewModel::focusTaskChanged};

    QCOMPARE(focusViewModel.focusState(), TaskFocusViewModel::FocusState::Suggested);
    QCOMPARE(focusViewModel.focusTaskId(), urgent.id().toString(QUuid::WithoutBraces));
    QCOMPARE(focusViewModel.focusEstimatedMinutes(), 0);
    QCOMPARE(focusViewModel.focusEstimatedText(), QStringLiteral("未设置预计用时"));
    QVERIFY(focusViewModel.focusCanStart());

    viewModel.setSearchText(QStringLiteral("普通"));
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(focusViewModel.focusTaskId(), urgent.id().toString(QUuid::WithoutBraces));

    QVERIFY(viewModel.startTask(urgent.id().toString(QUuid::WithoutBraces)));
    QVERIFY(focusChangedSpy.count() > 0);
    QCOMPARE(focusViewModel.focusState(), TaskFocusViewModel::FocusState::InProgress);
    QCOMPARE(focusViewModel.focusTaskId(), urgent.id().toString(QUuid::WithoutBraces));
    QCOMPARE(focusViewModel.focusEstimatedText(), QStringLiteral("未设置预计用时"));
    QVERIFY(focusViewModel.focusCanComplete());
    const int normalRow = rowForId(viewModel, normal.id().toString(QUuid::WithoutBraces));
    QVERIFY(normalRow >= 0);
    QVERIFY(!viewModel.data(viewModel.index(normalRow),
                            TaskListViewModel::CanStartRole).toBool());

    focusChangedSpy.clear();
    QVERIFY(viewModel.completeTask(urgent.id().toString(QUuid::WithoutBraces)));
    QVERIFY(focusChangedSpy.count() > 0);
    QCOMPARE(focusViewModel.focusState(), TaskFocusViewModel::FocusState::Suggested);
    QCOMPARE(focusViewModel.focusTaskId(), normal.id().toString(QUuid::WithoutBraces));
    QCOMPARE(focusViewModel.focusEstimatedMinutes(), 30);
    QCOMPARE(focusViewModel.focusEstimatedText(), QStringLiteral("预计 30 分钟"));
}


void TaskViewModelsTest::listSelectsStableTaskDetails()
{
    const Task stored = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("详情任务"), TaskStatus::Todo, 1700000001000,
        TaskPriority::High, QStringLiteral("详情描述"),
        utcTime(1600000000000));
    FakeTaskRepository repository{{stored}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    ProjectionSources sources{service};
    TaskDetailsViewModel viewModel{sources.plan, sources.categories};

    QVERIFY(viewModel.selectTask(stored.id().toString(QUuid::WithoutBraces)));
    QCOMPARE(viewModel.selectedTitle(), QStringLiteral("详情任务"));
    QCOMPARE(viewModel.selectedDescription(), QStringLiteral("详情描述"));
    QCOMPARE(viewModel.selectedPriorityText(), QStringLiteral("高"));
    QCOMPARE(viewModel.selectedStatusVisual(), TaskStatusVisual::Todo);
    QCOMPARE(viewModel.selectedPriorityVisual(), TaskPriorityVisual::High);
    QVERIFY(viewModel.selectedOverdue());
    QVERIFY(!viewModel.selectedCreatedAtText().isEmpty());
    QVERIFY(viewModel.selectedCanEditTask());
    viewModel.clearSelection();
    QVERIFY(viewModel.selectedTaskId().isEmpty());
    QCOMPARE(viewModel.selectedStatusVisual(), TaskStatusVisual::Todo);
    QCOMPARE(viewModel.selectedPriorityVisual(), TaskPriorityVisual::Normal);
    QVERIFY(!viewModel.selectedOverdue());
}
