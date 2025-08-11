#ifndef FADINGLABEL_H
#define FADINGLABEL_H

#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>

class FadingLabel : public QLabel
{
    Q_OBJECT

public:
    explicit FadingLabel(const QString &text = "", QWidget *parent = nullptr);
    
    // Fade in/out animations
    void fadeIn();
    void fadeOut();

private:
    QGraphicsOpacityEffect *m_effect;
    QPropertyAnimation *m_animation;
};

#endif // FADINGLABEL_H