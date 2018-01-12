#include "CTranscodingWidget.h"

CTranscodingWidget::CTranscodingWidget(QWidget *parent)
	: QWidget(parent)
{
	m_name = "CTranscodingWidget";

	m_noticeLab = new QLabel(this);
	m_noticeLab->setText(m_name.c_str());
	m_noticeLab->setStyleSheet(QString("font-family:'PingFangSC-Regular';color:#505229;"));
	QFont noticeLabFont = m_noticeLab->font();
	noticeLabFont.setWeight(QFont::DemiBold);
	m_noticeLab->setFont(noticeLabFont);

	QHBoxLayout *headWLayout = new QHBoxLayout;
	headWLayout->addWidget(m_noticeLab);
	setLayout(headWLayout);
}

CTranscodingWidget::~CTranscodingWidget()
{

}

string CTranscodingWidget::GetName()
{
	return m_name;
}
