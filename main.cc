#include "calibrationwidget.hh"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setApplicationName("wacom-distortion");

	CalibrationWidget w(app.arguments().value(1, "<Your device>"));
	w.show();
	w.nextStep();

	return app.exec();
}
