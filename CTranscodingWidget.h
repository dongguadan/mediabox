#ifndef CTRANSCODINGWWDGET_H
#define CTRANSCODINGWWDGET_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>

#include <string>
using namespace std;

class CTranscodingWidget : public QWidget
{
	Q_OBJECT

public:
	CTranscodingWidget(QWidget *parent);
	~CTranscodingWidget();

public:
	string GetName();

private:
	string   m_name;
	QLabel   *m_noticeLab;
	
};

#endif // CTRANSCODINGWWDGET_H
