#include "calibrationdialog.hh"
#include <QApplication>
#include <QProcess>
#include <QCommandLineParser>
#include <QCommandLineOption>

extern "C" {
#include "lmath.h"
}
#include <cmath>
#include <QTextStream>

QTextStream cout(stdout);

int linearCalibration(const QString& device, CalibrationDialog* w, const QVector<int>& area);
QVector<double> borderCalibration(CalibrationDialog* w);
QVector<int> readTabletArea(const QString& device);

void fix_area(double slope, double offset, double range, double old_min, double old_max, int* new_min, int* new_max)
{
	*new_min = std::round(old_min - (old_max - old_min) * offset / (slope * range));
	*new_max = std::round((old_max - old_min) / slope) + *new_min;
}

int linearCalibration(const QString& device, CalibrationDialog* w, const QVector<int>& area)
{
	w->setText("Please add contol points to calibrate the Center of the screen.\n"
			   "Don't add control points too close to the borders.\n"
			   "The [F] key switches the window in fullscreen mode.\n"
			   "You can remove the last point with [backspace].\n"
			   "The [delete] key resets all.\n"
			   "Please press [enter] when you are finished.");
	w->setBorders(false);

	if (w->exec() == QDialog::Accepted) {
		const QVector<QPointF>& raw = w->getRawPoints();
		const QVector<QPointF>& phy = w->getPhysicalPoints();

		int new_area[4];
		QVector<double> a, arhs;
		for (int i = 0; i < raw.size(); ++i) {
			a << raw[i].x() << 1.0;
			arhs << phy[i].x();
		}
		double res[2];
		int r = least_squares(arhs.size(), 2, a.data(), arhs.data(), res);
		if (r != 0) {
			cout << "Cant apply least squares method. Not enough points ?" << endl;
			return 1;
		}
		// phy = res[0] * raw + res[1]
		fix_area(res[0], res[1], w->getScreenWidth(), area[0], area[2], &new_area[0], &new_area[2]);

		a.clear(); arhs.clear();
		for (int i = 0; i < raw.size(); ++i) {
			a << raw[i].y() << 1.0;
			arhs << phy[i].y();
		}
		r = least_squares(arhs.size(), 2, a.data(), arhs.data(), res);
		if (r != 0) {
			cout << "Cant apply least squares method. Not enough points ?" << endl;
			return 1;
		}
		// phy = res[0] * raw + res[1]
		fix_area(res[0], res[1], w->getScreenHeight(), area[1], area[3], &new_area[1], &new_area[3]);

		QString command = "xinput set-int-prop \"";
		command += device.isEmpty() ? "<device>" : device;
		command += "\" \"Wacom Tablet Area\" 32 ";
		for (int i = 0; i < 4; ++i) command += QString(" %1").arg(new_area[i]);

		if (device.isEmpty()) {
			cout << "You have to execute :\n" << command << endl;
		} else {
			cout << "> " << command << endl;
			r = QProcess::execute(command);
			if (r != 0) {
				cout << "xinput returns " << r << endl;
				cout << "Error, cannot set the Wacom Tablet Area parameters" << endl;
				return 1;
			}
		}
	} else {
		cout << "Linear calibration skipped" << endl;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setApplicationName("wacom-distortion");

	QCommandLineParser parser;
	parser.setApplicationDescription("calibrate the wacom stylus to fix the distortion in the borders");
	parser.addHelpOption();
	parser.addPositionalArgument("device", "Name of the device as it appear in xinput");

	QCommandLineOption skipLinearCalibration("skip-linear", "Skip the linear part of the calibration");
	parser.addOption(skipLinearCalibration);

	//QCommandLineOption checkResult("check-result", "Check the result at the end");
	//parser.addOption(checkResult);

	parser.process(app);
	QString device = parser.positionalArguments().value(0, "");

	if (device.isEmpty()) {
		cout << "No device given in argument" << endl;
	}

	QVector<int> area;
	if (!device.isEmpty()) {
		QString command = QString(
					"xinput set-float-prop \"%1\" \"Wacom Border Distortion\" "
					"0 0 0 0 1 0   0 0 0 0 1 0   0 0 0 0 1 0   0 0 0 0 1 0").arg(device);
		cout << "> " << command << endl;
		int r = QProcess::execute(command);
		if (r != 0) {
			cout << "xinput returns " << r << endl;
			cout << "Error, cannot set the distortion parameters to identity." << endl;
		}

		area = readTabletArea(device);
		if (area.isEmpty())	{
			cout << "Error, cannot read Wacom Tablet Area parameter." << endl;
			cout << "Considere Wacom Tablet Area to be 0 0 10000 10000" << endl;
			area << 0 << 0 << 10000 << 10000;
		}
	} else {
		cout << "Considere Wacom Tablet Area to be 0 0 10000 10000" << endl;
		area << 0 << 0 << 10000 << 10000;
	}

	CalibrationDialog w;


	if (!parser.isSet(skipLinearCalibration)) {
		cout << endl << "LINEAR CALIBRATION PHASE" << endl;
		linearCalibration(device, &w, area);
		w.clearAll();
	}


	cout << endl << "BORDER DISTORTION CALIBRATION PHASE" << endl;

	w.setText("With a ruler make a curve for each border.\n"
			  "The lines determines which part of the border must be corrected.\n"
			  "Only the last curve for each border will be taken in account.\n"
			  "The [F] key switches the window in fullscreen mode.\n"
			  "You can remove the last cruve with [backspace].\n"
			  "The [delete] key resets all.\n"
			  "Please press [enter] when you are finished.");
	w.setBorders(true);
	w.setLineMode(true);

	if (w.exec() == QDialog::Accepted) {
		QVector<double> values = borderCalibration(&w);

		QString command = "xinput set-float-prop ";
		command += device.isEmpty() ? "<device>" : "\""+device+"\"";
		command += " \"Wacom Border Distortion\"";
		for (int i = 0; i < values.size(); ++i) command += QString(" %1").arg(values[i]);

		if (device.isEmpty()) {
			cout << "You have to execute :\n" << command << endl;
		} else {
			cout << "> " << command << endl;
			int r = QProcess::execute(command);
			if (r != 0) {
				cout << "Error, cannot set Wacom Border Distortion parameters." << endl;
			}
		}
	} else {
		cout << "Distortion calibration skipped" << endl;
	}

	if (true/*parser.isSet(checkResult)*/) {
		w.setText("Now you can test the result.");
		w.setBorders(false);
		w.clearAll();
		w.exec();
	}

	return 0;
}

QVector<int> readTabletArea(const QString& device)
{
	// run commande xinput list-props <device>
	QProcess p;
	p.setProgram("xinput");
	p.setArguments(QStringList() << "list-props" << device);
	p.start();
	p.waitForFinished();

	cout << QString("> xinput list-props \"%1\"").arg(device) << endl;

	if (p.exitCode() != 0) cout << "xinput returns " << p.exitCode() << endl;
	//qDebug("xinput return %d", p.exitCode());

	// parse output
	QByteArray output = p.readAllStandardOutput();
	int pos = output.indexOf("Wacom Tablet Area");
	if (pos == -1) return QVector<int>();
	pos = output.indexOf(':', pos);
	pos++; // ignore the ':'
	int end = output.indexOf('\n', pos);
	output = output.mid(pos, end-pos);
	QString svalues(output);
	QStringList list = svalues.split(",", QString::SkipEmptyParts);
	if (list.size() != 4) return QVector<int>();

	QVector<int> values;
	for (int i = 0; i < list.size(); ++i) {
		bool ok;
		values << list[i].trimmed().toInt(&ok);
		if (!ok) return QVector<int>();
	}
	return values;
}

QVector<double> borderCalibration(CalibrationDialog* w)
{
	QVector<double> values;
	for (int i = 0; i < 4; ++i) {
		values << 0.0 << 0.0 << 0.0 << 0.0 << 1.0 << 0.0;
	}

	for (int border = 0; border < 4; ++border) {
		if (border < 2)
			values[6 * border] = w->getBorderLimit(border) / w->getScreenWH(border);
		else
			values[6 * border] = 1.0 - w->getBorderLimit(border) / w->getScreenWH(border);

		for (int i = 0; i < 5; ++ i) {
			values[6 * border + i + 1] = w->getPolyCoeff(border, i);
		}
	}

	return values;
}
