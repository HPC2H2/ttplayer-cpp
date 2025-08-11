#include "mainwindow.h"
#include "playlist.h"
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
#include <QKeySequence>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent),
      m_player(nullptr),
      m_currentIndex(0),
      m_shuffleMode(false),
      m_currentPlayingPath(QString()),
      m_dragging(false),
      m_playlistWindow(nullptr)
{
    // Enable drag and drop
    setAcceptDrops(true);
    
    // Initialize player
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(new QAudioOutput(this));
    
    initUI();
}

MainWindow::~MainWindow()
{
    if (m_animation) {
        delete m_animation;
    }
    
    if (m_playlistWindow) {
        delete m_playlistWindow;
    }
}

void MainWindow::initUI()
{
    // Remove title bar
    setWindowFlags(Qt::FramelessWindowHint);
    move(800, 400);
    
    // Start fade-in animation
    startAnimation(0, 1);
    
    // Load background image
    QString backgroundPath = ":/skin/Purple/player_skin.bmp";
    QPixmap pixmap(backgroundPath);
    
    // Round the corners
    pixmap = roundPixmap(pixmap, 8);
    
    // Set window size to match image size
    setFixedSize(pixmap.width(), pixmap.height());
    
    // Set background
    QPalette palette;
    palette.setBrush(QPalette::Window, QBrush(pixmap));
    setPalette(palette);
    setAutoFillBackground(true);
    
    // Create UI elements
    m_musicListBtn = new QPushButton(this);
    m_previewBtn = new QPushButton(this);
    m_playBtn = new QPushButton(this);
    m_nextBtn = new QPushButton(this);
    m_fixedBtn = new QPushButton(this);
    m_miniTopBtn = new QPushButton(this);
    m_minBtn = new QPushButton(this);
    m_closeBtn = new QPushButton(this);
    m_lrcBtn = new QPushButton(this);
    
    // Position UI elements
    m_musicListBtn->setGeometry(20, 145, 31, 13);
    m_previewBtn->setGeometry(80, 136, 35, 35);
    m_playBtn->setGeometry(130, 130, 50, 50);
    m_nextBtn->setGeometry(200, 136, 35, 35);
    m_fixedBtn->setGeometry(220, 7, 17, 15);
    m_miniTopBtn->setGeometry(240, 7, 17, 15);
    m_minBtn->setGeometry(260, 7, 17, 15);
    m_closeBtn->setGeometry(280, 7, 17, 15);
    m_lrcBtn->setGeometry(260, 145, 31, 13);
    
    // Setup button images
    QMap<QPushButton*, QString> buttonImages;
    buttonImages[m_musicListBtn] = ":/skin/Purple/playlist.bmp";
    buttonImages[m_previewBtn] = ":/skin/Purple/prev.bmp";
    buttonImages[m_playBtn] = ":/skin/Purple/play.bmp";
    buttonImages[m_nextBtn] = ":/skin/Purple/next.BMP";
    buttonImages[m_fixedBtn] = ":/skin/Purple/ontop.bmp";
    buttonImages[m_miniTopBtn] = ":/skin/Purple/minimode.bmp";
    buttonImages[m_minBtn] = ":/skin/Purple/minimize.bmp";
    buttonImages[m_closeBtn] = ":/skin/Purple/close.bmp";
    buttonImages[m_lrcBtn] = ":/skin/Purple/lyric.bmp";
    
    // Apply images to buttons
    for (auto it = buttonImages.begin(); it != buttonImages.end(); ++it) {
        QPushButton *button = it.key();
        QString imagePath = it.value();
        
        QList<QPixmap> images = cropImageIntoFourHorizontal(imagePath);
        if (images.size() >= 3) {
            int round = 5;
            QPixmap normalPixmap = roundPixmap(images[0], round);
            QPixmap hoverPixmap = roundPixmap(images[1], round);
            QPixmap pressedPixmap = roundPixmap(images[2], round);
            
            setupHoverPressedIcon(button, normalPixmap, hoverPixmap, pressedPixmap);
        }
    }
    
    // Create custom progress slider
    QPixmap sliderPixmap = cropImageIntoFourHorizontal(":/skin/Purple/progress_thumb.bmp")[0];
    sliderPixmap = roundPixmap(sliderPixmap, 5);
    m_progressSlider = new ImageSlider(sliderPixmap, this);
    m_progressSlider->move(10, 112);
    m_progressSlider->setFixedWidth(290);
    
    // Create custom volume slider
    QPixmap volumePixmap = cropImageIntoFourHorizontal(":/skin/Purple/progress_thumb.bmp")[0];
    volumePixmap = roundPixmap(volumePixmap, 5);
    
    // Scale the volume slider thumb
    int scaledWidth = static_cast<int>(volumePixmap.width() * 1.1);
    int scaledHeight = static_cast<int>(volumePixmap.height() * 1.1);
    volumePixmap = volumePixmap.scaled(scaledWidth, scaledHeight, Qt::KeepAspectRatio);
    
    m_volumeSlider = new ImageSlider(volumePixmap, this);
    m_volumeSlider->move(205, 71);
    m_volumeSlider->setFixedWidth(92);
    m_volumeSlider->setValue(m_volumeSlider->currentVolume());
    
    // Create lyrics label
    m_currentLyricLabel = new FadingLabel("", this);
    m_currentLyricLabel->setStyleSheet(
        "color: #9370DB; font-size: 16px; font-weight: normal; font-family: PingFang SC;"
    );
    m_currentLyricLabel->setMinimumHeight(60);
    m_currentLyricLabel->move(15, 30);
    
    // Create playlist window
    m_playlistWindow = new PlayList(
        geometry().x(),
        geometry().y(),
        geometry().width(),
        geometry().height(),
        this
    );
    
    // Setup shortcuts
    m_spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    m_upShortcut = new QShortcut(QKeySequence(Qt::Key_Up), this);
    m_downShortcut = new QShortcut(QKeySequence(Qt::Key_Down), this);
    
    // Connect signals and slots
    connect(m_closeBtn, &QPushButton::clicked, this, &MainWindow::exitAll);
    connect(m_fixedBtn, &QPushButton::clicked, this, &MainWindow::winFixed);
    connect(m_minBtn, &QPushButton::clicked, this, &MainWindow::minimizeWindow);
    connect(m_musicListBtn, &QPushButton::clicked, this, &MainWindow::showMusicList);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::playAudio);
    connect(m_player, &QMediaPlayer::positionChanged, this, &MainWindow::updateSliderPosition);
    connect(m_player, &QMediaPlayer::durationChanged, this, &MainWindow::setSliderDuration);
    connect(m_progressSlider, &QSlider::sliderPressed, this, &MainWindow::sliderPressed);
    connect(m_progressSlider, &QSlider::sliderReleased, this, &MainWindow::sliderReleased);
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_player && m_player->audioOutput()) {
            m_player->audioOutput()->setVolume(value / 100.0);
        }
    });
    
    connect(m_spaceShortcut, &QShortcut::activated, this, &MainWindow::playAudio);
    connect(m_upShortcut, &QShortcut::activated, this, &MainWindow::increaseVolume);
    connect(m_downShortcut, &QShortcut::activated, this, &MainWindow::decreaseVolume);
}

