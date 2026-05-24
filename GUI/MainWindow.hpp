#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QSlider>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QStackedWidget>
#include "Client.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(Client *client, QWidget *parent = nullptr);

private slots:
    void onPlaylistReceived(const QStringList &songs);
    void onRefresh();
    void onPlay();
    void onDownload();
    void onSongReady(const QString &path);
    void onDownloadDone(const QString &path);
    void onError(const QString &msg);
    void onProgress(qint64 received, qint64 total);
    void onDurationChanged(qint64 duration);
    void onPositionChanged(qint64 position);
    void onSeek(int value);
    void onVolumeChanged(int value);

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
    QLabel        *m_timeCurrent;
    QLabel        *m_timeTotal;
    QLabel        *m_statusLabel;
    QProgressBar  *m_progressBar;
    QSlider       *m_seekSlider;
    QSlider       *m_volSlider;
    QMediaPlayer  *m_player;
    QAudioOutput  *m_audio;

    QStringList    m_songs;
    bool           m_isSeeking = false;

    QString currentSongName();
    void    setStatus(const QString &msg, bool isError = false);
    QString getDownloadDir();
    void    switchTab(int index);
    void    startYtDlp(const QString &query);
    void    playYtDlp(const QString &videoId, const QString &title);
};
