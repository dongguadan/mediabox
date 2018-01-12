#ifndef MEDIABOX_H
#define MEDIABOX_H

#include "FMFTNet.h"

#include <QtWidgets/QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QToolBar>

#include "CTransferWidget.h"
#include "CTranscodingWidget.h"

class MediaBox : public QMainWindow
{
	Q_OBJECT

public:
	MediaBox(QWidget *parent = 0);
	~MediaBox();

private:
	FMFTNet            *m_fmft_net;
	CTransferWidget    *m_transfer_widget;
	CTranscodingWidget *m_transcoding_widget;
private:
	QStackedWidget *_stackWdg;
	QPushButton    *_switchButton;
	QToolBar       *_toolBar;

private slots:
    void slotTransfer();
};

#endif // MEDIABOX_H
