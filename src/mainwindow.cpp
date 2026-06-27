// Modified: 2025-08-13
// Version: 2.0 - 集成 SkinEngine，支持拖放 .skn 换肤
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
#ifdef QT_MULTIMEDIA_ENABLED
#include <QAudioOutput>
#endif
#include <QKeySequence>
#include <QDebug>

// ============================================================
// 构造 / 析构
// ============================================================

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent),
      // 按钮指针全部初始化为 nullptr（配合 applyDefaultSkinUI 的幂等保护）
      m_musicListBtn(nullptr),
      m_previewBtn(nullptr),
      m_playBtn(nullptr),
      m_nextBtn(nullptr),
      m_fixedBtn(nullptr),
      m_miniTopBtn(nullptr),
      m_minBtn(nullptr),
      m_closeBtn(nullptr),
      m_lrcBtn(nullptr),
      // 滑块 / 标签 / 频谱
      m_progressSlider(nullptr),
      m_volumeSlider(nullptr),
      m_currentLyricLabel(nullptr),
      m_spectrumBars(nullptr),
#ifdef QT_MULTIMEDIA_ENABLED
      m_player(nullptr),
#else
      m_audioPlayer(nullptr),
#endif
      m_currentIndex(0),
      m_shuffleMode(false),
      m_currentPlayingPath(QString()),
      m_dragging(false),
      m_playlistWindow(nullptr),
      // 快捷键 / 动画
      m_spaceShortcut(nullptr),
      m_upShortcut(nullptr),
      m_downShortcut(nullptr),
      m_animation(nullptr),
      m_usingExternalSkin(false)
{
    // Enable drag and drop
    setAcceptDrops(true);

#ifdef QT_MULTIMEDIA_ENABLED
    // Initialize player (Qt Multimedia)
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(new QAudioOutput(this));

    // 连接播放器诊断信号
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString &errorString) {
        qCritical() << "[Player] 播放器错误:" << error << "-" << errorString
                    << "source:" << m_player->source();
        if (m_currentLyricLabel) {
            m_currentLyricLabel->setText(QString("播放错误: %1").arg(errorString));
            m_currentLyricLabel->fadeIn();
        }
    });
    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState state) {
        qDebug() << "[Player] playbackStateChanged:" << state;
        // 仅在正常播放完毕后切换下一首，避免播放失败时循环调用
        if (state == QMediaPlayer::StoppedState
            && m_playlistWindow
            && !m_player->source().isEmpty()) {
            QMetaObject::invokeMethod(m_playlistWindow, "nextSong", Qt::DirectConnection);
        }
    });
#else
    // Initialize player (Windows native waveOut)
    m_audioPlayer = new AudioPlayer(this);
    connect(m_audioPlayer, &AudioPlayer::finished, this, [this]() {
        qDebug() << "[Player] 播放完成";
        if (m_playlistWindow) {
            QMetaObject::invokeMethod(m_playlistWindow, "nextSong", Qt::DirectConnection);
        }
    });
    connect(m_audioPlayer, &AudioPlayer::positionChanged, this, &MainWindow::updateSliderPosition);
    connect(m_audioPlayer, &AudioPlayer::durationChanged, this, &MainWindow::setSliderDuration);
#endif

    // 监听皮肤引擎变化信号
    connect(&SkinEngine::instance(), &SkinEngine::skinChanged,
            this, &MainWindow::onSkinChanged);

    initUI();
}

MainWindow::~MainWindow()
{
}

// ============================================================
// UI 初始化
// ============================================================

void MainWindow::initUI()
{
    // Remove title bar
    setWindowFlags(Qt::FramelessWindowHint);
    move(800, 400);

    // Start fade-in animation
    startAnimation(0, 1);

    // 尝试从皮肤引擎加载（如果已加载了外部皮肤）
    if (SkinEngine::instance().hasValidSkin() && !SkinEngine::instance().isDefaultSkin()) {
        applySkin();
        return;
    }

    // 回退到默认 Purple 皮肤
    applyDefaultSkinUI();
}

