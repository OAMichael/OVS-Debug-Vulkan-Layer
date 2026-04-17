#ifndef TIMELINEVIEW_H
#define TIMELINEVIEW_H

#include <QtMath>
#include <QGraphicsView>

class TimelineView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit TimelineView(QWidget *parent = nullptr);

    static constexpr qreal getTimelineBarHeight() { return 30.0; }

    void setScene(QGraphicsScene *scene);

private slots:
    void setupMatrix();

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

    void wheelEvent(QWheelEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    inline qreal getHGridWidth() const {
        qreal hGridWidthRaw = 500.0 / scale;
        qreal log = qLn(hGridWidthRaw) / qLn(10.0);
        qreal flooredLog = qFloor(log);
        qreal tenPow = qPow(10.0, flooredLog);
        int dig = (hGridWidthRaw / tenPow);
        if (dig < 5) {
            return tenPow;
        }
        return 5 * tenPow;
    }

    qreal scale{1.0};
};

#endif // TIMELINEVIEW_H
