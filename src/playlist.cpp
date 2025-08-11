#include "playlist.h"
#include "mainwindow.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QBrush>
#include <QMimeData>
#include <QUrl>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QRect>
#include <QAudioOutput>

PlayList::PlayList(int x, int y, int width, int height, MainWindow *mainWindow)
    : QWidget(nullptr),
      m_x(x),
      m_y(y),
      m_width(width),
      m_height(height),
      m_mainWindow(mainWindow),
      m_dragging(false),
      m_currentLyricIndex(-1)
{
    // Enable drag and drop
    setAcceptDrops(true);
    
    initUI();
}

PlayList::~PlayList()
{
    if (m_lyricTimer) {
        m_lyricTimer->stop();
        delete m_lyricTimer;
    }
    
    if (m_animation) {
        delete m_animation;
    }
}

void PlayList::initUI()
{
    // Remove title bar
    setWindowFlags(Qt::FramelessWindowHint);
    
    // Create UI elements
    m_closeBtn = new QPushButton(this);
    m_songList = new QListWidget(this);
    
    // Set playlist transparent style
    m_songList->setStyleSheet(
        "QListWidget {"
        "    background-color: transparent;"
        "    border: none;"
        "}"
        "QListWidget::item {"
        "    background-color: transparent;"
        "    color: white;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: rgba(100, 100, 100, 100);"
        "}"
    );
    
    // Load background image
    QString backgroundPath = ":/skin/Purple/playlist_skin.bmp";
    QPixmap pixmap(backgroundPath);
    pixmap = roundPixmap(pixmap, 8);
    
    // Set window size to match image size
    setFixedSize(pixmap.width(), pixmap.height());
    
    // Set background
    QPalette palette;
    palette.setBrush(QPalette::Window, QBrush(pixmap));
    setPalette(palette);
    setAutoFillBackground(true);
    
    // Position window relative to main window
    move(m_x, m_y + m_height);
    resize(m_width, height());
    
    // Position UI elements
    m_closeBtn->setGeometry(280, 7, 17, 15);
    m_songList->setGeometry(10, 50, 291, 128);
    
    // Setup close button
    QList<QPixmap> closeImages = cropImageIntoFourHorizontal(":/skin/Purple/close.bmp");
    if (!closeImages.isEmpty()) {
        QPixmap normalPixmap = roundPixmap(closeImages[0], 3);
        m_closeBtn->setIcon(QIcon(normalPixmap));
        m_closeBtn->setIconSize(m_closeBtn->size());
        m_closeBtn->setStyleSheet(
            "QPushButton {"
            "    border: none;"
            "    padding: 0px;"
            "    margin: 0px;"
            "    background: transparent;"
            "}"
        );
    }
    
    // Start fade-in animation
    startAnimation(0, 1);
    
    // Load music playlist
    loadMusicFolder();
    
    // Create lyrics timer
    m_lyricTimer = new QTimer(this);
    connect(m_lyricTimer, &QTimer::timeout, this, &PlayList::updateLyrics);
    m_lyricTimer->start(100);  // Check lyrics every 100ms
    
    // Connect signals and slots
    connect(m_closeBtn, &QPushButton::clicked, this, &PlayList::exitAll);
    connect(m_songList, &QListWidget::itemDoubleClicked, this, &PlayList::selectSong);
}

void PlayList::loadMusicFolder()
{
    try {
        QFile file("play_list.txt");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            in.setCodec("UTF-8");
#endif
            m_playlist.clear();
            
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (!line.isEmpty()) {
                    m_playlist.append(line);
                }
            }
            
            file.close();
            updatePlaylistDisplay();
        }
    } catch (const std::exception &e) {
        qDebug("Failed to load playlist: %s", e.what());
    }
}

void PlayList::updatePlaylistDisplay()
{
    m_songList->clear();
    for (const QString &path : m_playlist) {
        // Extract filename without extension
        QFileInfo fileInfo(path);
        QString nameWithoutExt = fileInfo.baseName();
        
        // Create list item with center alignment
        QListWidgetItem *item = new QListWidgetItem(nameWithoutExt);
        item->setTextAlignment(Qt::AlignCenter);
        
        m_songList->addItem(item);
    }
}

