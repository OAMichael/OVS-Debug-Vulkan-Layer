#include <apitracewindow.h>
#include "ui_apitracewindow.h"

#include <QFile>
#include <QThread>
#include <QtMinMax>
#include <QMessageBox>
#include <QProgressBar>
#include <QSizePolicy>
#include <QItemSelectionModel>

APITraceWindow::APITraceWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::APITraceWindow)
    , signaturesModel(new QStandardItemModel(this))
    , treeModel(new QStandardItemModel(this))
    , statusBar(new APITraceStatusBar(this))
{
    ui->setupUi(this);

    QStringList headers = { "Id", "Thread", "Signature" };
    signaturesModel->setColumnCount(headers.count());
    signaturesModel->setHorizontalHeaderLabels(headers);

    int sigRow = headers.count() - 1;

    ui->tableView->setModel(signaturesModel);
    ui->tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    ui->tableView->horizontalHeader()->setSectionResizeMode(sigRow, QHeaderView::ResizeToContents);

    signaturesModel->horizontalHeaderItem(sigRow)->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    QStringList treeHeaders = { "Name", "Value", "Type" };
    treeModel->setColumnCount(treeHeaders.count());
    treeModel->setHorizontalHeaderLabels(treeHeaders);

    ui->treeView->setModel(treeModel);
    ui->treeView->header()->setDefaultAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    ui->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    QItemSelectionModel *selectionModel = ui->tableView->selectionModel();
    connect(selectionModel, &QItemSelectionModel::currentChanged, this, &APITraceWindow::currentSignatureChanged);
}

APITraceWindow::~APITraceWindow()
{
    delete ui;
}

void APITraceWindow::openFile(const QString &fname)
{
    fileName = fname;

    QThread *thread = new QThread();
    APITraceFileWorker *worker = new APITraceFileWorker(fileName);

    worker->moveToThread(thread);

    connect(thread, &QThread::started,                     statusBar, &APITraceStatusBar::showProgressBar);
    connect(thread, &QThread::started,                     worker,    &APITraceFileWorker::openFile);
    connect(worker, &APITraceFileWorker::fileOpenFinished, thread,    &QThread::quit);

    connect(thread, &QThread::finished, statusBar, &APITraceStatusBar::hideProgressBar);
    connect(thread, &QThread::finished, statusBar, &APITraceStatusBar::showLabel);
    connect(thread, &QThread::finished, worker,    &QObject::deleteLater);
    connect(thread, &QThread::finished, thread,    &QObject::deleteLater);

    connect(worker, &APITraceFileWorker::fileDataUpdated,  this, &APITraceWindow::fileDataUpdated);
    connect(worker, &APITraceFileWorker::fileOpenFinished, this, &APITraceWindow::fileOpenFinished);
    connect(worker, &APITraceFileWorker::fileOpenError,    this, &APITraceWindow::fileOpenError);

    connect(worker, &APITraceFileWorker::progressBarMaximumUpdated, statusBar, &APITraceStatusBar::setSignatureCount);
    connect(worker, &APITraceFileWorker::progressBarValueUpdated,   statusBar, &APITraceStatusBar::setSignatureProcessed);

    thread->start();
}

void APITraceWindow::fileDataUpdated(QVector<OVS::SignatureSerializer::SignatureSharedPtr> sigPtrs, QVector<QString> sigStrs)
{
    size_t sigCount = qMin(sigPtrs.size(), sigStrs.size());
    for (size_t i = 0; i < sigCount; ++i) {
        auto &sigPtr = sigPtrs[i];
        auto &sigStr = sigStrs[i];

        signatures.emplaceBack(sigPtr);
        QList<QStandardItem*> row = {
            new QStandardItem(QString::number(sigPtr->header.globalIndex)),
            new QStandardItem(QString::number(sigPtr->header.thread)),
            new QStandardItem(sigStr)
        };
        signaturesModel->appendRow(row);
    }
}

void APITraceWindow::fileOpenFinished()
{
    qDebug() << "Opened file: " << fileName;
}

void APITraceWindow::fileOpenError(const QString &error)
{
    qDebug() << "Could not open file: " << fileName;
    QMessageBox::critical(this, tr("Error"), tr("Could not open file: %1: %2").arg(fileName).arg(error));
}

void APITraceWindow::currentSignatureChanged(const QModelIndex &current, const QModelIndex &previous)
{
    int row = current.row();
    QStandardItem *item = signaturesModel->item(row, 2);
    ui->label->setText(item->text());

    if (current.row() != previous.row()) {
        OVS::SignatureSerializer::SignatureSharedPtr sig = signatures[row];
        OVS::SignatureSerializer::ParamNode rootParamNode;
        sig->SerializeToParamTree(rootParamNode);

        treeModel->removeRows(0, treeModel->rowCount());
        buildTreeModel(rootParamNode, *treeModel->invisibleRootItem());
        ui->treeView->expand(treeModel->index(0, 0));
    }
}

