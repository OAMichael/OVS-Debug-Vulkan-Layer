#include <QWheelEvent>

#include <timelineview.h>
#include <gpuzone.h>

static inline QString formatterS(qreal v) {
    return QString("%1 s").arg(uint64_t(v / 1000000.0));
}

static inline QString formatterMS(qreal v) {
    return QString("%1 ms").arg(uint64_t(v / 1000.0));
}

static inline QString formatterUS(qreal v) {
    return QString("%1 us").arg(uint64_t(v));
}

static inline QString formatterNS(qreal v) {
    return QString("%1 ns").arg(uint64_t(v * 1000.0));
}

using PFN_Formatter = QString(*)(qreal);

TimelineView::TimelineView(QWidget *parent)
    : QGraphicsView(parent)
{
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setRenderHint(QPainter::Antialiasing, false);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    setupMatrix();
}

void TimelineView::setScene(QGraphicsScene *scene)
{
    QGraphicsView::setScene(scene);

    QGraphicsRectItem *timelineRect = new QGraphicsRectItem(0, -getTimelineBarHeight() - 1, 1, getTimelineBarHeight());
    scene->addItem(timelineRect);
}

void TimelineView::setupMatrix()
{
    QTransform matrix;
    matrix.scale(scale, 1.0);

    setTransform(matrix);
}

void TimelineView::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter, rect);

    painter->save();
    painter->setPen(QPen(QColor(0, 0, 0, 63), 0.0f));

    qreal hGridWidth = getHGridWidth();
    qreal vGridWidth = 2.0 * QGPUZone::getHeightValue();

    qreal lineY = int(rect.top() / vGridWidth) * vGridWidth;
    while (lineY < rect.bottom()) {
        QLineF line(rect.left(), lineY, rect.right(), lineY);
        painter->drawLine(line);
        lineY += vGridWidth;
    }

    qreal lineX = int(rect.left() / hGridWidth) * hGridWidth;
    while (lineX < rect.right()) {
        QLineF line(lineX, rect.top(), lineX, rect.bottom());
        painter->drawLine(line);
        lineX += hGridWidth;
    }

    painter->restore();
}

void TimelineView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QRectF timelineRect = rect;
    timelineRect.setHeight(getTimelineBarHeight());

    painter->save();

    painter->setPen(QPen(QColor(0, 0, 0), 0.0f));
    painter->setBrush(QColor(230, 230, 230));
    painter->drawRect(timelineRect);

    qreal hGridWidth = getHGridWidth();
    PFN_Formatter hGridFormatter;
    if (hGridWidth > 1000000.0) {
        hGridFormatter = &formatterS;
    }
    else if (hGridWidth > 1000.0) {
        hGridFormatter = &formatterMS;
    }
    else if (hGridWidth > 1.0) {
        hGridFormatter = &formatterUS;
    }
    else {
        hGridFormatter = &formatterNS;
    }

    qreal tickX = int(rect.left() / hGridWidth) * hGridWidth;
    qreal tickYTop = timelineRect.top() + timelineRect.height() * 0.66;
    qreal tickYBottom = timelineRect.bottom();
    qreal tickTextYCenter = timelineRect.top() + timelineRect.height() * 0.33;
    while (tickX < rect.right()) {
        QLineF line(tickX, tickYTop, tickX, tickYBottom);
        painter->drawLine(line);

        QString tickText = hGridFormatter(tickX);
        QRectF tickTextRect = painter->fontMetrics().boundingRect(tickText);
        tickTextRect.moveCenter(QPointF(tickX * scale, tickTextYCenter));

        painter->scale(1.0 / scale, 1.0);
        painter->drawText(tickTextRect, Qt::AlignCenter, tickText);
        painter->scale(scale, 1.0);

        tickX += hGridWidth;
    }

    painter->restore();

    QGraphicsView::drawForeground(painter, rect);
}

void TimelineView::wheelEvent(QWheelEvent *e)
{
    qreal amplifier = 1.2;
    if (e->modifiers() & Qt::ShiftModifier) {
        amplifier = 2.5;
    }

    if (e->angleDelta().y() > 0) {
        scale *= amplifier;
    }
    else {
        scale /= amplifier;
    }
    setupMatrix();
    e->accept();
}

void TimelineView::mouseDoubleClickEvent(QMouseEvent *e)
{
    scale = 1.0;
    setupMatrix();
    e->accept();
}