void MainWindow::applyDefaultSkinUI()
{
    m_usingExternalSkin = false;

    // ---- 背景图 ----
    QString backgroundPath = ":/skin/Purple/player_skin.bmp";
    QPixmap pixmap(backgroundPath);

    pixmap = roundPixmap(pixmap, 8);
    setFixedSize(pixmap.width(), pixmap.height());

    QPalette palette;
    palette.setBrush(QPalette::Window, QBrush(pixmap));
    setPalette(palette);
    setAutoFillBackground(true);

    // ---- 创建按钮（幂等保护：避免重复创建）----
    if (!m_musicListBtn) m_musicListBtn = new QPushButton(this);
    if (!m_previewBtn) m_previewBtn = new QPushButton(this);   // prev
    if (!m_playBtn) m_playBtn = new QPushButton(this);
    if (!m_nextBtn) m_nextBtn = new QPushButton(this);
    if (!m_fixedBtn) m_fixedBtn = new QPushButton(this);     // ontop
    if (!m_miniTopBtn) m_miniTopBtn = new QPushButton(this);   // minimode
    if (!m_minBtn) m_minBtn = new QPushButton(this);           // minimize
    if (!m_closeBtn) m_closeBtn = new QPushButton(this);       // close
    if (!m_lrcBtn) m_lrcBtn = new QPushButton(this);           // lyric

    // 布局位置（Purple 默认皮肤）
    m_musicListBtn->setGeometry(20, 145, 31, 13);
    m_previewBtn->setGeometry(80, 136, 35, 35);
    m_playBtn->setGeometry(130, 130, 50, 50);
    m_nextBtn->setGeometry(200, 136, 35, 35);
    m_fixedBtn->hide();  // 原版千千静听无此置顶按钮
    m_miniTopBtn->setGeometry(245, 7, 17, 15);
    m_minBtn->setGeometry(268, 7, 17, 15);
    m_closeBtn->setGeometry(290, 7, 17, 15);     // 贴近右边缘
    m_lrcBtn->setGeometry(260, 145, 31, 13);

    // 按钮图片映射（默认 Purple 皮肤）
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

    for (auto it = buttonImages.begin(); it != buttonImages.end(); ++it) {
        QList<QPixmap> images = cropImageIntoFourHorizontal(it.value());
        if (images.size() >= 3) {
            int round = 5;
            setupHoverPressedIcon(it.key(),
                roundPixmap(images[0], round),
                roundPixmap(images[1], round),
                roundPixmap(images[2], round));
        }
    }

    // ---- 进度条（幂等保护）----
    if (!m_progressSlider) {
        QPixmap sliderPixmap = cropImageIntoFourHorizontal(":/skin/Purple/progress_thumb.bmp")[0];
        sliderPixmap = roundPixmap(sliderPixmap, 5);
        m_progressSlider = new ImageSlider(sliderPixmap, this);
    }
    m_progressSlider->move(10, 112);
    m_progressSlider->setFixedWidth(290);

    // ---- 音量滑块（幂等保护）----
    if (!m_volumeSlider) {
        QPixmap volumePixmap = cropImageIntoFourHorizontal(":/skin/Purple/progress_thumb.bmp")[0];
        volumePixmap = roundPixmap(volumePixmap, 5);
        int scaledW = static_cast<int>(volumePixmap.width() * 1.1);
        int scaledH = static_cast<int>(volumePixmap.height() * 1.1);
        volumePixmap = volumePixmap.scaled(scaledW, scaledH, Qt::KeepAspectRatio);
        m_volumeSlider = new ImageSlider(volumePixmap, this);
    }
    m_volumeSlider->move(205, 71);
    m_volumeSlider->setFixedWidth(92);
    m_volumeSlider->setValue(m_volumeSlider->currentVolume());

    // ---- 歌词标签（幂等保护）----
    if (!m_currentLyricLabel) {
        m_currentLyricLabel = new FadingLabel("", this);
    }
    m_currentLyricLabel->setStyleSheet(
        "color: #9370DB; font-size: 14px; font-weight: normal; "
        "font-family: 'PingFang SC', 'Microsoft YaHei', sans-serif; "
        "text-overflow: ellipsis; word-wrap: break-word;"
    );
    m_currentLyricLabel->setMinimumHeight(60);
    m_currentLyricLabel->setFixedWidth(280);
    m_currentLyricLabel->setAlignment(Qt::AlignCenter);
    m_currentLyricLabel->move(15, 30);

    // ---- 频谱柱状图（幂等保护）----
    if (!m_spectrumBars) {
        m_spectrumBars = new SpectrumBars(this);
    }
    m_spectrumBars->setGeometry(15, 70, 280, 40);
    m_spectrumBars->setStyleSheet("background-color: transparent;");
#ifdef QT_MULTIMEDIA_ENABLED
    m_spectrumBars->setMediaPlayer(m_player);
#endif
    m_spectrumBars->setColors(
        QColor("#8CEFFD"), QColor("#71CDFD"),
        QColor("#4C5FD1"), QColor("#FF71CD")
    );
    m_spectrumBars->raise();
    m_spectrumBars->show();

    // ---- 播放列表窗口（幂等保护）----
    if (!m_playlistWindow) {
        m_playlistWindow = new PlayList(
            geometry().x(), geometry().y(),
            geometry().width(), geometry().height(), this
        );
    }

    // ---- 快捷键 ----
    m_spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    m_upShortcut = new QShortcut(QKeySequence(Qt::Key_Up), this);
    m_downShortcut = new QShortcut(QKeySequence(Qt::Key_Down), this);

    // ---- 连接信号槽 ----
    connect(m_closeBtn, &QPushButton::clicked, this, &MainWindow::exitAll);
    connect(m_fixedBtn, &QPushButton::clicked, this, &MainWindow::winFixed);
    connect(m_minBtn, &QPushButton::clicked, this, &MainWindow::minimizeWindow);
    connect(m_musicListBtn, &QPushButton::clicked, this, &MainWindow::showMusicList);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::playAudio);
    connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
        if (m_playlistWindow) QMetaObject::invokeMethod(m_playlistWindow, "nextSong", Qt::DirectConnection);
    });
    connect(m_previewBtn, &QPushButton::clicked, this, [this]() {
        if (m_playlistWindow) QMetaObject::invokeMethod(m_playlistWindow, "previousSong", Qt::DirectConnection);
    });
    connect(m_lrcBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentLyricLabel) m_currentLyricLabel->setVisible(!m_currentLyricLabel->isVisible());
    });
