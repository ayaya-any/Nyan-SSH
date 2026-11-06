#include "ui/FilePanel.h"

#include "core/AppPaths.h"
#include "core/SftpWorker.h"
#include "ui/Dialogs.h"
#include "ui/Icons.h"
#include "ui/MaterialWidgets.h"
#include "ui/Theme.h"

#include <QAbstractTableModel>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QProgressBar>
#include <QStackedWidget>
#include <QStringList>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace nyan::ui {
namespace {

QString formatBytes(qint64 bytes)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    if (unit == 0)
        return QStringLiteral("%1 B").arg(bytes);
    return QStringLiteral("%1 %2").arg(value, 0, 'f', value < 10 ? 1 : 0).arg(QLatin1String(units[unit]));
}

QString fileIconFor(const SftpEntry &entry)
{
    if (entry.isDirectory)
        return QStringLiteral("folder");
    if (entry.isSymlink)
        return QStringLiteral("link");
    const QString suffix = QFileInfo(entry.name).suffix().toLower();
    if (QStringList{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                    QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("svg"),
                    QStringLiteral("bmp"), QStringLiteral("ico")}
            .contains(suffix)) {
        return QStringLiteral("image");
    }
    if (QStringList{QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("log"),
                    QStringLiteral("json"), QStringLiteral("yaml"), QStringLiteral("yml"),
                    QStringLiteral("ini"), QStringLiteral("cfg"), QStringLiteral("conf"),
                    QStringLiteral("xml"), QStringLiteral("html"), QStringLiteral("css"),
                    QStringLiteral("js"), QStringLiteral("ts"), QStringLiteral("cpp"),
                    QStringLiteral("cc"), QStringLiteral("c"), QStringLiteral("h"),
                    QStringLiteral("hpp"), QStringLiteral("py"), QStringLiteral("rs"),
                    QStringLiteral("go"), QStringLiteral("java"), QStringLiteral("sh")}
            .contains(suffix)) {
        return QStringLiteral("file-text");
    }
    return QStringLiteral("file");
}

QString remoteName(const QString &path)
{
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? path : path.mid(slash + 1);
}

QString uniqueLocalPath(const QString &directory, const QString &fileName)
{
    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();
    QString candidate = QDir(directory).filePath(fileName);
    int counter = 1;
    while (QFileInfo::exists(candidate)) {
        const QString name = suffix.isEmpty()
                                 ? QStringLiteral("%1 (%2)").arg(base).arg(counter)
                                 : QStringLiteral("%1 (%2).%3").arg(base).arg(counter).arg(suffix);
        candidate = QDir(directory).filePath(name);
        ++counter;
    }
    return candidate;
}

} // namespace

// ---------------------------------------------------------------------------
// RemoteFileModel
// ---------------------------------------------------------------------------

class RemoteFileModel : public QAbstractTableModel {
public:
    enum Column {
        NameColumn,
        SizeColumn,
        ModifiedColumn,
        PermissionColumn,
        ColumnCount,
    };

    explicit RemoteFileModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent)
    {
    }

    void setEntries(const QVector<SftpEntry> &entries)
    {
        m_entries = entries;
        rebuild();
    }

    void setShowHidden(bool show)
    {
        if (m_showHidden == show)
            return;
        m_showHidden = show;
        rebuild();
    }

    SftpEntry entryAt(int row) const
    {
        if (row < 0 || row >= m_visible.size())
            return {};
        return m_visible.at(row);
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_visible.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : ColumnCount;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size())
            return {};
        const SftpEntry &entry = m_visible.at(index.row());
        const Palette &palette = Theme::instance().palette();

        switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
            case NameColumn:
                return entry.name;
            case SizeColumn:
                return entry.sizeText();
            case ModifiedColumn:
                return entry.modified.isValid() ? entry.modified.toString(QStringLiteral("MM-dd HH:mm"))
                                                : QStringLiteral("—");
            case PermissionColumn:
                return entry.permissionText().isEmpty() ? QStringLiteral("—") : entry.permissionText();
            default:
                return {};
            }
        case Qt::DecorationRole:
            if (index.column() == NameColumn) {
                const QColor color = entry.isDirectory
                                         ? palette.accent
                                         : (entry.isSymlink ? palette.success : palette.textMuted);
                return Icons::icon(fileIconFor(entry), color, 18);
            }
            return {};
        case Qt::ForegroundRole:
            if (index.column() == NameColumn)
                return QColor(palette.text);
            return QColor(palette.textMuted);
        case Qt::TextAlignmentRole:
            if (index.column() == SizeColumn)
                return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            if (index.column() == PermissionColumn)
                return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        case Qt::ToolTipRole:
            return QStringLiteral("%1\n%2%3\n修改：%4")
                .arg(entry.path, entry.sizeText(),
                     entry.isDirectory ? QStringLiteral(" · 文件夹") : QString(),
                     entry.modified.isValid() ? entry.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                              : QStringLiteral("未知"));
        default:
            return {};
        }
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return {};
        switch (section) {
        case NameColumn:
            return QStringLiteral("名称");
        case SizeColumn:
            return QStringLiteral("大小");
        case ModifiedColumn:
            return QStringLiteral("修改时间");
        case PermissionColumn:
            return QStringLiteral("权限");
        default:
            return {};
        }
    }

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override
    {
        m_sortColumn = column;
        m_sortOrder = order;
        rebuild();
    }

