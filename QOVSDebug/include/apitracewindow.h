#ifndef APITRACEWINDOW_H
#define APITRACEWINDOW_H

#include <QWidget>
#include <QString>
#include <QVector>
#include <QStandardItemModel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QLabel>

#include <VulkanLayer.h>
#include <SignatureGenerated.h>

class APITraceStatusBar;

namespace Ui {
class APITraceWindow;
}

class APITraceWindow : public QWidget
{
    Q_OBJECT

public:
    explicit APITraceWindow(QWidget *parent = nullptr);
    ~APITraceWindow();

    void openFile(const QString &fname);

    inline APITraceStatusBar *getStatusBarWidget() const { return statusBar; }

private slots:
    void fileDataUpdated(QVector<OVS::SignatureSerializer::SignatureSharedPtr> sigPtrs, QVector<QString> sigStrs);
    void fileOpenFinished();
    void fileOpenError(const QString &error);

    void currentSignatureChanged(const QModelIndex &current, const QModelIndex &previous);

private:
    void buildTreeModel(const OVS::SignatureSerializer::ParamNode &paramNode, QStandardItem &modelNode) const;

    Ui::APITraceWindow *ui;

    QString fileName;
    QVector<OVS::SignatureSerializer::SignatureSharedPtr> signatures;
    QStandardItemModel *signaturesModel;
    QStandardItemModel *treeModel;
    APITraceStatusBar *statusBar;
};

class APITraceStatusBar : public QWidget {
    Q_OBJECT

public:
    explicit APITraceStatusBar(QWidget *parent = nullptr);
    ~APITraceStatusBar();

    inline QProgressBar *getProgressBar() const { return progressBar; }
    inline QLabel *getLabel() const { return label; }

public slots:
    inline void setSignatureCount(int m) {
        progressBar->setMaximum(m);
        label->setText(QString::number(m) + " Signatures Total");
    }

    inline void setSignatureProcessed(int v) { progressBar->setValue(v); }
    inline void showProgressBar() { progressBar->show(); }
    inline void hideProgressBar() { progressBar->hide(); }
    inline void showLabel() { label->show(); }
    inline void hideLabel() { label->hide(); }

private:
    QVBoxLayout *layout;
    QProgressBar *progressBar;
    QLabel *label;
};

class APITraceFileWorker : public QObject
{
    Q_OBJECT

public:
    explicit APITraceFileWorker(const QString &fname) : fileName(fname) {}

public slots:
    void openFile();

signals:
    void fileDataUpdated(QVector<OVS::SignatureSerializer::SignatureSharedPtr> sigPtrs, QVector<QString> sigStrs);
    void fileOpenFinished();
    void fileOpenError(const QString &error);
    void progressBarMaximumUpdated(int m);
    void progressBarValueUpdated(int v);

private:
    QString fileName;
};

#endif // APITRACEWINDOW_H