QPropertyAnimation* MainWindow::startAnimation(float start, float end)
{
    m_animation = new QPropertyAnimation(this, "windowOpacity");
    m_animation->setDuration(1000);  // Animation duration in ms
    m_animation->setStartValue(start);
    m_animation->setEndValue(end);
    m_animation->start();
    return m_animation;
}

void MainWindow::winFixed()
{
    Qt::WindowFlags flags = windowFlags();
    if (flags & Qt::WindowStaysOnTopHint) {
        setWindowFlags(flags & ~Qt::WindowStaysOnTopHint);
        if (m_playlistWindow) {
            m_playlistWindow->setWindowFlags(m_playlistWindow->windowFlags() & ~Qt::WindowStaysOnTopHint);
        }
    } else {
        setWindowFlags(flags | Qt::WindowStaysOnTopHint);
        if (m_playlistWindow) {
            m_playlistWindow->setWindowFlags(m_playlistWindow->windowFlags() | Qt::WindowStaysOnTopHint);
        }
    }
    
    show();  // Must show() again for flags to take effect
    if (m_playlistWindow && m_playlistWindow->isVisible()) {
        m_playlistWindow->show();
    }
}

void MainWindow::minimizeWindow()
{
    showMinimized();
    if (m_playlistWindow) {
        m_playlistWindow->showMinimized();
    }
}

void MainWindow::exitAll()
{
    QPropertyAnimation *anim = startAnimation(1, 0);
    if (m_playlistWindow) {
        m_playlistWindow->startAnimation(1, 0);
    }
    
    connect(anim, &QPropertyAnimation::finished, this, &QWidget::close);
}

