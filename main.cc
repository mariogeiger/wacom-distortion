#include "widget.hh"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	Widget w;
	w.showFullScreen();

	return a.exec();
}
