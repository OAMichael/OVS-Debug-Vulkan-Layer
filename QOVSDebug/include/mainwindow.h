#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QVector>

#include <layeritemdelegate.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void onCurrentLayerChanged(const QModelIndex &current, const QModelIndex &previous);

private:
    Ui::MainWindow *ui;

    QStandardItemModel *layersModel;
    QVector<QWidget*> layers;
    QStackedWidget *statusBarStackedWidget;
    LayerItemDelegate *layerItemDelegate;
};
#endif // MAINWINDOW_H