QPropertyAnimation* PlayList::startAnimation(float start, float end)
{
    m_animation = new QPropertyAnimation(this, "windowOpacity");
    m_animation->setDuration(800);  // Animation duration in ms
    m_animation->setStartValue(start);
    m_animation->setEndValue(end);
    m_animation->start();
    return m_animation;
}

void PlayList::exitAll()
{
    QPropertyAnimation *anim = startAnimation(1, 0);
    connect(anim, &QPropertyAnimation::finished, this, &PlayList::hide);
}

void PlayList::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void PlayList::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        for (const QUrl &url : urls) {
            QString filePath = url.toLocalFile();
            if (filePath.toLower().endsWith(".mp3")) {
                qDebug("Dropped file path: %s", qUtf8Printable(filePath));
                addPlaylist(filePath);
            } else {
                qDebug("Ignoring non-MP3 file: %s", qUtf8Printable(filePath));
            }
        }
    } else {
        event->ignore();
    }
}

void PlayList::addPlaylist(const QString &filePath)
{
    QFile file("play_list.txt");
    if (file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        // Check if file already contains this path
        QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        in.setCodec("UTF-8");
#endif
        QString content = in.readAll();
        
        if (!content.contains(filePath)) {
            // Append the new path
            file.seek(file.size());
            QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            out.setCodec("UTF-8");
#endif
            out << filePath << "\n";
            qDebug("Added to playlist");
        } else {
            qDebug("Already exists in playlist");
        }
        
        file.close();
        loadMusicFolder();
        updatePlaylistDisplay();
    }
}

QPixmap PlayList::roundPixmap(const QPixmap &pixmap, int radius)
{
    QPixmap rounded(pixmap.size());
    rounded.fill(Qt::transparent);
    
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    QPainterPath path;
    QRect rect = pixmap.rect();
    path.addRoundedRect(rect.x(), rect.y(), rect.width(), rect.height(), radius, radius);
    painter.setClipPath(path);
    
    painter.drawPixmap(0, 0, pixmap);
    painter.end();
    
    return rounded;
}

QList<QPixmap> PlayList::cropImageIntoFourHorizontal(const QString &imagePath)
{
    QList<QPixmap> croppedImages;
    
    // Load original image
    QPixmap originalPixmap(imagePath);
    if (originalPixmap.isNull()) {
        qDebug("Failed to load image, check path");
        return croppedImages;
    }
    
    // Get original image dimensions
    int width = originalPixmap.width();
    int height = originalPixmap.height();
    
    // Calculate width of each part
    int partWidth = width / 4;
    
    // Crop image into four parts
    for (int i = 0; i < 4; ++i) {
        QRect rect(i * partWidth, 0, partWidth, height);
        QPixmap cropped = originalPixmap.copy(rect);
        croppedImages.append(cropped);
    }
    
    return croppedImages;
}

void PlayList::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_offset = event->globalPosition().toPoint() - window()->pos();
    }
}

void PlayList::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        window()->move(event->globalPosition().toPoint() - m_offset);
    }
}

void PlayList::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
}

void PlayList::selectSong(QListWidgetItem *item)
{
    int index = m_songList->row(item);
    if (index >= 0 && index < m_playlist.size() && m_mainWindow) {
        QMediaPlayer *player = m_mainWindow->findChild<QMediaPlayer*>();
        if (player) {
            player->setSource(QUrl::fromLocalFile(m_playlist[index]));
            
            // Set volume
            ImageSlider *volumeSlider = m_mainWindow->findChild<ImageSlider*>("m_volumeSlider");
            if (volumeSlider) {
                int volume = qMin(volumeSlider->currentVolume(), 100);
                QAudioOutput *audioOutput = player->audioOutput();
                if (audioOutput) {
                    audioOutput->setVolume(volume / 100.0);
                }
            }
            
            player->play();
            loadLyrics(m_playlist[index], m_mainWindow);
        }
    }
}

