#include "mediabox.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPaintEvent>
#include <QPainter>
#include <QtMath>

#define TOTALSIZE (1024*1024*1024*10)  //(((1BYTE * 1024)K * 1024)MB * 1024)GB * num

#define RATE (1*1024*800)    //(1Kb/S * 1024)Mb/S * num 
#define MTU  (1*1400*1)    //(1BYTE * 1400)KB * num
#define REMOTE     "10.0.15.59"
#define SNIFFPORT 36109
#define REMOTEPORT 36108
#define LOCALPORT  36201
 
#define CENTRALTYPE_TRANSFER    0
#define CENTRALTYPE_TRANSCODING 1

MediaBox::MediaBox(QWidget *parent)
	: QMainWindow(parent)
{
	m_transfer_widget = new CTransferWidget(this);
	if (m_transfer_widget == NULL)
	{
		OutputDebugStringA("MediaBox CTransferWidget error\n");
		return;
	}

	m_transcoding_widget = new CTranscodingWidget(this);
	if (m_transcoding_widget == NULL)
	{
		OutputDebugStringA("MediaBox m_transcoding_widget error\n");
		return;
	}

	_stackWdg = new QStackedWidget(this);
	_stackWdg->insertWidget(CENTRALTYPE_TRANSFER, m_transfer_widget);
	_stackWdg->insertWidget(CENTRALTYPE_TRANSCODING, m_transcoding_widget);
	_stackWdg->setContentsMargins(0, 0, 0, 0);
	_stackWdg->setStyleSheet("QStackedWidget{border:none;}");
	setCentralWidget(_stackWdg);
	_stackWdg->setCurrentIndex(CENTRALTYPE_TRANSFER);



	_switchButton = new QPushButton(this);
	_switchButton->setText("send");
	_switchButton->setStyleSheet("border:none;");
	connect(_switchButton, SIGNAL(clicked()), this, SLOT(slotTransfer()));

	QHBoxLayout *headLayout = new QHBoxLayout;
	headLayout->addWidget(_switchButton, 0);
	QWidget *headWdg = new QWidget(this);
	headWdg->setLayout(headLayout);
	headWdg->setContentsMargins(0, 0, 0, 0);

	_toolBar = addToolBar("HeadToolBar");
	_toolBar->setStyleSheet("background-color:#FFFFFF");
	_toolBar->setMovable(false);
	_toolBar->addWidget(headWdg);
	_toolBar->setContentsMargins(0, 0, 0, 0);
	m_fmft_net = NULL;

	return;

}

MediaBox::~MediaBox()
{
	delete m_fmft_net;
	m_fmft_net = NULL;
}


void MediaBox::slotTransfer()
{
	if (m_fmft_net != NULL)
	{
		m_fmft_net->Stop();
		delete m_fmft_net;
		m_fmft_net = NULL;
		return;
	}

	m_fmft_net = new FMFTNet();
	if (m_fmft_net == NULL)
	{
		OutputDebugStringA("new FMFTNet() error\n");
		return;
	}
	m_fmft_net->Init(TOTALSIZE, RATE, MTU);
	m_fmft_net->Start(REMOTE, REMOTEPORT, LOCALPORT);

	return;
}
