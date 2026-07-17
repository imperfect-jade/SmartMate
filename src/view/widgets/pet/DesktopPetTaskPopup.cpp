#include "view/widgets/pet/DesktopPetTaskPopup.h"

#include "view/widgets/pet/DesktopPetGeometry.h"
#include "viewmodel/contracts/TaskFocusContract.h"
#include "viewmodel/contracts/TaskListContract.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

namespace smartmate::view::widgets::pet {

DesktopPetTaskPopup::DesktopPetTaskPopup(
    viewmodel::TaskFocusContract &focus,
    viewmodel::TaskListContract &tasks, QWidget *parent)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint
                          | Qt::WindowStaysOnTopHint)
    , m_focus(focus)
    , m_tasks(tasks)
    , m_stateLabel(new QLabel(this))
    , m_titleLabel(new QLabel(this))
    , m_detailLabel(new QLabel(this))
    , m_errorLabel(new QLabel(this))
    , m_startButton(new QPushButton(tr("开始"), this))
    , m_completeButton(new QPushButton(tr("完成"), this))
    , m_openButton(new QPushButton(tr("打开 SmartMate"), this))
    , m_errorTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("desktopPetTaskPopup"));
    setAttribute(Qt::WA_TranslucentBackground);
    // QFrame 配合 WA_StyledBackground 确保顶层透明窗口仍绘制不透明气泡表面。
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedWidth(340);
    setStyleSheet(QStringLiteral(
        "#desktopPetTaskPopup { background: #fffdf8; border: 1px solid #d9d2c3;"
        " border-radius: 12px; }"
        "#desktopPetPopupState { color: #6f766f; font-size: 11pt; }"
        "#desktopPetPopupTitle { color: #26332c; font-size: 13pt; font-weight: 600; }"
        "#desktopPetPopupDetail { color: #59625c; }"
        "#desktopPetPopupError { color: #b13a3a; }"));

    m_stateLabel->setObjectName(QStringLiteral("desktopPetPopupState"));
    m_titleLabel->setObjectName(QStringLiteral("desktopPetPopupTitle"));
    m_detailLabel->setObjectName(QStringLiteral("desktopPetPopupDetail"));
    m_errorLabel->setObjectName(QStringLiteral("desktopPetPopupError"));
    m_startButton->setObjectName(QStringLiteral("desktopPetStartButton"));
    m_completeButton->setObjectName(QStringLiteral("desktopPetCompleteButton"));
    m_openButton->setObjectName(QStringLiteral("desktopPetOpenButton"));
    m_titleLabel->setWordWrap(true);
    m_detailLabel->setWordWrap(true);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);
    layout->addWidget(m_stateLabel);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_detailLabel);
    layout->addWidget(m_errorLabel);
    auto *actions = new QHBoxLayout;
    actions->addWidget(m_startButton);
    actions->addWidget(m_completeButton);
    actions->addStretch();
    actions->addWidget(m_openButton);
    layout->addLayout(actions);

    m_errorTimer->setSingleShot(true);
    m_errorTimer->setInterval(4000);
    connect(m_errorTimer, &QTimer::timeout, m_errorLabel, &QWidget::hide);
    connect(&focus, &viewmodel::TaskFocusContract::focusTaskChanged,
            this, &DesktopPetTaskPopup::refresh);
    connect(&tasks, &viewmodel::TaskListContract::notificationRaised,
            this, &DesktopPetTaskPopup::showError);
    connect(m_startButton, &QPushButton::clicked, this, [this] {
        if (!m_taskId.isEmpty()) {
            m_tasks.startTask(m_taskId);
        }
    });
    connect(m_completeButton, &QPushButton::clicked, this, [this] {
        if (!m_taskId.isEmpty()) {
            m_tasks.completeTask(m_taskId);
        }
    });
    connect(m_openButton, &QPushButton::clicked, this, [this] {
        emit openMainWindowRequested();
    });
    qApp->installEventFilter(this);
    refresh();
}