#ifdef QT_MULTIMEDIA_ENABLED
    connect(m_player, &QMediaPlayer::positionChanged, this, &MainWindow::updateSliderPosition);
    connect(m_player, &QMediaPlayer::durationChanged, this, &MainWindow::setSliderDuration);
#endif
    connect(m_progressSlider, &QSlider::sliderPressed, this, &MainWindow::sliderPressed);
    connect(m_progressSlider, &QSlider::sliderReleased, this, &MainWindow::sliderReleased);
    connect(m_progressSlider, &QSlider::valueChanged, this, [this]() {
        if (m_progressSlider->isSliderDown() && m_spectrumBars)
            m_spectrumBars->updateForPosition(m_progressSlider->value());
    });
#ifdef QT_MULTIMEDIA_ENABLED
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_player && m_player->audioOutput())
            m_player->audioOutput()->setVolume(static_cast<float>(value) / 100.0f);
    });
#else
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_audioPlayer) m_audioPlayer->setVolume(value);
    });
#endif

    connect(m_spaceShortcut, &QShortcut::activated, this, &MainWindow::playAudio);
    connect(m_upShortcut, &QShortcut::activated, this, &MainWindow::increaseVolume);
    connect(m_downShortcut, &QShortcut::activated, this, &MainWindow::decreaseVolume);
}

// ============================================================
// 皮肤应用 - 从 SkinEngine 配置动态构建 UI
// ============================================================

void MainWindow::applySkin()
{
    SkinEngine& engine = SkinEngine::instance();

    if (!engine.hasValidSkin()) {
        applyDefaultSkinUI();
        return;
    }

    m_usingExternalSkin = true;
    rebuildFromSkinConfig();
}

