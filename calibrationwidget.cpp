#include "calibrationwidget.h"
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QMessageBox>
#include <QProcess>
#include <QTextStream>

extern "C" {
#include "lmath.h"
}

CalibrationWidget::CalibrationWidget(const QString& dev, QWidget *parent) : QWidget(parent)
{
	m_device = dev;

	QList<QScreen*> screens = QGuiApplication::screens();
	if (screens.isEmpty()) {
		m_w = m_h = -1;
	} else {
		m_w = screens.first()->size().width();
		m_h = screens.first()->size().height();
	}

	clearAll();
	m_borliMode = true;
	m_curveMode = false;
	m_state = 0;

	QPalette pal = palette();
	pal.setColor(QPalette::Window, Qt::white);
	setPalette(pal);
	setCursor(QCursor(Qt::CrossCursor));
	setMouseTracking(true);
	setWindowState(windowState() | Qt::WindowFullScreen);

	nextStep();
}

CalibrationWidget::~CalibrationWidget()
{

}

void CalibrationWidget::mousePressEvent(QMouseEvent* event)
{
	bool grab = false;
	for (BorderLimit& elem : m_borderLimits) {
		if (elem.state == 1) {
			elem.state = 2;
			grab = true;
		}
	}

	if (grab) {
		setCursor(QCursor(Qt::ClosedHandCursor));
		update();
		return;
	}

	if (m_curveMode) {
		if (event->buttons() & Qt::LeftButton) {
			Curve c;
			c.border = -1;
			c.pts.append(event->globalPos());
			m_curves.append(c);
		}
	} else {
		if (m_raw_points.size() == m_phy_points.size()) {
			m_phy_points << event->globalPos();
			setCursor(QCursor(Qt::BlankCursor));

			m_text = "Now tap the more precisely in the center of the circle";
		} else if (m_raw_points.size() < m_phy_points.size()) {
			m_raw_points << event->globalPos();
			setCursor(QCursor(Qt::CrossCursor));

			m_text = "Add other control points or press Enter if you think you have enough points";
		}
	}
	update();
}

void CalibrationWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (m_phy_points.size() != m_raw_points.size()) return;

	bool grab = false;
	if (m_borliMode) {
		for (BorderLimit& elem : m_borderLimits) {
			if (elem.state == 2) {
				elem.move(elem.horizontal ? event->globalY() : event->globalX());
				grab = true;
			}
		}
		if (grab) {
			fitCurves();
			update();
			return;
		}
	}

	if (m_curveMode) {
		if (event->buttons() & Qt::LeftButton) {
			m_curves.last().pts.append(event->globalPos());
			if (m_borliMode) fitCurves();
			update();
			return;
		}
	}

	if (m_borliMode) {
		bool ungrab = false;
		for (BorderLimit& elem : m_borderLimits) {
			if (std::abs((elem.horizontal ? event->globalY() : event->globalX()) - elem.pos) <= 5) {
				elem.state = 1;
				grab = true;
			} else if (elem.state == 1) {
				elem.state = 0;
				ungrab = true;
			}
		}
		if (grab) setCursor(QCursor(Qt::OpenHandCursor));
		if (ungrab) setCursor(QCursor(Qt::CrossCursor));
	}
	update();
}

void CalibrationWidget::mouseReleaseEvent(QMouseEvent* event)
{
	Q_UNUSED(event);

	for (BorderLimit& elem : m_borderLimits) {
		if (elem.state == 2) {
			elem.state = 1;
			setCursor(QCursor(Qt::OpenHandCursor));
		}
	}

	if (m_curveMode) {
		if (!m_curves.isEmpty() && m_curves.last().pts.size() <= 1) m_curves.removeLast();
	}

	update();
}

