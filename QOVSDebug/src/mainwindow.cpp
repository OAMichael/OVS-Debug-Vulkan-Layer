#include <mainwindow.h>
#include "ui_mainwindow.h"

#include <apitracewindow.h>
#include <gpuprofilerwindow.h>
#include <shaderprofilerwindow.h>

#include <QListView>
#include <QStringList>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>

#include <VulkanLayer.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , layersModel(new QStandardItemModel(this))
    , statusBarStackedWidget(new QStackedWidget(this))
    , layerItemDelegate(new LayerItemDelegate(this))
{
    ui->setupUi(this);

    QWidget *centralWidget = ui->centralwidget;
    QStatusBar *statusBar = ui->statusbar;
    QListView *listView = ui->listView;
    QStackedWidget *stackedWidget = ui->stackedWidget;

    listView->setModel(layersModel);
    listView->setItemDelegate(layerItemDelegate);
    stackedWidget->setCurrentIndex(0);

    QItemSelectionModel *selectionModel = listView->selectionModel();
    connect(selectionModel, &QItemSelectionModel::currentChanged, this, &MainWindow::onCurrentLayerChanged);

    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);
    ui->actionOpen->setShortcut(QKeySequence::Open);
    ui->actionExit->setShortcut(QKeySequence::Close);

    QLayout *centralLayout = centralWidget->layout();
    QMargins margins = centralLayout->contentsMargins();
    margins.setTop(0);

    statusBar->addPermanentWidget(statusBarStackedWidget, 1);
    statusBarStackedWidget->setContentsMargins(margins);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::currentPath(), tr("OVS files (*.ovs)"));
    if (fileName.isEmpty()) {
        return;
    }

    qDebug() << "Open file: " << fileName;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open file: " << fileName;
        QMessageBox::critical(this, tr("Error"), tr("Could not open file: %1").arg(fileName));
        return;
    }

    OVS::OVSFileHeader ovsHeader{};
    file.read((char*)&ovsHeader, sizeof(OVS::OVSFileHeader));
    if (ovsHeader.magic != OVS::OVSFileMagic) {
        file.close();
        qDebug() << "Could not open file: " << fileName;
        QMessageBox::critical(this, tr("Error"), tr("Could not open file: %1").arg(fileName));
        return;
    }

    if (ovsHeader.version != OVS::OVSFileVersion) {
        file.close();
        qDebug() << "Could not open file: " << fileName;
        QMessageBox::critical(this, tr("Error"), tr("Could not open file: %1").arg(fileName));
        return;
    }

    uint32_t layerTypeU32 = ovsHeader.layerType;
    if (layerTypeU32 == uint32_t(OVS::VulkanLayerType::None) || layerTypeU32 >= uint32_t(OVS::VulkanLayerType::Count)) {
        file.close();
        qDebug() << "Could not open file: " << fileName;
        QMessageBox::critical(this, tr("Error"), tr("Could not open file: %1").arg(fileName));
        return;
    }

    file.close();

    OVS::VulkanLayerType layerType = OVS::VulkanLayerType(layerTypeU32);
    QString layerTypeStr = OVS::GetLayerReadableTypeName(layerType);
    QString layerIconPath;
    QWidget *window = nullptr;
    QWidget *statusBarWidget = nullptr;
    switch (layerType) {
        case OVS::VulkanLayerType::APITrace: {
            APITraceWindow *apitracewindow = new APITraceWindow(this);
            apitracewindow->openFile(fileName);
            statusBarWidget = apitracewindow->getStatusBarWidget();
            window = apitracewindow;
            layerIconPath = ":/Images/APITraceLogo.png";
            break;
        }
        case OVS::VulkanLayerType::GPUProfiler: {
            GPUProfilerWindow *gpuprofilerwindow = new GPUProfilerWindow(this);
            gpuprofilerwindow->openFile(fileName);
            statusBarWidget = gpuprofilerwindow->getStatusBarWidget();
            window = gpuprofilerwindow;
            layerIconPath = ":/Images/GPUProfilerLogo.png";
            break;
        }
        case OVS::VulkanLayerType::ShaderProfiler: {
            ShaderProfilerWindow *shaderprofilerwindow = new ShaderProfilerWindow(this);
            shaderprofilerwindow->openFile(fileName);
            statusBarWidget = shaderprofilerwindow->getStatusBarWidget();
            window = shaderprofilerwindow;
            layerIconPath = ":/Images/ShaderProfilerLogo.png";
            break;
        }
        default: {
            qDebug() << "File " << fileName << " is valid OVS file but it is of type " << layerTypeStr << " which can't be handled by this tool";
            break;
        }
    }

    if (window) {
        QIcon layerIcon(layerIconPath);
        QStandardItem *layerItem = new QStandardItem(layerIcon, layerTypeStr);
        layerItem->setData(fileName, Qt::UserRole);

        layers.emplaceBack(window);
        layersModel->appendRow(layerItem);

        int rowCount = layersModel->rowCount();
        QModelIndex index = layersModel->index(rowCount - 1, 0);
        ui->listView->setCurrentIndex(index);
        ui->stackedWidget->addWidget(window);
        ui->stackedWidget->setCurrentIndex(rowCount);
        statusBarStackedWidget->addWidget(statusBarWidget);
        statusBarStackedWidget->setCurrentIndex(rowCount - 1);
    }
}

void MainWindow::onCurrentLayerChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QStackedWidget *stackedWidget = ui->stackedWidget;
    stackedWidget->setCurrentIndex(current.row() + 1);
    statusBarStackedWidget->setCurrentIndex(current.row());
}