void MainWindow::rebuildFromSkinConfig()
{
    SkinEngine& engine = SkinEngine::instance();

    // ---- 背景 ----
    QPixmap bgImage = engine.getWindowBackground("player_window");
    if (bgImage.isNull()) {
        qWarning() << "[MainWindow] 无法加载主界面背景图，回退到默认皮肤";
        applyDefaultSkinUI();
        return;
    }

    bgImage = roundPixmap(bgImage, 8);
    setFixedSize(bgImage.width(), bgImage.height());

    QPalette palette;
    palette.setBrush(QPalette::Window, QBrush(bgImage));
    setPalette(palette);
    setAutoFillBackground(true);

    // ---- 查找按钮元素映射 ----
    // 将 Skin.xml 中定义的元素映射到我们的内部按钮
    struct ButtonMapping {
        QPushButton* btnPtr;       // 直接指向成员变量
        const char* elemName;     // Skin.xml 中的元素名
        const char* fallbackName;  // 备选名
        const char* defaultRes;    // 默认资源路径（Purple 皮肤）
    };

    ButtonMapping mappings[] = {
        {m_musicListBtn, "playlist", nullptr, "playlist.bmp"},
        {m_previewBtn,   "prev",     nullptr, "prev.bmp"},
        {m_playBtn,      "play",     nullptr, "play.bmp"},
        {m_nextBtn,       "next",     nullptr, "next.BMP"},
        {m_fixedBtn,     "ontop",    nullptr, "ontop.bmp"},   // XML中可能叫其他名字
        {m_miniTopBtn,   "open",     nullptr, "minimode.bmp"}, // 千千静听用 open 表示打开文件
        {m_minBtn,       "minimize", nullptr, "minimize.bmp"},
        {m_closeBtn,     "exit",     nullptr, "close.bmp"},
        {m_lrcBtn,       "lyric",    nullptr, "lyric.bmp"},
        {nullptr, nullptr, nullptr, nullptr}
    };

    // 创建或复用按钮
    // 使用数组索引来访问成员变量（因为 btnPtr 现在是值拷贝）
    QPushButton** btnPtrs[] = {
        &m_musicListBtn, &m_previewBtn, &m_playBtn, &m_nextBtn,
        &m_fixedBtn, &m_miniTopBtn, &m_minBtn, &m_closeBtn, &m_lrcBtn
    };
    for (int i = 0; i < 9 && mappings[i].elemName; ++i) {
        QPushButton* btn;

        // 如果按钮不存在则创建
        if (!*(btnPtrs[i])) {
            btn = new QPushButton(this);
            *(btnPtrs[i]) = btn;
        } else {
            btn = *(btnPtrs[i]);
        }

        // 从 Skin.xml 获取图片文件名
        QString imageName;
        QRect elemRect;

        const SkinElement* elem = engine.getConfig().findElement("player_window", mappings[i].elemName);
        if (elem && !elem->image.isEmpty()) {
            imageName = elem->image;
            elemRect = elem->position;
        } else if (mappings[i].fallbackName) {
            elem = engine.getConfig().findElement("player_window", mappings[i].fallbackName);
            if (elem && !elem->image.isEmpty()) {
                imageName = elem->image;
                elemRect = elem->position;
            }
        }

        // 设置位置（优先使用 Skin.xml 的坐标）
        if (!elemRect.isEmpty()) {
            btn->setGeometry(elemRect);
        } else if (i == 0) {
            // 第一个按钮的默认位置映射表
            static QMap<QString, QRect> fallbackPositions = {
                {"playlist",  {20, 145, 31, 13}},
                {"prev",      {80, 136, 35, 35}},
                {"play",      {130, 130, 50, 50}},
                {"next",      {200, 136, 35, 35}},
                {"ontop",     {220, 7, 17, 15}},
                {"open",      {130, 3, 19, 19}},
                {"minimize",  {229, 6, 15, 15}},
                {"exit",      {245, 6, 15, 15}},
                {"lyric",     {158, 3, 19, 19}}
            };
            auto it = fallbackPositions.find(mappings[i].elemName);
            if (it != fallbackPositions.end())
                btn->setGeometry(it.value());
        }

        // 加载并设置按钮图片
        if (!imageName.isEmpty()) {
            QList<QPixmap> images = loadButtonImages(imageName);
            if (images.size() >= 3) {
                int round = 5;
                setupHoverPressedIcon(btn,
                    roundPixmap(images[0], round),
                    roundPixmap(images[1], round),
                    roundPixmap(images[2], round));
            }
        }
    }

    // ---- 进度条 ----
    const SkinElement* progressElem = engine.getConfig().findElement("player_window", "progress");
    QString thumbImg = progressElem ? progressElem->thumbImage : "";
    if (thumbImg.isEmpty())
        thumbImg = "progress_thumb.bmp"; // 回退

    QPixmap sliderPixmap = engine.getImage(thumbImg);
    if (sliderPixmap.isNull())
        sliderPixmap.load(":/skin/Purple/progress_thumb.bmp");

    // 四宫格裁剪取第一个状态
    if (sliderPixmap.width() >= 4 * sliderPixmap.height()) {
        int partW = sliderPixmap.width() / 4;
        sliderPixmap = sliderPixmap.copy(0, 0, partW, sliderPixmap.height());
    }

    sliderPixmap = roundPixmap(sliderPixmap, 5);

    if (!m_progressSlider) {
        m_progressSlider = new ImageSlider(sliderPixmap, this);
    } else {
        // 更新已有滑块的图片
        m_progressSlider->setThumbImage(sliderPixmap);
    }
    m_progressSlider->move(progressElem ? progressElem->position.x() : 10,
                           progressElem ? progressElem->position.y() : 112);
    m_progressSlider->setFixedWidth(progressElem ? progressElem->position.width() : 290);

    // ---- 音量滑块 ----
    const SkinElement* volElem = engine.getConfig().findElement("player_window", "volume");
    QString volThumbImg = (volElem && !volElem->thumbImage.isEmpty()) ? volElem->thumbImage : "volume_thumb.bmp";

    QPixmap volPixmap = engine.getImage(volThumbImg);
    if (volPixmap.isNull()) {
        volPixmap = engine.getImage(thumbImg); // 回退到进度条的 thumb
        if (volPixmap.isNull())
            volPixmap.load(":/skin/Purple/progress_thumb.bmp");
    }
    if (volPixmap.width() >= 4 * volPixmap.height()) {
        int partW = volPixmap.width() / 4;
        volPixmap = volPixmap.copy(0, 0, partW, volPixmap.height());
    }

    int vScaleW = static_cast<int>(volPixmap.width() * 1.1);
    int vScaleH = static_cast<int>(volPixmap.height() * 1.1);
    volPixmap = volPixmap.scaled(vScaleW, vScaleH, Qt::KeepAspectRatio);

    if (!m_volumeSlider) {
        m_volumeSlider = new ImageSlider(volPixmap, this);
    } else {
        m_volumeSlider->setThumbImage(volPixmap);
    }
    m_volumeSlider->move(volElem ? volElem->position.x() : 205,
                         volElem ? volElem->position.y() : 71);
    m_volumeSlider->setFixedWidth(volElem ? volElem->position.width() : 92);
    m_volumeSlider->setValue(m_volumeSlider->currentVolume());

    // ---- 歌词标签 ----
    if (!m_currentLyricLabel) {
        m_currentLyricLabel = new FadingLabel("", this);
    }
    m_currentLyricLabel->setStyleSheet(QString(
        "color: %1; font-size: %2px; font-weight: normal; font-family: '%3', 'Microsoft YaHei', sans-serif; "
        "text-overflow: ellipsis; word-wrap: break-word;"
    ).arg(engine.getLyricTextColor().name())
     .arg(qAbs(engine.getLyricFont().pixelSize()))
     .arg(engine.getLyricFont().family()));

    const SkinElement* infoElem = engine.getConfig().findElement("player_window", "info");
    if (infoElem && !infoElem->position.isEmpty()) {
        m_currentLyricLabel->setGeometry(infoElem->position);
    } else {
        m_currentLyricLabel->setMinimumHeight(60);
        m_currentLyricLabel->setFixedWidth(280);
        m_currentLyricLabel->setAlignment(Qt::AlignCenter);
        m_currentLyricLabel->move(15, 30);
    }

    // ---- 频谱柱状图 ----
    if (!m_spectrumBars) {
        m_spectrumBars = new SpectrumBars(this);
    }
    const SkinElement* visualElem = engine.getConfig().findElement("player_window", "visual");
    if (visualElem && !visualElem->position.isEmpty()) {
        m_spectrumBars->setGeometry(visualElem->position);
    } else {
        m_spectrumBars->setGeometry(15, 70, 280, 40);
    }
    m_spectrumBars->setStyleSheet("background-color: transparent;");
    m_spectrumBars->setColors(
        engine.getSpectrumTopColor(),
        engine.getSpectrumBottomColor(),
        engine.getSpectrumMidColor(),
        engine.getSpectrumPeakColor()
    );

    // 如果播放器还没连接，重新连接
#ifdef QT_MULTIMEDIA_ENABLED
    if (m_player)
        m_spectrumBars->setMediaPlayer(m_player);
#endif

    m_spectrumBars->raise();
    m_spectrumBars->show();

    // ---- 播放列表窗口 ----
    if (!m_playlistWindow) {
        m_playlistWindow = new PlayList(
            geometry().x(), geometry().y(),
            geometry().width(), geometry().height(), this
        );
    }

    // ---- 快捷键（仅创建一次）----
    if (!m_spaceShortcut) {
        m_spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
        m_upShortcut = new QShortcut(QKeySequence(Qt::Key_Up), this);
        m_downShortcut = new QShortcut(QKeySequence(Qt::Key_Down), this);
    }

    // ---- 重连所有信号（防止重复连接）----
    disconnect(m_closeBtn, nullptr, this, nullptr);
    disconnect(m_fixedBtn, nullptr, this, nullptr);
    disconnect(m_minBtn, nullptr, this, nullptr);
    disconnect(m_musicListBtn, nullptr, this, nullptr);
    disconnect(m_playBtn, nullptr, this, nullptr);
    disconnect(m_nextBtn, nullptr, this, nullptr);
    disconnect(m_previewBtn, nullptr, this, nullptr);
    disconnect(m_lrcBtn, nullptr, this, nullptr);

    connect(m_closeBtn, &QPushButton::clicked, this, &MainWindow::exitAll);
    connect(m_fixedBtn, &QPushButton::clicked, this, &MainWindow::winFixed);
    connect(m_minBtn, &QPushButton::clicked, this, &MainWindow::minimizeWindow);
    connect(m_musicListBtn, &QPushButton::clicked, this, &MainWindow::showMusicList);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::playAudio);
    connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
        if (m_playlistWindow) QMetaObject::invokeMethod(m_playlistWindow, "nextSong", Qt::DirectConnection);
    });
    connect(m_previewBtn, &QPushButton::clicked, this, [this]() {
        if (m_playlistWindow) QMetaObject::invokeMethod(m_playlistWindow, "previousSong", Qt::DirectConnection);
    });
    connect(m_lrcBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentLyricLabel)
            m_currentLyricLabel->setVisible(!m_currentLyricLabel->isVisible());
    });

    qDebug() << "[MainWindow] 皮肤应用成功:" << engine.skinName()
             << "- 窗口尺寸:" << width() << "x" << height();
}

