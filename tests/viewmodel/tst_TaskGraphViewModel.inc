// 依赖图 ViewModel 的布局、路由、选择和交互投影测试。

void TaskViewModelsTest::graphProjectsNodesEdgesAndSelectionDetails()
{
    const Task predecessor = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("需求确认"), TaskStatus::Todo, 1700000001000);
    const Task successor = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("接口实现"), TaskStatus::Todo, 1700000002000,
        TaskPriority::High);
    const Task isolated = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("独立任务"), TaskStatus::Done, 1700000003000);
    FakeTaskRepository repository{{isolated, successor, predecessor}};
    FakeTaskDependencyRepository dependencyRepository{{
        {predecessor.id(), successor.id()},
    }};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    TaskGraphViewModel graph{service};

    QCOMPARE(graph.rowCount(), 3);
    QVERIFY(!graph.empty());
    QVERIFY(graph.contentWidth() > 0.0);
    QVERIFY(graph.contentHeight() > 0.0);
    const int predecessorRow = graphRowForId(
        graph, predecessor.id().toString(QUuid::WithoutBraces));
    const int successorRow = graphRowForId(
        graph, successor.id().toString(QUuid::WithoutBraces));
    QVERIFY(predecessorRow >= 0);
    QVERIFY(successorRow >= 0);

    const qreal predecessorY = graph.data(
        graph.index(predecessorRow), TaskGraphViewModel::NodeYRole).toReal();
    const qreal successorY = graph.data(
        graph.index(successorRow), TaskGraphViewModel::NodeYRole).toReal();
    QVERIFY(successorY > predecessorY);
    QCOMPARE(graph.data(graph.index(successorRow),
                        TaskGraphViewModel::BlockedRole).toBool(), true);

    // 层级投影后所有节点矩形都不得重叠，QML无需再次执行布局算法。
    for (int first = 0; first < graph.rowCount(); ++first) {
        const QRectF firstRect{
            graph.data(graph.index(first), TaskGraphViewModel::NodeXRole).toReal(),
            graph.data(graph.index(first), TaskGraphViewModel::NodeYRole).toReal(),
            graph.data(graph.index(first), TaskGraphViewModel::NodeWidthRole).toReal(),
            graph.data(graph.index(first), TaskGraphViewModel::NodeHeightRole).toReal(),
        };
        for (int second = first + 1; second < graph.rowCount(); ++second) {
            const QRectF secondRect{
                graph.data(graph.index(second), TaskGraphViewModel::NodeXRole).toReal(),
                graph.data(graph.index(second), TaskGraphViewModel::NodeYRole).toReal(),
                graph.data(graph.index(second), TaskGraphViewModel::NodeWidthRole).toReal(),
                graph.data(graph.index(second), TaskGraphViewModel::NodeHeightRole).toReal(),
            };
            QVERIFY(!firstRect.intersects(secondRect));
        }
    }

    QAbstractItemModel *edges = graph.edges();
    QCOMPARE(edges->rowCount(), 1);
    const int routePointsRole = roleForName(*edges, QByteArrayLiteral("routePoints"));
    const int arrowTipXRole = roleForName(*edges, QByteArrayLiteral("arrowTipX"));
    const int arrowTipYRole = roleForName(*edges, QByteArrayLiteral("arrowTipY"));
    const int satisfiedRole = roleForName(*edges, QByteArrayLiteral("satisfied"));
    const int cancelledRole = roleForName(*edges, QByteArrayLiteral("cancelled"));
    const int highlightedRole = roleForName(*edges, QByteArrayLiteral("highlighted"));
    QVERIFY(routePointsRole >= 0 && arrowTipXRole >= 0 && arrowTipYRole >= 0
            && satisfiedRole >= 0 && cancelledRole >= 0);
    const QVariantList route = edges->data(edges->index(0, 0), routePointsRole).toList();
    QVERIFY(route.size() >= 4);
    const QPointF firstPoint = route.constFirst().toPointF();
    const QPointF lastPoint = route.constLast().toPointF();
    QCOMPARE(firstPoint.y(), predecessorY + graph.data(
        graph.index(predecessorRow), TaskGraphViewModel::NodeHeightRole).toReal());
    QCOMPARE(lastPoint.y(), successorY);
    QCOMPARE(edges->data(edges->index(0, 0), arrowTipXRole).toReal(), lastPoint.x());
    QCOMPARE(edges->data(edges->index(0, 0), arrowTipYRole).toReal(), lastPoint.y());
    for (int point = 1; point < route.size(); ++point) {
        const QPointF previous = route.at(point - 1).toPointF();
        const QPointF current = route.at(point).toPointF();
        QVERIFY(qFuzzyCompare(previous.x(), current.x())
                || qFuzzyCompare(previous.y(), current.y()));
    }
    QVERIFY(!edges->data(edges->index(0, 0), satisfiedRole).toBool());
    QVERIFY(!edges->data(edges->index(0, 0), cancelledRole).toBool());

    QVERIFY(graph.selectTask(successor.id().toString(QUuid::WithoutBraces)));
    QCOMPARE(graph.selectedTaskTitle(), QStringLiteral("接口实现"));
    QCOMPARE(graph.selectedStatusText(), QStringLiteral("待办"));
    QCOMPARE(graph.selectedPriorityText(), QStringLiteral("高"));
    QCOMPARE(graph.selectedPredecessorCount(), 1);
    QCOMPARE(graph.selectedSuccessorCount(), 0);
    QVERIFY(graph.canEditSelectedDependencies());
    QVERIFY(graph.selectedBlockingReason().contains(QStringLiteral("需求确认")));
    QVERIFY(edges->data(edges->index(0, 0), highlightedRole).toBool());
    QCOMPARE(graph.selectedPredecessors()->rowCount(), 1);
    QCOMPARE(graph.selectedSuccessors()->rowCount(), 0);
    QCOMPARE(graph.data(graph.index(predecessorRow),
                        TaskGraphViewModel::EmphasisLevelRole).toInt(),
             static_cast<int>(TaskGraphViewModel::DirectEmphasis));

    graph.setSearchText(QStringLiteral("独立"));
    QVERIFY(graph.locateFirstMatch());
    QCOMPARE(graph.selectedTaskTitle(), QStringLiteral("独立任务"));
    graph.setStatusFilterIndex(1);
    QCOMPARE(graph.data(graph.index(graphRowForId(
                         graph, isolated.id().toString(QUuid::WithoutBraces))),
                        TaskGraphViewModel::FilterMatchedRole).toBool(), false);
    graph.setStatusFilterIndex(0);
    graph.setSearchText(QString{});
    QVERIFY(graph.selectTask(successor.id().toString(QUuid::WithoutBraces)));

    // 取消只改变边的派生解析状态；关系仍存在，并通过 cancelled 角色交给QML绘制灰色。
    QVERIFY(service.cancelTask(predecessor.id()).ok());
    QCOMPARE(edges->rowCount(), 1);
    QVERIFY(!edges->data(edges->index(0, 0), satisfiedRole).toBool());
    QVERIFY(edges->data(edges->index(0, 0), cancelledRole).toBool());
    const int refreshedSuccessorRow = graphRowForId(
        graph, successor.id().toString(QUuid::WithoutBraces));
    QVERIFY(refreshedSuccessorRow >= 0);
    QVERIFY(!graph.data(graph.index(refreshedSuccessorRow),
                        TaskGraphViewModel::BlockedRole).toBool());
    QVERIFY(graph.selectedBlockingReason().isEmpty());
}


