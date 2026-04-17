#include <gpuprofilerwindow.h>
#include "ui_gpuprofilerwindow.h"

#include <QFile>
#include <QThread>
#include <QtMinMax>
#include <QMessageBox>
#include <QProgressBar>
#include <QSizePolicy>
#include <QItemSelectionModel>

#include <gpuzone.h>

GPUProfilerWindow::GPUProfilerWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GPUProfilerWindow)
    , gpuZonesPlot(new QGraphicsScene(this))
    , statusBar(new GPUProfilerStatusBar(this))
{
    ui->setupUi(this);

    ui->timelineView->setScene(gpuZonesPlot);

    connect(gpuZonesPlot, &QGraphicsScene::selectionChanged, this, &GPUProfilerWindow::selectedItemChanged);
}

GPUProfilerWindow::~GPUProfilerWindow()
{
    delete ui;
}

void GPUProfilerWindow::selectedItemChanged()
{
    QList<QGraphicsItem *> selectedItems = gpuZonesPlot->selectedItems();
    if (selectedItems.empty()) {
        ui->label->setText(QString());
        return;
    }

    QGPUZone *gpuZone = qgraphicsitem_cast<QGPUZone *>(selectedItems.front());
    if (!gpuZone) {
        return;
    }

    ui->label->setText(gpuZone->getDescription());
}

void GPUProfilerWindow::fileOpenError(const QString &error)
{
    qDebug() << "Could not open file: " << fileName;
    QMessageBox::critical(this, tr("Error"), tr("Could not open file: %1: %2").arg(fileName).arg(error));
}

void GPUProfilerWindow::openFile(const QString &fname)
{
    fileName = fname;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        fileOpenError(file.errorString());
        return;
    }

    OVS::OVSFileHeader ovsHeader{};
    if (file.read((char*)&ovsHeader, sizeof(OVS::OVSFileHeader)) != sizeof(OVS::OVSFileHeader)) {
        fileOpenError(QString("Could not read OVS File Header"));
        return;
    }

    if (ovsHeader.magic != OVS::OVSFileMagic) {
        fileOpenError(QString("Invalid magic"));
        return;
    }

    if (ovsHeader.version != OVS::OVSFileVersion) {
        fileOpenError(QString("Invalid file version"));
        return;
    }

    uint32_t layerTypeU32 = ovsHeader.layerType;
    if (layerTypeU32 == uint32_t(OVS::VulkanLayerType::None) || layerTypeU32 >= uint32_t(OVS::VulkanLayerType::Count)) {
        fileOpenError(QString("Invalid layer type"));
        return;
    }

    OVS::GPUProfilerFileHeader gpuProfHeader{};
    if (file.read((char*)&gpuProfHeader, sizeof(OVS::GPUProfilerFileHeader)) != sizeof(OVS::GPUProfilerFileHeader)) {
        fileOpenError(QString("Could not read GPU Profiler File Header"));
        return;
    }

    std::vector<uint8_t> data(gpuProfHeader.byteSize);
    if (file.read((char*)data.data(), gpuProfHeader.byteSize) != gpuProfHeader.byteSize) {
        fileOpenError(QString("Could not read profile data"));
        return;
    }

    OVS::ReadStream stream(data);

    OVS::GPUProfileInfo profileInfo;
    OVS::DeserializeFromStream(profileInfo, stream);

    const auto& frameInfos = profileInfo.frameInfos;
    uint32_t framesCount = frameInfos.size();
    statusBar->setFramesCount(framesCount);
    statusBar->showLabel();

    if (framesCount == 0) {
        return;
    }

    const auto& firstFrame = frameInfos.front();
    for (const auto& commandBufferInfo : firstFrame.commandBufferInfos) {
        const auto& rootZone = commandBufferInfo.rootZone;
        timestampMin = qMin(rootZone.begin, timestampMin);
    }

    const auto& lastFrame = frameInfos.back();
    for (const auto& commandBufferInfo : lastFrame.commandBufferInfos) {
        const auto& rootZone = commandBufferInfo.rootZone;
        timestampMax = qMax(rootZone.end, timestampMax);
    }

    timestampPeriod = gpuProfHeader.timestampPeriod;

    for (const auto& frameInfo : frameInfos) {
        const auto& commandBufferInfos = frameInfo.commandBufferInfos;
        for (const auto& commandBufferInfo : commandBufferInfos) {
            plotGPUZone(commandBufferInfo.rootZone, 0);
        }
    }
}

void GPUProfilerWindow::plotGPUZone(const OVS::GPUZone& zone, int layer)
{
    uint64_t zoneBeginU64 = zone.begin - timestampMin;
    uint64_t zoneEndU64 = zone.end - timestampMin;
    uint64_t zoneDurU64 = zoneEndU64 - zoneBeginU64;

    float zoneBeginF = zoneBeginU64 * timestampPeriod / 1000.0f;
    float zoneDurF = zoneDurU64 * timestampPeriod / 1000.0f;

    QString zoneName(zone.name.c_str());
    QGPUZone *qgpuzone = new QGPUZone(zoneName, zoneBeginF, zoneDurF, QColor(255, 0, 255));
    qgpuzone->setPos(zoneBeginF, layer * QGPUZone::getHeightValue());
    gpuZonesPlot->addItem(qgpuzone);

    for (const auto& child : zone.children) {
        plotGPUZone(child, layer + 1);
    }
}

GPUProfilerStatusBar::GPUProfilerStatusBar(QWidget *parent)
    : QWidget(parent)
    , layout(new QVBoxLayout(this))
    , label(new QLabel(this))
{
    label->hide();

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);

    layout->addWidget(label);
}

GPUProfilerStatusBar::~GPUProfilerStatusBar()
{
    delete layout;
}