private:
    void rebuild()
    {
        beginResetModel();
        m_visible.clear();
        m_visible.reserve(m_entries.size());
        for (const SftpEntry &entry : m_entries) {
            if (!m_showHidden && entry.name.startsWith(QLatin1Char('.')))
                continue;
            m_visible.append(entry);
        }

        std::sort(m_visible.begin(), m_visible.end(), [this](const SftpEntry &a, const SftpEntry &b) {
            if (a.isDirectory != b.isDirectory)
                return a.isDirectory;
            int comparison = 0;
            switch (m_sortColumn) {
            case SizeColumn:
                comparison = a.size < b.size ? -1 : (a.size > b.size ? 1 : 0);
                break;
            case ModifiedColumn:
                comparison = a.modified < b.modified ? -1 : (a.modified > b.modified ? 1 : 0);
                break;
            case PermissionColumn:
                comparison = QString::compare(a.permissionText(), b.permissionText());
                break;
            case NameColumn:
            default:
                comparison = QString::compare(a.name, b.name, Qt::CaseInsensitive);
                break;
            }
            if (comparison == 0)
                comparison = QString::compare(a.name, b.name, Qt::CaseInsensitive);
            return m_sortOrder == Qt::AscendingOrder ? comparison < 0 : comparison > 0;
        });
        endResetModel();
    }

    QVector<SftpEntry> m_entries;
    QVector<SftpEntry> m_visible;
    bool m_showHidden = false;
    int m_sortColumn = NameColumn;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};

// ---------------------------------------------------------------------------
// FilePanel
// ---------------------------------------------------------------------------

FilePanel::FilePanel(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    setMinimumWidth(330);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    static const bool registered = [] {
        qRegisterMetaType<nyan::SftpEntry>("nyan::SftpEntry");
        qRegisterMetaType<QVector<nyan::SftpEntry>>("QVector<nyan::SftpEntry>");
        return true;
    }();
    Q_UNUSED(registered);

    buildUi();

    connect(&Theme::instance(), &Theme::changed, this, [this] {
        update();
        if (m_table)
            m_table->viewport()->update();
        updateStatus();
    });
}

FilePanel::~FilePanel()
{
    m_shuttingDown = true;
    shutdown();
}

void FilePanel::setSettings(AppSettings *settings)
{
    m_settings = settings;
    if (!m_settings)
        return;
    m_showHidden = m_settings->showHiddenFiles;
    if (m_model)
        m_model->setShowHidden(m_showHidden);
    if (m_hiddenButton)
        m_hiddenButton->setActive(m_showHidden);
    updateStatus();
}

void FilePanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header -----------------------------------------------------------------
    auto *header = new QWidget;
    header->setFixedHeight(46);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 0, 8, 0);
    headerLayout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("远程文件"));
    title->setFont(Theme::instance().uiFont(10, QFont::DemiBold));
    headerLayout->addWidget(title);

    m_titleStatus = new QLabel;
    m_titleStatus->setFixedSize(7, 7);
    m_titleStatus->setStyleSheet(QStringLiteral("background:#6A6A72;border-radius:3px;"));
    headerLayout->addWidget(m_titleStatus);
    headerLayout->addStretch();

    auto *refreshButton = new MIconButton(QStringLiteral("refresh"), QStringLiteral("刷新"), header);
    refreshButton->setButtonSize(30);
    connect(refreshButton, &QAbstractButton::clicked, this, &FilePanel::refresh);
    headerLayout->addWidget(refreshButton);

    auto *closeButton = new MIconButton(QStringLiteral("close"), QStringLiteral("关闭文件面板"), header);
    closeButton->setButtonSize(30);
    connect(closeButton, &QAbstractButton::clicked, this, &FilePanel::closeRequested);
    headerLayout->addWidget(closeButton);
    root->addWidget(header);

    auto *hairline = new QFrame;
    hairline->setObjectName(QStringLiteral("hairline"));
    hairline->setFixedHeight(1);
    root->addWidget(hairline);

    // Path bar ---------------------------------------------------------------
    m_pathBar = new QWidget;
    auto *pathLayout = new QHBoxLayout(m_pathBar);
    pathLayout->setContentsMargins(10, 8, 10, 2);
    pathLayout->setSpacing(4);

    auto *homeButton = new MIconButton(QStringLiteral("home"), QStringLiteral("主目录"), m_pathBar);
    homeButton->setButtonSize(30);
    connect(homeButton, &QAbstractButton::clicked, this, [this] {
        if (m_ready)
            navigateTo(m_homePath);
    });
    pathLayout->addWidget(homeButton);

    auto *upButton = new MIconButton(QStringLiteral("arrow-up"), QStringLiteral("上级目录"), m_pathBar);
    upButton->setButtonSize(30);
    connect(upButton, &QAbstractButton::clicked, this, [this] {
        if (m_ready)
            navigateTo(parentRemotePath(m_currentPath));
    });
    pathLayout->addWidget(upButton);

    m_pathEdit = new QLineEdit(m_pathBar);
    m_pathEdit->setPlaceholderText(QStringLiteral("/home/nyan"));
    m_pathEdit->setFont(Theme::instance().monoFont(9));
    m_pathEdit->setMinimumHeight(30);
    m_pathEdit->setMaximumHeight(30);
    m_pathEdit->setClearButtonEnabled(false);
    connect(m_pathEdit, &QLineEdit::returnPressed, this, [this] {
        if (!m_ready)
            return;
        const QString target = m_pathEdit->text().trimmed();
        if (!target.isEmpty())
            navigateTo(target);
    });
    pathLayout->addWidget(m_pathEdit, 1);
    root->addWidget(m_pathBar);

    // Action bar -------------------------------------------------------------
    m_actionsBar = new QWidget;
    auto *actions = new QHBoxLayout(m_actionsBar);
    actions->setContentsMargins(10, 0, 10, 8);
    actions->setSpacing(2);

    const auto addAction = [&](const QString &icon, const QString &tooltip, auto slot) {
        auto *button = new MIconButton(icon, tooltip, m_actionsBar);
        button->setButtonSize(30);
        connect(button, &QAbstractButton::clicked, this, slot);
        actions->addWidget(button);
        return button;
    };

    addAction(QStringLiteral("upload"), QStringLiteral("上传文件"), [this] {
        if (!m_ready)
            return;
        const QString directory = m_settings && !m_settings->lastLocalDirectory.isEmpty()
                                      ? m_settings->lastLocalDirectory
                                      : QDir::homePath();
        const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("上传文件"), directory);
        if (files.isEmpty())
            return;
        if (m_settings) {
            m_settings->lastLocalDirectory = QFileInfo(files.first()).absolutePath();
            m_settings->save();
        }
        uploadLocalFiles(files);
    });
    addAction(QStringLiteral("download"), QStringLiteral("下载所选"), [this] { downloadEntries(selectedEntries()); });
    addAction(QStringLiteral("folder-plus"), QStringLiteral("新建文件夹"), [this] { promptNewFolder(); });
    addAction(QStringLiteral("edit"), QStringLiteral("重命名"), [this] { promptRename(); });
    addAction(QStringLiteral("trash"), QStringLiteral("删除"), [this] { confirmDelete(); });
    actions->addStretch();

    m_hiddenButton = new MIconButton(QStringLiteral("eye"), QStringLiteral("显示隐藏文件"), m_actionsBar);
    m_hiddenButton->setButtonSize(30);
    m_hiddenButton->setCheckable(false);
    connect(m_hiddenButton, &QAbstractButton::clicked, this, [this] {
        toggleHidden(!m_showHidden);
    });
    actions->addWidget(m_hiddenButton);
    root->addWidget(m_actionsBar);

    // Stack ------------------------------------------------------------------
    m_stack = new QStackedWidget;
    root->addWidget(m_stack, 1);

    // Loading page
    m_loadingPage = new QWidget;
    auto *loadingLayout = new QVBoxLayout(m_loadingPage);
    loadingLayout->setContentsMargins(24, 24, 24, 24);
    loadingLayout->addStretch();
    m_loadingSpinner = new MSpinner;
    loadingLayout->addWidget(m_loadingSpinner, 0, Qt::AlignHCenter);
    m_loadingLabel = new QLabel(QStringLiteral("正在建立 SFTP 会话…"));
    m_loadingLabel->setAlignment(Qt::AlignCenter);
    m_loadingLabel->setWordWrap(true);
    m_loadingLabel->setProperty("role", QStringLiteral("muted"));
    loadingLayout->addWidget(m_loadingLabel);
    loadingLayout->addStretch();
    m_stack->addWidget(m_loadingPage);

    // Content page
    m_contentPage = new QWidget;
    auto *contentLayout = new QVBoxLayout(m_contentPage);
    contentLayout->setContentsMargins(8, 0, 8, 8);
    contentLayout->setSpacing(6);

    m_model = new RemoteFileModel(this);
    m_model->setShowHidden(m_showHidden);
    m_table = new QTableView(m_contentPage);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(34);
    m_table->setIconSize(QSize(18, 18));
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->setFont(Theme::instance().uiFont(9.5));
    m_table->setSortingEnabled(true);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(RemoteFileModel::NameColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(RemoteFileModel::SizeColumn, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(RemoteFileModel::ModifiedColumn, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(RemoteFileModel::PermissionColumn, QHeaderView::Fixed);
    m_table->setColumnWidth(RemoteFileModel::SizeColumn, 62);
    m_table->setColumnWidth(RemoteFileModel::ModifiedColumn, 104);
    m_table->setColumnWidth(RemoteFileModel::PermissionColumn, 96);
    m_table->horizontalHeader()->setSortIndicator(RemoteFileModel::NameColumn, Qt::AscendingOrder);
    contentLayout->addWidget(m_table, 1);

    m_transferArea = new QWidget(m_contentPage);
    m_transferArea->hide();
    m_transferLayout = new QVBoxLayout(m_transferArea);
    m_transferLayout->setContentsMargins(6, 2, 2, 2);
    m_transferLayout->setSpacing(6);
    auto *transferHeader = new QHBoxLayout;
    m_transferHeader = new QLabel(QStringLiteral("传输队列"));
    m_transferHeader->setFont(Theme::instance().uiFont(8.5, QFont::DemiBold));
    m_transferHeader->setProperty("role", QStringLiteral("muted"));
    transferHeader->addWidget(m_transferHeader);
    transferHeader->addStretch();
    m_clearTransfers = new MIconButton(QStringLiteral("trash"), QStringLiteral("清除已完成"), m_transferArea);
    m_clearTransfers->setButtonSize(24);
    connect(m_clearTransfers, &QAbstractButton::clicked, this, &FilePanel::clearFinishedTransfers);
    transferHeader->addWidget(m_clearTransfers);
    m_transferLayout->addLayout(transferHeader);
    contentLayout->addWidget(m_transferArea);

    m_statusLabel = new QLabel;
    m_statusLabel->setProperty("role", QStringLiteral("muted"));
    m_statusLabel->setFont(Theme::instance().uiFont(8.5));
    m_statusLabel->setContentsMargins(6, 0, 6, 0);
    contentLayout->addWidget(m_statusLabel);
    m_stack->addWidget(m_contentPage);

    // Error page
    m_errorPage = new QWidget;
    auto *errorLayout = new QVBoxLayout(m_errorPage);
    errorLayout->setContentsMargins(28, 24, 28, 24);
    errorLayout->addStretch();
    auto *errorIcon = new QLabel;
    errorIcon->setPixmap(Icons::pixmap(QStringLiteral("alert"), Theme::instance().palette().danger, 34, 2.0));
    errorIcon->setAlignment(Qt::AlignCenter);
    errorLayout->addWidget(errorIcon);
    m_errorLabel = new QLabel(QStringLiteral("SFTP 连接失败"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setProperty("role", QStringLiteral("muted"));
    errorLayout->addWidget(m_errorLabel);
    auto *retry = new MButton(QStringLiteral("重试"), MButton::Variant::Outlined);
    retry->setIconName(QStringLiteral("refresh"));
    connect(retry, &QAbstractButton::clicked, this, [this] {
        if (!m_config.host.isEmpty())
            openSession(m_config, m_expectedFingerprint, m_currentPath);
    });
    errorLayout->addWidget(retry, 0, Qt::AlignHCenter);
    errorLayout->addStretch();
    m_stack->addWidget(m_errorPage);

    m_stack->setCurrentWidget(m_loadingPage);
    showLoading(QStringLiteral("正在建立 SFTP 会话…"));

    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        openEntry(entryAtRow(index.row()));
    });
    connect(m_table, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        showContextMenu(m_table->viewport()->mapToGlobal(position));
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { updateStatus(); });

    connect(&Theme::instance(), &Theme::changed, this, [this] {
        if (m_table) {
            m_table->setFont(Theme::instance().uiFont(9.5));
            m_table->viewport()->update();
        }
        if (m_hiddenButton)
            m_hiddenButton->setActive(m_showHidden);
    });
}

void FilePanel::setBusy(bool busy)
{
    emit busyChanged(busy);
}

void FilePanel::showLoading(const QString &message)
{
    m_loading = true;
    m_ready = false;
    if (m_loadingSpinner)
        m_loadingSpinner->start();
    if (m_loadingLabel)
        m_loadingLabel->setText(message);
    if (m_stack)
        m_stack->setCurrentWidget(m_loadingPage);
    if (m_pathBar)
        m_pathBar->setEnabled(false);
    if (m_actionsBar)
        m_actionsBar->setEnabled(false);
    if (m_titleStatus) {
        const QColor color = Theme::instance().palette().warning;
        m_titleStatus->setStyleSheet(
            QStringLiteral("background:%1;border-radius:3px;").arg(color.name()));
    }
    setBusy(true);
}

void FilePanel::showError(const QString &message)
{
    m_loading = false;
    m_ready = false;
    if (m_loadingSpinner)
        m_loadingSpinner->stop();
    if (m_errorLabel)
        m_errorLabel->setText(message);
    if (m_stack)
        m_stack->setCurrentWidget(m_errorPage);
    if (m_pathBar)
        m_pathBar->setEnabled(false);
    if (m_actionsBar)
        m_actionsBar->setEnabled(false);
    if (m_titleStatus) {
        const QColor color = Theme::instance().palette().danger;
        m_titleStatus->setStyleSheet(
            QStringLiteral("background:%1;border-radius:3px;").arg(color.name()));
    }
    setBusy(false);
}

void FilePanel::showContent()
{
    m_loading = false;
    if (m_loadingSpinner)
        m_loadingSpinner->stop();
    if (m_stack)
        m_stack->setCurrentWidget(m_contentPage);
    if (m_pathBar)
        m_pathBar->setEnabled(true);
    if (m_actionsBar)
        m_actionsBar->setEnabled(true);
    if (m_titleStatus) {
        const QColor color = Theme::instance().palette().success;
        m_titleStatus->setStyleSheet(
            QStringLiteral("background:%1;border-radius:3px;").arg(color.name()));
    }
    setBusy(false);
}

void FilePanel::openSession(const SshConfig &config, const QString &expectedFingerprint,
                            const QString &initialPath)
{
    const bool sameSession = m_worker && m_config.host == config.host && m_config.port == config.port
                             && m_config.username == config.username
                             && m_expectedFingerprint == expectedFingerprint;
    if (sameSession && m_ready) {
        if (!initialPath.isEmpty())
            navigateTo(initialPath);
        return;
    }
    if (sameSession && m_loading)
        return;

    m_config = config;
    m_expectedFingerprint = expectedFingerprint;
    m_initialPath = initialPath;
    m_ready = false;
    m_entries.clear();
    if (m_model)
        m_model->setEntries({});
    destroyWorker();
    showLoading(QStringLiteral("正在建立 SFTP 会话…"));
    createWorker();
}

void FilePanel::createWorker()
{
    m_worker = new SftpWorker;
    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, [this] {
        // Nothing: open() is posted after the event loop starts.
    });
    connect(m_worker, &SftpWorker::opened, this, &FilePanel::handleOpened);
    connect(m_worker, &SftpWorker::openFailed, this, &FilePanel::handleOpenFailed);
    connect(m_worker, &SftpWorker::closed, this, [this] {
        if (!m_shuttingDown) {
            m_ready = false;
            showError(QStringLiteral("SFTP 会话已关闭"));
        }
    });
    connect(m_worker, &SftpWorker::directoryListed, this, &FilePanel::handleListing);
    connect(m_worker, &SftpWorker::directoryListFailed, this, &FilePanel::handleListingFailed);
    connect(m_worker, &SftpWorker::operationFinished, this, &FilePanel::handleOperation);
    connect(m_worker, &SftpWorker::transferProgress, this, [this](int id, qint64 done, qint64 total) {
        for (TransferJob &job : m_transfers) {
            if (job.id == id && job.state == TransferJob::State::Running) {
                job.done = done;
                if (total > 0)
                    job.total = total;
                updateTransferRow(job);
                break;
            }
        }
    });
    connect(m_worker, &SftpWorker::transferFinished, this,
            [this](int id, bool ok, const QString &message) { finishTransfer(id, ok, message); });
    connect(m_worker, &SftpWorker::errorOccurred, this,
            [this](const QString &message) { emit notify(message); });

    m_thread->start();
    const SshConfig config = m_config;
    const QString fingerprint = m_expectedFingerprint;
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, config, fingerprint] { worker->open(config, fingerprint); },
        Qt::QueuedConnection);
    emit busyChanged(true);
}

