#include "view/widgets/focus/FocusPage.h"
#include "view/widgets/theme/WidgetTheme.h"

#include <QAbstractListModel>
#include <QAbstractButton>
#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>

using namespace smartmate;

namespace {

struct HistoryRow {
    QString sessionId;
    QString taskId;
    QString title;
    QString duration;
    QString started;
    QString completed;
    QString category;
    viewmodel::FocusContract::CategoryColor color;
    QString tooltip;
    QString accessible;
};

class FakeHistoryModel final : public viewmodel::FocusHistoryContract {
public:
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_rows.size();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
            return {};
        }
        const auto &row = m_rows.at(index.row());
        switch (role) {
        case SessionIdRole: return row.sessionId;
        case TaskIdRole: return row.taskId;
        case Qt::DisplayRole:
        case TaskTitleRole: return row.title;
        case DurationMillisecondsRole: return 90'000;
        case DurationTextRole: return row.duration;
        case StartedAtTextRole: return row.started;
        case CompletedAtTextRole: return row.completed;
        case CategoryNameRole: return row.category;
        case CategoryColorRole: return static_cast<int>(row.color);
        case TooltipRole: return row.tooltip;
        case AccessibleTextRole: return row.accessible;
        default: return {};
        }
    }

    void replace(QList<HistoryRow> rows)
    {
        beginResetModel();
        m_rows = std::move(rows);
        endResetModel();
    }

private:
    QList<HistoryRow> m_rows;
};

class FakeFocusContract final : public viewmodel::FocusContract {
public:
    QAbstractItemModel *history() noexcept override { return &historyModel; }
    PageState pageState() const noexcept override { return state; }
    QString taskId() const override { return currentTaskId; }
    QString sessionId() const override { return currentSessionId; }
    QString taskTitle() const override { return title; }
    bool hasCategory() const noexcept override { return categoryPresent; }
    QString categoryName() const override { return category; }
    CategoryColor categoryColor() const noexcept override { return color; }
    bool hasEstimatedMinutes() const noexcept override { return estimatedPresent; }
    int estimatedMinutes() const noexcept override { return 25; }
    QString estimatedText() const override { return estimate; }
    QString startedAtText() const override { return started; }
    qint64 elapsedMilliseconds() const noexcept override { return elapsedMs; }
    QString elapsedText() const override { return elapsed; }
    QString stateText() const override { return stateLabel; }
    bool canStart() const noexcept override { return startAllowed; }
    bool canPause() const noexcept override { return pauseAllowed; }
    bool canResume() const noexcept override { return resumeAllowed; }
    bool canComplete() const noexcept override { return completeAllowed; }
    bool canAbandon() const noexcept override { return abandonAllowed; }
    int historyCount() const noexcept override { return historyModel.rowCount(); }
    bool hasHistory() const noexcept override { return historyModel.rowCount() > 0; }
    QString historyEmptyStateText() const override { return QStringLiteral("完成一次专注后，这里会显示记录"); }
    QString emptyStateText() const override { return QStringLiteral("当前没有进行中的任务"); }
    bool hasStorageWarning() const noexcept override { return warningVisible; }
    QString storageWarningText() const override { return warningText; }

    bool startFocus(const QString &id) override { lastStart = id; return commandResult; }
    bool pauseFocus(const QString &id) override { lastPause = id; return commandResult; }
    bool resumeFocus(const QString &id) override { lastResume = id; return commandResult; }
    bool completeFocus(const QString &id) override { lastComplete = id; return commandResult; }
    bool abandonFocus(const QString &id) override { lastAbandon = id; return commandResult; }
    void reload() override { ++reloadCount; }

    void publishState() { emit focusChanged(); }
    void publishWarning() { emit storageWarningChanged(); }

