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
QVector<double> calibrate(CalibrationDialog* w);
QVector<int> readTabletArea(const QString& device);

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setApplicationName("wacom-distortion");

	QCommandLineParser parser;
	parser.setApplicationDescription("calibrate the wacom stylus to fix the distortion in the borders");
	parser.addHelpOption();
	parser.addPositionalArgument("device", "Name of the device as it appear in xinput");
	QCommandLineOption toleranceOption("tol",
									   "Tolerance in pixel to consider a point as perfect",
									   "tolerance", "2");
	parser.addOption(toleranceOption);
	QCommandLineOption skipLinearCalibration("skip-linear", "Skip the linear part of the calibration");
	parser.addOption(skipLinearCalibration);

	parser.process(app);
	QString device = parser.positionalArguments().value(0, "");
	bool ok;
	double tolerance = parser.value(toleranceOption).toDouble(&ok);
	if (!ok || tolerance < 0.0) tolerance = 5.0;


	if (device.isEmpty()) {
		cout << "No device given in argument" << endl;
	}

	QVector<int> area;
	if (!device.isEmpty()) {
		QString command = QString(
					"xinput set-float-prop \"%1\" \"Wacom Border Distortion\" "
					"0 0 0 1 0   0 0 0 1 0   0 0 0 1 0   0 0 0 1 0").arg(device);
		cout << command << endl;
		int r = QProcess::execute(command);
		if (r != 0) {
			cout << "Error, cannot set the distortion parameters to identity. It will quit." << endl;
			return 1;
		}

		area = readTabletArea(device);
		if (area.isEmpty())	{
			cout << "Error, cannot read Wacom Tablet Area parameter. It will quit." << endl;
			return 1;
		}
	} else {
		cout << "Considere Wacom Tablet Area to be 0 0 10000 10000" << endl;
		area << 0 << 0 << 10000 << 10000;
	}

	CalibrationDialog w;

	if (!parser.isSet(skipLinearCalibration)) {
		int r = linearCalibration(device, &w, area);
		if (r != 0) {
			cout << "Quit..." << endl;
			return 1;
		}
		w.clearAll();
	}

	w.setText("Please add as much control points as you want in the borders.\n"
			  "The lines determines which part of the border must be corrected.\n"
			  "Grab and release the lines by clicking on it.\n"
			  "The F key switches the window in fullscreen mode.\n"
			  "You can remove the last point with backspace.\n"
			  "The key Delete resets all the points and borders.\n"
			  "Please press Enter when you are finished.");
	w.setCreateBorders(true);
	w.setTolerance(tolerance);

	if (w.exec() == QDialog::Accepted) {
		QVector<double> values = calibrate(&w);

		QString command = "xinput set-float-prop ";
		command += device.isEmpty() ? "<device>" : "\""+device+"\"";
		command += " \"Wacom Border Distortion\"";
		for (int i = 0; i < values.size(); ++i) command += QString(" %1").arg(values[i]);

		if (device.isEmpty()) {
			cout << "You have to execute :\n" << command << endl;
		} else {
			cout << command << endl;
			int r = QProcess::execute(command);
			if (r != 0) {
				cout << "Error, cannot set Wacom Border Distortion parameters. Quit..." << endl;
				return 1;
			}
		}
	} else {
		cout << "Distortion calibration skipped" << endl;
	}

	cout << "Finish successfully" << endl;
	return 0;
}

void fix_area(double slope, double offset, double range, double old_min, double old_max, int* new_min, int* new_max)
{
	*new_min = std::round(old_min - (old_max - old_min) * offset / (slope * range));
	*new_max = std::round((old_max - old_min) / slope) + *new_min;
}