void FilePanel::destroyWorker()
{
    if (!m_worker && !m_thread)
        return;

    SftpWorker *worker = m_worker;
    QThread *thread = m_thread;
    m_worker = nullptr;
    m_thread = nullptr;

    if (worker && thread) {
        if (thread->isRunning()) {
            worker->cancelTransfer(m_activeTransferId);
            QMetaObject::invokeMethod(worker, &SftpWorker::close, Qt::BlockingQueuedConnection);
            thread->quit();
            if (!thread->wait(8000)) {
                // Extremely unlikely: keep the objects alive and let the thread
                // clean itself up rather than destroying a running QThread.
                worker->setParent(nullptr);
                thread->setParent(nullptr);
                QObject::connect(thread, &QThread::finished, worker, [worker] { worker->deleteLater(); });
                QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
                return;
            }
            delete worker;
            delete thread;
        } else {
            delete worker;
            delete thread;
        }
    } else {
        delete worker;
        delete thread;
    }
}

void FilePanel::closeSession()
{
    m_shuttingDown = true;
    destroyWorker();
    showError(QStringLiteral("终端会话已断开"));
    m_shuttingDown = false;
}

void FilePanel::shutdown()
{
    m_shuttingDown = true;
    if (m_worker) {
        for (TransferJob &job : m_transfers) {
            if (job.state == TransferJob::State::Running)
                m_worker->cancelTransfer(job.id);
        }
    }
    destroyWorker();
    m_transfers.clear();
    m_ready = false;
}