    FakeHistoryModel historyModel;
    PageState state{NoInProgressTask};
    QString currentTaskId;
    QString currentSessionId;
    QString title;
    bool categoryPresent{false};
    QString category{QStringLiteral("未分类")};
    CategoryColor color{Unclassified};
    bool estimatedPresent{false};
    QString estimate{QStringLiteral("未设置预计用时")};
    QString started;
    qint64 elapsedMs{0};
    QString elapsed{QStringLiteral("0:00:00")};
    QString stateLabel{QStringLiteral("暂无进行中任务")};
    bool startAllowed{false};
    bool pauseAllowed{false};
    bool resumeAllowed{false};
    bool completeAllowed{false};
    bool abandonAllowed{false};
    bool warningVisible{false};
    QString warningText;
    bool commandResult{true};
    int reloadCount{0};
    QString lastStart;
    QString lastPause;
    QString lastResume;
    QString lastComplete;
    QString lastAbandon;
};

template<typename T>
T *requiredChild(QObject &parent, const char *name)
{
    T *result = parent.findChild<T *>(QString::fromLatin1(name));
    Q_ASSERT(result != nullptr);
    return result;
}

void prepareSession(FakeFocusContract &focus,
                    viewmodel::FocusContract::PageState state)
{
    focus.state = state;
    focus.currentTaskId = QStringLiteral("task-42");
    focus.currentSessionId = state == viewmodel::FocusContract::ReadyToStart
        ? QString{} : QStringLiteral("session-7");
    focus.title = QStringLiteral("完成专注页面");
    focus.categoryPresent = true;
    focus.category = QStringLiteral("开发");
    focus.color = viewmodel::FocusContract::Teal;
    focus.estimatedPresent = true;
    focus.estimate = QStringLiteral("25 分钟");
    focus.started = QStringLiteral("2026/7/17 09:00");
    focus.elapsedMs = 65'000;
    focus.elapsed = QStringLiteral("0:01:05");
    focus.stateLabel = state == viewmodel::FocusContract::ReadyToStart
        ? QStringLiteral("可以开始")
        : state == viewmodel::FocusContract::Running
            ? QStringLiteral("专注中") : QStringLiteral("已暂停");
    focus.startAllowed = state == viewmodel::FocusContract::ReadyToStart;
    focus.pauseAllowed = state == viewmodel::FocusContract::Running;
    focus.resumeAllowed = state == viewmodel::FocusContract::Paused;
    focus.completeAllowed = state != viewmodel::FocusContract::ReadyToStart;
    focus.abandonAllowed = focus.completeAllowed;
}

} // namespace

class FocusWidgetsTest final : public QObject {
    Q_OBJECT

private slots:
    void projectsStatesAndForwardsStableCommands();
    void requestsTaskNavigationAndConfirmsAbandon();
    void rebuildsHistoryAndShowsStorageWarning();
    void reloadsOnShowAndUsesThemeBackground();
};

void FocusWidgetsTest::projectsStatesAndForwardsStableCommands()
{
    FakeFocusContract focus;
    view::widgets::FocusPage page{focus};

    auto *empty = requiredChild<QLabel>(page, "focusEmptyState");
    auto *showTasks = requiredChild<QPushButton>(page, "showTasksFromFocusButton");
    QVERIFY(empty->isVisibleTo(&page) || !page.isVisible());
    QVERIFY(!showTasks->isHidden());

    prepareSession(focus, viewmodel::FocusContract::ReadyToStart);
    focus.publishState();
    auto *start = requiredChild<QPushButton>(page, "startFocusButton");
    QVERIFY(!start->isHidden());
    QVERIFY(start->isEnabled());
    start->click();
    QCOMPARE(focus.lastStart, QStringLiteral("task-42"));

    prepareSession(focus, viewmodel::FocusContract::Running);
    focus.publishState();
    auto *pause = requiredChild<QPushButton>(page, "pauseFocusButton");
    auto *complete = requiredChild<QPushButton>(page, "completeFocusButton");
    QVERIFY(!pause->isHidden());
    QVERIFY(!complete->isHidden());
    pause->click();
    complete->click();
    QCOMPARE(focus.lastPause, QStringLiteral("session-7"));
    QCOMPARE(focus.lastComplete, QStringLiteral("session-7"));

    prepareSession(focus, viewmodel::FocusContract::Paused);
    focus.publishState();
    auto *resume = requiredChild<QPushButton>(page, "resumeFocusButton");
    QVERIFY(!resume->isHidden());
    resume->click();
    QCOMPARE(focus.lastResume, QStringLiteral("session-7"));
    QCOMPARE(requiredChild<QLabel>(page, "focusElapsedText")->text(),
             QStringLiteral("0:01:05"));
}

