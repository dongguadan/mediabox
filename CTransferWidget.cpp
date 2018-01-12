#include "CTransferWidget.h"

#include <QApplication>
#include <QDesktopWidget>

CTransferWidget::CTransferWidget(QWidget *parent)
	: QWidget(parent)
{
	QDesktopWidget *desktopWidget = QApplication::desktop();

	double heightScle = 0.667;

	int hight = desktopWidget->height() * heightScle;
	int width = (hight * 1.0) * 340 / 600;
	setFixedSize(width, hight);

	m_name = "CTransferWidget";


	//bottom
	m_noticeLab = new QLabel(this);
	m_noticeLab->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	m_noticeLab->setText(m_name.c_str());
	m_noticeLab->setStyleSheet(QString("font-family:'PingFangSC-Regular';color:#505229;"));
	QFont noticeLabFont = m_noticeLab->font();
	noticeLabFont.setWeight(QFont::DemiBold);
	m_noticeLab->setFont(noticeLabFont);

	QWidget *leftMargin = new QWidget(this);
	QWidget *rightMargin = new QWidget(this);

	QHBoxLayout *bootomLayout = new QHBoxLayout;
	bootomLayout->addWidget(leftMargin, 67);
	bootomLayout->addWidget(m_noticeLab, 206);
	bootomLayout->addWidget(leftMargin, 67);



	//middle
	_middleWdg = new QWidget(this);
	_middleWdg->setStyleSheet("background-color:#EEEEEE;");





	//top
	QWidget *topWdg = new QWidget(this);
	QWidget *botWdg = new QWidget(this);
	_frame = new QFrame(this);
	_frame->setStyleSheet(QString("QFrame{border-image:url(%1);}").arg(QString(":/Resources/Resources/%1").arg(QString("upload_normal_x%1.png").arg(2))));


	QVBoxLayout *dropLayout = new QVBoxLayout;
	dropLayout->addWidget(topWdg, 27);
	dropLayout->addWidget(_frame, 110);
	dropLayout->addWidget(botWdg, 20);

	QWidget *leftWdg = new QWidget(this);
	QWidget *rightWdg = new QWidget(this);

	QHBoxLayout *topLayout = new QHBoxLayout;
	topLayout->addWidget(leftWdg, 67);
	topLayout->addLayout(dropLayout, 206);
	topLayout->addWidget(rightWdg, 67);






	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(topLayout, 183);
	mainLayout->addWidget(_middleWdg, 6);
	mainLayout->addLayout(bootomLayout, 363);
	mainLayout->setMargin(0);
	setLayout(mainLayout);

}

CTransferWidget::~CTransferWidget()
{

}

string CTransferWidget::GetName()
{
	return m_name;
}