// ============================================================
// 辅助方法
// ============================================================

QList<QPixmap> MainWindow::loadButtonImages(const QString& filename)
{
    // 先尝试从当前皮肤目录加载
    if (SkinEngine::instance().hasValidSkin() && !SkinEngine::instance().isDefaultSkin()) {
        return SkinEngine::instance().cropFourStates(filename);
    }
    // 回退到 Qt 资源系统
    QString resPath = ":/skin/Purple/" + filename;
    return cropImageIntoFourHorizontal(resPath);
}

QPropertyAnimation* MainWindow::startAnimation(float start, float end)
{
    m_animation = new QPropertyAnimation(this, "windowOpacity");
    m_animation->setDuration(1000);
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
        if (m_playlistWindow)
            m_playlistWindow->setWindowFlags(m_playlistWindow->windowFlags() & ~Qt::WindowStaysOnTopHint);
    } else {
        setWindowFlags(flags | Qt::WindowStaysOnTopHint);
        if (m_playlistWindow)
            m_playlistWindow->setWindowFlags(m_playlistWindow->windowFlags() | Qt::WindowStaysOnTopHint);
    }
    show();
    if (m_playlistWindow && m_playlistWindow->isVisible())
        m_playlistWindow->show();
}

void MainWindow::minimizeWindow()
{
    showMinimized();
    if (m_playlistWindow) m_playlistWindow->showMinimized();
}

