#ifndef SHADERPROFILERWINDOW_H
#define SHADERPROFILERWINDOW_H

#include <QWidget>
#include <QString>
#include <QVector>
#include <QStandardItemModel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableView>
#include <QTreeView>

#include <VulkanLayer.h>

class ShaderProfilerStatusBar;

namespace Ui {
class ShaderProfilerWindow;
}

class ShaderProfilerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ShaderProfilerWindow(QWidget *parent = nullptr);
    ~ShaderProfilerWindow();

    void openFile(const QString &fname);

    inline ShaderProfilerStatusBar *getStatusBarWidget() const { return statusBar; }

private slots:
    void fileOpenError(const QString &error);
    void currentResourceChanged(const QModelIndex &current, const QModelIndex &previous);

private:
    struct ShaderMVCInfo {
        QStandardItemModel *sourceModel{nullptr};
        QStandardItemModel *spirvModel{nullptr};
        QTableView *sourceView{nullptr};
        QTreeView *spirvView{nullptr};
    };

    Ui::ShaderProfilerWindow *ui;

    QString fileName;
    QStandardItemModel *resourcesTreeModel;
    QVector<ShaderMVCInfo> shadersMVCInfos;
    ShaderProfilerStatusBar *statusBar;
};

class ShaderProfilerStatusBar : public QWidget {
    Q_OBJECT

public:
    explicit ShaderProfilerStatusBar(QWidget *parent = nullptr);
    ~ShaderProfilerStatusBar();

    inline QLabel *getLabel() const { return label; }

public slots:
    inline void setPipelinesAndShadersCount(uint32_t pipelinesCount, uint32_t shadersCount) {
        QString str = QString::number(pipelinesCount) + " Pipelines Total, "
                      + QString::number(shadersCount) + " Shaders Total";

        label->setText(str);
    }

    inline void showLabel() { label->show(); }
    inline void hideLabel() { label->hide(); }

private:
    QVBoxLayout *layout;
    QLabel *label;
};

#endif // SHADERPROFILERWINDOW_H