void DesktopPetTaskPopup::showNextTo(const QRect &petGeometry)
{
    refresh();
    adjustSize();
    QScreen *screen = QGuiApplication::screenAt(petGeometry.center());
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen != nullptr) {
        move(taskPopupPosition(screen->availableGeometry(), petGeometry, size()));
    }
    show();
    raise();
}

void DesktopPetTaskPopup::toggleNextTo(const QRect &petGeometry)
{
    if (isVisible()) {
        hide();
    } else {
        showNextTo(petGeometry);
    }
}

bool DesktopPetTaskPopup::eventFilter(QObject *watched, QEvent *event)
{
    if (isVisible() && event->type() == QEvent::MouseButtonPress) {
        auto *widget = qobject_cast<QWidget *>(watched);
        QWidget *petWindow = parentWidget();
        const bool insidePet = widget != nullptr && petWindow != nullptr
            && (widget == petWindow || petWindow->isAncestorOf(widget));
        if (widget != nullptr && widget != this && !isAncestorOf(widget)
            && !insidePet) {
            hide();
        }
    }
    return QFrame::eventFilter(watched, event);
}

void DesktopPetTaskPopup::paintEvent(QPaintEvent *)
{
    // 透明顶层窗口在部分平台不会自动绘制 QSS 背景，因此显式绘制气泡表面。
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen{QColor{QStringLiteral("#d9d2c3")}, 1.0});
    painter.setBrush(QColor{QStringLiteral("#fffdf8")});
    painter.drawRoundedRect(QRectF{rect()}.adjusted(0.5, 0.5, -0.5, -0.5),
                            12.0, 12.0);
}

void DesktopPetTaskPopup::refresh()
{
    using State = viewmodel::TaskFocusContract::FocusState;
    const State state = m_focus.focusState();
    m_taskId = m_focus.focusTaskId();
    m_startButton->setVisible(state == State::Suggested);
    m_completeButton->setVisible(state == State::InProgress);
    m_startButton->setEnabled(m_focus.focusCanStart());
    m_completeButton->setEnabled(m_focus.focusCanComplete());

    switch (state) {
    case State::Suggested:
        m_stateLabel->setText(tr("现在最推荐"));
        m_titleLabel->setText(m_focus.focusTitle());
        m_detailLabel->setText(
            QStringLiteral("%1\n%2 · %3")
                .arg(m_focus.focusReasonText(),
                     m_focus.focusPriorityText(),
                     m_focus.focusDeadlineText()));
        break;
    case State::InProgress:
        m_stateLabel->setText(tr("正在进行"));
        m_titleLabel->setText(m_focus.focusTitle());
        m_detailLabel->setText(
            tr("%1 · 预计 %2 分钟")
                .arg(m_focus.focusStatusText())
                .arg(m_focus.focusEstimatedMinutes()));
        break;
    case State::NoTasks:
        m_stateLabel->setText(tr("今天很轻松"));
        m_titleLabel->setText(tr("暂时没有待处理任务"));
        m_detailLabel->setText(tr("可以打开 SmartMate 创建下一项任务。"));
        m_taskId.clear();
        break;
    case State::AllBlocked:
        m_stateLabel->setText(tr("任务正在等待"));
        m_titleLabel->setText(tr("当前任务都被前置依赖阻塞"));
        m_detailLabel->setText(tr("打开 SmartMate 查看依赖关系和解锁条件。"));
        m_taskId.clear();
        break;
    }
    if (isVisible()) {
        adjustSize();
    }
}

void DesktopPetTaskPopup::showError(
    const common::UiNotification &notification)
{
    if (notification.severity != common::UiSeverity::Error) {
        return;
    }
    m_errorLabel->setText(notification.message);
    m_errorLabel->show();
    m_errorTimer->start();
    if (isVisible()) {
        adjustSize();
    }
}

} // namespace smartmate::view::widgets::pet
