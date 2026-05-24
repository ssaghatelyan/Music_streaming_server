#include "MainWindow.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStandardPaths>
#include <QUrl>

// ─────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────
MainWindow::MainWindow(Client *client, QWidget *parent)
    : QMainWindow(parent), m_client(client)
{
    setWindowTitle("MusicStream");
    setMinimumSize(640, 480);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->setSpacing(8);
    root->setContentsMargins(10, 10, 10, 10);

    // ── Tab bar ──────────────────────────────
    const QString tabStyle =
        "QPushButton { border: none; padding: 6px 18px; border-radius: 4px; }"
        "QPushButton:checked { background: #7c6af5; color: white; font-weight: bold; }"
        "QPushButton:!checked { background: transparent; color: #aaa; }";

    m_tabServer  = new QPushButton("Server Library");
    m_tabYoutube = new QPushButton("YouTube");
    m_tabServer->setCheckable(true);
    m_tabYoutube->setCheckable(true);
    m_tabServer->setStyleSheet(tabStyle);
    m_tabYoutube->setStyleSheet(tabStyle);
    m_tabServer->setChecked(true);

    auto *tabBar = new QHBoxLayout();
    tabBar->addWidget(m_tabServer);
    tabBar->addWidget(m_tabYoutube);
    tabBar->addStretch();
    root->addLayout(tabBar);

    // ── Stacked pages ────────────────────────
    m_stack = new QStackedWidget();
    root->addWidget(m_stack);

    // ── Page 0 : Server ──────────────────────
    auto *serverPage   = new QWidget();
    auto *serverLayout = new QVBoxLayout(serverPage);
    serverLayout->setSpacing(6);

    m_list = new QListWidget();
    m_list->setAlternatingRowColors(true);
    serverLayout->addWidget(m_list);

    auto *serverBtns = new QHBoxLayout();
    m_refreshBtn  = new QPushButton("Refresh");
    m_playBtn     = new QPushButton("Play");
    m_stopBtn     = new QPushButton("Stop");
    m_downloadBtn = new QPushButton("Download");
    serverBtns->addWidget(m_refreshBtn);
    serverBtns->addWidget(m_playBtn);
    serverBtns->addWidget(m_stopBtn);
    serverBtns->addWidget(m_downloadBtn);
    serverLayout->addLayout(serverBtns);

    m_stack->addWidget(serverPage);   // index 0

    // ── Page 1 : YouTube ─────────────────────
    auto *ytPage   = new QWidget();
    auto *ytLayout = new QVBoxLayout(ytPage);
    ytLayout->setSpacing(6);

    auto *ytSearchRow = new QHBoxLayout();
    m_ytSearchEdit = new QLineEdit();
    m_ytSearchEdit->setPlaceholderText("Search YouTube…");
    m_ytSearchBtn  = new QPushButton("Search");
    ytSearchRow->addWidget(m_ytSearchEdit);
    ytSearchRow->addWidget(m_ytSearchBtn);
    ytLayout->addLayout(ytSearchRow);

    m_ytResultsList = new QListWidget();
    m_ytResultsList->setAlternatingRowColors(true);
    ytLayout->addWidget(m_ytResultsList);

    auto *ytBtns = new QHBoxLayout();
    m_ytPlayBtn = new QPushButton("Play selected");
    auto *ytStopBtn = new QPushButton("Stop");
    ytBtns->addWidget(m_ytPlayBtn);
    ytBtns->addWidget(ytStopBtn);
    ytLayout->addLayout(ytBtns);

    m_ytStatusLabel = new QLabel("Ready.");
    m_ytStatusLabel->setWordWrap(true);
    ytLayout->addWidget(m_ytStatusLabel);

    m_stack->addWidget(ytPage);       // index 1

    // ── Shared player bar ─────────────────────
    auto *playerBar = new QVBoxLayout();

    m_nowPlaying = new QLabel("No track playing");
    m_nowPlaying->setAlignment(Qt::AlignCenter);
    m_nowPlaying->setStyleSheet("color: #7c6af5; font-weight: bold;");
    playerBar->addWidget(m_nowPlaying);

    auto *ctrlRow = new QHBoxLayout();
    m_prevBtn = new QPushButton("Prev");
    m_nextBtn = new QPushButton("Next");
    ctrlRow->addWidget(m_prevBtn);
    ctrlRow->addWidget(m_nextBtn);
    ctrlRow->addStretch();
    playerBar->addLayout(ctrlRow);

    m_statusLabel = new QLabel("");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    playerBar->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 100);
    playerBar->addWidget(m_progressBar);

    root->addLayout(playerBar);

    // ── Connect: tabs ─────────────────────────
    connect(m_tabServer,  &QPushButton::clicked, this, [=]{ switchTab(0); });
    connect(m_tabYoutube, &QPushButton::clicked, this, [=]{ switchTab(1); });

    // ── Connect: server buttons ───────────────
    connect(m_refreshBtn,  &QPushButton::clicked, this, &MainWindow::onRefresh);
    connect(m_playBtn,     &QPushButton::clicked, this, &MainWindow::onPlay);
    connect(m_stopBtn,     &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_downloadBtn, &QPushButton::clicked, this, &MainWindow::onDownload);

    // ── Connect: youtube buttons ──────────────
    connect(m_ytSearchBtn,  &QPushButton::clicked,    this, &MainWindow::onYouTubeSearch);
    connect(m_ytPlayBtn,    &QPushButton::clicked,    this, &MainWindow::onYouTubePlay);
    connect(ytStopBtn,      &QPushButton::clicked,    this, &MainWindow::onStop);
    connect(m_ytSearchEdit, &QLineEdit::returnPressed, this, &MainWindow::onYouTubeSearch);

    // ── Connect: prev / next ──────────────────
    connect(m_prevBtn, &QPushButton::clicked, this, [=]{
        int row = m_list->currentRow();
        if (row > 0) { m_list->setCurrentRow(row - 1); onPlay(); }
    });
    connect(m_nextBtn, &QPushButton::clicked, this, [=]{
        int row = m_list->currentRow();
        if (row < m_list->count() - 1) { m_list->setCurrentRow(row + 1); onPlay(); }
    });

    // ── Connect: Client signals ───────────────
    connect(m_client, &Client::playlistReceived, this, &MainWindow::onPlaylistReceived);
    connect(m_client, &Client::songReady,        this, &MainWindow::onSongReady);
    connect(m_client, &Client::downloadDone,     this, &MainWindow::onDownloadDone);
    connect(m_client, &Client::error,            this, &MainWindow::onError);
    connect(m_client, &Client::downloadProgress, this, &MainWindow::onProgress);

    onRefresh();
}