void APITraceWindow::buildTreeModel(const OVS::SignatureSerializer::ParamNode &paramNode, QStandardItem &modelNode) const
{
    QList<QStandardItem*> row = {
        new QStandardItem(QString::fromStdString(paramNode.name)),
        new QStandardItem(QString::fromStdString(paramNode.value)),
        new QStandardItem(QString::fromStdString(paramNode.type))
    };
    modelNode.appendRow(row);

    QStandardItem &mainItem = *row[0];
    for (const auto &child : paramNode.children) {
        buildTreeModel(child, mainItem);
    }
}

void APITraceFileWorker::openFile() {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        emit fileOpenError(file.errorString());
        return;
    }

    OVS::OVSFileHeader ovsHeader{};
    if (file.read((char*)&ovsHeader, sizeof(OVS::OVSFileHeader)) != sizeof(OVS::OVSFileHeader)) {
        emit fileOpenError(QString("Could not read OVS File Header"));
        return;
    }

    if (ovsHeader.magic != OVS::OVSFileMagic) {
        emit fileOpenError(QString("Invalid magic"));
        return;
    }

    if (ovsHeader.version != OVS::OVSFileVersion) {
        emit fileOpenError(QString("Invalid file version"));
        return;
    }

    uint32_t layerTypeU32 = ovsHeader.layerType;
    if (layerTypeU32 == uint32_t(OVS::VulkanLayerType::None) || layerTypeU32 >= uint32_t(OVS::VulkanLayerType::Count)) {
        emit fileOpenError(QString("Invalid layer type"));
        return;
    }

    OVS::APITraceFileHeader apitraceHeader{};
    if (file.read((char*)&apitraceHeader, sizeof(OVS::APITraceFileHeader)) != sizeof(OVS::APITraceFileHeader)) {
        emit fileOpenError(QString("Could not read API Trace File Header"));
        return;
    }

    emit progressBarMaximumUpdated(apitraceHeader.signatureCount);

    QVector<OVS::SignatureSerializer::SignatureSharedPtr> sigPtrs;
    QVector<QString> sigStrs;

    constexpr size_t progressStep = 1000;

    constexpr size_t sigHeaderSize = sizeof(OVS::SignatureSerializer::SignatureHeader);

    std::vector<uint8_t> sigData(sigHeaderSize);
    for (uint64_t i = 0; i < apitraceHeader.signatureCount; ++i) {
        if (file.read((char*)sigData.data(), sigHeaderSize) != sigHeaderSize) {
            emit fileOpenError(QString("Could not read Signature Header"));
            return;
        }

        const auto sigHeader = *reinterpret_cast<const OVS::SignatureSerializer::SignatureHeader*>(sigData.data());
        size_t leftSize = sigHeader.byteSize - sigHeaderSize;

        sigData.resize(sigHeader.byteSize);
        if (file.read((char*)sigData.data() + sigHeaderSize, leftSize) != leftSize) {
            emit fileOpenError(QString("Could not read Signature"));
            return;
        }

        auto signature = OVS::SignatureSerializer::CreateSignature(sigHeader.callID);

        OVS::ReadStream stream(sigData);
        signature->DeserializeFromStream(stream);

        std::stringstream ss;
        signature->SerializeToString(ss);

        OVS::SignatureSerializer::SignatureSharedPtr sigPtr = std::move(signature);
        QString sigStr = QString::fromStdString(ss.str());

        sigPtrs.emplaceBack(std::move(sigPtr));
        sigStrs.emplaceBack(std::move(sigStr));

        if ((i + 1) % progressStep == 0) {
            emit fileDataUpdated(sigPtrs, sigStrs);
            emit progressBarValueUpdated(i + 1);
            sigPtrs.clear();
            sigStrs.clear();
        }
    }

    emit fileDataUpdated(sigPtrs, sigStrs);
    emit progressBarValueUpdated(apitraceHeader.signatureCount);
    emit fileOpenFinished();
}

APITraceStatusBar::APITraceStatusBar(QWidget *parent)
    : QWidget(parent)
    , layout(new QVBoxLayout(this))
    , progressBar(new QProgressBar(this))
    , label(new QLabel(this))
{
    progressBar->setOrientation(Qt::Orientation::Horizontal);
    progressBar->setAlignment(Qt::AlignmentFlag::AlignCenter);
    progressBar->setRange(0, 0);
    progressBar->setValue(0);
    progressBar->setFormat("Opening File: %v/%m (%p%)");
    progressBar->hide();

    label->hide();

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);

    layout->addWidget(progressBar);
    layout->addWidget(label);
}

APITraceStatusBar::~APITraceStatusBar()
{
    delete layout;
}
