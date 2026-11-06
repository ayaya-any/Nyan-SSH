#pragma once

#include "core/AppSettings.h"
#include "core/SftpTypes.h"
#include "core/SshTypes.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QProgressBar;
class QStackedWidget;
class QTableView;
class QThread;
class QVBoxLayout;
class QWidget;

namespace nyan {
class SftpWorker;
}

namespace nyan::ui {

class MButton;
class MIconButton;
class MSpinner;
class RemoteFileModel;

class FilePanel : public QWidget {
    Q_OBJECT
public:
    explicit FilePanel(QWidget *parent = nullptr);
    ~FilePanel() override;

    void setSettings(AppSettings *settings);
    void openSession(const SshConfig &config, const QString &expectedFingerprint, const QString &initialPath = {});
    void closeSession();
    void shutdown();

    bool isReady() const { return m_ready; }
    QString currentPath() const { return m_currentPath; }

    void uploadLocalFiles(const QStringList &paths);

signals:
    void notify(const QString &message, const QString &actionText = QString());
    void closeRequested();
    void busyChanged(bool busy);

protected:
    void paintEvent(QPaintEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    struct TransferJob {
        enum class State {
            Queued,
            Running,
            Done,
            Failed,
            Cancelled,
        };

        int id = 0;
        bool download = false;
        QString remotePath;
        QString localPath;
        QString displayName;
        qint64 done = 0;
        qint64 total = 0;
        State state = State::Queued;
        QString message;
        bool openWhenDone = false;
        QWidget *row = nullptr;
        QProgressBar *progress = nullptr;
        QLabel *detail = nullptr;
        MIconButton *cancel = nullptr;
    };

    void buildUi();
    void createWorker();
    void destroyWorker();
    void showLoading(const QString &message);
    void showError(const QString &message);
    void showContent();

    void navigateTo(const QString &path, bool force = false);
    void refresh();
    void updatePathEditor();
    void updateStatus();
    void updateEmptyState();
    void setBusy(bool busy);

    void handleOpened(const QString &homePath, const QString &keyType);
    void handleOpenFailed(const QString &message);
    void handleListing(const QString &path, const QVector<SftpEntry> &entries);
    void handleListingFailed(const QString &path, const QString &message);
    void handleOperation(const QString &operation, const QString &path, bool ok, const QString &message);

    QVector<int> selectedRows() const;
    QVector<SftpEntry> selectedEntries() const;
    SftpEntry entryAtRow(int row) const;
    void openEntry(const SftpEntry &entry);
    void downloadEntries(const QVector<SftpEntry> &entries);
    void beginDownload(const SftpEntry &entry, const QString &localPath, bool openWhenDone = false);
    void beginUpload(const QString &localPath, const QString &remotePath);
    void promptNewFolder();
    void promptRename();
    void confirmDelete();
    void copySelectedPaths();
    void showContextMenu(const QPoint &globalPosition);
    void toggleHidden(bool hidden);

    int addTransfer(bool download, const QString &remotePath, const QString &localPath,
                    const QString &displayName, qint64 total, bool openWhenDone);
    void startNextTransfer();
    void updateTransferRow(TransferJob &job);
    void finishTransfer(int id, bool ok, const QString &message);
    void removeTransferRow(int id);
    void cancelTransfer(int id);
    void clearFinishedTransfers();
    bool remoteEntryExists(const QString &name) const;

    AppSettings *m_settings = nullptr;
    SftpWorker *m_worker = nullptr;
    QThread *m_thread = nullptr;
    SshConfig m_config;
    QString m_expectedFingerprint;

    bool m_ready = false;
    bool m_loading = false;
    bool m_shuttingDown = false;
    bool m_showHidden = false;
    QString m_homePath;
    QString m_currentPath;
    QString m_requestedPath;
    QString m_initialPath;
    QVector<SftpEntry> m_entries;

    QStackedWidget *m_stack = nullptr;
    QWidget *m_loadingPage = nullptr;
    QWidget *m_contentPage = nullptr;
    QWidget *m_errorPage = nullptr;
    MSpinner *m_loadingSpinner = nullptr;
    QLabel *m_loadingLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
    QLabel *m_titleStatus = nullptr;
    MIconButton *m_hiddenButton = nullptr;
    QWidget *m_pathBar = nullptr;
    QWidget *m_actionsBar = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QTableView *m_table = nullptr;
    RemoteFileModel *m_model = nullptr;
    QLabel *m_statusLabel = nullptr;
    QWidget *m_transferArea = nullptr;
    QVBoxLayout *m_transferLayout = nullptr;
    QLabel *m_transferHeader = nullptr;
    MIconButton *m_clearTransfers = nullptr;

    QVector<TransferJob> m_transfers;
    int m_nextTransferId = 1;
    int m_activeTransferId = -1;
};

} // namespace nyan::ui