int linearCalibration(const QString& device, CalibrationDialog* w, const QVector<int>& area)
{
	w->setText("Please add contol points to calibrate the Center of the screen.\n"
			  "Don't add control points too close to the borders. The central calibration must be linear.\n"
			  "Please press [enter] when you are finished.");
	w->setCreateBorders(false);

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
			cout << command << endl;
			r = QProcess::execute(command);
			if (r != 0) {
				cout << "Error, cannot set the Wacom Tablet Area parameters" << endl;
				return 1;
			}
		}
	} else {
		cout << "Linear calibration skipped" << endl;
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

	qDebug("xinput return %d", p.exitCode());

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

QVector<double> find_polynomial(double d, QVector<double> raw, QVector<double> phy)
{
	QVector<double> a, arhs;
	a.reserve(4 * raw.size());
	arhs.reserve(raw.size());

	for (int i = 0; i < raw.size(); ++i) {
		a << pow(raw[i], 3.) << pow(raw[i], 2.) << raw[i] << 1.0;
		arhs << phy[i];
	}

	const double constraints[4] = {
		pow(d, 3), pow(d, 2), d, 1
	};
	const double crhs[1] = {
		d
	};
	QVector<double> poly;
	poly.resize(4);

	int r = least_squares_constraint(4, arhs.size(), 1, a.data(), arhs.data(), constraints, crhs, poly.data());
	if (r != 0) poly.resize(0);
	return poly;
}

QVector<double> calibrate(CalibrationDialog* w)
{
	QVector<double> values;
	for (int i = 0; i < 4; ++i) {
		values << 0.0 << 0.0 << 0.0 << 1.0 << 0.0;
	}

	if (w->getBorderTopX() > 0) {
		QVector<double> raw, phy;
		for (int i = 0; i < w->getRawPoints().size(); ++i) {
			if (w->getRawPoints()[i].x() < w->getBorderTopX()) {
				raw << w->getRawPoints()[i].x()      / w->getScreenWidth();
				phy << w->getPhysicalPoints()[i].x() / w->getScreenWidth();
			}
		}
		double d = w->getBorderTopX() / w->getScreenWidth();
		QVector<double> poly = find_polynomial(d, raw, phy);

		if (poly.size() == 4) {
			values[0] = d;
			for (int i = 0; i < 4; ++i) values[1+i] = poly[i];
		}
	}

	if (w->getBorderTopY() > 0) {
		QVector<double> raw, phy;
		for (int i = 0; i < w->getRawPoints().size(); ++i) {
			if (w->getRawPoints()[i].y() < w->getBorderTopY()) {
				raw << w->getRawPoints()[i].y() / w->getScreenHeight();
				phy << w->getPhysicalPoints()[i].y() / w->getScreenHeight();
			}
		}
		double d = w->getBorderTopY() / w->getScreenHeight();
		QVector<double> poly = find_polynomial(d, raw, phy);

		if (poly.size() == 4) {
			values[5] = d;
			for (int i = 0; i < 4; ++i) values[6+i] = poly[i];
		}
	}

	if (w->getBorderBottomX() > 0) {
		QVector<double> raw, phy;
		for (int i = 0; i < w->getRawPoints().size(); ++i) {
			if (w->getRawPoints()[i].x() > w->getBorderBottomX()) {
				raw << 1.0 - w->getRawPoints()[i].x() / w->getScreenWidth();
				phy << 1.0 - w->getPhysicalPoints()[i].x() / w->getScreenWidth();
			}
		}
		double d = 1.0 - w->getBorderBottomX() / w->getScreenWidth();
		QVector<double> poly = find_polynomial(d, raw, phy);

		if (poly.size() == 4) {
			values[10] = d;
			for (int i = 0; i < 4; ++i) values[11+i] = poly[i];
		}
	}

	if (w->getBorderBottomY() > 0) {
		QVector<double> raw, phy;
		for (int i = 0; i < w->getRawPoints().size(); ++i) {
			if (w->getRawPoints()[i].y() > w->getBorderBottomY()) {
				raw << 1.0 - w->getRawPoints()[i].y() / w->getScreenHeight();
				phy << 1.0 - w->getPhysicalPoints()[i].y() / w->getScreenHeight();
			}
		}
		double d = 1.0 - w->getBorderBottomY() / w->getScreenHeight();
		QVector<double> poly = find_polynomial(d, raw, phy);

		if (poly.size() == 4) {
			values[15] = d;
			for (int i = 0; i < 4; ++i) values[16+i] = poly[i];
		}
	}

	return values;
}
