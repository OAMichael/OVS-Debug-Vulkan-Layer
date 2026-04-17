#ifndef GPUZONE_H
#define GPUZONE_H

#include <QColor>
#include <QString>
#include <QGraphicsItem>

class QGPUZone : public QGraphicsItem
{
public:
    QGPUZone(const QString &name, float begin, float duration, const QColor &color);

    enum { Type = UserType + 1 };

    int type() const override {
        return Type;
    }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *item, QWidget *widget) override;

    inline QString getDescription() const {
        return QString("%1: %2 us").arg(name).arg(duration);
    }

    static constexpr qreal getHeightValue() { return 40.0; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:

    QString name;
    float begin{0.0f};
    float duration{0.0f};
    QColor color;
};

#endif // GPUZONE_H