void TaskViewModelsTest::graphMinimizesSimpleLayerCrossing()
{
    const Task rootA = task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                            QStringLiteral("根 A"), TaskStatus::Todo, 1700000001000);
    const Task rootB = task(QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
                            QStringLiteral("根 B"), TaskStatus::Todo, 1700000002000);
    const Task childX = task(QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
                             QStringLiteral("子 X"), TaskStatus::Todo, 1700000003000);
    const Task childY = task(QStringLiteral("{44444444-4444-4444-4444-444444444444}"),
                             QStringLiteral("子 Y"), TaskStatus::Todo, 1700000004000);
    FakeTaskRepository repository{{rootA, rootB, childX, childY}};
    FakeTaskDependencyRepository dependencies{{
        {rootA.id(), childY.id()}, {rootB.id(), childX.id()}}};
    FakeTaskCreationRepository creation{repository, dependencies};
    TaskService service{repository, dependencies, creation,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    TaskGraphViewModel graph{service};

    const auto xFor = [&graph](const Task &value) {
        const int row = graphRowForId(graph, value.id().toString(QUuid::WithoutBraces));
        return graph.data(graph.index(row), TaskGraphViewModel::NodeXRole).toReal();
    };
    // A→Y、B→X 在优化后保持同一水平顺序，因此两条边不会交叉。
    QVERIFY((xFor(rootA) - xFor(rootB)) * (xFor(childY) - xFor(childX)) > 0.0);
}