void FilePanel::handleOpened(const QString &homePath, const QString &keyType)
{
    Q_UNUSED(keyType);
    m_homePath = homePath;
    m_ready = true;
    showContent();
    QString target = m_initialPath.trimmed();
    m_initialPath.clear();
    if (target.isEmpty())
        target = homePath;
    else if (target == QStringLiteral("~") || target.startsWith(QStringLiteral("~/")))
        target = homePath + target.mid(1);
    m_currentPath = homePath;
    navigateTo(target, true);
}

void FilePanel::handleOpenFailed(const QString &message)
{
    m_ready = false;
    m_entries.clear();
    if (m_model)
        m_model->setEntries({});
    showError(message);
    emit notify(message);
}

void FilePanel::handleListing(const QString &path, const QVector<SftpEntry> &entries)
{
    if (path != m_requestedPath)
        return;
    m_currentPath = path;
    m_entries = entries;
    m_model->setEntries(entries);
    m_loading = false;
    updatePathEditor();
    showContent();
    updateStatus();
}

void FilePanel::handleListingFailed(const QString &path, const QString &message)
{
    if (path != m_requestedPath)
        return;
    m_loading = false;
    updatePathEditor();
    setBusy(false);
    emit notify(message);
}

void FilePanel::handleOperation(const QString &operation, const QString &path, bool ok,
                                const QString &message)
{
    Q_UNUSED(path);
    if (!ok) {
        emit notify(message);
        return;
    }
    if (operation == QStringLiteral("mkdir")) {
        emit notify(QStringLiteral("文件夹已创建"));
        refresh();
    } else if (operation == QStringLiteral("rename")) {
        emit notify(QStringLiteral("重命名成功"));
        refresh();
    } else if (operation == QStringLiteral("remove")) {
        emit notify(QStringLiteral("已删除"));
        refresh();
    }
}

void FilePanel::navigateTo(const QString &path, bool force)
{
    if (!m_ready && !force)
        return;
    QString target = path.trimmed();
    if (target.isEmpty())
        return;
    if (target == QStringLiteral("~"))
        target = m_homePath;
    else if (target.startsWith(QStringLiteral("~/")))
        target = m_homePath + target.mid(1);
    target = normalizeRemotePath(target);
    if (target == m_requestedPath && !force)
        return;

    m_requestedPath = target;
    m_loading = true;
    if (m_pathEdit)
        m_pathEdit->setText(target);
    if (m_statusLabel)
        m_statusLabel->setText(QStringLiteral("正在读取目录…"));
    if (!m_worker) {
        showError(QStringLiteral("SFTP 会话尚未建立"));
        return;
    }
    setBusy(true);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, target] { worker->listDirectory(target); }, Qt::QueuedConnection);
}

void FilePanel::refresh()
{
    if (!m_ready)
        return;
    navigateTo(m_currentPath.isEmpty() ? m_homePath : m_currentPath, true);
}

void FilePanel::updatePathEditor()
{
    if (m_pathEdit && !m_pathEdit->hasFocus())
        m_pathEdit->setText(m_currentPath);
}

void FilePanel::updateStatus()
{
    if (!m_statusLabel)
        return;
    const int visible = m_model ? m_model->rowCount() : 0;
    const int hidden = qMax(0, m_entries.size() - visible);
    int selected = 0;
    if (m_table && m_table->selectionModel())
        selected = m_table->selectionModel()->selectedRows().size();

    QString text = QStringLiteral("%1 个项目").arg(visible);
    if (hidden > 0)
        text += QStringLiteral("（隐藏 %1）").arg(hidden);
    if (selected > 0)
        text += QStringLiteral(" · 已选择 %1 项").arg(selected);
    m_statusLabel->setText(text);

    if (visible == 0 && !m_loading && m_ready) {
        m_statusLabel->setText(QStringLiteral("空目录"));
    }
    updateEmptyState();
}

void FilePanel::updateEmptyState()
{
    if (m_clearTransfers)
        m_clearTransfers->setVisible(!m_transfers.isEmpty());
    if (m_transferHeader) {
        int active = 0;
        for (const TransferJob &job : m_transfers) {
            if (job.state == TransferJob::State::Queued || job.state == TransferJob::State::Running)
                ++active;
        }
        m_transferHeader->setText(active > 0 ? QStringLiteral("传输队列 · %1 个进行中").arg(active)
                                             : QStringLiteral("传输队列"));
    }
}

void FilePanel::toggleHidden(bool hidden)
{
    m_showHidden = hidden;
    m_model->setShowHidden(hidden);
    if (m_hiddenButton)
        m_hiddenButton->setActive(hidden);
    if (m_settings) {
        m_settings->showHiddenFiles = hidden;
        m_settings->save();
    }
    updateStatus();
}

QVector<int> FilePanel::selectedRows() const
{
    QVector<int> rows;
    if (!m_table || !m_table->selectionModel())
        return rows;
    const QModelIndexList indexes = m_table->selectionModel()->selectedRows();
    for (const QModelIndex &index : indexes) {
        if (!rows.contains(index.row()))
            rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end());
    return rows;
}

QVector<SftpEntry> FilePanel::selectedEntries() const
{
    QVector<SftpEntry> entries;
    for (int row : selectedRows()) {
        const SftpEntry entry = entryAtRow(row);
        if (!entry.name.isEmpty())
            entries.append(entry);
    }
    return entries;
}