// ─────────────────────────────────────────────
//  Playback via mpg123 (no PulseAudio/GStreamer needed)
// ─────────────────────────────────────────────
void MainWindow::playFile(const QString &path)
{
    stopPlayback();

    m_playerProc = new QProcess(this);
    m_playerProc->start("mpg123", { "-q", path });

    connect(m_playerProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int, QProcess::ExitStatus) {
        setStatus("Finished.");
        m_playerProc->deleteLater();
        m_playerProc = nullptr;
    });

    setStatus("Playing…");
}

void MainWindow::stopPlayback()
{
    if (m_playerProc) {
        m_playerProc->kill();
        m_playerProc->waitForFinished(500);
        m_playerProc->deleteLater();
        m_playerProc = nullptr;
        setStatus("Stopped.");
    }
}

// ─────────────────────────────────────────────
//  Tab switch
// ─────────────────────────────────────────────
void MainWindow::switchTab(int index)
{
    m_stack->setCurrentIndex(index);
    m_tabServer->setChecked(index == 0);
    m_tabYoutube->setChecked(index == 1);
}

// ─────────────────────────────────────────────
//  Server tab slots
// ─────────────────────────────────────────────
void MainWindow::onRefresh()
{
    setStatus("Loading playlist…");
    m_client->requestList();
}

void MainWindow::onPlay()
{
    auto *item = m_list->currentItem();
    if (!item) { setStatus("Select a song first.", true); return; }

    QString name = item->text();
    setStatus("Downloading from server: " + name);
    m_nowPlaying->setText(name);
    m_client->playSong(name);
}

void MainWindow::onStop()
{
    stopPlayback();
}

void MainWindow::onDownload()
{
    auto *item = m_list->currentItem();
    if (!item) { setStatus("Select a song first.", true); return; }

    QString name = item->text();
    QString path = getDownloadDir() + "/" + name;

    setStatus("Downloading: " + name);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_client->downloadSong(name, path);
}

