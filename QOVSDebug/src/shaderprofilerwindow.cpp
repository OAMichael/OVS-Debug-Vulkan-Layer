#include <shaderprofilerwindow.h>
#include "ui_shaderprofilerwindow.h"

#include <QFile>
#include <QThread>
#include <QtMinMax>
#include <QMessageBox>
#include <QProgressBar>
#include <QSizePolicy>
#include <QItemSelectionModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QTableView>

#include <spirv-tools/libspirv.hpp>
#include <opt/build_module.h>
#include <spirv_constant.h>

#include <VulkanShader.h>

static void SPIRVErrorHandler(spv_message_level_t, const char*, const spv_position_t&, const char* m) {
    std::cout << "SPIRV: " << m << '\n';
};

static constexpr QColor ColorExecutedMin = QColor(0,   255, 0, 63);
static constexpr QColor ColorExecutedAvg = QColor(255, 255, 0, 63);
static constexpr QColor ColorExecutedMax = QColor(255,   0, 0, 63);

static inline QColor Lerp(QColor a, QColor b, double t) {
    float ar = a.redF();
    float ag = a.greenF();
    float ab = a.blueF();
    float aa = a.alphaF();

    float br = b.redF();
    float bg = b.greenF();
    float bb = b.blueF();
    float ba = b.alphaF();

    float cr = ar * (1.0 - t) + br * t;
    float cg = ag * (1.0 - t) + bg * t;
    float cb = ab * (1.0 - t) + bb * t;
    float ca = aa * (1.0 - t) + ba * t;

    QColor c;
    c.setRedF(cr);
    c.setGreenF(cg);
    c.setBlueF(cb);
    c.setAlphaF(ca);
    return c;
}

class TreeItemDelegate : public QStyledItemDelegate
{
public:
    explicit TreeItemDelegate(QObject * parent = nullptr) : QStyledItemDelegate(parent) {}

    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        if (index.data(Qt::UserRole + 3).isValid()) {
            opt.font.setBold(true);
        }

        QStyledItemDelegate::paint(painter, opt, index);

        painter->save();
        painter->setPen(QPen(Qt::black, 1.0f));
        auto rect = option.rect;
        rect.adjust(0, 1, 0, -1);
        painter->drawRect(rect);
        painter->restore();
    }

    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QSize sz = QStyledItemDelegate::sizeHint(option, index);
        sz.setHeight(sz.height() + 6);
        if (index.data(Qt::UserRole + 1).isValid()) {
            sz.setHeight(sz.height() * 0.8);
        }
        return sz;
    }
};

ShaderProfilerWindow::ShaderProfilerWindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ShaderProfilerWindow)
    , resourcesTreeModel(new QStandardItemModel(this))
    , statusBar(new ShaderProfilerStatusBar(this))
{
    ui->setupUi(this);

    QStringList resourcesTreeHeaders = { "Resource", "Type" };
    resourcesTreeModel->setColumnCount(resourcesTreeHeaders.count());
    resourcesTreeModel->setHorizontalHeaderLabels(resourcesTreeHeaders);

    ui->resourcesTreeView->setModel(resourcesTreeModel);

    const QFontMetrics& resourcesFontMetrics = ui->resourcesTreeView->fontMetrics();

    ui->resourcesTreeView->header()->setMinimumHeight(1.3 * resourcesFontMetrics.height());
    ui->resourcesTreeView->header()->setDefaultAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    ui->resourcesTreeView->setItemDelegate(new TreeItemDelegate(this));

    QItemSelectionModel *selectionModel = ui->resourcesTreeView->selectionModel();
    connect(selectionModel, &QItemSelectionModel::currentChanged, this, &ShaderProfilerWindow::currentResourceChanged);
}

ShaderProfilerWindow::~ShaderProfilerWindow()
{
    delete ui;
}