void CalibrationWidget::paintEvent(QPaintEvent* event)
{
	Q_UNUSED(event);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	painter.setPen(Qt::darkGray);
	painter.drawText(rect(), Qt::AlignCenter, m_text);

	painter.translate(mapFromGlobal(QPoint(0,0)));

	if (m_borliMode) {
		for (BorderLimit& elem : m_borderLimits) elem.paint(&painter, m_w, m_h);
	}

	for (int i = 0; i < m_raw_points.size(); ++i) {
		painter.setPen(Qt::black);
		painter.drawLine(m_raw_points[i], m_phy_points[i]);
		painter.setPen(QPen(Qt::red, 2.5));
		painter.drawPoint(m_phy_points[i]);
		painter.setPen(QPen(Qt::blue, 2.5));
		painter.drawPoint(m_raw_points[i]);
	}

	painter.setPen(Qt::red);
	if (m_phy_points.size() > m_raw_points.size()) {
		QPointF p = m_phy_points.last();

		painter.setPen(QPen(Qt::red, 2.5));
		painter.drawPoint(p);

		QRectF r;
		r.setSize(QSizeF(10, 10));
		r.moveCenter(p);
		painter.setPen(QPen(Qt::blue, 1.5));
		painter.drawEllipse(r);
	}

	for (int i = 0; i < m_curves.size(); ++i) {
		const Curve& c = m_curves[i];
		if (c.pts.isEmpty()) continue;

		QPainterPath path_curve;
		path_curve.moveTo(c.pts[0]);
		for (int j = 1; j < c.pts.size(); ++j) {
			path_curve.lineTo(c.pts[j]);
		}
		painter.setPen(Qt::black);
		painter.drawPath(path_curve);

		if (m_borliMode && c.border != -1) {
			painter.setPen(Qt::blue);
			painter.setRenderHint(QPainter::Antialiasing, false);

			double x1, y1, x2, y2;
			if (c.border % 2 == 0) {
				y1 = path_curve.boundingRect().top();
				x1 = unitToPixel(c.border, c.ab[0] * y1 + c.ab[1]);
				y2 = path_curve.boundingRect().bottom();
				x2 = unitToPixel(c.border, c.ab[0] * y2 + c.ab[1]);
			} else {
				x1 = path_curve.boundingRect().left();
				y1 = unitToPixel(c.border, c.ab[0] * x1 + c.ab[1]);
				x2 = path_curve.boundingRect().right();
				y2 = unitToPixel(c.border, c.ab[0] * x2 + c.ab[1]);
			}
			painter.drawLine(x1, y1, x2, y2);

			QPainterPath path_corrected;
			for (int j = 0; j < c.pts.size(); ++j) {
				if (isInBorder(c.border, c.pts[j])) {
					double raw = pixelToUnit(c.border, xy(c.border, c.pts[j]));
					double phy = polynomial_evaluate(5, c.poly, raw);
					double x, y;
					if (c.border % 2 == 0) {
						x = unitToPixel(c.border, phy);
						y = c.pts[j].y();
					} else {
						x = c.pts[j].x();
						y = unitToPixel(c.border, phy);
					}
					if (path_corrected.elementCount() == 0)
						path_corrected.moveTo(x, y);
					else
						path_corrected.lineTo(x, y);
				}
			}

			painter.setPen(Qt::red);
			painter.drawPath(path_corrected);
		}
	}
}

void CalibrationWidget::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Delete) {
		clearAll();
		update();
	}
	if (event->key() == Qt::Key_Backspace) {
		if (m_phy_points.size() > 0) {
			if (m_phy_points.size() == m_raw_points.size()) {
				m_raw_points.removeLast();
				setCursor(QCursor(Qt::BlankCursor));
			} else {
				m_phy_points.removeLast();
				setCursor(QCursor(Qt::CrossCursor));
			}
		}
		if (m_curves.size() > 0) m_curves.removeLast();
		update();
	}
	if (event->key() == Qt::Key_Escape) {
		close();
	}
	if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
		nextStep();
	}
}

