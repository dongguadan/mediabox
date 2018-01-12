#ifndef CTRANSFERWIDGET_H
#define CTRANSFERWIDGET_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>

#include <string>
using namespace std;

class CTransferWidget : public QWidget
{
	Q_OBJECT

public:
	CTransferWidget(QWidget *parent);
	~CTransferWidget();

public:
	string GetName();

private:
	string   m_name;

	QLabel   *m_noticeLab;
	QWidget *_middleWdg;
	QWidget *_bottomWdg;
	QFrame  *_frame;
};

#endif // CTRANSFERWIDGET_H