void MainWindow::exitAll()
{
    QPropertyAnimation *anim = startAnimation(1, 0);
    if (m_playlistWindow) m_playlistWindow->startAnimation(1, 0);
    connect(anim, &QPropertyAnimation::finished, this, &QWidget::close);
}

void MainWindow::showMusicList()
{
    if (!m_playlistWindow) return;
    if (m_playlistWindow->isVisible()) {
        QPropertyAnimation *anim = m_playlistWindow->startAnimation(1, 0);
        connect(anim, &QPropertyAnimation::finished, m_playlistWindow, &QWidget::hide);
    } else {
        m_playlistWindow->startAnimation(0, 1);
        m_playlistWindow->show();
    }
}

// ============================================================
// 拖放事件 - 支持 .mp3 和 .skn 文件
// ============================================================

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
    if (urls.isEmpty()) {
        event->ignore();
        return;
    }

    bool foundValidFile = false;
    QString firstValidPath;
    QString sknPath;   // 记录遇到的 .skn 文件

    for (const QUrl &url : urls) {
        QString filePath = url.toLocalFile();
        QString suffix = QFileInfo(filePath).suffix().toLower();

        if (suffix == "mp3" || suffix == "wav" || suffix == "flac"
            || suffix == "ogg" || suffix == "m4a" || suffix == "aac") {
            addPlaylist(filePath);
            if (!foundValidFile) {
                foundValidFile = true;
                firstValidPath = filePath;
            }
        } else if (suffix == "skn") {
            sknPath = filePath;  // 记录 .skn 文件路径
        } else {
            qDebug("Ignoring unsupported file: %s", qUtf8Printable(filePath));
        }
    }

    // ====== 优先处理 .skn 换肤 ======
    if (!sknPath.isEmpty()) {
        qDebug() << "[MainWindow] 检测到 .skn 文件拖放，开始换肤:" << sknPath;

        if (SkinEngine::instance().loadSkin(sknPath)) {
            qDebug() << "[MainWindow] 换肤成功! 新皮肤:" << SkinEngine::instance().skinName();

            // 重建 UI
            applySkin();

            // 显示提示信息
            if (m_currentLyricLabel) {
                m_currentLyricLabel->setText(
                    QString("已切换皮肤: %1").arg(SkinEngine::instance().skinName()));
                m_currentLyricLabel->fadeIn();
            }
        } else {
            qWarning() << "[MainWindow] 换肤失败:" << sknPath;
            if (m_currentLyricLabel) {
                m_currentLyricLabel->setText("换肤失败!");
                m_currentLyricLabel->fadeIn();
            }
        }
    }

    // ====== 处理音频文件播放 ======
    qDebug() << "[DropEvent] foundValidFile=" << foundValidFile << "firstValidPath=" << firstValidPath
             << "m_playlistWindow=" << (m_playlistWindow ? "valid" : "NULL");

    if (foundValidFile && m_playlistWindow) {
        QFileInfo fileInfo(firstValidPath);
        QString displayName = fileInfo.baseName();

        if (m_currentLyricLabel && sknPath.isEmpty()) {
            m_currentLyricLabel->setText(QString("正在播放: %1").arg(displayName));
            m_currentLyricLabel->fadeIn();
        }

        m_currentPlayingPath = firstValidPath;

#ifdef QT_MULTIMEDIA_ENABLED
        m_player->setSource(QUrl::fromLocalFile(firstValidPath));
        m_player->play();
#else
        if (m_audioPlayer) m_audioPlayer->playFile(firstValidPath);
#endif

        // 切换为暂停图标（只传文件名，loadButtonImages 会自动加前缀）
        QString pauseImage = "pause.bmp";
        QList<QPixmap> images = loadButtonImages(pauseImage);
        if (images.size() >= 3) {
            int round = 5;
            setupHoverPressedIcon(m_playBtn,
                roundPixmap(images[0], round),
                roundPixmap(images[1], round),
                roundPixmap(images[2], round));
        }

        if (m_playlistWindow)
            m_playlistWindow->loadLyrics(firstValidPath, this);
    }

    if (!foundValidFile && sknPath.isEmpty())
        event->ignore();
}

// ============================================================
// 播放控制
// ============================================================

