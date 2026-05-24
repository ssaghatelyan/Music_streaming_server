#include "MainWindow.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStandardPaths>
#include <QProcess>
#include <QUrl>

MainWindow::MainWindow(Client *client, QWidget *parent)
    : QMainWindow(parent), m_client(client)
{
    setWindowTitle("MusicStream");

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *layout = new QVBoxLayout(central);

    // LIST
    m_list = new QListWidget(this);
    layout->addWidget(m_list);

    // YouTube input
    youtubeInput = new QLineEdit(this);
    youtubeInput->setPlaceholderText("YouTube search...");
    layout->addWidget(youtubeInput);

    // BUTTONS
    auto *btnRow = new QHBoxLayout();

    auto *refreshBtn = new QPushButton("Refresh");
    auto *playBtn    = new QPushButton("Play");
    auto *downBtn    = new QPushButton("Download");
    auto *ytBtn      = new QPushButton("YouTube Play");

    btnRow->addWidget(refreshBtn);
    btnRow->addWidget(playBtn);
    btnRow->addWidget(downBtn);
    btnRow->addWidget(ytBtn);

    layout->addLayout(btnRow);

    // ===== CONNECT UI =====

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refresh);
    connect(playBtn,    &QPushButton::clicked, this, &MainWindow::play);
    connect(downBtn,    &QPushButton::clicked, this, &MainWindow::download);

    connect(ytBtn, &QPushButton::clicked, this, &MainWindow::youtube);

    // ===== CLIENT SIGNALS =====

    connect(m_client, &Client::playlist,
            this, [&](QStringList l){
                m_list->clear();
                m_list->addItems(l);
            });

    connect(m_client, &Client::songReady,
            this, [&](QString path){
                // обычное проигрывание (mp3 из сервера или yt-dlp)
                QProcess::startDetached("mpg123", { path });
            });

    connect(m_client, &Client::error,
            this, [&](QString msg){
                m_list->addItem("ERROR: " + msg);
            });

    refresh();
}

// ---------------- SERVER ----------------

void MainWindow::refresh() {
    m_client->list();
}

void MainWindow::play() {
    auto item = m_list->currentItem();
    if (!item) return;

    m_client->play(item->text());
}

void MainWindow::download() {
    auto item = m_list->currentItem();
    if (!item) return;

    m_client->download(item->text());
}

// ---------------- YOUTUBE ----------------

void MainWindow::youtube() {
    QString query = youtubeInput->text().trimmed();
    if (query.isEmpty()) return;

    // локальный файл
    QString out = "/tmp/yt_audio.mp3";

    QProcess *proc = new QProcess(this);

    QStringList args = {
        "-x",
        "--audio-format", "mp3",
        "-o", out,
        "ytsearch1:" + query
    };

    proc->start("yt-dlp", args);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int code, QProcess::ExitStatus){

        if (code == 0) {
            emit m_client->songReady(out);
        } else {
            emit m_client->error("YouTube download failed");
        }

        proc->deleteLater();
    });
}