void MainWindow::onPlaylistReceived(const QStringList &songs)
{
    m_songs = songs;
    m_list->clear();
    m_list->addItems(songs);
    setStatus(QString("Loaded %1 tracks.").arg(songs.size()));
}

void MainWindow::onSongReady(const QString &path)
{
    playFile(path);
}

void MainWindow::onDownloadDone(const QString &path)
{
    m_progressBar->setVisible(false);
    setStatus("Saved to " + path);
}

void MainWindow::onError(const QString &msg)
{
    setStatus(msg, true);
    m_progressBar->setVisible(false);
}

void MainWindow::onProgress(qint64 received, qint64 total)
{
    if (total > 0)
        m_progressBar->setValue(static_cast<int>(received * 100 / total));
}

// ─────────────────────────────────────────────
//  YouTube tab slots
// ─────────────────────────────────────────────
void MainWindow::onYouTubeSearch()
{
    QString query = m_ytSearchEdit->text().trimmed();
    if (query.isEmpty()) return;
    startYtDlp(query);
}

void MainWindow::onYouTubePlay()
{
    int row = m_ytResultsList->currentRow();
    if (row < 0 || row >= m_ytVideoIds.size()) {
        m_ytStatusLabel->setText("Select a result first.");
        return;
    }
    playYtDlp(m_ytVideoIds[row], m_ytResultsList->item(row)->text());
}

void MainWindow::startYtDlp(const QString &query)
{
    m_ytResultsList->clear();
    m_ytVideoIds.clear();
    m_ytStatusLabel->setText("Searching…");
    m_ytPlayBtn->setEnabled(false);

    QProcess *proc = new QProcess(this);
    proc->start("yt-dlp", {
        "--cookies-from-browser", "chromium:/home/ssaghatelyan/snap/chromium/common/chromium",
        "--no-playlist", "--flat-playlist",
        "--print", "%(id)s|||%(title)s",
        "ytsearch5:" + query
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int code, QProcess::ExitStatus) {
        if (code == 0) {
            for (auto &line : QString(proc->readAllStandardOutput()).split('\n', Qt::SkipEmptyParts)) {
                int sep = line.indexOf("|||");
                if (sep != -1) {
                    m_ytVideoIds.append(line.left(sep).trimmed());
                    m_ytResultsList->addItem(line.mid(sep + 3).trimmed());
                }
            }
            if (m_ytResultsList->count() == 0)
                m_ytStatusLabel->setText("No results found.");
            else {
                m_ytStatusLabel->setText(
                    QString("Found %1 results. Select and press Play.").arg(m_ytResultsList->count()));
                m_ytPlayBtn->setEnabled(true);
            }
        } else {
            QString err = proc->readAllStandardError();
            m_ytStatusLabel->setText("Search failed: " + (err.isEmpty() ? "is yt-dlp installed?" : err.left(120)));
        }
        proc->deleteLater();
    });
}

void MainWindow::playYtDlp(const QString &videoId, const QString &title)
{
    stopPlayback();
    m_ytStatusLabel->setText("Downloading: " + title);
    m_nowPlaying->setText(title);
    m_ytPlayBtn->setEnabled(false);

    QString out = "/tmp/yt_" + videoId + ".mp3";

    QProcess *proc = new QProcess(this);
    proc->start("yt-dlp", {
        "--cookies-from-browser", "chromium:/home/ssaghatelyan/snap/chromium/common/chromium",
        "-x", "--audio-format", "mp3",
        "-o", out,
        "--no-playlist",
        "https://www.youtube.com/watch?v=" + videoId
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int code, QProcess::ExitStatus) {
        m_ytPlayBtn->setEnabled(true);
        if (code == 0) {
            m_ytStatusLabel->setText("Playing: " + title);
            playFile(out);
        } else {
            QString err = proc->readAllStandardError();
            m_ytStatusLabel->setText("Download failed: " + (err.isEmpty() ? "check yt-dlp & ffmpeg" : err.left(120)));
        }
        proc->deleteLater();
    });
}

// ─────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────
void MainWindow::setStatus(const QString &msg, bool isError)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(isError ? "color: #e05555;" : "color: #aaa;");
}

QString MainWindow::getDownloadDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return dir;
}
