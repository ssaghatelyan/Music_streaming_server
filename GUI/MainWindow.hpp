#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QSlider>
#include <QLineEdit>
#include <QStackedWidget>
#include <QProcess>
#include "Client.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Client *client, QWidget *parent = nullptr);

private slots:
    void onPlaylistReceived(const QStringList &songs);
    void onRefresh();
    void onPlay();
    void onStop();
    void onDownload();
    void onSongReady(const QString &path);
    void onDownloadDone(const QString &path);
    void onError(const QString &msg);
    void onProgress(qint64 received, qint64 total);

    // YouTube
    void onYouTubeSearch();
    void onYouTubePlay();

private:
    Client        *m_client;

    // Tabs
    QStackedWidget *m_stack;
    QPushButton    *m_tabServer;
    QPushButton    *m_tabYoutube;

    // Server tab
    QListWidget   *m_list;
    QPushButton   *m_playBtn;
    QPushButton   *m_stopBtn;
    QPushButton   *m_downloadBtn;
    QPushButton   *m_refreshBtn;

    // YouTube tab
    QLineEdit     *m_ytSearchEdit;
    QPushButton   *m_ytSearchBtn;
    QListWidget   *m_ytResultsList;
    QPushButton   *m_ytPlayBtn;
    QLabel        *m_ytStatusLabel;
    QStringList    m_ytVideoIds;

    // Shared player bar
    QPushButton   *m_prevBtn;
    QPushButton   *m_nextBtn;
    QLabel        *m_nowPlaying;
    QLabel        *m_statusLabel;
    QProgressBar  *m_progressBar;

    // mpg123 process (replaces QMediaPlayer)
    QProcess      *m_playerProc = nullptr;

    QStringList    m_songs;

    void setStatus(const QString &msg, bool isError = false);
    QString getDownloadDir();
    void switchTab(int index);
    void startYtDlp(const QString &query);
    void playYtDlp(const QString &videoId, const QString &title);
    void playFile(const QString &path);
    void stopPlayback();
};