void TaskViewModelsTest::graphRoutesLongEdgesDeterministically()
{
    const Task first = task(QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
                            QStringLiteral("第一层"), TaskStatus::Todo, 1700000001000);
    const Task second = task(QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
                             QStringLiteral("第二层"), TaskStatus::Todo, 1700000002000);
    const Task third = task(QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
                            QStringLiteral("第三层"), TaskStatus::Todo, 1700000003000);
    FakeTaskRepository repository{{third, second, first}};
    FakeTaskDependencyRepository dependencies{{
        {first.id(), second.id()}, {second.id(), third.id()}, {first.id(), third.id()}}};
    FakeTaskCreationRepository creation{repository, dependencies};
    TaskService service{repository, dependencies, creation,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    TaskGraphViewModel firstProjection{service};
    TaskGraphViewModel secondProjection{service};

    QAbstractItemModel *firstEdges = firstProjection.edges();
    QAbstractItemModel *secondEdges = secondProjection.edges();
    const int routeRole = roleForName(*firstEdges, QByteArrayLiteral("routePoints"));
    QCOMPARE(firstEdges->rowCount(), 3);
    QCOMPARE(secondEdges->rowCount(), 3);
    for (int row = 0; row < firstEdges->rowCount(); ++row) {
        const QVariantList firstRoute = firstEdges->data(firstEdges->index(row, 0), routeRole).toList();
        const QVariantList secondRoute = secondEdges->data(secondEdges->index(row, 0), routeRole).toList();
        QCOMPARE(firstRoute, secondRoute);
        for (int point = 1; point < firstRoute.size(); ++point) {
            const QPointF previous = firstRoute.at(point - 1).toPointF();
            const QPointF current = firstRoute.at(point).toPointF();
            QVERIFY(qFuzzyCompare(previous.x(), current.x())
                    || qFuzzyCompare(previous.y(), current.y()));
        }
    }

    const int firstRow = graphRowForId(firstProjection,
        first.id().toString(QUuid::WithoutBraces));
    const int thirdRow = graphRowForId(firstProjection,
        third.id().toString(QUuid::WithoutBraces));
    QVERIFY(firstProjection.data(firstProjection.index(thirdRow), TaskGraphViewModel::NodeYRole).toReal()
            > firstProjection.data(firstProjection.index(firstRow), TaskGraphViewModel::NodeYRole).toReal());
}


void TaskViewModelsTest::graphReloadPreservesVisibleSelectionAndClearsHiddenSelection()
{
    const Task predecessor = task(
        QStringLiteral("{11111111-1111-1111-1111-111111111111}"),
        QStringLiteral("A"), TaskStatus::Done, 1700000001000);
    const Task successor = task(
        QStringLiteral("{22222222-2222-2222-2222-222222222222}"),
        QStringLiteral("B"), TaskStatus::Done, 1700000002000);
    const Task isolated = task(
        QStringLiteral("{33333333-3333-3333-3333-333333333333}"),
        QStringLiteral("C"), TaskStatus::Done, 1700000003000);
    FakeTaskRepository repository{{predecessor, successor, isolated}};
    FakeTaskDependencyRepository dependencyRepository{{
        {predecessor.id(), successor.id()},
    }};
    FakeTaskCreationRepository creationRepository{repository, dependencyRepository};
    TaskService service{repository, dependencyRepository, creationRepository,
                        batchTransitionsFor(repository), deletionRepository,
                        categoryRepository};
    TaskGraphViewModel graph{service};

    const QString successorId = successor.id().toString(QUuid::WithoutBraces);
    QVERIFY(graph.selectTask(successorId));
    graph.reload();
    QCOMPARE(graph.selectedTaskId(), successorId);

    // 先归档前置时组件仍有活动后继，因此选中节点应继续可见。
    QVERIFY(service.archiveTask(predecessor.id()).ok());
    QCOMPARE(graph.selectedTaskId(), successorId);
    // 两端都归档后成为纯归档组件，Model隐藏该组件且ViewModel清除过期选择。
    QVERIFY(service.archiveTask(successor.id()).ok());
    QVERIFY(graph.selectedTaskId().isEmpty());
    QCOMPARE(graph.rowCount(), 1);
    QCOMPARE(graph.data(graph.index(0), TaskGraphViewModel::TitleRole).toString(),
             QStringLiteral("C"));
}