SftpEntry FilePanel::entryAtRow(int row) const
{
    if (!m_model)
        return {};
    return m_model->entryAt(row);
}

bool FilePanel::remoteEntryExists(const QString &name) const
{
    for (const SftpEntry &entry : m_entries) {
        if (entry.name == name)
            return true;
    }
    return false;
}

void FilePanel::openEntry(const SftpEntry &entry)
{
    if (!m_ready || entry.name.isEmpty())
        return;
    if (entry.isDirectory) {
        navigateTo(entry.path);
        return;
    }

    QString directory = m_settings && !m_settings->lastLocalDirectory.isEmpty()
                            ? m_settings->lastLocalDirectory
                            : AppPaths::defaultDownloadDir();
    QDir().mkpath(directory);
    const QString local = uniqueLocalPath(directory, entry.name);
    beginDownload(entry, local, true);
}

void FilePanel::downloadEntries(const QVector<SftpEntry> &entries)
{
    if (!m_ready) {
        emit notify(QStringLiteral("SFTP 会话尚未就绪"));
        return;
    }
    QVector<SftpEntry> files;
    for (const SftpEntry &entry : entries) {
        if (!entry.isDirectory)
            files.append(entry);
    }
    if (files.isEmpty()) {
        emit notify(QStringLiteral("请选择要下载的文件"));
        return;
    }

    const QString start = m_settings && !m_settings->lastLocalDirectory.isEmpty()
                              ? m_settings->lastLocalDirectory
                              : QDir::homePath();
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("下载到文件夹"), start);
    if (directory.isEmpty())
        return;
    if (m_settings) {
        m_settings->lastLocalDirectory = directory;
        m_settings->save();
    }

    QVector<QPair<SftpEntry, QString>> targets;
    bool overwrite = false;
    for (const SftpEntry &entry : files) {
        QString local = QDir(directory).filePath(entry.name);
        if (QFileInfo::exists(local)) {
            if (!overwrite
                && !Dialogs::confirm(this, QStringLiteral("覆盖本地文件"),
                                     QStringLiteral("目标文件夹中已存在同名文件，继续将覆盖：\n%1")
                                         .arg(entry.name),
                                     QStringLiteral("覆盖"), false)) {
                return;
            }
            overwrite = true;
        }
        targets.append({entry, local});
    }

    for (const auto &target : targets)
        beginDownload(target.first, target.second, false);
}

void FilePanel::beginDownload(const SftpEntry &entry, const QString &localPath, bool openWhenDone)
{
    addTransfer(true, entry.path, localPath, entry.name, entry.size, openWhenDone);
}

void FilePanel::beginUpload(const QString &localPath, const QString &remotePath)
{
    const QString name = remoteName(remotePath);
    if (remoteEntryExists(name)) {
        if (!Dialogs::confirm(this, QStringLiteral("覆盖远程文件"),
                              QStringLiteral("远程目录中已存在 %1，继续将覆盖。").arg(name),
                              QStringLiteral("覆盖"), false)) {
            return;
        }
    }
    const QFileInfo info(localPath);
    addTransfer(false, remotePath, localPath, info.fileName(), info.size(), false);
}

void FilePanel::uploadLocalFiles(const QStringList &paths)
{
    if (!m_ready) {
        emit notify(QStringLiteral("SFTP 会话尚未就绪"));
        return;
    }
    if (paths.isEmpty())
        return;
    const QString base = m_currentPath.isEmpty() ? m_homePath : m_currentPath;
    bool overwriteAll = false;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (!info.isFile())
            continue;
        const QString remote = joinRemotePath(base, info.fileName());
        if (remoteEntryExists(info.fileName())) {
            if (!overwriteAll
                && !Dialogs::confirm(this, QStringLiteral("覆盖远程文件"),
                                     QStringLiteral("远程目录中已存在 %1，继续将覆盖。").arg(info.fileName()),
                                     QStringLiteral("覆盖全部"), false)) {
                continue;
            }
            overwriteAll = true;
        }
        addTransfer(false, remote, path, info.fileName(), info.size(), false);
    }
}

void FilePanel::promptNewFolder()
{
    if (!m_ready)
        return;
    const QString name = Dialogs::promptText(this, QStringLiteral("新建文件夹"),
                                             QStringLiteral("文件夹名称"), QString(),
                                             QStringLiteral("新的文件夹"), QStringLiteral("folder-plus"));
    if (name.isEmpty())
        return;
    if (name.contains(QLatin1Char('/')) || name == QStringLiteral(".") || name == QStringLiteral("..")) {
        emit notify(QStringLiteral("文件夹名称不合法"));
        return;
    }
    const QString path = joinRemotePath(m_currentPath, name);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, path] { worker->makeDirectory(path); }, Qt::QueuedConnection);
}

void FilePanel::promptRename()
{
    if (!m_ready)
        return;
    const QVector<int> rows = selectedRows();
    if (rows.size() != 1) {
        emit notify(QStringLiteral("请选择一个文件或文件夹进行重命名"));
        return;
    }
    const SftpEntry entry = entryAtRow(rows.first());
    const QString name = Dialogs::promptText(this, QStringLiteral("重命名"),
                                             QStringLiteral("新的名称"), entry.name, entry.name,
                                             QStringLiteral("edit"));
    if (name.isEmpty() || name == entry.name)
        return;
    if (name.contains(QLatin1Char('/')) || name == QStringLiteral(".") || name == QStringLiteral("..")) {
        emit notify(QStringLiteral("名称不合法"));
        return;
    }
    const QString target = joinRemotePath(parentRemotePath(entry.path), name);
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, oldPath = entry.path, target] {
            worker->renameEntry(oldPath, target);
        },
        Qt::QueuedConnection);
}