void ShaderProfilerWindow::openFile(const QString &fname)
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

    OVS::ShaderProfilerFileHeader shaderProfHeader{};
    if (file.read((char*)&shaderProfHeader, sizeof(OVS::ShaderProfilerFileHeader)) != sizeof(OVS::ShaderProfilerFileHeader)) {
        fileOpenError(QString("Could not read Shader Profiler File Header"));
        return;
    }

    std::vector<uint8_t> data(shaderProfHeader.byteSize);
    if (file.read((char*)data.data(), shaderProfHeader.byteSize) != shaderProfHeader.byteSize) {
        fileOpenError(QString("Could not read profile data"));
        return;
    }

    OVS::ReadStream stream(data);

    uint64_t collectedProfileInfoSize = 0;
    stream.Read(collectedProfileInfoSize);

    std::vector<OVS::CollectedPipelineProfileInfo> collectedProfileInfos(collectedProfileInfoSize);
    for (auto& collectedProfileInfo : collectedProfileInfos) {
        OVS::DeserializeFromStream(collectedProfileInfo, stream);
    }

    uint32_t pipelinesCount = collectedProfileInfoSize;
    uint32_t shadersCount = 0;
    for (const auto& collectedProfileInfo : collectedProfileInfos) {
        shadersCount += collectedProfileInfo.shaderInfos.size();
    }

    statusBar->setPipelinesAndShadersCount(pipelinesCount, shadersCount);
    statusBar->showLabel();

    QStandardItem *resourcesRootNode = resourcesTreeModel->invisibleRootItem();
    for (const auto& profileInfo : collectedProfileInfos) {
        QString pipelineDesc = "Pipeline 0x" + QString::number(uint64_t(profileInfo.pipeline), 16);

        QList<QStandardItem*> pipelineRow = {
            new QStandardItem(pipelineDesc),
            new QStandardItem(OVS::GetVulkanPipelineBindPointName(profileInfo.bindPoint)),
        };
        resourcesRootNode->appendRow(pipelineRow);

        int shaderMVCInfoIndex = shadersMVCInfos.size();
        pipelineRow[0]->setData(shaderMVCInfoIndex, Qt::UserRole + 2);
        pipelineRow[1]->setData(shaderMVCInfoIndex, Qt::UserRole + 2);

        pipelineRow[0]->setData(1, Qt::UserRole + 3);
        pipelineRow[1]->setData(1, Qt::UserRole + 3);

        for (const auto& shaderInfo : profileInfo.shaderInfos) {
            const auto& profileData = shaderInfo.profileData;

            QString shaderDesc = "Shader 0x" + QString::number(uint64_t(shaderInfo.shader), 16);

            QList<QStandardItem*> shaderRow = {
                new QStandardItem(shaderDesc),
                new QStandardItem(OVS::GetVulkanShaderStageName(shaderInfo.stage)),
            };
            pipelineRow[0]->appendRow(shaderRow);
            shaderRow[0]->setData(1, Qt::UserRole + 1);
            shaderRow[1]->setData(1, Qt::UserRole + 1);

            shaderMVCInfoIndex = shadersMVCInfos.size();
            shaderRow[0]->setData(shaderMVCInfoIndex, Qt::UserRole + 2);
            shaderRow[1]->setData(shaderMVCInfoIndex, Qt::UserRole + 2);

            ShaderMVCInfo& shaderMVCInfo = shadersMVCInfos.emplaceBack();

            auto context = spvtools::BuildModule(SPV_ENV_VULKAN_1_4, &SPIRVErrorHandler, shaderInfo.code.data(), shaderInfo.code.size(), true);
            if (!context) {
                qDebug() << "Could not build module for shader 0x" << QString::number(uint64_t(shaderInfo.shader), 16);
                continue;
            }

            auto& m = *context->module();

            OVS::SPIRVProfileInfo spvProfileInfo;
            OVS::SetupSPIRVProfileInfo(m, profileData, spvProfileInfo);

            if (OVS::ParseSPIRVDebugInfo(m, spvProfileInfo.debugInfo)) {
                OVS::ComputeLinesExecuted(m, spvProfileInfo);

                shaderMVCInfo.sourceModel = new QStandardItemModel(this);
                shaderMVCInfo.sourceView = new QTableView(this);

                QStandardItemModel* sourceModel = shaderMVCInfo.sourceModel;
                QTableView* sourceView = shaderMVCInfo.sourceView;

                QStringList sourceHeaders = { "Source", "Executions", "Execution Percent" };
                sourceModel->setColumnCount(sourceHeaders.count());
                sourceModel->setHorizontalHeaderLabels(sourceHeaders);
                sourceView->setModel(sourceModel);

                QFont font = sourceView->font();
                font.setPointSize(12);
                sourceView->setFont(font);

                QFontMetrics fontMetrics = sourceView->fontMetrics();
                sourceView->horizontalHeader()->setMinimumHeight(1.3 * fontMetrics.height());
                sourceView->horizontalHeader()->setDefaultAlignment(Qt::AlignVCenter | Qt::AlignLeft);
                sourceView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
                sourceView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
                sourceView->setEditTriggers(QAbstractItemView::NoEditTriggers);
                sourceView->setAutoScroll(false);

                sourceView->verticalHeader()->setDefaultSectionSize(0);

                ui->sourceStackedWidget->addWidget(sourceView);

                const auto& source = spvProfileInfo.debugInfo.source;
                auto lines = source | std::views::split('\n');

                size_t lineNum = 1;
                size_t lineExecutedCount = 0;
                double lineExecutedPercentMin = std::numeric_limits<double>::max();
                double lineExecutedPercentMax = 0.0;
                for (const auto& line : lines) {
                    auto lineExecutedIt = spvProfileInfo.linesExecuted.find(lineNum);
                    if (lineExecutedIt != spvProfileInfo.linesExecuted.end()) {
                        uint64_t lineExecuted = lineExecutedIt->second;
                        double lineExecutedPercent = 100.0 * double(lineExecuted) / double(spvProfileInfo.totalInstExecuted);
                        lineExecutedPercentMin = std::min<double>(lineExecutedPercentMin, lineExecutedPercent);
                        lineExecutedPercentMax = std::max<double>(lineExecutedPercentMax, lineExecutedPercent);
                        ++lineExecutedCount;
                    }
                    ++lineNum;
                }

                double lineExecutedPercentAvg = 100.0 / double(lineExecutedCount);
                double tLineMin = lineExecutedPercentMin / lineExecutedPercentAvg;
                double tLineMax = lineExecutedPercentAvg / lineExecutedPercentMax;
                QColor lineColorMin = Lerp(ColorExecutedMin, ColorExecutedAvg, tLineMin);
                QColor lineColorAvg = ColorExecutedAvg;
                QColor lineColorMax = Lerp(ColorExecutedMax, ColorExecutedAvg, tLineMax);

                lineNum = 1;
                for (const auto& line : lines) {
                    std::string lineStr = std::string(line.data(), line.size());
                    QString qline = QString::fromStdString(lineStr);
                    QString qexecuted;
                    QString qexecutedPercent;
                    QColor lineColor;

                    auto lineExecutedIt = spvProfileInfo.linesExecuted.find(lineNum);
                    if (lineExecutedIt != spvProfileInfo.linesExecuted.end()) {
                        uint64_t lineExecuted = lineExecutedIt->second;
                        double lineExecutedPercent = 100.0 * double(lineExecuted) / double(spvProfileInfo.totalInstExecuted);
                        qexecuted = QString("%1").arg(lineExecuted);
                        qexecutedPercent = QString("%1").arg(lineExecutedPercent) + "%";

                        if (lineExecutedPercent < lineExecutedPercentAvg) {
                            double t = lineExecutedPercent / lineExecutedPercentAvg;
                            lineColor = Lerp(lineColorMin, lineColorAvg, t);
                        }
                        else if (lineExecutedPercent > lineExecutedPercentAvg) {
                            double t = lineExecutedPercentAvg / lineExecutedPercent;
                            lineColor = Lerp(lineColorMax, lineColorAvg, t);
                        }
                        else {
                            lineColor = lineColorAvg;
                        }
                    }

                    QList<QStandardItem*> sourceRow = {
                        new QStandardItem(qline),
                        new QStandardItem(qexecuted),
                        new QStandardItem(qexecutedPercent),
                    };
                    sourceModel->appendRow(sourceRow);

                    if (lineColor.isValid()) {
                        sourceRow[0]->setBackground(QBrush(lineColor));
                        sourceRow[1]->setBackground(QBrush(lineColor));
                        sourceRow[2]->setBackground(QBrush(lineColor));
                    }

                    ++lineNum;
                }

                sourceView->resizeColumnsToContents();
            }

            shaderMVCInfo.spirvModel = new QStandardItemModel(this);
            shaderMVCInfo.spirvView = new QTreeView(this);

            QStandardItemModel *spirvModel = shaderMVCInfo.spirvModel;
            QTreeView *spirvView = shaderMVCInfo.spirvView;

            QStringList spirvTreeHeaders = { "Instruction", "Line", "Executions", "Execution Percent" };
            spirvModel->setColumnCount(spirvTreeHeaders.count());
            spirvModel->setHorizontalHeaderLabels(spirvTreeHeaders);
            spirvView->setModel(spirvModel);

            spirvView->setStyleSheet(
                "QTreeView::branch:!has-children:adjoins-item:has-siblings {"
                "    border-image: url(:/Images/branch-more.png) 0;"
                "}"

                "QTreeView::branch:!has-children:adjoins-item:!has-siblings {"
                "    border-image: url(:/Images/branch-end.png) 0;"
                "}"
            );

            QFont font("Monospace", 12);
            font.setStyleHint(QFont::Monospace);
            spirvView->setFont(font);

            const QFontMetrics& spirvFontMetrics = spirvView->fontMetrics();

            spirvView->header()->setMinimumHeight(1.3 * spirvFontMetrics.height());
            spirvView->header()->setDefaultAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            spirvView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
            spirvView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
            spirvView->setEditTriggers(QAbstractItemView::NoEditTriggers);
            spirvView->setAutoScroll(false);
            spirvView->header()->setStretchLastSection(false);
            spirvView->setItemDelegate(new TreeItemDelegate(this));

            ui->spirvStackedWidget->addWidget(spirvView);

            size_t funcExecutedCount = 0;
            double funcExecutedPercentMin = std::numeric_limits<double>::max();
            double funcExecutedPercentMax = 0.0;

            size_t bbExecutedCount = 0;
            double bbExecutedPercentMin = std::numeric_limits<double>::max();
            double bbExecutedPercentMax = 0.0;

            size_t instExecutedCount = 0;
            double instExecutedPercentMin = std::numeric_limits<double>::max();
            double instExecutedPercentMax = 0.0;
            for (const auto& f : m) {
                uint64_t funcExecuted = spvProfileInfo.funcExecuted[f.result_id()];
                double funcExecutedPercent = 100.0 * double(funcExecuted) / double(spvProfileInfo.totalFuncExecuted);
                funcExecutedPercentMin = std::min<double>(funcExecutedPercentMin, funcExecutedPercent);
                funcExecutedPercentMax = std::max<double>(funcExecutedPercentMax, funcExecutedPercent);
                ++funcExecutedCount;

                for (const auto& bb : f) {
                    uint64_t bbExecuted = spvProfileInfo.bbExecuted[bb.id()];
                    double bbExecutedPercent = 100.0 * double(bbExecuted) / double (spvProfileInfo.totalBBExecuted);
                    bbExecutedPercentMin = std::min<double>(bbExecutedPercentMin, bbExecutedPercent);
                    bbExecutedPercentMax = std::max<double>(bbExecutedPercentMax, bbExecutedPercent);
                    ++bbExecutedCount;

                    for (const auto& inst : bb) {
                        uint64_t instExecuted = spvProfileInfo.instExecuted[inst.unique_id()];
                        double instExecutedPercent = 100.0 * double(instExecuted) / double(spvProfileInfo.totalInstExecuted);
                        instExecutedPercentMin = std::min<double>(instExecutedPercentMin, instExecutedPercent);
                        instExecutedPercentMax = std::max<double>(instExecutedPercentMax, instExecutedPercent);
                        ++instExecutedCount;
                    }
                }
            }

            double funcExecutedPercentAvg = 100.0 / double(funcExecutedCount);
            double tFuncMin = funcExecutedPercentMin / funcExecutedPercentAvg;
            double tFuncMax = funcExecutedPercentAvg / funcExecutedPercentMax;
            QColor funcColorMin = Lerp(ColorExecutedMin, ColorExecutedAvg, tFuncMin);
            QColor funcColorAvg = ColorExecutedAvg;
            QColor funcColorMax = Lerp(ColorExecutedMax, ColorExecutedAvg, tFuncMax);
            funcColorMin.setAlphaF(funcColorMin.alphaF() * 2.0f);
            funcColorAvg.setAlphaF(funcColorAvg.alphaF() * 2.0f);
            funcColorMax.setAlphaF(funcColorMax.alphaF() * 2.0f);

            double bbExecutedPercentAvg = 100.0 / double(bbExecutedCount);
            double tBBMin = bbExecutedPercentMin / bbExecutedPercentAvg;
            double tBBMax = bbExecutedPercentAvg / bbExecutedPercentMax;
            QColor bbColorMin = Lerp(ColorExecutedMin, ColorExecutedAvg, tBBMin);
            QColor bbColorAvg = ColorExecutedAvg;
            QColor bbColorMax = Lerp(ColorExecutedMax, ColorExecutedAvg, tBBMax);
            bbColorMin.setAlphaF(bbColorMin.alphaF() * 1.5f);
            bbColorAvg.setAlphaF(bbColorAvg.alphaF() * 1.5f);
            bbColorMax.setAlphaF(bbColorMax.alphaF() * 1.5f);

            double instExecutedPercentAvg = 100.0 / double(instExecutedCount);
            double tInstMin = instExecutedPercentMin / instExecutedPercentAvg;
            double tInstMax = instExecutedPercentAvg / instExecutedPercentMax;
            QColor instColorMin = Lerp(ColorExecutedMin, ColorExecutedAvg, tInstMin);
            QColor instColorAvg = ColorExecutedAvg;
            QColor instColorMax = Lerp(ColorExecutedMax, ColorExecutedAvg, tInstMax);

            QStandardItem *spirvRootNode = spirvModel->invisibleRootItem();

            constexpr uint32_t printFlags = SPV_BINARY_TO_TEXT_OPTION_INDENT | SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES | SPV_BINARY_TO_TEXT_OPTION_NESTED_INDENT;

            size_t instIdx = 0;
            size_t bbIdx = 0;
            size_t funcIdx = 0;
            for (const auto& f : m) {
                uint64_t funcExecuted = spvProfileInfo.funcExecuted[f.result_id()];
                double funcExecutedPercent = 100.0 * double(funcExecuted) / double(spvProfileInfo.totalFuncExecuted);
                QString funcDesc = "Function #" + QString::number(funcIdx++) + QString(", id = %1").arg(f.result_id());

                QColor funcColor = funcColorAvg;
                if (funcExecutedPercent < funcExecutedPercentAvg) {
                    double t = funcExecutedPercent / funcExecutedPercentAvg;
                    funcColor = Lerp(funcColorMin, funcColorAvg, t);
                }
                else if (funcExecutedPercent > funcExecutedPercentAvg) {
                    double t = funcExecutedPercentAvg / funcExecutedPercent;
                    funcColor = Lerp(funcColorMax, funcColorAvg, t);
                }

                QList<QStandardItem*> funcRow = {
                    new QStandardItem(funcDesc),
                    new QStandardItem(),
                    new QStandardItem(QString("%1").arg(funcExecuted)),
                    new QStandardItem(QString("%1").arg(funcExecutedPercent) + "%"),
                };
                spirvRootNode->appendRow(funcRow);

                funcRow[0]->setData(1, Qt::UserRole + 3);
                funcRow[1]->setData(1, Qt::UserRole + 3);
                funcRow[2]->setData(1, Qt::UserRole + 3);
                funcRow[3]->setData(1, Qt::UserRole + 3);

                funcRow[0]->setBackground(QBrush(funcColor));
                funcRow[1]->setBackground(QBrush(funcColor));
                funcRow[2]->setBackground(QBrush(funcColor));
                funcRow[3]->setBackground(QBrush(funcColor));

                QStandardItem *funcNode = funcRow[0];
                for (const auto& bb : f) {
                    uint64_t bbExecuted = spvProfileInfo.bbExecuted[bb.id()];
                    double bbExecutedPercent = 100.0 * double(bbExecuted) / double(spvProfileInfo.totalBBExecuted);
                    QString bbDesc = "Basic Block #" + QString::number(bbIdx++) + QString(", id = %1").arg(bb.id());

                    QColor bbColor = bbColorAvg;
                    if (bbExecutedPercent < bbExecutedPercentAvg) {
                        double t = bbExecutedPercent / bbExecutedPercentAvg;
                        bbColor = Lerp(bbColorMin, bbColorAvg, t);
                    }
                    else if (bbExecutedPercent > bbExecutedPercentAvg) {
                        double t = bbExecutedPercentAvg / bbExecutedPercent;
                        bbColor = Lerp(bbColorMax, bbColorAvg, t);
                    }

                    QList<QStandardItem*> bbRow = {
                        new QStandardItem(bbDesc),
                        new QStandardItem(),
                        new QStandardItem(QString("%1").arg(bbExecuted)),
                        new QStandardItem(QString("%1").arg(bbExecutedPercent) + "%"),
                    };
                    funcNode->appendRow(bbRow);

                    bbRow[0]->setData(1, Qt::UserRole + 3);
                    bbRow[1]->setData(1, Qt::UserRole + 3);
                    bbRow[2]->setData(1, Qt::UserRole + 3);
                    bbRow[3]->setData(1, Qt::UserRole + 3);

                    bbRow[0]->setBackground(QBrush(bbColor));
                    bbRow[1]->setBackground(QBrush(bbColor));
                    bbRow[2]->setBackground(QBrush(bbColor));
                    bbRow[3]->setBackground(QBrush(bbColor));

                    QStandardItem *bbNode = bbRow[0];
                    for (const auto& inst : bb) {
                        uint64_t instExecuted = spvProfileInfo.instExecuted[inst.unique_id()];
                        double instExecutedPercent = 100.0 * double(instExecuted) / double(spvProfileInfo.totalInstExecuted);
                        std::string instStr = inst.PrettyPrint(printFlags);
                        QString instDesc = QString::fromStdString(instStr);

                        QString lineStr;
                        auto instLineIt = spvProfileInfo.debugInfo.instLines.find(inst.unique_id());
                        if (instLineIt != spvProfileInfo.debugInfo.instLines.end()) {
                            auto instLine = instLineIt->second;
                            lineStr = QString("%1").arg(instLine);
                        }

                        QColor instColor = instColorAvg;
                        if (instExecutedPercent < instExecutedPercentAvg) {
                            double t = instExecutedPercent / instExecutedPercentAvg;
                            instColor = Lerp(instColorMin, instColorAvg, t);
                        }
                        else if (instExecutedPercent > instExecutedPercentAvg) {
                            double t = instExecutedPercentAvg / instExecutedPercent;
                            instColor = Lerp(instColorMax, instColorAvg, t);
                        }

                        QList<QStandardItem*> instRow = {
                            new QStandardItem(instDesc),
                            new QStandardItem(lineStr),
                            new QStandardItem(QString("%1").arg(instExecuted)),
                            new QStandardItem(QString("%1").arg(instExecutedPercent) + "%"),
                        };
                        bbNode->appendRow(instRow);

                        instRow[0]->setBackground(QBrush(instColor));
                        instRow[1]->setBackground(QBrush(instColor));
                        instRow[2]->setBackground(QBrush(instColor));
                        instRow[3]->setBackground(QBrush(instColor));
                    }
                }
            }

            for (int i = 0; i < spirvTreeHeaders.count(); ++i) {
                spirvView->resizeColumnToContents(i);
            }
        }
    }

    QModelIndex index = resourcesTreeModel->index(0, 0);
    ui->resourcesTreeView->setCurrentIndex(index);
    ui->resourcesTreeView->expand(index);

    for (int i = 0; i < resourcesTreeModel->columnCount(); ++i) {
        ui->resourcesTreeView->resizeColumnToContents(i);
    }
}

