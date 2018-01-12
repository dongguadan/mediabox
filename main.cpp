#include "mediabox.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MediaBox w;
	w.show();
	return a.exec();
}