void MainWindow::addPlaylist(const QString &filePath)
{
    QFile file("play_list.txt");
    if (file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        in.setCodec("UTF-8");
#endif
        QString content = in.readAll();

        if (!content.contains(filePath)) {
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

        if (m_playlistWindow) {
            m_playlistWindow->loadMusicFolder();
            m_playlistWindow->updatePlaylistDisplay();

#ifdef QT_MULTIMEDIA_ENABLED
            if (m_player->playbackState() == QMediaPlayer::StoppedState) {
#else
            if (true) {  // 无 Multimedia 时始终自动播放第一首
#endif
                QListWidgetItem *firstItem = m_playlistWindow->findChild<QListWidget*>()->item(0);
                if (firstItem) {
                    QMetaObject::invokeMethod(m_playlistWindow, "selectSong", Qt::DirectConnection,
                                             Q_ARG(QListWidgetItem*, firstItem));
                    QString pImg = "pause.bmp";
                    QList<QPixmap> images = loadButtonImages(pImg);
                    if (images.size() >= 3) {
                        int r = 5;
                        setupHoverPressedIcon(m_playBtn,
                            roundPixmap(images[0], r), roundPixmap(images[1], r), roundPixmap(images[2], r));
                    }
                }
            }
        }
    }
}

void MainWindow::playAudio()
{
    bool hasSongs = false;
    if (m_playlistWindow) {
        QListWidget *songList = m_playlistWindow->findChild<QListWidget*>();
        hasSongs = songList && songList->count() > 0;
    }

#ifdef QT_MULTIMEDIA_ENABLED
    QMediaPlayer::PlaybackState state = m_player->playbackState();
    qDebug() << "playAudio: hasSongs=" << hasSongs
             << "source=" << m_player->source()
             << "state=" << state;

    if (!hasSongs && m_player->source().isEmpty()) {
#else
    bool hasSource = m_audioPlayer && !m_currentPlayingPath.isEmpty();
    qDebug() << "playAudio: hasSongs=" << hasSongs
             << "hasSource=" << hasSource
             << "isPlaying=" << (m_audioPlayer ? m_audioPlayer->isPlaying() : false)
             << "isPaused=" << (m_audioPlayer ? m_audioPlayer->isPaused() : false);

    if (!hasSongs && !hasSource) {
#endif
        qDebug() << "没有可播放的歌曲";
        return;
    }

#ifdef QT_MULTIMEDIA_ENABLED
    if (state == QMediaPlayer::PlayingState) {
        // 正在播放 -> 暂停
        m_player->pause();
        switchToPlayIcon();
    } else {
        // 未播放（暂停/停止）-> 播放
        if (m_player->source().isEmpty() && m_playlistWindow) {
            // 没有音源，从列表选一首
            QListWidget *songList = m_playlistWindow->findChild<QListWidget*>();
            if (songList && songList->count() > 0) {
                QListWidgetItem *firstItem = songList->item(0);
                if (firstItem) {
                    QMetaObject::invokeMethod(m_playlistWindow, "selectSong", Qt::DirectConnection,
                                             Q_ARG(QListWidgetItem*, firstItem));
                    return;
                }
            }
        }
        m_player->play();
        switchToPauseIcon();
    }
#else
    if (m_audioPlayer && m_audioPlayer->isPlaying() && !m_audioPlayer->isPaused()) {
        // 正在播放 -> 暂停
        m_audioPlayer->pause();
        switchToPlayIcon();
    } else if (m_audioPlayer && m_audioPlayer->isPaused()) {
        // 已暂停 -> 恢复
        m_audioPlayer->resume();
        switchToPauseIcon();
    } else {
        // 未开始播放，从列表选一首
        if (!hasSource && m_playlistWindow) {
            QListWidget *songList = m_playlistWindow->findChild<QListWidget*>();
            if (songList && songList->count() > 0) {
                QListWidgetItem *firstItem = songList->item(0);
                if (firstItem) {
                    QMetaObject::invokeMethod(m_playlistWindow, "selectSong", Qt::DirectConnection,
                                             Q_ARG(QListWidgetItem*, firstItem));
                    return;
                }
            }
        }
        if (!m_currentPlayingPath.isEmpty() && m_audioPlayer) {
            m_audioPlayer->resume();
            switchToPauseIcon();
        }
    }
#endif
}

void MainWindow::switchToPauseIcon()
{
    QString img = "pause.bmp";
    QList<QPixmap> images = loadButtonImages(img);
    if (images.size() >= 3) {
        setupHoverPressedIcon(m_playBtn,
            roundPixmap(images[0], 5), roundPixmap(images[1], 5), roundPixmap(images[2], 5));
    }
}

void MainWindow::switchToPlayIcon()
{
    QString img = "play.bmp";
    QList<QPixmap> images = loadButtonImages(img);
    if (images.size() >= 3) {
        setupHoverPressedIcon(m_playBtn,
            roundPixmap(images[0], 5), roundPixmap(images[1], 5), roundPixmap(images[2], 5));
    }
}

// ============================================================
// 图片处理工具
// ============================================================

QList<QPixmap> MainWindow::cropImageIntoFourHorizontal(const QString &imagePath)
{
    QList<QPixmap> croppedImages;
    QPixmap originalPixmap(imagePath);
    if (originalPixmap.isNull()) {
        qDebug("Failed to load image: %s", qUtf8Printable(imagePath));
        return croppedImages;
    }

    int width = originalPixmap.width();
    int height = originalPixmap.height();
    int partWidth = width / 4;

    for (int i = 0; i < 4; ++i) {
        croppedImages.append(originalPixmap.copy(i * partWidth, 0, partWidth, height));
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

    return rounded;
}

void MainWindow::setupHoverPressedIcon(QPushButton *button, const QPixmap &normalPixmap,
                                        const QPixmap &hoverPixmap, const QPixmap &pressedPixmap)
{
    button->setProperty("normalPixmap", QVariant(normalPixmap));
    button->setProperty("hoverPixmap", QVariant(hoverPixmap));
    button->setProperty("pressedPixmap", QVariant(pressedPixmap));

    button->setIcon(QIcon(normalPixmap));
    button->setIconSize(button->size());
    button->setStyleSheet(
        "QPushButton { border: none; padding: 0px; margin: 0px; background: transparent; }");

    class ButtonEventFilter : public QObject {
    public:
        ButtonEventFilter(QObject *parent = nullptr) : QObject(parent) {}
        bool eventFilter(QObject *watched, QEvent *event) override {
            QPushButton *button = qobject_cast<QPushButton*>(watched);
            if (!button) return false;

            QPixmap normalPixmap = button->property("normalPixmap").value<QPixmap>();
            QPixmap hoverPixmap = button->property("hoverPixmap").value<QPixmap>();
            QPixmap pressedPixmap = button->property("pressedPixmap").value<QPixmap>();

            switch (event->type()) {
                case QEvent::Enter:
                    button->setIcon(QIcon(hoverPixmap)); return false;
                case QEvent::Leave:
                    button->setIcon(QIcon(normalPixmap)); return false;
                case QEvent::MouseButtonPress:
                    if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton)
                        button->setIcon(QIcon(pressedPixmap));
                    return false;
                case QEvent::MouseButtonRelease:
                {
                    QMouseEvent *me = static_cast<QMouseEvent*>(event);
                    QWidget *wdg = qobject_cast<QWidget*>(watched);
                    if (wdg && QRect(QPoint(0, 0), wdg->size()).contains(me->pos()))
                        button->setIcon(QIcon(hoverPixmap));
                    else
                        button->setIcon(QIcon(normalPixmap));
                    return false;
                }
                default: return false;
            }
        }
    };

    button->installEventFilter(new ButtonEventFilter(button));
}

// ============================================================
// 窗口事件
// ============================================================

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_offset = event->globalPosition().toPoint() - window()->pos();
#else
        m_offset = event->globalPos() - window()->pos();
#endif
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        window()->move(event->globalPosition().toPoint() - m_offset);
#else
        window()->move(event->globalPos() - m_offset);
#endif
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragging = false;
}

// ============================================================
// 滑块与音量
// ============================================================

void MainWindow::updateSliderPosition(qint64 position)
{
    if (!m_progressSlider->isSliderDown())
        m_progressSlider->setPosition(position);
}

void MainWindow::setSliderDuration(qint64 duration)
{
    m_progressSlider->setRange(0, duration);
}

void MainWindow::sliderPressed()
{
    if (m_playlistWindow) {
        QTimer *lyricTimer = m_playlistWindow->findChild<QTimer*>("lyricsUpdateTimer");
        if (lyricTimer) lyricTimer->stop();
    }
}

void MainWindow::sliderReleased()
{
    qint64 position = m_progressSlider->value();
#ifdef QT_MULTIMEDIA_ENABLED
    m_player->setPosition(position);
#else
    if (m_audioPlayer) m_audioPlayer->setPosition(position);
#endif

    if (m_spectrumBars)
        m_spectrumBars->updateForPosition(position);

    if (m_playlistWindow) {
        QTimer *lyricTimer = m_playlistWindow->findChild<QTimer*>("lyricsUpdateTimer");
        if (lyricTimer) {
            lyricTimer->start(100);
            QMetaObject::invokeMethod(m_playlistWindow, "updateLyrics", Qt::DirectConnection);
        }
    }
}

void MainWindow::increaseVolume()
{
    m_volumeSlider->setValue(qMin(m_volumeSlider->value() + 15, 100));
}

void MainWindow::decreaseVolume()
{
    m_volumeSlider->setValue(qMax(m_volumeSlider->value() - 15, 0));
}

// ============================================================
// 皮肤变化回调
// ============================================================

void MainWindow::onSkinChanged(const QString& skinName)
{
    qDebug() << "[MainWindow] 收到皮肤变化通知:" << skinName;
    // 如果是外部触发的换肤（如拖放），applySkin 已在 dropEvent 中调用
    // 这里主要用于程序化调用的场景
}
