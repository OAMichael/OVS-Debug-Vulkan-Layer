#ifndef LAYERITEMDELEGATE_H
#define LAYERITEMDELEGATE_H

#include <QStyle>
#include <QStyledItemDelegate>

class LayerItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit LayerItemDelegate(QObject *parent = nullptr);
    ~LayerItemDelegate();

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QStyle *style;
};

#endif // LAYERITEMDELEGATE_H
