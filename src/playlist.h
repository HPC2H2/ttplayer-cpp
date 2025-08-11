#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QWidget>
#include <QPushButton>
#include <QListWidget>
#include <QPropertyAnimation>
#include <QTimer>
#include <QMediaPlayer>
#include <QPair>

class MainWindow;

class PlayList : public QWidget
{
    Q_OBJECT

public:
    explicit PlayList(int x = 255, int y = 255, int width = 2, int height = 0, MainWindow *mainWindow = nullptr);
    ~PlayList();

    void loadMusicFolder();
    void updatePlaylistDisplay();
    QPropertyAnimation* startAnimation(float start, float end);
    void loadLyrics(const QString &audioPath, MainWindow *mainWindow);
    void updateLyrics();
    void addPlaylist(const QString &filePath);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void exitAll();
    void selectSong(QListWidgetItem *item);

private:
    void initUI();
    QPixmap roundPixmap(const QPixmap &pixmap, int radius);
    QList<QPixmap> cropImageIntoFourHorizontal(const QString &imagePath);

    // UI Elements
    QPushButton *m_closeBtn;
    QListWidget *m_songList;
    
    // Window positioning
    int m_x;
    int m_y;
    int m_width;
    int m_height;
    
    // Drag support
    bool m_dragging;
    QPoint m_offset;
    
    // Main window reference
    MainWindow *m_mainWindow;
    
    // Playlist data
    QStringList m_playlist;
    
    // Lyrics
    QList<QPair<qint64, QString>> m_lyrics;
    int m_currentLyricIndex;
    
    // Animation
    QPropertyAnimation *m_animation;
    
    // Timer for lyrics
    QTimer *m_lyricTimer;
};

#endif // PLAYLIST_H