void ShaderProfilerWindow::fileOpenError(const QString &error)
{
    qDebug() << "Could not open file: " << fileName;
    QMessageBox::critical(this, tr("Error"), tr("Could not open file: %1: %2").arg(fileName).arg(error));
}

void ShaderProfilerWindow::currentResourceChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QVariant shaderMVCInfoVariant = current.data(Qt::UserRole + 2);
    if (!shaderMVCInfoVariant.isValid()) {
        ui->sourceStackedWidget->setCurrentIndex(0);
        ui->spirvStackedWidget->setCurrentIndex(0);
        ui->sourceStackedWidget->hide();
        ui->spirvStackedWidget->hide();
        return;
    }

    int shaderMVCInfoIndex = shaderMVCInfoVariant.toInt();
    const auto& shaderMVCInfo = shadersMVCInfos[shaderMVCInfoIndex];
    if (shaderMVCInfo.sourceView) {
        ui->sourceStackedWidget->setCurrentWidget(shaderMVCInfo.sourceView);
        ui->sourceStackedWidget->show();
    }
    else {
        ui->sourceStackedWidget->setCurrentIndex(1);
        ui->sourceStackedWidget->hide();
    }

    if (shaderMVCInfo.spirvView) {
        ui->spirvStackedWidget->setCurrentWidget(shaderMVCInfo.spirvView);
        ui->spirvStackedWidget->show();
    }
    else {
        ui->spirvStackedWidget->setCurrentIndex(1);
        ui->spirvStackedWidget->hide();
    }
}

ShaderProfilerStatusBar::ShaderProfilerStatusBar(QWidget *parent)
    : QWidget(parent)
    , layout(new QVBoxLayout(this))
    , label(new QLabel(this))
{
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);

    layout->addWidget(label);
}

ShaderProfilerStatusBar::~ShaderProfilerStatusBar()
{
    delete layout;
}