void FilePanel::confirmDelete()
{
    if (!m_ready)
        return;
    const QVector<SftpEntry> entries = selectedEntries();
    if (entries.isEmpty()) {
        emit notify(QStringLiteral("请选择要删除的项目"));
        return;
    }
    QStringList names;
    bool hasDirectory = false;
    for (const SftpEntry &entry : entries) {
        names.append(entry.name);
        hasDirectory = hasDirectory || entry.isDirectory;
    }
    QString message = QStringLiteral("将要删除以下 %1 个项目：\n%2").arg(entries.size()).arg(names.mid(0, 6).join(QStringLiteral("、")));
    if (names.size() > 6)
        message += QStringLiteral(" 等");
    if (hasDirectory)
        message += QStringLiteral("\n\n文件夹中的内容也会被递归删除，此操作不可撤销。");
    if ((!m_settings || m_settings->confirmBeforeDelete)
        && !Dialogs::confirm(this, QStringLiteral("删除远程文件"), message, QStringLiteral("删除"), true)) {
        return;
    }

    for (const SftpEntry &entry : entries) {
        QMetaObject::invokeMethod(
            m_worker, [worker = m_worker, path = entry.path, isDir = entry.isDirectory] {
                worker->removeEntry(path, isDir);
            },
            Qt::QueuedConnection);
    }
}

void FilePanel::copySelectedPaths()
{
    const QVector<SftpEntry> entries = selectedEntries();
    if (entries.isEmpty())
        return;
    QStringList paths;
    for (const SftpEntry &entry : entries)
        paths.append(entry.path);
    QGuiApplication::clipboard()->setText(paths.join(QLatin1Char('\n')));
    emit notify(QStringLiteral("路径已复制"));
}

void FilePanel::showContextMenu(const QPoint &globalPosition)
{
    const QModelIndex index = m_table->indexAt(m_table->viewport()->mapFromGlobal(globalPosition));
    if (index.isValid() && !m_table->selectionModel()->isRowSelected(index.row(), QModelIndex()))
        m_table->selectRow(index.row());

    const QVector<SftpEntry> entries = selectedEntries();
    const bool hasSelection = !entries.isEmpty();
    bool hasFile = false;
    for (const SftpEntry &entry : entries)
        hasFile = hasFile || !entry.isDirectory;

    QMenu menu(this);
    QAction *open = menu.addAction(QStringLiteral("打开"));
    open->setEnabled(hasSelection);
    QAction *download = menu.addAction(QStringLiteral("下载"));
    download->setEnabled(hasFile);
    menu.addSeparator();
    QAction *newFolder = menu.addAction(QStringLiteral("新建文件夹"));
    QAction *upload = menu.addAction(QStringLiteral("上传文件…"));
    menu.addSeparator();
    QAction *rename = menu.addAction(QStringLiteral("重命名"));
    rename->setEnabled(entries.size() == 1);
    QAction *remove = menu.addAction(QStringLiteral("删除"));
    remove->setEnabled(hasSelection);
    menu.addSeparator();
    QAction *copyPaths = menu.addAction(QStringLiteral("复制路径"));
    copyPaths->setEnabled(hasSelection);
    QAction *refreshAction = menu.addAction(QStringLiteral("刷新"));

    QAction *chosen = menu.exec(globalPosition);
    if (!chosen)
        return;
    if (chosen == open) {
        if (entries.size() == 1)
            openEntry(entries.first());
    } else if (chosen == download) {
        downloadEntries(entries);
    } else if (chosen == newFolder) {
        promptNewFolder();
    } else if (chosen == upload) {
        if (!m_ready)
            return;
        const QString directory = m_settings && !m_settings->lastLocalDirectory.isEmpty()
                                      ? m_settings->lastLocalDirectory
                                      : QDir::homePath();
        const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("上传文件"), directory);
        if (!files.isEmpty())
            uploadLocalFiles(files);
    } else if (chosen == rename) {
        promptRename();
    } else if (chosen == remove) {
        confirmDelete();
    } else if (chosen == copyPaths) {
        copySelectedPaths();
    } else if (chosen == refreshAction) {
        refresh();
    }
}

int FilePanel::addTransfer(bool download, const QString &remotePath, const QString &localPath,
                           const QString &displayName, qint64 total, bool openWhenDone)
{
    TransferJob job;
    job.id = m_nextTransferId++;
    job.download = download;
    job.remotePath = remotePath;
    job.localPath = localPath;
    job.displayName = displayName;
    job.total = total;
    job.openWhenDone = openWhenDone;
    job.state = TransferJob::State::Queued;

    auto *row = new QWidget(m_transferArea);
    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    auto *top = new QHBoxLayout;
    top->setSpacing(6);
    auto *icon = new QLabel;
    icon->setPixmap(Icons::pixmap(download ? QStringLiteral("download") : QStringLiteral("upload"),
                                  Theme::instance().palette().accent, 14, 2.0));
    icon->setFixedSize(16, 16);
    top->addWidget(icon);
    auto *name = new QLabel(displayName);
    name->setFont(Theme::instance().uiFont(8.5, QFont::Medium));
    name->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    top->addWidget(name, 1);
    auto *detail = new QLabel(QStringLiteral("等待中"));
    detail->setFont(Theme::instance().uiFont(8));
    detail->setProperty("role", QStringLiteral("muted"));
    top->addWidget(detail);
    auto *cancel = new MIconButton(QStringLiteral("close"), QStringLiteral("取消传输"), row);
    cancel->setButtonSize(20);
    cancel->setIconName(QStringLiteral("close"));
    top->addWidget(cancel);
    layout->addLayout(top);

    auto *progress = new QProgressBar(row);
    progress->setRange(0, 1000);
    progress->setValue(0);
    progress->setTextVisible(false);
    progress->setFixedHeight(4);
    layout->addWidget(progress);

    job.row = row;
    job.progress = progress;
    job.detail = detail;
    job.cancel = cancel;
    connect(cancel, &QAbstractButton::clicked, this, [this, id = job.id] { cancelTransfer(id); });

    m_transferLayout->addWidget(row);
    m_transferArea->show();
    m_transfers.append(job);
    updateTransferRow(m_transfers.last());
    updateEmptyState();
    startNextTransfer();
    return job.id;
}

