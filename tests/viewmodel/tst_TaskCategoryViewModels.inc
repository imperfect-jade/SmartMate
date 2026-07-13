// 类别相关ViewModel的稳定ID、草稿隔离、组合筛选与分类图投影测试。

void TaskViewModelsTest::categoryViewModelProjectsCrudAndUsageCounts()
{
    const TaskCategory work = category(
        QStringLiteral("{aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa}"),
        QStringLiteral("工作"), TaskCategoryColor::Blue);
    FakeTaskRepository repository{{task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("写报告"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal, QStringLiteral("description"), std::nullopt,
        work.id)}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskCategoryRepository categories{{work}};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categories};
    TaskCategoryService categoryService{categories};
    TaskCategoryViewModel viewModel{service, &categoryService};

    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(viewModel.data(viewModel.index(0),
                            TaskCategoryViewModel::TaskCountRole).toInt(), 1);
    QCOMPARE(viewModel.data(viewModel.index(0),
                            TaskCategoryViewModel::AccentRole).toString(),
             QStringLiteral("#2563eb"));

    viewModel.beginCreate();
    viewModel.setDraftName(QStringLiteral("学习"));
    viewModel.setDraftColorIndex(static_cast<int>(TaskCategoryColor::Green));
    QVERIFY(viewModel.canSave());
    QVERIFY(viewModel.save());
    QCOMPARE(viewModel.count(), 2);

    QVERIFY(viewModel.beginEdit(work.id.toString(QUuid::WithoutBraces)));
    viewModel.setDraftName(QStringLiteral("正式工作"));
    QVERIFY(viewModel.dirty());
    QVERIFY(viewModel.save());
    QCOMPARE(categories.categories().constFirst().name, QStringLiteral("正式工作"));
}

void TaskViewModelsTest::categoryDeletionKeepsOpenEditorDraftClean()
{
    const TaskCategory work = category(
        QStringLiteral("{aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa}"),
        QStringLiteral("工作"), TaskCategoryColor::Blue);
    FakeTaskRepository repository{{task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("编写报告"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal, QStringLiteral("description"), std::nullopt,
        work.id)}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskCategoryRepository categories{{work}};
    categories.setUnassignedTaskCount(1);
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categories};
    TaskCategoryService categoryService{categories};
    TaskEditorViewModel editor{service, categoryService};

    QVERIFY(editor.beginEdit(
        repository.tasks().constFirst().id().toString(QUuid::WithoutBraces)));
    QVERIFY(editor.hasCategory());
    QVERIFY(!editor.dirty());
    QCOMPARE(repository.updateCount(), 0);

    const auto deletion = categoryService.deleteCategory(work.id);
    QVERIFY(deletion.ok());
    QVERIFY(!editor.hasCategory());
    QCOMPARE(editor.selectedCategoryId(), QString{});
    // 删除类别已在Model中原子解除归属，不应伪装成未保存的用户编辑。
    QVERIFY(!editor.dirty());
    QVERIFY(!editor.canSave());
    QCOMPARE(repository.updateCount(), 0);
    QVERIFY(!editor.errorMessage().isEmpty());
}

void TaskViewModelsTest::categoryViewModelRetainsUsageCountsWhenTaskReadFails()
{
    const TaskCategory work = category(
        QStringLiteral("{aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa}"),
        QStringLiteral("工作"), TaskCategoryColor::Blue);
    FakeTaskRepository repository{{task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("编写报告"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal, QStringLiteral("description"), std::nullopt,
        work.id)}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskCategoryRepository categories{{work}};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categories};
    TaskCategoryService categoryService{categories};
    TaskCategoryViewModel viewModel{service, &categoryService};

    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(viewModel.data(viewModel.index(0),
                            TaskCategoryViewModel::TaskCountRole).toInt(), 1);
    QSignalSpy countSpy{&viewModel, &TaskCategoryViewModel::countChanged};
    QSignalSpy resetSpy{&viewModel, &QAbstractItemModel::modelReset};

    repository.setReadFailure(true);
    viewModel.reload();

    // 使用数未知时保留最后一份有效投影，避免删除确认误报“空类别”。
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(viewModel.data(viewModel.index(0),
                            TaskCategoryViewModel::TaskCountRole).toInt(), 1);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(viewModel.errorMessage(),
             QStringLiteral("任务数据访问失败，无法统计类别使用数量。"));
}

void TaskViewModelsTest::editorPersistsStableCategorySelection()
{
    const TaskCategory study = category(
        QStringLiteral("{bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb}"),
        QStringLiteral("学习"), TaskCategoryColor::Violet);
    FakeTaskRepository repository;
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskCategoryRepository categories{{study}};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categories};
    TaskCategoryService categoryService{categories};
    TaskEditorViewModel editor{service, categoryService};

    QVERIFY(editor.beginCreate());
    QCOMPARE(editor.categoryOptions().size(), 2);
    editor.setTitle(QStringLiteral("阅读架构资料"));
    editor.setSelectedCategoryId(study.id.toString(QUuid::WithoutBraces));
    QVERIFY(editor.hasCategory());
    QCOMPARE(editor.selectedCategoryName(), QStringLiteral("学习"));
    QVERIFY(editor.save());
    QCOMPARE(repository.tasks().size(), 1);
    QVERIFY(repository.tasks().constFirst().categoryId().has_value());
    QCOMPARE(*repository.tasks().constFirst().categoryId(), study.id);
}