void PlayList::loadLyrics(const QString &audioPath, MainWindow *mainWindow)
{
    QFileInfo fileInfo(audioPath);
    QString basePath = fileInfo.absolutePath() + "/" + fileInfo.baseName();
    QString lrcPath = basePath + ".lrc";
    
    m_lyrics.clear();
    m_currentLyricIndex = -1;
    
    QFile file(lrcPath);
    if (!file.exists()) {
        FadingLabel *lyricLabel = mainWindow->findChild<FadingLabel*>();
        if (lyricLabel) {
            lyricLabel->setText("No lyrics found");
        }
        return;
    }
    
    // Try multiple encodings to read lyrics file
    QStringList encodings = {"UTF-8", "GBK", "GB2312", "Latin1"};
    QStringList lines;
    
    for (const QString &encoding : encodings) {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            in.setCodec(qPrintable(encoding));
#endif
            
            try {
                lines = in.readAll().split('\n');
                file.close();
                break;
            } catch (...) {
                file.close();
                continue;
            }
        }
    }
    
    if (lines.isEmpty()) {
        return;
    }
    
    // Parse lyrics
    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) {
            continue;
        }
        
        // Handle standard lyrics format [mm:ss.xx]lyrics
        if (trimmedLine.startsWith('[') && trimmedLine.contains(']')) {
            QStringList parts = trimmedLine.split(']');
            QString text = parts.mid(1).join(']').trimmed();
            
            // Handle multiple time tags
            for (int i = 0; i < parts.size() - 1; ++i) {
                QString timePart = parts[i].mid(1);  // Remove '['
                
                try {
                    // Parse time
                    qint64 totalMs = 0;
                    if (timePart.contains(':') && timePart.contains('.')) {
                        // Format: mm:ss.xx
                        QStringList timeComponents = timePart.split(':');
                        int minutes = timeComponents[0].toInt();
                        QStringList secondsComponents = timeComponents[1].split('.');
                        int seconds = secondsComponents[0].toInt();
                        int millis = secondsComponents[1].toInt() * 10;
                        totalMs = minutes * 60 * 1000 + seconds * 1000 + millis;
                    } else if (timePart.contains(':')) {
                        // Format: mm:ss
                        QStringList timeComponents = timePart.split(':');
                        int minutes = timeComponents[0].toInt();
                        int seconds = timeComponents[1].toInt();
                        totalMs = minutes * 60 * 1000 + seconds * 1000;
                    } else {
                        continue;
                    }
                    
                    m_lyrics.append(qMakePair(totalMs, text));
                } catch (...) {
                    continue;
                }
            }
        }
    }
    
    // Sort lyrics by time
    std::sort(m_lyrics.begin(), m_lyrics.end(), 
              [](const QPair<qint64, QString> &a, const QPair<qint64, QString> &b) {
                  return a.first < b.first;
              });
    
    m_currentLyricIndex = -1;  // Reset current lyrics index
}

void PlayList::updateLyrics()
{
    if (m_lyrics.isEmpty() || !m_mainWindow) {
        return;
    }
    
    QMediaPlayer *player = m_mainWindow->findChild<QMediaPlayer*>();
    if (!player) {
        return;
    }
    
    qint64 currentTime = player->position();
    
    // Find current lyrics line
    int newIndex = -1;
    for (int i = 0; i < m_lyrics.size(); ++i) {
        if (m_lyrics[i].first <= currentTime) {
            if (i < m_lyrics.size() - 1 && m_lyrics[i + 1].first > currentTime) {
                newIndex = i;
                break;
            }
        } else {
            break;
        }
    }
    
    // If it's the last line of lyrics
    if (newIndex == -1 && !m_lyrics.isEmpty() && m_lyrics.last().first <= currentTime) {
        newIndex = m_lyrics.size() - 1;
    }
    
    // If lyrics line changed, update display
    if (newIndex != m_currentLyricIndex) {
        m_currentLyricIndex = newIndex;
        
        // Update top large font lyrics
        FadingLabel *lyricLabel = m_mainWindow->findChild<FadingLabel*>();
        if (lyricLabel) {
            if (0 <= newIndex && newIndex < m_lyrics.size()) {
                lyricLabel->setText(m_lyrics[newIndex].second);
                lyricLabel->adjustSize();
                lyricLabel->fadeIn();
            } else {
                lyricLabel->setText("");
                lyricLabel->adjustSize();
            }
        }
    }
}