void FilePanel::startNextTransfer()
{
    if (m_activeTransferId != -1 || !m_worker || !m_ready)
        return;
    for (TransferJob &job : m_transfers) {
        if (job.state != TransferJob::State::Queued)
            continue;
        job.state = TransferJob::State::Running;
        m_activeTransferId = job.id;
        updateTransferRow(job);
        emit busyChanged(true);
        const int id = job.id;
        const QString remote = job.remotePath;
        const QString local = job.localPath;
        if (job.download) {
            QMetaObject::invokeMethod(
                m_worker, [worker = m_worker, id, remote, local] {
                    worker->downloadFile(id, remote, local);
                },
                Qt::QueuedConnection);
        } else {
            QMetaObject::invokeMethod(
                m_worker, [worker = m_worker, id, local, remote] {
                    worker->uploadFile(id, local, remote);
                },
                Qt::QueuedConnection);
        }
        return;
    }
}

void FilePanel::updateTransferRow(TransferJob &job)
{
    if (!job.row)
        return;
    if (job.progress) {
        if (job.total > 0)
            job.progress->setValue(static_cast<int>(
                qBound<qint64>(qint64(0), job.done * 1000 / job.total, qint64(1000))));
        else if (job.state == TransferJob::State::Done)
            job.progress->setValue(1000);
        else
            job.progress->setValue(0);
    }
    if (job.detail) {
        switch (job.state) {
        case TransferJob::State::Queued:
            job.detail->setText(QStringLiteral("等待中"));
            break;
        case TransferJob::State::Running:
            if (job.total > 0) {
                job.detail->setText(QStringLiteral("%1 / %2")
                                        .arg(formatBytes(job.done), formatBytes(job.total)));
            } else {
                job.detail->setText(formatBytes(job.done));
            }
            break;
        case TransferJob::State::Done:
            job.detail->setText(QStringLiteral("完成"));
            break;
        case TransferJob::State::Failed:
            job.detail->setText(QStringLiteral("失败"));
            break;
        case TransferJob::State::Cancelled:
            job.detail->setText(QStringLiteral("已取消"));
            break;
        }
    }
    if (job.cancel) {
        job.cancel->setEnabled(job.state == TransferJob::State::Queued
                               || job.state == TransferJob::State::Running);
        job.cancel->setVisible(job.state == TransferJob::State::Queued
                               || job.state == TransferJob::State::Running);
    }
}

void FilePanel::finishTransfer(int id, bool ok, const QString &message)
{
    for (TransferJob &job : m_transfers) {
        if (job.id != id)
            continue;
        if (job.state == TransferJob::State::Done || job.state == TransferJob::State::Failed
            || job.state == TransferJob::State::Cancelled) {
            return;
        }
        job.state = ok ? TransferJob::State::Done : TransferJob::State::Failed;
        if (message.contains(QStringLiteral("取消")))
            job.state = TransferJob::State::Cancelled;
        job.message = message;
        if (ok)
            job.done = job.total > 0 ? job.total : job.done;
        updateTransferRow(job);
        if (m_activeTransferId == id)
            m_activeTransferId = -1;
        if (!ok && job.state != TransferJob::State::Cancelled)
            emit notify(message);
        if (ok) {
            if (job.openWhenDone && job.download)
                QDesktopServices::openUrl(QUrl::fromLocalFile(job.localPath));
            refresh();
        }
        const int jobId = id;
        QTimer::singleShot(4500, this, [this, jobId] { removeTransferRow(jobId); });
        updateEmptyState();
        startNextTransfer();
        return;
    }
}

void FilePanel::removeTransferRow(int id)
{
    for (int i = 0; i < m_transfers.size(); ++i) {
        if (m_transfers.at(i).id != id)
            continue;
        TransferJob &job = m_transfers[i];
        if (job.state == TransferJob::State::Running || job.state == TransferJob::State::Queued)
            return;
        if (job.row) {
            m_transferLayout->removeWidget(job.row);
            job.row->deleteLater();
        }
        m_transfers.removeAt(i);
        break;
    }
    if (m_transfers.isEmpty())
        m_transferArea->hide();
    updateEmptyState();
}

void FilePanel::cancelTransfer(int id)
{
    for (TransferJob &job : m_transfers) {
        if (job.id != id)
            continue;
        if (job.state == TransferJob::State::Queued) {
            job.state = TransferJob::State::Cancelled;
            job.message = QStringLiteral("已取消");
            updateTransferRow(job);
            const int jobId = id;
            QTimer::singleShot(2500, this, [this, jobId] { removeTransferRow(jobId); });
            updateEmptyState();
            return;
        }
        if (job.state == TransferJob::State::Running) {
            if (m_worker)
                m_worker->cancelTransfer(id);
            if (job.detail)
                job.detail->setText(QStringLiteral("正在取消…"));
            return;
        }
    }
}

void FilePanel::clearFinishedTransfers()
{
    QVector<int> removeIds;
    for (const TransferJob &job : m_transfers) {
        if (job.state == TransferJob::State::Done || job.state == TransferJob::State::Failed
            || job.state == TransferJob::State::Cancelled) {
            removeIds.append(job.id);
        }
    }
    for (int id : removeIds)
        removeTransferRow(id);
}

void FilePanel::paintEvent(QPaintEvent *)
{
    const Palette &palette = Theme::instance().palette();
    QPainter painter(this);
    painter.fillRect(rect(), palette.surface);
    painter.setPen(QPen(palette.border, 1));
    painter.drawLine(0, 0, 0, height());
}

void FilePanel::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile()) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    QWidget::dragEnterEvent(event);
}

void FilePanel::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile())
            files.append(url.toLocalFile());
    }
    if (!files.isEmpty()) {
        uploadLocalFiles(files);
        event->acceptProposedAction();
        return;
    }
    QWidget::dropEvent(event);
}

} // namespace nyan::ui
