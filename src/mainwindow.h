#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QMediaPlayer>
#include <QShortcut>
#include <QPoint>
#include <QPropertyAnimation>
#include <QPixmap>
#include <QMap>

#include "fadinglabel.h"
#include "imageslider.h"
#include "spectrumbars.h"

class PlayList;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void exitAll();
    void winFixed();
    void minimizeWindow();
    void showMusicList();
    void playAudio();
    void updateSliderPosition(qint64 position);
    void setSliderDuration(qint64 duration);
    void sliderPressed();
    void sliderReleased();
    void increaseVolume();
    void decreaseVolume();

private:
    void initUI();
    QPropertyAnimation* startAnimation(float start, float end);
    QList<QPixmap> cropImageIntoFourHorizontal(const QString &imagePath);
    QPixmap roundPixmap(const QPixmap &pixmap, int radius);
    void addPlaylist(const QString &filePath);
    void setupHoverPressedIcon(QPushButton *button, const QPixmap &normalPixmap, 
                              const QPixmap &hoverPixmap, const QPixmap &pressedPixmap);

    // UI Elements
    QPushButton *m_musicListBtn;
    QPushButton *m_previewBtn;
    QPushButton *m_playBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_fixedBtn;
    QPushButton *m_miniTopBtn;
    QPushButton *m_minBtn;
    QPushButton *m_closeBtn;
    QPushButton *m_lrcBtn;
    
    ImageSlider *m_progressSlider;
    ImageSlider *m_volumeSlider;
    FadingLabel *m_currentLyricLabel;
    SpectrumBars *m_spectrumBars;
    
    // Player components
    QMediaPlayer *m_player;
    QList<QString> m_playlist;
    int m_currentIndex;
    bool m_shuffleMode;
    QString m_currentPlayingPath;
    
    // Drag support
    bool m_dragging;
    QPoint m_offset;
    
    // Playlist window
    PlayList *m_playlistWindow;
    
    // Shortcuts
    QShortcut *m_spaceShortcut;
    QShortcut *m_upShortcut;
    QShortcut *m_downShortcut;
    
    // Animation
    QPropertyAnimation *m_animation;
};

#endif // MAINWINDOW_H