void TaskViewModelsTest::listCombinesCategoryWithExistingFilters()
{
    const TaskCategory work = category(
        QStringLiteral("{aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa}"),
        QStringLiteral("工作"), TaskCategoryColor::Blue);
    const TaskCategory study = category(
        QStringLiteral("{bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb}"),
        QStringLiteral("学习"), TaskCategoryColor::Green);
    FakeTaskRepository repository{{
        task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
             QStringLiteral("工作报告"), TaskStatus::Todo, 1700000001000,
             TaskPriority::High, QStringLiteral("季度"), std::nullopt, work.id),
        task(QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
             QStringLiteral("课程报告"), TaskStatus::Todo, 1700000002000,
             TaskPriority::High, QStringLiteral("MVVM"), std::nullopt, study.id),
        task(QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
             QStringLiteral("杂项"), TaskStatus::Todo, 1700000003000,
             TaskPriority::Low),
    }};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskCategoryRepository categories{{work, study}};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categories};
    TaskCategoryService categoryService{categories};
    TaskListViewModel viewModel{service, categoryService};

    QVERIFY(viewModel.setCategoryFilter(2, work.id.toString(QUuid::WithoutBraces)));
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(viewModel.data(viewModel.index(0),
                            TaskListViewModel::CategoryNameRole).toString(),
             QStringLiteral("工作"));
    viewModel.setPriorityFilterIndex(3);
    viewModel.setSearchText(QStringLiteral("报告"));
    QCOMPARE(viewModel.count(), 1);
    viewModel.clearFilters();
    QCOMPARE(viewModel.count(), 3);
    QVERIFY(viewModel.setCategoryFilter(1));
    QCOMPARE(viewModel.count(), 1);
    QCOMPARE(viewModel.data(viewModel.index(0),
                            TaskListViewModel::HasCategoryRole).toBool(), false);
}

void TaskViewModelsTest::graphProjectsCoreAndCrossCategoryContext()
{
    const TaskCategory work = category(
        QStringLiteral("{aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa}"),
        QStringLiteral("工作"), TaskCategoryColor::Blue);
    const TaskCategory study = category(
        QStringLiteral("{bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb}"),
        QStringLiteral("学习"), TaskCategoryColor::Green);
    const Task core = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("工作需求"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal, QStringLiteral("description"), std::nullopt, work.id);
    const Task context = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("学习资料"), TaskStatus::Todo, 1700000002000,
        TaskPriority::Normal, QStringLiteral("description"), std::nullopt, study.id);
    const Task unrelated = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("无关学习"), TaskStatus::Todo, 1700000003000,
        TaskPriority::Normal, QStringLiteral("description"), std::nullopt, study.id);
    FakeTaskRepository repository{{unrelated, context, core}};
    FakeTaskDependencyRepository dependencyRepository{{{core.id(), context.id()}}};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskCategoryRepository categories{{work, study}};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categories};
    TaskCategoryService categoryService{categories};
    TaskGraphViewModel graph{service, categoryService};

    QVERIFY(graph.setCategoryFilter(2, work.id.toString(QUuid::WithoutBraces)));
    QCOMPARE(graph.rowCount(), 2);
    const int coreRow = graphRowForId(graph, core.id().toString(QUuid::WithoutBraces));
    const int contextRow = graphRowForId(
        graph, context.id().toString(QUuid::WithoutBraces));
    QVERIFY(coreRow >= 0);
    QVERIFY(contextRow >= 0);
    QCOMPARE(graph.data(graph.index(coreRow), TaskGraphViewModel::CoreNodeRole).toBool(),
             true);
    QCOMPARE(graph.data(graph.index(contextRow),
                        TaskGraphViewModel::CoreNodeRole).toBool(), false);
    QCOMPARE(graph.data(graph.index(contextRow),
                        TaskGraphViewModel::CategoryNameRole).toString(),
             QStringLiteral("学习"));
    QCOMPARE(graph.edges()->rowCount(), 1);
}

void TaskViewModelsTest::graphCategoryFilterFailureRetainsPreviousStateAndSnapshot()
{
    const TaskCategory work = category(
        QStringLiteral("{aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa}"),
        QStringLiteral("工作"), TaskCategoryColor::Blue);
    const Task categorized = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("工作任务"), TaskStatus::Todo, 1700000001000,
        TaskPriority::Normal, QStringLiteral("description"), std::nullopt, work.id);
    const Task uncategorized = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("未分类任务"), TaskStatus::Todo, 1700000002000);
    FakeTaskRepository repository{{categorized, uncategorized}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    FakeTaskCategoryRepository categories{{work}};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categories};
    TaskCategoryService categoryService{categories};
    TaskGraphViewModel graph{service, categoryService};

    QCOMPARE(graph.categoryFilterMode(), 0);
    QCOMPARE(graph.rowCount(), 2);
    QVERIFY(graphRowForId(graph, categorized.id().toString(QUuid::WithoutBraces)) >= 0);
    QVERIFY(graphRowForId(graph, uncategorized.id().toString(QUuid::WithoutBraces)) >= 0);
    QSignalSpy filterSpy{&graph, &TaskGraphViewModel::categoryFilterChanged};
    QSignalSpy graphSpy{&graph, &TaskGraphViewModel::graphChanged};

    repository.setReadFailure(true);
    QVERIFY(!graph.setCategoryFilter(
        2, work.id.toString(QUuid::WithoutBraces)));

    // 子图查询失败必须像一次未提交交互：筛选和画布同时保留。
    QCOMPARE(graph.categoryFilterMode(), 0);
    QCOMPARE(graph.categoryFilterCategoryId(), QString{});
    QCOMPARE(graph.rowCount(), 2);
    QVERIFY(graphRowForId(graph, categorized.id().toString(QUuid::WithoutBraces)) >= 0);
    QVERIFY(graphRowForId(graph, uncategorized.id().toString(QUuid::WithoutBraces)) >= 0);
    QCOMPARE(filterSpy.count(), 0);
    QCOMPARE(graphSpy.count(), 0);
    QVERIFY(!graph.errorMessage().isEmpty());
}
