#ifndef GPUPROFILERWINDOW_H
#define GPUPROFILERWINDOW_H

#include <QWidget>
#include <QString>
#include <QVector>
#include <QVBoxLayout>
#include <QGraphicsScene>
#include <QLabel>

#include <timelineview.h>

#include <VulkanLayer.h>

class GPUProfilerStatusBar;

namespace Ui {
class GPUProfilerWindow;
}

class GPUProfilerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit GPUProfilerWindow(QWidget *parent = nullptr);
    ~GPUProfilerWindow();

    void openFile(const QString &fname);

    inline GPUProfilerStatusBar *getStatusBarWidget() const { return statusBar; }

private slots:
    void selectedItemChanged();

private:
    void fileOpenError(const QString &error);

    int calculateGPUZoneDepth(const OVS::GPUZone& zone);
    void plotGPUZone(const OVS::GPUZone& zone, uint32_t frame, int depth);
    void plotGPUFrame(uint32_t frame, uint64_t begin, uint64_t end, int maxDepth);

    Ui::GPUProfilerWindow *ui;

    QString fileName;
    OVS::GPUProfileInfo profileInfo;
    QGraphicsScene *gpuZonesPlot;
    GPUProfilerStatusBar *statusBar;

    uint64_t originTimestamp{0};
    float timestampPeriod{0.0f};
};

class GPUProfilerStatusBar : public QWidget {
    Q_OBJECT

public:
    explicit GPUProfilerStatusBar(QWidget *parent = nullptr);
    ~GPUProfilerStatusBar();

    inline QLabel *getLabel() const { return label; }

public slots:
    inline void setFramesCount(uint32_t f) { label->setText(QString::number(f) + " Frames Total"); }
    inline void showLabel() { label->show(); }
    inline void hideLabel() { label->hide(); }

private:
    QVBoxLayout *layout;
    QLabel *label;
};

#endif // GPUPROFILERWINDOW_H
