#include "MainWindow.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QThread>
#include <QFileDialog>
#include <QStandardPaths>
#include <QUrl>
#include <QTime>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

static const QString MAIN_STYLE = R"(
QMainWindow, QWidget#central {
    background: #1a1a1a;
}
QListWidget {
    background: #111;
    border: none;
    color: #ddd;
    font-size: 13px;
    outline: none;
}
QListWidget::item {
    padding: 8px 12px;
    border-bottom: 1px solid #1f1f1f;
}
QListWidget::item:selected {
    background: #2a2a2a;
    color: #a89df7;
}
QListWidget::item:hover { background: #222; }
QLineEdit {
    background: #2a2a2a;
    color: #ddd;
    border: 1px solid #3a3a3a;
    border-radius: 6px;
    padding: 5px 10px;
    font-size: 13px;
}
QLineEdit:focus { border-color: #7c6af5; }
QSlider::groove:horizontal {
    height: 3px;
    background: #333;
    border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background: #7c6af5;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #a89df7;
    width: 12px;
    height: 12px;
    margin: -5px 0;
    border-radius: 6px;
}
QProgressBar {
    background: #2a2a2a;
    border: none;
    border-radius: 3px;
    height: 4px;
    text-align: center;
    color: transparent;
}
QProgressBar::chunk {
    background: #7c6af5;
    border-radius: 3px;
}
QScrollBar:vertical {
    background: #1a1a1a;
    width: 6px;
}
QScrollBar::handle:vertical {
    background: #333;
    border-radius: 3px;
}
)";

static const QString BTN_PRIMARY = R"(
QPushButton { background:#7c6af5; color:white; border:none; border-radius:6px; padding:7px 18px; font-size:13px; }
QPushButton:hover { background:#9080f7; }
QPushButton:disabled { background:#2a2a2a; color:#555; }
)";
static const QString BTN_OUTLINE = R"(
QPushButton { background:transparent; color:#888; border:1px solid #333; border-radius:6px; padding:7px 18px; font-size:13px; }
QPushButton:hover { background:#2a2a2a; color:#ccc; }
QPushButton:disabled { color:#444; }
)";
static const QString BTN_ICON = R"(
QPushButton { background:transparent; color:#888; border:none; font-size:18px; padding:4px 8px; border-radius:4px; }
QPushButton:hover { color:#ccc; }
QPushButton:disabled { color:#444; }
)";
static const QString BTN_PLAY_MAIN = R"(
QPushButton { background:#7c6af5; color:white; border:none; border-radius:18px; font-size:16px; min-width:36px; max-width:36px; min-height:36px; max-height:36px; }
QPushButton:hover { background:#9080f7; }
QPushButton:disabled { background:#2a2a2a; color:#555; }
)";
static const QString BTN_TAB_ACTIVE = R"(
QPushButton { background:#2a2a2a; color:#a89df7; border:none; border-bottom:2px solid #7c6af5; padding:8px 20px; font-size:13px; font-weight:500; }
)";
static const QString BTN_TAB_INACTIVE = R"(
QPushButton { background:transparent; color:#555; border:none; border-bottom:2px solid transparent; padding:8px 20px; font-size:13px; }
QPushButton:hover { color:#888; }
)";

static QString fmtTime(qint64 ms) {
    int s = ms / 1000;
    return QTime(0, s / 60, s % 60).toString("m:ss");
}

static QPushButton* makeBtn(const QString &text, const QString &style, QWidget *parent) {
    auto *b = new QPushButton(text, parent);
    b->setStyleSheet(style);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

MainWindow::MainWindow(Client *client, QWidget *parent)
    : QMainWindow(parent), m_client(client)
{
    setWindowTitle("MusicStream");
    setMinimumSize(700, 560);
    setStyleSheet(MAIN_STYLE);

    auto *central = new QWidget(this);
    central->setObjectName("central");
    setCentralWidget(central);

    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Top bar ──────────────────────────────────────────────────────────────
    auto *topBar = new QWidget(central);
    topBar->setStyleSheet("background:#1f1f1f; border-bottom:1px solid #2a2a2a;");
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(16, 8, 16, 0);
    topLayout->setSpacing(0);

    auto *logo = new QLabel("♪ MusicStream", topBar);
    logo->setStyleSheet("color:#a89df7; font-size:15px; font-weight:500; padding-right:20px;");

    m_tabServer  = makeBtn("Server Library", BTN_TAB_ACTIVE,   topBar);
    m_tabYoutube = makeBtn("YouTube",        BTN_TAB_INACTIVE, topBar);

    m_refreshBtn = makeBtn("⟳", BTN_OUTLINE, topBar);
    m_refreshBtn->setFixedWidth(36);
    m_refreshBtn->setToolTip("Refresh playlist");

    topLayout->addWidget(logo);
    topLayout->addWidget(m_tabServer);
    topLayout->addWidget(m_tabYoutube);
    topLayout->addStretch();
    topLayout->addWidget(m_refreshBtn);
    root->addWidget(topBar);

    // ── Stacked area ─────────────────────────────────────────────────────────
    m_stack = new QStackedWidget(central);

    // Page 0: Server playlist
    auto *serverPage = new QWidget();
    auto *spLayout = new QVBoxLayout(serverPage);
    spLayout->setContentsMargins(0, 0, 0, 0);
    spLayout->setSpacing(0);

    m_list = new QListWidget(serverPage);
    m_list->setAlternatingRowColors(false);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    spLayout->addWidget(m_list, 1);

    auto *actBar = new QWidget(serverPage);
    actBar->setStyleSheet("background:#1a1a1a; border-top:1px solid #222;");
    auto *actLayout = new QHBoxLayout(actBar);
    actLayout->setContentsMargins(16, 10, 16, 10);
    actLayout->setSpacing(10);

    m_playBtn     = makeBtn("▶  Play",     BTN_PRIMARY, actBar);
    m_downloadBtn = makeBtn("⬇  Download", BTN_OUTLINE, actBar);
    m_playBtn->setEnabled(false);
    m_downloadBtn->setEnabled(false);

    m_statusLabel = new QLabel("", actBar);
    m_statusLabel->setStyleSheet("color:#888; font-size:12px;");

    actLayout->addWidget(m_playBtn);
    actLayout->addWidget(m_downloadBtn);
    actLayout->addStretch();
    actLayout->addWidget(m_statusLabel);
    spLayout->addWidget(actBar);

    m_stack->addWidget(serverPage);

    // Page 1: YouTube
    auto *ytPage = new QWidget();
    ytPage->setStyleSheet("background:#1a1a1a;");
    auto *ytLayout = new QVBoxLayout(ytPage);
    ytLayout->setContentsMargins(16, 16, 16, 12);
    ytLayout->setSpacing(10);

    auto *ytSearchRow = new QHBoxLayout();
    m_ytSearchEdit = new QLineEdit(ytPage);
    m_ytSearchEdit->setPlaceholderText("Search YouTube... (e.g. Radiohead Creep)");
    m_ytSearchBtn  = makeBtn("Search", BTN_PRIMARY, ytPage);
    m_ytSearchBtn->setFixedWidth(80);
    ytSearchRow->addWidget(m_ytSearchEdit, 1);
    ytSearchRow->addWidget(m_ytSearchBtn);
    ytLayout->addLayout(ytSearchRow);

    m_ytResultsList = new QListWidget(ytPage);
    ytLayout->addWidget(m_ytResultsList, 1);

    auto *ytActRow = new QHBoxLayout();
    m_ytPlayBtn = makeBtn("▶  Play selected", BTN_PRIMARY, ytPage);
    m_ytPlayBtn->setEnabled(false);
    m_ytStatusLabel = new QLabel("", ytPage);
    m_ytStatusLabel->setStyleSheet("color:#888; font-size:12px;");
    ytActRow->addWidget(m_ytPlayBtn);
    ytActRow->addStretch();
    ytActRow->addWidget(m_ytStatusLabel);
    ytLayout->addLayout(ytActRow);

    m_stack->addWidget(ytPage);
    root->addWidget(m_stack, 1);

    // ── Progress bar ─────────────────────────────────────────────────────────
    m_progressBar = new QProgressBar(central);
    m_progressBar->setRange(0, 100);
    m_progressBar->setFixedHeight(4);
    m_progressBar->setVisible(false);
    root->addWidget(m_progressBar);

    // ── Player bar ───────────────────────────────────────────────────────────
    auto *playerBar = new QWidget(central);
    playerBar->setStyleSheet("background:#111; border-top:1px solid #222;");
    auto *pbLayout = new QVBoxLayout(playerBar);
    pbLayout->setContentsMargins(16, 12, 16, 12);
    pbLayout->setSpacing(8);

    m_nowPlaying = new QLabel("Not playing", playerBar);
    m_nowPlaying->setStyleSheet("color:#888; font-size:12px;");
    pbLayout->addWidget(m_nowPlaying);

    auto *seekRow = new QHBoxLayout();
    seekRow->setSpacing(8);
    m_timeCurrent = new QLabel("0:00", playerBar);
    m_timeCurrent->setStyleSheet("color:#555; font-size:11px; min-width:30px;");
    m_seekSlider = new QSlider(Qt::Horizontal, playerBar);
    m_seekSlider->setRange(0, 1000);
    m_seekSlider->setValue(0);
    m_seekSlider->setEnabled(false);
    m_timeTotal = new QLabel("0:00", playerBar);
    m_timeTotal->setStyleSheet("color:#555; font-size:11px; min-width:30px; text-align:right;");
    seekRow->addWidget(m_timeCurrent);
    seekRow->addWidget(m_seekSlider, 1);
    seekRow->addWidget(m_timeTotal);
    pbLayout->addLayout(seekRow);

    auto *ctrlRow = new QHBoxLayout();
    ctrlRow->setSpacing(4);
    m_prevBtn = makeBtn("⏮", BTN_ICON, playerBar);
    auto *playPauseBtn = makeBtn("⏸", BTN_PLAY_MAIN, playerBar);
    playPauseBtn->setObjectName("playPauseBtn");
    m_nextBtn = makeBtn("⏭", BTN_ICON, playerBar);

    auto *volIcon = new QLabel("🔊", playerBar);
    volIcon->setStyleSheet("color:#555; font-size:13px;");
    m_volSlider = new QSlider(Qt::Horizontal, playerBar);
    m_volSlider->setRange(0, 100);
    m_volSlider->setValue(80);
    m_volSlider->setFixedWidth(80);

    ctrlRow->addWidget(m_prevBtn);
    ctrlRow->addWidget(playPauseBtn);
    ctrlRow->addWidget(m_nextBtn);
    ctrlRow->addStretch();
    ctrlRow->addWidget(volIcon);
    ctrlRow->addWidget(m_volSlider);
    pbLayout->addLayout(ctrlRow);

    root->addWidget(playerBar);

    // ── Media player ─────────────────────────────────────────────────────────
    m_player = new QMediaPlayer(this);
    m_audio  = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);
    m_audio->setVolume(0.8f);

    // ── Connections ──────────────────────────────────────────────────────────

    // Tab switching
    connect(m_tabServer, &QPushButton::clicked, this, [this]{ switchTab(0); });
    connect(m_tabYoutube, &QPushButton::clicked, this, [this]{ switchTab(1); });

    // Server tab
    connect(m_refreshBtn,  &QPushButton::clicked, this, &MainWindow::onRefresh);
    connect(m_playBtn,     &QPushButton::clicked, this, &MainWindow::onPlay);
    connect(m_downloadBtn, &QPushButton::clicked, this, &MainWindow::onDownload);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &MainWindow::onPlay);

    // YouTube tab
    connect(m_ytSearchBtn,  &QPushButton::clicked, this, &MainWindow::onYouTubeSearch);
    connect(m_ytPlayBtn,    &QPushButton::clicked, this, &MainWindow::onYouTubePlay);
    connect(m_ytSearchEdit, &QLineEdit::returnPressed, this, &MainWindow::onYouTubeSearch);
    connect(m_ytResultsList, &QListWidget::itemDoubleClicked, this, &MainWindow::onYouTubePlay);
    connect(m_ytResultsList, &QListWidget::currentRowChanged, this, [this](int row){
        m_ytPlayBtn->setEnabled(row >= 0 && row < m_ytVideoIds.size());
    });

    // Prev/Next (server playlist)
    connect(m_prevBtn, &QPushButton::clicked, this, [this]{
        int row = m_list->currentRow();
        if (row > 0) { m_list->setCurrentRow(row - 1); onPlay(); }
    });
    connect(m_nextBtn, &QPushButton::clicked, this, [this]{
        int row = m_list->currentRow();
        if (row < m_list->count() - 1) { m_list->setCurrentRow(row + 1); onPlay(); }
    });

    // Play/Pause toggle
    connect(playPauseBtn, &QPushButton::clicked, this, [this, playPauseBtn]{
        if (m_player->playbackState() == QMediaPlayer::PlayingState) {
            m_player->pause();
            playPauseBtn->setText("▶");
        } else {
            m_player->play();
            playPauseBtn->setText("⏸");
        }
    });

    // Seek
    connect(m_seekSlider, &QSlider::sliderPressed,  this, [this]{ m_isSeeking = true; });
    connect(m_seekSlider, &QSlider::sliderReleased, this, [this]{
        m_isSeeking = false;
        onSeek(m_seekSlider->value());
    });
    connect(m_volSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);

    // Player signals
    connect(m_player, &QMediaPlayer::durationChanged, this, &MainWindow::onDurationChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &MainWindow::onPositionChanged);

    // Client signals
    connect(m_client, &Client::playlistReceived, this, &MainWindow::onPlaylistReceived);
    connect(m_client, &Client::songReady,        this, &MainWindow::onSongReady);
    connect(m_client, &Client::downloadDone,     this, &MainWindow::onDownloadDone);
    connect(m_client, &Client::error,            this, &MainWindow::onError);
    connect(m_client, &Client::downloadProgress, this, &MainWindow::onProgress);

    onRefresh();
}

// ─── Tab switching ────────────────────────────────────────────────────────────

void MainWindow::switchTab(int index) {
    m_stack->setCurrentIndex(index);
    m_tabServer->setStyleSheet(index == 0 ? BTN_TAB_ACTIVE : BTN_TAB_INACTIVE);
    m_tabYoutube->setStyleSheet(index == 1 ? BTN_TAB_ACTIVE : BTN_TAB_INACTIVE);
    m_refreshBtn->setVisible(index == 0);
}

// ─── Server tab ───────────────────────────────────────────────────────────────

void MainWindow::onRefresh() {
    m_list->clear();
    m_songs.clear();
    m_playBtn->setEnabled(false);
    m_downloadBtn->setEnabled(false);
    setStatus("Loading...");

    QThread *t = QThread::create([this]{ m_client->requestList(); });
    connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

void MainWindow::onPlaylistReceived(const QStringList &songs) {
    m_songs = songs;
    m_list->clear();
    for (int i = 0; i < songs.size(); i++) {
        QString display = QString("%1.  %2").arg(i + 1, 2).arg(songs[i]);
        m_list->addItem(display);
    }
    if (!songs.isEmpty()) {
        m_list->setCurrentRow(0);
        m_playBtn->setEnabled(true);
        m_downloadBtn->setEnabled(true);
    }
    setStatus(QString("%1 songs").arg(songs.size()));
}

QString MainWindow::currentSongName() {
    int row = m_list->currentRow();
    if (row < 0 || row >= m_songs.size()) return {};
    return m_songs[row];
}

void MainWindow::onPlay() {
    QString song = currentSongName();
    if (song.isEmpty()) return;

    m_playBtn->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    setStatus("Downloading...");

    QThread *t = QThread::create([this, song]{ m_client->playSong(song); });
    connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

void MainWindow::onDownload() {
    QString song = currentSongName();
    if (song.isEmpty()) return;

    QString dir  = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    QString path = QFileDialog::getSaveFileName(this, "Save song", dir + "/" + song, "Audio (*.mp3)");
    if (path.isEmpty()) return;

    m_downloadBtn->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    setStatus("Downloading...");

    QThread *t = QThread::create([this, song, path]{ m_client->downloadSong(song, path); });
    connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

void MainWindow::onSongReady(const QString &path) {
    m_progressBar->setVisible(false);
    m_playBtn->setEnabled(true);
    setStatus("Playing: " + currentSongName());

    m_nowPlaying->setText("▶  " + currentSongName());
    m_player->setSource(QUrl::fromLocalFile(path));
    m_seekSlider->setEnabled(true);
    m_player->play();
}

void MainWindow::onDownloadDone(const QString &path) {
    m_progressBar->setVisible(false);
    m_downloadBtn->setEnabled(true);
    setStatus("Saved: " + path);
}

void MainWindow::onError(const QString &msg) {
    m_progressBar->setVisible(false);
    m_playBtn->setEnabled(true);
    m_downloadBtn->setEnabled(true);
    setStatus(msg, true);
}

void MainWindow::onProgress(qint64 received, qint64 total) {
    if (total > 0)
        m_progressBar->setValue(int(received * 100 / total));
}

// ─── YouTube tab ──────────────────────────────────────────────────────────────
//
// Поиск: yt-dlp --dump-json --flat-playlist "ytsearch5:<query>"
// Воспроизведение: yt-dlp -x --audio-format mp3 -o /tmp/yt_stream.mp3 <videoId>
//
// Требуется: yt-dlp установлен и доступен в PATH
// Установка: pip install yt-dlp  или  apt install yt-dlp

void MainWindow::onYouTubeSearch() {
    QString query = m_ytSearchEdit->text().trimmed();
    if (query.isEmpty()) return;

    m_ytResultsList->clear();
    m_ytVideoIds.clear();
    m_ytPlayBtn->setEnabled(false);
    m_ytStatusLabel->setText("Searching...");

    startYtDlp(query);
}

void MainWindow::startYtDlp(const QString &query) {
    // Запускаем yt-dlp в отдельном потоке, чтобы UI не замерзал
    QThread *t = QThread::create([this, query] {
        QProcess proc;
        proc.start("yt-dlp", {
            "--dump-json",
            "--flat-playlist",
            "--no-warnings",
            "ytsearch5:" + query
        });

        if (!proc.waitForStarted(5000)) {
            QMetaObject::invokeMethod(this, [this]{
                m_ytStatusLabel->setText("yt-dlp not found. Install: pip install yt-dlp");
            }, Qt::QueuedConnection);
            return;
        }

        proc.waitForFinished(30000);
        QByteArray output = proc.readAllStandardOutput();

        // yt-dlp --flat-playlist выдаёт несколько JSON объектов (по одному на строку)
        QStringList ids, titles;
        for (const QByteArray &line : output.split('\n')) {
            if (line.trimmed().isEmpty()) continue;
            QJsonDocument doc = QJsonDocument::fromJson(line);
            if (doc.isNull()) continue;
            QJsonObject obj = doc.object();
            QString id    = obj["id"].toString();
            QString title = obj["title"].toString();
            QString dur   = obj["duration_string"].toString();
            if (!id.isEmpty() && !title.isEmpty()) {
                ids    << id;
                titles << (dur.isEmpty() ? title : title + "  [" + dur + "]");
            }
        }

        QMetaObject::invokeMethod(this, [this, ids, titles]{
            m_ytVideoIds = ids;
            m_ytResultsList->clear();
            if (ids.isEmpty()) {
                m_ytStatusLabel->setText("No results found");
                return;
            }
            for (const QString &t : titles)
                m_ytResultsList->addItem(t);
            m_ytResultsList->setCurrentRow(0);
            m_ytPlayBtn->setEnabled(true);
            m_ytStatusLabel->setText(QString("%1 results").arg(ids.size()));
        }, Qt::QueuedConnection);
    });

    connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

void MainWindow::onYouTubePlay() {
    int row = m_ytResultsList->currentRow();
    if (row < 0 || row >= m_ytVideoIds.size()) return;

    QString videoId = m_ytVideoIds[row];
    QString title   = m_ytResultsList->item(row)->text();

    // Убираем " [duration]" из конца для отображения
    int bracket = title.lastIndexOf("  [");
    if (bracket != -1) title = title.left(bracket);

    m_ytPlayBtn->setEnabled(false);
    m_ytStatusLabel->setText("Downloading audio...");
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    playYtDlp(videoId, title);
}

void MainWindow::playYtDlp(const QString &videoId, const QString &title) {
    QThread *t = QThread::create([this, videoId, title] {
        QString outPath = "/tmp/yt_stream.%(ext)s";
        QString finalPath = "/tmp/yt_stream.mp3";

        // Удаляем старый файл, чтобы не путаться
        QFile::remove(finalPath);

        QProcess proc;
        // -x — только аудио, --audio-format mp3 — конвертируем в mp3
        proc.start("yt-dlp", {
            "-x",
            "--audio-format", "mp3",
            "--audio-quality", "0",
            "--no-playlist",
            "--no-warnings",
            "-o", outPath,
            "https://www.youtube.com/watch?v=" + videoId
        });

        if (!proc.waitForStarted(5000)) {
            QMetaObject::invokeMethod(this, [this]{
                m_ytStatusLabel->setText("yt-dlp not found. Install: pip install yt-dlp");
                m_progressBar->setVisible(false);
                m_ytPlayBtn->setEnabled(true);
            }, Qt::QueuedConnection);
            return;
        }

        // Читаем вывод для прогресс-бара
        while (proc.state() != QProcess::NotRunning) {
            proc.waitForReadyRead(200);
            QString out = proc.readAllStandardOutput() + proc.readAllStandardError();
            // Ищем строки вида "[download]  52.3% ..."
            for (const QString &line : out.split('\n')) {
                if (line.contains("[download]") && line.contains('%')) {
                    int pct = 0;
                    QRegExp rx("(\\d+\\.?\\d*)%");
                    if (rx.indexIn(line) != -1)
                        pct = (int)rx.cap(1).toDouble();
                    QMetaObject::invokeMethod(this, [this, pct]{
                        m_progressBar->setValue(pct);
                    }, Qt::QueuedConnection);
                }
            }
        }

        proc.waitForFinished(60000);

        bool ok = QFile::exists(finalPath);

        QMetaObject::invokeMethod(this, [this, ok, finalPath, title]{
            m_progressBar->setVisible(false);
            m_ytPlayBtn->setEnabled(true);
            if (!ok) {
                m_ytStatusLabel->setText("Download failed (ffmpeg required for conversion)");
                return;
            }
            m_ytStatusLabel->setText("Playing: " + title);
            m_nowPlaying->setText("▶  " + title + "  [YouTube]");
            m_player->setSource(QUrl::fromLocalFile(finalPath));
            m_seekSlider->setEnabled(true);
            m_player->play();
        }, Qt::QueuedConnection);
    });

    connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

// ─── Player controls ─────────────────────────────────────────────────────────

void MainWindow::onDurationChanged(qint64 duration) {
    m_seekSlider->setRange(0, duration > 0 ? (int)duration : 1000);
    m_timeTotal->setText(fmtTime(duration));
}

void MainWindow::onPositionChanged(qint64 position) {
    if (!m_isSeeking) {
        m_seekSlider->setValue((int)position);
        m_timeCurrent->setText(fmtTime(position));
    }
}

void MainWindow::onSeek(int value) {
    m_player->setPosition(value);
}

void MainWindow::onVolumeChanged(int value) {
    m_audio->setVolume(value / 100.0f);
}

void MainWindow::setStatus(const QString &msg, bool isError) {
    m_statusLabel->setStyleSheet(isError
        ? "color:#e24b4b; font-size:12px;"
        : "color:#888; font-size:12px;");
    m_statusLabel->setText(msg);
}

QString MainWindow::getDownloadDir() {
    return QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
}
