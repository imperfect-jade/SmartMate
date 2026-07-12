// 依赖编辑 ViewModel 的候选投影、草稿与结构化错误测试。

void TaskViewModelsTest::dependencyDraftCancelDoesNotChangeModel()
{
    const Task target = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("目标任务"), TaskStatus::Todo, 1700000001000);
    const Task candidate = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("普通候选"), TaskStatus::Todo, 1700000002000,
        TaskPriority::High);
    const Task completed = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("完成候选"), TaskStatus::Done, 1700000003000);
    const Task selectedArchived = task(
        QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
        QStringLiteral("原有归档前置"), TaskStatus::Archived, 1700000004000);
    const Task unselectedArchived = task(
        QStringLiteral("{55555555-5555-5555-5555-555555555555}"),
        QStringLiteral("不可新增归档"), TaskStatus::Archived, 1700000005000);
    const Task selectedCancelled = task(
        QStringLiteral("{66666666-6666-6666-6666-666666666666}"),
        QStringLiteral("原有取消前置"), TaskStatus::Cancelled, 1700000006000);
    const Task unselectedCancelled = task(
        QStringLiteral("{77777777-7777-7777-7777-777777777777}"),
        QStringLiteral("不可新增取消"), TaskStatus::Cancelled, 1700000007000);
    FakeTaskRepository repository{{target, candidate, completed,
                                   selectedArchived, unselectedArchived,
                                   selectedCancelled, unselectedCancelled}};
    FakeTaskDependencyRepository dependencyRepository{{
        TaskDependency{selectedArchived.id(), target.id()},
        TaskDependency{selectedCancelled.id(), target.id()},
    }};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskDependencyViewModel editor{service};

    QVERIFY(editor.beginEdit(target.id().toString(QUuid::WithoutBraces)));
    // 已取消任务不得作为新候选，但已有取消关系必须继续显示并允许移除。
    QCOMPARE(editor.count(), 4);
    QCOMPARE(editor.selectedCount(), 2);
    QVERIFY(!editor.dirty());

    const QString candidateId = candidate.id().toString(QUuid::WithoutBraces);
    const int candidateRow = dependencyRowForId(editor, candidateId);
    const int archivedRow = dependencyRowForId(
        editor, selectedArchived.id().toString(QUuid::WithoutBraces));
    const int cancelledRow = dependencyRowForId(
        editor, selectedCancelled.id().toString(QUuid::WithoutBraces));
    QVERIFY(candidateRow >= 0);
    QVERIFY(archivedRow >= 0);
    QVERIFY(cancelledRow >= 0);
    QCOMPARE(dependencyRowForId(
                 editor, unselectedCancelled.id().toString(QUuid::WithoutBraces)),
             -1);
    QCOMPARE(editor.data(editor.index(candidateRow),
                         TaskDependencyViewModel::ShortIdRole).toString(),
             QStringLiteral("22222222"));
    QCOMPARE(editor.data(editor.index(candidateRow),
                         TaskDependencyViewModel::PriorityTextRole).toString(),
             QStringLiteral("高"));
    QCOMPARE(editor.data(editor.index(archivedRow),
                         TaskDependencyViewModel::SelectedRole).toBool(), true);
    QCOMPARE(editor.data(editor.index(archivedRow),
                         TaskDependencyViewModel::SelectableRole).toBool(), true);
    QCOMPARE(editor.data(editor.index(cancelledRow),
                         TaskDependencyViewModel::SelectedRole).toBool(), true);
    QCOMPARE(editor.data(editor.index(cancelledRow),
                         TaskDependencyViewModel::SelectableRole).toBool(), true);

    QVERIFY(editor.setPredecessorSelected(candidateId, true));
    QVERIFY(editor.dirty());
    QCOMPARE(editor.selectedCount(), 3);
    editor.cancel();

    QVERIFY(!editor.dirty());
    QCOMPARE(editor.selectedCount(), 2);
    QCOMPARE(dependencyRepository.replaceCount(), 0);
    const QList<TaskDependency> expectedDependencies{
        TaskDependency{selectedArchived.id(), target.id()},
        TaskDependency{selectedCancelled.id(), target.id()},
    };
    QCOMPARE(dependencyRepository.dependencies(), expectedDependencies);
}


void TaskViewModelsTest::dependencyDraftSavesStableTaskIds()
{
    const Task target = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("目标任务"), TaskStatus::Todo, 1700000001000);
    const Task first = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("第一前置"), TaskStatus::Todo, 1700000002000);
    const Task second = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("第二前置"), TaskStatus::Done, 1700000003000);
    FakeTaskRepository repository{{second, target, first}};
    FakeTaskDependencyRepository dependencyRepository;
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskDependencyViewModel editor{service};
    QSignalSpy savedSpy{&editor, &TaskDependencyViewModel::saved};

    QVERIFY(editor.beginEdit(target.id().toString(QUuid::WithoutBraces)));
    QVERIFY(editor.setPredecessorSelected(
        first.id().toString(QUuid::WithoutBraces), true));
    QVERIFY(editor.setPredecessorSelected(
        second.id().toString(QUuid::WithoutBraces), true));
    QVERIFY(editor.save());

    QCOMPARE(savedSpy.count(), 1);
    QCOMPARE(dependencyRepository.replaceCount(), 1);
    QCOMPARE(dependencyRepository.dependencies().size(), 2);
    QVERIFY(std::any_of(dependencyRepository.dependencies().cbegin(),
                        dependencyRepository.dependencies().cend(),
                        [&first, &target](const TaskDependency &dependency) {
                            return dependency.predecessorId == first.id()
                                && dependency.successorId == target.id();
                        }));
    QVERIFY(std::any_of(dependencyRepository.dependencies().cbegin(),
                        dependencyRepository.dependencies().cend(),
                        [&second, &target](const TaskDependency &dependency) {
                            return dependency.predecessorId == second.id()
                                && dependency.successorId == target.id();
                        }));
    QVERIFY(!editor.dirty());
    QVERIFY(!editor.canSave());
}


void TaskViewModelsTest::dependencyDraftMapsCompleteCyclePath()
{
    const Task first = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("需求"), TaskStatus::Todo, 1700000001000);
    const Task second = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("设计"), TaskStatus::Archived, 1700000002000);
    const Task third = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("编码"), TaskStatus::Todo, 1700000003000);
    FakeTaskRepository repository{{first, second, third}};
    FakeTaskDependencyRepository dependencyRepository{{
        TaskDependency{first.id(), second.id()},
        TaskDependency{second.id(), third.id()},
    }};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository};
    TaskDependencyViewModel editor{service};

    QVERIFY(editor.beginEdit(first.id().toString(QUuid::WithoutBraces)));
    QVERIFY(editor.setPredecessorSelected(
        third.id().toString(QUuid::WithoutBraces), true));
    QVERIFY(!editor.save());

    QVERIFY(editor.errorMessage().contains(QStringLiteral("循环依赖")));
    QVERIFY(editor.errorMessage().contains(QStringLiteral("需求")));
    QVERIFY(editor.errorMessage().contains(QStringLiteral("设计")));
    QVERIFY(editor.errorMessage().contains(QStringLiteral("编码")));
    QVERIFY(editor.errorMessage().contains(QStringLiteral("→")));
    QCOMPARE(dependencyRepository.replaceCount(), 0);
    QVERIFY(editor.dirty());
}