void CalibrationWidget::fitCurves()
{
	for (int i = 0; i < m_curves.size(); ++i) {
		Curve& c = m_curves[i];
		if (c.pts.size() <= 3) continue;
		int nb[4] = {0, 0, 0, 0};
		for (int j = 0;j < c.pts.size(); ++j) {
			if (c.pts[j].x() < m_borderLimits[TopX].pos) nb[TopX]++;
			if (c.pts[j].y() < m_borderLimits[TopY].pos) nb[TopY]++;
			if (c.pts[j].x() > m_borderLimits[BottomX].pos) nb[BottomX]++;
			if (c.pts[j].y() > m_borderLimits[BottomY].pos) nb[BottomY]++;
		}
		c.border = -1;
		for (int border = 0; border < 4; ++border) {
			if (nb[border] > 0) {
				int other;
				for (other = border+1; other < 4; ++other) if (nb[other] > 0) break;
				if (other == 4) c.border = border;
				break;
			}
		}

		if (c.border != -1) {
			QVector<double> a, arhs;
			for (int j = 0; j < c.pts.size(); ++j) {
				if (!isInBorder(c.border, c.pts[j])) {
					a << yx(c.border, c.pts[j]) << 1.0;
					arhs << pixelToUnit(c.border, xy(c.border, c.pts[j])); // phy
				}
			}
			if (arhs.isEmpty()) {
				c.border = -1;
			} else {
				least_squares(arhs.size(), 2, a.data(), arhs.data(), c.ab);

				a.clear();
				arhs.clear();
				for (int j = 0; j < c.pts.size(); ++j) {
					if (isInBorder(c.border, c.pts[j])) {
						double raw = pixelToUnit(c.border, xy(c.border, c.pts[j]));
						double phy = c.ab[0]*yx(c.border, c.pts[j]) + c.ab[1];
						a << pow(raw, 4.) << pow(raw, 3.) << pow(raw, 2.) << raw << 1.0;
						arhs << phy;
					}
				}
				double d = pixelToUnit(c.border, m_borderLimits[c.border].pos);
				double cons[10] = {
					4.*d*d*d,      3.*d*d,      2.*d,      1.0,   0.0,
					d*d*d*d,       d*d*d,       d*d,       d,     1.0
				};
				double crhs[2] = {
					1.0,
					d
				};
				least_squares_constraint(arhs.size(), 5, 2, a.data(), arhs.data(), cons, crhs, c.poly);
			}
		}
	}
}

void CalibrationWidget::clearAll()
{
	m_borderLimits[TopX].pos = 0.1 * m_w;
	m_borderLimits[TopX].limit = 0.4 * m_w;
	m_borderLimits[TopX].horizontal = false;
	m_borderLimits[TopX].state = 0;

	m_borderLimits[TopY].pos = 0.1 * m_h;
	m_borderLimits[TopY].limit = 0.4 * m_h;
	m_borderLimits[TopY].horizontal = true;
	m_borderLimits[TopY].state = 0;

	m_borderLimits[BottomX].pos = 0.9 * m_w;
	m_borderLimits[BottomX].limit = 0.6 * m_w;
	m_borderLimits[BottomX].horizontal = false;
	m_borderLimits[BottomX].state = 0;

	m_borderLimits[BottomY].pos = 0.9 * m_h;
	m_borderLimits[BottomY].limit = 0.6 * m_h;
	m_borderLimits[BottomY].horizontal = true;
	m_borderLimits[BottomY].state = 0;

	m_phy_points.clear();
	m_raw_points.clear();
	setCursor(QCursor(Qt::CrossCursor));

	m_curves.clear();
}

void fix_area(double slope, double offset, double range, double old_min, double old_max, int& new_min, int& new_max)
{
	new_min = std::round(old_min - (old_max - old_min) * offset / (slope * range));
	new_max = std::round((old_max - old_min) / slope) + new_min;
}

