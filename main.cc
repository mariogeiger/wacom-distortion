#include "calibrationdialog.hh"
#include <QApplication>
#include <QProcess>

extern "C" {
#include "lmath.h"
}
#include <cmath>
#include <QTextStream>

QVector<double> calibrate(CalibrationDialog* w);
QVector<int> readTabletArea(const QString& device);

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	QTextStream cout(stdout);
	QString device = a.arguments().value(1, "");

	if (device.isEmpty()) {
		cout << "No device given in argument, run directly to distortion calibration" << endl;
	}

	QVector<int> area;
	if (!device.isEmpty()) {
		QString command = QString(
					"xinput set-float-prop \"%1\" \"Wacom Border Distortion\" "
					"0 0 0 1 0   0 0 0 1 0   0 0 0 1 0   0 0 0 1 0").arg(device);
		cout << "execute : " << command << endl;
		int r = QProcess::execute(command);
		cout << "returned " << r << endl;


		area = readTabletArea(device);
		if (area.isEmpty())	{
			cout << "Cannot read Wacom Tablet Area" << endl;
		}
	}

	CalibrationDialog w;

	if (!area.isEmpty()) {
		w.setText("Please add contol points to calibrate the Center of the screen.\n"
				  "Dont add control point to close to the borders. The central calibration must be linear.\n"
				  "Please press [enter] when you are finished");
		w.setCreateBorders(false);

		if (w.exec() == QDialog::Accepted) {
			QVector<double> rawX, rawY, phyX, phyY;
			const QVector<QPointF>& raw = w.getRawPoints();
			const QVector<QPointF>& phy = w.getPhysicalPoints();
			for (int i = 0; i < raw.size(); ++i) {
				rawX << raw[i].x();
				rawY << raw[i].y();
				phyX << phy[i].x();
				phyY << phy[i].y();
			}
			int new_area[4];
			QVector<double> a, arhs;
			for (int i = 0; i < raw.size(); ++i) {
				a << rawX[i] << 1.0;
				arhs << phyX[i];
			}
			double res[2];
			int r = least_squares(arhs.size(), 2, a.data(), arhs.data(), res);
			if (r != 0) {
				cout << "Cant apply least squares method. Not enough points ?" << endl;
				return 1;
			}
			// phy = res[0] * raw + res[1]
			new_area[0] = area[0] - round(res[1] * (area[2]-area[0]) / (double)(res[0] * w.getScreenWidth()));
			new_area[2] = round((area[2] - area[0]) / res[0] + area[0] - (res[1] * (area[2] - area[0])) / (res[0] * w.getScreenWidth()));

			a.clear(); arhs.clear();
			for (int i = 0; i < raw.size(); ++i) {
				a << rawY[i] << 1.0;
				arhs << phyY[i];
			}
			r = least_squares(arhs.size(), 2, a.data(), arhs.data(), res);
			if (r != 0) {
				cout << "Cant apply least squares method. Not enough points ?" << endl;
				return 1;
			}
			// phy = res[0] * raw + res[1]
			new_area[1] = area[1] - round(res[1] * (area[3]-area[1]) / (double)(res[0] * w.getScreenHeight()));
			new_area[3] = round((area[3] - area[1]) / res[0] + area[1] - (res[1] * (area[3] - area[1])) / (res[0] * w.getScreenHeight()));

			QString command = "xinput set-int-prop ";
			command += "\""+device+"\"";
			command += " \"Wacom Tablet Area\"";
			for (int i = 0; i < 4; ++i) command += QString(" %1").arg(new_area[i]);

			cout << "execute : " << command << endl;
			r = QProcess::execute(command);
			cout << "returned " << r << endl;
		} else {
			cout << "Aborted" << endl;
			return 1;
		}

		w.clearAll();
	}

	w.setText("Please click with the mouse to set the widths of the border distortion.\n"
			  "Then add as much as you want control points in the borders\n"
			  "The key F swich the window in fullscreen\n"
			  "You can remove the lase point with backspace.\n"
			  "The key Delete reset all the points and borders.\n"
			  "Please press Enter when you are finished.");
	w.setCreateBorders(true);

	if (w.exec() == QDialog::Accepted) {
		qDebug("dialog accepted");

		QVector<double> values = calibrate(&w);

		QString command = "xinput set-float-prop ";
		command += device.isEmpty() ? "<device>" : "\""+device+"\"";
		command += " \"Wacom Border Distortion\"";
		for (int i = 0; i < values.size(); ++i) command += QString(" %1").arg(values[i]);

		if (device.isEmpty()) {
			cout << "You have to execute " << command << endl;
		} else {
			cout << "execute : " << command << endl;
			int r = QProcess::execute(command);
			cout << "returned " << r << endl;
		}
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