void MainWindow::showMusicList()
{
    if (!m_playlistWindow) {
        return;
    }
    
    if (m_playlistWindow->isVisible()) {
        QPropertyAnimation *anim = m_playlistWindow->startAnimation(1, 0);
        connect(anim, &QPropertyAnimation::finished, m_playlistWindow, &QWidget::hide);
    } else {
        m_playlistWindow->startAnimation(0, 1);
        m_playlistWindow->show();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
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

void MainWindow::addPlaylist(const QString &filePath)
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
            out << filePath << "\n";
            qDebug("Added to playlist");
        } else {
            qDebug("Already exists in playlist");
        }
        
        file.close();
        
        if (m_playlistWindow) {
            m_playlistWindow->loadMusicFolder();
            m_playlistWindow->updatePlaylistDisplay();
        }
    }
}

void MainWindow::playAudio()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
    } else {
        m_player->play();
    }
}

QList<QPixmap> MainWindow::cropImageIntoFourHorizontal(const QString &imagePath)
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

QPixmap MainWindow::roundPixmap(const QPixmap &pixmap, int radius)
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

void MainWindow::setupHoverPressedIcon(QPushButton *button, const QPixmap &normalPixmap, 
                                      const QPixmap &hoverPixmap, const QPixmap &pressedPixmap)
{
    // Store pixmaps in button's property
    button->setProperty("normalPixmap", QVariant(normalPixmap));
    button->setProperty("hoverPixmap", QVariant(hoverPixmap));
    button->setProperty("pressedPixmap", QVariant(pressedPixmap));
    
    // Set initial icon
    button->setIcon(QIcon(normalPixmap));
    button->setIconSize(button->size());
    button->setStyleSheet(
        "QPushButton {"
        "    border: none;"
        "    padding: 0px;"
        "    margin: 0px;"
        "    background: transparent;"
        "}"
    );
    
    // Install event filter to handle hover and press events
    class ButtonEventFilter : public QObject
    {
    public:
        ButtonEventFilter(QObject *parent = nullptr) : QObject(parent) {}
        
        bool eventFilter(QObject *watched, QEvent *event) override
        {
            QPushButton *button = qobject_cast<QPushButton*>(watched);
            if (!button) return false;
            
            QPixmap normalPixmap = button->property("normalPixmap").value<QPixmap>();
            QPixmap hoverPixmap = button->property("hoverPixmap").value<QPixmap>();
            QPixmap pressedPixmap = button->property("pressedPixmap").value<QPixmap>();
            
            switch (event->type()) {
                case QEvent::Enter:
                    button->setIcon(QIcon(hoverPixmap));
                    return false;
                    
                case QEvent::Leave:
                    button->setIcon(QIcon(normalPixmap));
                    return false;
                    
                case QEvent::MouseButtonPress:
                    if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
                        button->setIcon(QIcon(pressedPixmap));
                    }
                    return false;
                    
                case QEvent::MouseButtonRelease:
                    if (button->rect().contains(static_cast<QMouseEvent*>(event)->pos())) {
                        button->setIcon(QIcon(hoverPixmap));
                    } else {
                        button->setIcon(QIcon(normalPixmap));
                    }
                    return false;
                    
                default:
                    return false;
            }
        }
    };
    
    button->installEventFilter(new ButtonEventFilter(button));
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_offset = event->globalPosition().toPoint() - window()->pos();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        window()->move(event->globalPosition().toPoint() - m_offset);
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
}

void MainWindow::updateSliderPosition(qint64 position)
{
    if (!m_progressSlider->isSliderDown()) {
        m_progressSlider->setValue(position);
    }
}

void MainWindow::setSliderDuration(qint64 duration)
{
    m_progressSlider->setRange(0, duration);
}

void MainWindow::sliderPressed()
{
    if (m_playlistWindow) {
        m_playlistWindow->findChild<QTimer*>()->stop();  // Stop lyrics update
    }
}

void MainWindow::sliderReleased()
{
    qint64 position = m_progressSlider->value();
    m_player->setPosition(position);
    
    if (m_playlistWindow) {
        QTimer *lyricTimer = m_playlistWindow->findChild<QTimer*>();
        if (lyricTimer) {
            lyricTimer->start(100);  // Resume lyrics update
            m_playlistWindow->updateLyrics();  // Update lyrics immediately
        }
    }
}

void MainWindow::increaseVolume()
{
    int currentVolume = m_volumeSlider->value();
    m_volumeSlider->setValue(qMin(currentVolume + 15, 100));
}

void MainWindow::decreaseVolume()
{
    int currentVolume = m_volumeSlider->value();
    m_volumeSlider->setValue(qMax(currentVolume - 15, 0));
}