void CalibrationWidget::nextStep()
{
	QTextStream cout(stdout);
	QString command;
	QProcess pro;

	if (m_state == 0) {
		// distortion to identity
		command = "xinput set-float-prop \"%1\" \"Wacom Border Distortion\" 0 0 0 0 1 0   0 0 0 0 1 0   0 0 0 0 1 0   0 0 0 0 1 0";
		command = command.arg(m_device);
		cout << "> " << command << endl;
		pro.start(command); pro.waitForFinished();
		cout << pro.readAllStandardOutput();
		cout << pro.readAllStandardError();

		if (pro.exitCode() != 0) {
			pro.start("xinput"); pro.waitForFinished();
			if (pro.exitCode() != 0) {
				cout << "You need to install xinput (sudo apt-get install xinput)" << endl;
			} else {
				cout << "\nThis is the list of your devices :\n" << pro.readAllStandardOutput();
				cout << "Execute this programm with the stylus device in argument\n" << endl;
			}
		}

		// get area
		command = "xinput list-props \"%1\"";
		command = command.arg(m_device);
		cout << "> " << command << endl;
		pro.start(command);
		pro.waitForFinished();
		cout << pro.readAllStandardOutput();
		cout << pro.readAllStandardError();

		m_area.clear();
		if (pro.exitCode() == 0) {
			QByteArray output = pro.readAllStandardOutput();
			int pos = output.indexOf("Wacom Tablet Area");
			if (pos != -1) {
				pos = output.indexOf(':', pos);
				pos++; // ignore the ':'
				int end = output.indexOf('\n', pos);
				output = output.mid(pos, end-pos);
				QString svalues(output);
				QStringList list = svalues.split(",", QString::SkipEmptyParts);
				if (list.size() == 4) {
					for (int i = 0; i < list.size(); ++i) {
						bool ok;
						m_area << list[i].trimmed().toInt(&ok);
						if (!ok) {
							m_area.clear();
							break;
						}
					}
				}
			}
		}
		if (m_area.isEmpty()) {
			cout << "Assume that Wacom Tablet Area is 0 0 10000 10000" << endl;
			m_area << 0 << 0 << 10000 << 10000;
		}

		m_borliMode = false;
		m_curveMode = false;
		m_text = "Linear calibration\n"
				 "Tap anywhere to add control point";
		m_state = 1;
	} else if (m_state == 1) {
		int new_area[4];
		QVector<double> a, arhs;
		for (int i = 0; i < m_raw_points.size(); ++i) {
			a << m_raw_points[i].x() << 1.0;
			arhs << m_phy_points[i].x();
		}
		double res[2];
		int r = least_squares(arhs.size(), 2, a.data(), arhs.data(), res);
		if (r != 0) {
			m_text = "Please, add more points or quit with Escape";
			update();
			return;
		}
		// phy = res[0] * raw + res[1]
		fix_area(res[0], res[1], m_w, m_area[0], m_area[2], new_area[0], new_area[2]);

		a.clear(); arhs.clear();
		for (int i = 0; i < m_raw_points.size(); ++i) {
			a << m_raw_points[i].y() << 1.0;
			arhs << m_phy_points[i].y();
		}
		r = least_squares(arhs.size(), 2, a.data(), arhs.data(), res);
		if (r != 0) {
			m_text = "More points please";
			update();
			return;
		}
		// phy = res[0] * raw + res[1]
		fix_area(res[0], res[1], m_h, m_area[1], m_area[3], new_area[1], new_area[3]);

		command = "xinput set-int-prop \"%1\" \"Wacom Tablet Area\" 32 %2 %3 %4 %5";
		command = command.arg(m_device).arg(new_area[0]).arg(new_area[1]).arg(new_area[2]).arg(new_area[3]);
		cout << "> " << command << endl;
		pro.start(command); pro.waitForFinished();
		cout << pro.readAllStandardOutput();
		cout << pro.readAllStandardError();

		m_borliMode = true;
		m_curveMode = true;
		m_text = "Distortion calibration\nPress Enter when finish";
		m_state = 2;
		clearAll();
		update();
	} else if (m_state == 2) {

		QVector<double> values;
		for (int i = 0; i < 4; ++i) {
			values << 0.0 << 0.0 << 0.0 << 0.0 << 1.0 << 0.0;
		}

		for (int border = 0; border < 4; ++border) {
			// take only the last curve
			for (int j = 0; j < m_curves.size(); ++j) {
				if (m_curves[j].border == border) {
					values[6 * border] = pixelToUnit(border, m_borderLimits[border].pos);
					for (int i = 0; i < 5; ++ i) {
						values[6 * border + i + 1] = m_curves[j].poly[i];
					}
				}
			}
		}

		command = "xinput set-float-prop \"%1\" \"Wacom Border Distortion\"";
		command = command.arg(m_device);
		for (int i = 0; i < values.size(); ++i) command += QString(" %1").arg(values[i]);
		cout << "> " << command << endl;
		pro.start(command); pro.waitForFinished();
		cout << pro.readAllStandardOutput();
		cout << pro.readAllStandardError();

		m_state = 3;
		m_borliMode = false;
		m_text = "Test the result";
		clearAll();
		update();
	} else if (m_state == 3) {
		close();
	}
}

void CalibrationWidget::BorderLimit::paint(QPainter* p, double w, double h)
{
	switch (state) {
		case 0:
			p->setPen(QPen(Qt::black, 1.0));
			break;
		case 1:
			p->setPen(QPen(Qt::red, 3.0));
			break;
		case 2:
			p->setPen(QPen(Qt::red, 1.0));
			break;
	}
	if (horizontal) p->drawLine(0, pos, w, pos);
	else p->drawLine(pos, 0, pos, h);
}

void CalibrationWidget::BorderLimit::move(double new_pos)
{
	if (pos < limit && new_pos < limit) pos = new_pos;
	if (pos > limit && new_pos > limit) pos = new_pos;
}