void FocusWidgetsTest::requestsTaskNavigationAndConfirmsAbandon()
{
    FakeFocusContract focus;
    view::widgets::FocusPage page{focus};
    QSignalSpy navigationSpy{&page,
        &view::widgets::FocusPage::showTasksRequested};
    requiredChild<QPushButton>(page, "showTasksFromFocusButton")->click();
    QCOMPARE(navigationSpy.count(), 1);

    prepareSession(focus, viewmodel::FocusContract::Running);
    focus.publishState();
    auto *abandon = requiredChild<QPushButton>(page, "abandonFocusButton");

    QTimer::singleShot(0, [] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *box = qobject_cast<QMessageBox *>(widget)) {
                box->button(QMessageBox::Cancel)->click();
            }
        }
    });
    abandon->click();
    QVERIFY(focus.lastAbandon.isEmpty());

    QTimer::singleShot(0, [] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *box = qobject_cast<QMessageBox *>(widget)) {
                box->button(QMessageBox::Yes)->click();
            }
        }
    });
    abandon->click();
    QCOMPARE(focus.lastAbandon, QStringLiteral("session-7"));
}

void FocusWidgetsTest::rebuildsHistoryAndShowsStorageWarning()
{
    FakeFocusContract focus;
    view::widgets::FocusPage page{focus};
    focus.historyModel.replace({
        {QStringLiteral("session-a"), QStringLiteral("task-a"),
         QStringLiteral("整理测试"), QStringLiteral("0:01:30"),
         QStringLiteral("2026/7/17 09:00"), QStringLiteral("2026/7/17 09:01"),
         QStringLiteral("开发"), viewmodel::FocusContract::Blue,
         QStringLiteral("原始 Tooltip"), QStringLiteral("完整无障碍摘要")},
    });

    const auto rows = page.findChildren<QFrame *>(QStringLiteral("focusHistoryRow"));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first()->toolTip(), QStringLiteral("原始 Tooltip"));
    QCOMPARE(rows.first()->accessibleDescription(), QStringLiteral("完整无障碍摘要"));
    QCOMPARE(requiredChild<QLabel>(page, "focusHistoryCount")->text(),
             QStringLiteral("1 条"));

    focus.warningVisible = true;
    focus.warningText = QStringLiteral("专注记录暂时无法保存，正在重试");
    focus.publishWarning();
    auto *warning = requiredChild<QLabel>(page, "focusStorageWarning");
    QVERIFY(!warning->isHidden());
    QCOMPARE(warning->text(), focus.warningText);
}

void FocusWidgetsTest::reloadsOnShowAndUsesThemeBackground()
{
    FakeFocusContract focus;
    view::widgets::FocusPage page{focus};
    const auto theme = view::widgets::WidgetTheme::fromAccentIndex(1);
    page.setPalette(theme.palette());
    page.resize(900, 620);
    page.show();
    QTRY_VERIFY(focus.reloadCount > 0);
    QCOMPARE(page.viewport()->backgroundRole(), QPalette::Window);
    QCOMPARE(page.widget()->backgroundRole(), QPalette::Window);
    QCOMPARE(page.viewport()->palette().color(QPalette::Window), theme.background);
    QVERIFY(page.verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff);
    QVERIFY(!requiredChild<QPushButton>(page, "refreshFocusButton")
                 ->accessibleName().isEmpty());
}

QTEST_MAIN(FocusWidgetsTest)
#include "tst_FocusWidgets.moc"
