#include "calibrationwidget.hh"
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QProcess>
#include <QTextStream>
#include <QInputDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>

extern "C" {
#include "lmath.h"
}

CalibrationWidget::CalibrationWidget(const QString& dev, QWidget *parent) : QWidget(parent)
{
	m_device = dev;

	m_screen = QGuiApplication::screens().value(0, nullptr);
	if (m_screen) {
		m_w = m_screen->size().width();
		m_h = m_screen->size().height();
		connect(m_screen, &QScreen::geometryChanged, this, &CalibrationWidget::screenChanged);
	}

	clearAll();
	m_borliMode = true;
	m_curveMode = false;
	m_drawRuler = false;
	m_state = 0;

	QPalette pal = palette();
	pal.setColor(QPalette::Window, Qt::white);
	setPalette(pal);
	setCursor(QCursor(Qt::CrossCursor));
	setMouseTracking(true);
	setWindowState(windowState() | Qt::WindowFullScreen);

	m_text = new QLabel(this);
	m_text->setAlignment(Qt::AlignCenter);

	QPushButton *button = new QPushButton("Ok", this);
	button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	connect(button, &QPushButton::clicked, this, &CalibrationWidget::nextStep);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addStretch(1);
	vlay->addWidget(m_text);
	vlay->addWidget(button, 0, Qt::AlignCenter);
	vlay->addStretch(1);

	QHBoxLayout* hlay = new QHBoxLayout(this);
	hlay->addStretch(1);
	hlay->addLayout(vlay);
	hlay->addStretch(1);
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

			m_text->setText("Now tap the more precisely in the center of the circle");
		} else if (m_raw_points.size() < m_phy_points.size()) {
			m_raw_points << event->globalPos();
			setCursor(QCursor(Qt::CrossCursor));

			m_text->setText("Add other control points or press Ok if you think you have enough points");
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

	bool grabed = false;
	for (BorderLimit& elem : m_borderLimits) {
		if (elem.state == 2) {
			elem.state = 1;
			setCursor(QCursor(Qt::OpenHandCursor));
			grabed = true;
		}
	}

	if (m_curveMode && !grabed && !m_curves.isEmpty()) {
		if (m_curves.last().pts.size() <= 10) {
			m_curves.removeLast();
		} else if (m_state == 2) {
			if (m_drawRuler) {
				m_drawRuler = false;
				m_text->setText("Move the border limit to separate the strait and the distorted part of your line\n"
								"Then repeat the procedure for the other borders");
			} else {
				m_text->setText("Only the last line of each border is taken in account\n"
								"Press Ok when you have finished");
			}
		}
	}

	update();
}

void CalibrationWidget::paintEvent(QPaintEvent* event)
{
	Q_UNUSED(event);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

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
			painter.setPen(QPen(Qt::blue, 1.2));

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

			painter.setPen(QPen(Qt::red, 1.2));
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
					painter.drawPoint(x, y);
				}
			}
		}
	}

	if (m_drawRuler) {
		painter.setRenderHint(QPainter::Antialiasing, true);
		int dx = 10;
		painter.translate(m_w-24*dx, 0.5*m_h);
		painter.rotate(-20);
		painter.setPen(QPen(Qt::black, 2));
		painter.drawRect(-26*dx, -25, 52*dx, 50);
		for (int i = 0; i <= 50; ++i) {
			int x = -25*dx + i*dx;
			if (i % 5 == 0) {
				painter.setPen(QPen(Qt::black, 2));
				painter.drawLine(x, -25, x, 10);
			} else {
				painter.setPen(QPen(Qt::black, 1));
				painter.drawLine(x, -25, x, 0);
			}
		}
	}

	QWidget::paintEvent(event);
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

int CalibrationWidget::rotation()
{
	//qDebug() << m_screen->nativeOrientation() << m_screen->primaryOrientation() << m_screen->orientation();
	if (m_screen->nativeOrientation() == Qt::PrimaryOrientation) {
		return m_screen->angleBetween(Qt::LandscapeOrientation, m_screen->orientation()) / 90;
	} else {
		return m_screen->angleBetween(m_screen->nativeOrientation(), m_screen->orientation()) / 90;
	}
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
        cout << pro.readAllStandardError() << flush;

		if (pro.exitCode() != 0) {
			pro.start("xinput"); pro.waitForFinished();
			if (pro.exitCode() != 0) {
				cout << "You need to install xinput (sudo apt-get install xinput)" << endl;
			} else {
				QStringList devices;
				QByteArray out = pro.readAllStandardOutput();
				int end_pos = out.indexOf("id=");
				while (end_pos != -1) {
					int beg_pos = end_pos;
					char c = out[beg_pos];
					while (isspace(c) || isalnum(c)) c = out[--beg_pos];
					beg_pos++;

					devices << out.mid(beg_pos, end_pos-beg_pos).trimmed();
					end_pos = out.indexOf("id=", end_pos+1);
				}
				bool ok;
				QString selectedDevice = QInputDialog::getItem(this, "Select device", "Select your stylus device from the list.", devices, 0, false, &ok);

				if (ok) {
					m_device = selectedDevice;
					nextStep();
					return;
				}
			}
		}

		// get area
		command = "xinput list-props \"%1\"";
		command = command.arg(m_device);
		cout << "> " << command << endl;
		pro.start(command);
		pro.waitForFinished();
        QByteArray output = pro.readAllStandardOutput();
        cout << output;
        cout << pro.readAllStandardError() << flush;

		m_area.clear();
		if (pro.exitCode() == 0) {
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
        } else {
            cout << "Wacom Tablet Area is " << m_area[0] << " " << m_area[1] << " " << m_area[2] << " " << m_area[3] << endl;
        }

        m_rotation = rotation();
        cout << "The orientation of the screen is " << m_rotation << endl;

		m_borliMode = false;
		m_curveMode = false;
		m_text->setText("Linear calibration : "
						"Tap anywhere away from the borders to add a new control point");
		m_state = 1;

	} else if (m_state == 1) {

		QVector<int> old_area(4);
		QVector<int> new_area(4);
		old_area = m_area;
		// TopX, TopY, BottomX, BottomY
        for (int i = 0; i < m_rotation; ++i) old_area.prepend(old_area.takeLast());

		QVector<double> a, arhs;
		for (int i = 0; i < m_raw_points.size(); ++i) {
			a << m_raw_points[i].x() << 1.0;
			arhs << m_phy_points[i].x();
		}
		double res[2];
		int r = least_squares(arhs.size(), 2, a.data(), arhs.data(), res);
		if (r != 0) {
			m_text->setText("Please, add more points or quit with Escape");
			update();
			return;
		}
		// phy = res[0] * raw + res[1]
		fix_area(res[0], res[1], m_w, old_area[TopX], old_area[BottomX], new_area[TopX], new_area[BottomX]);

		a.clear(); arhs.clear();
		for (int i = 0; i < m_raw_points.size(); ++i) {
			a << m_raw_points[i].y() << 1.0;
			arhs << m_phy_points[i].y();
		}
		r = least_squares(arhs.size(), 2, a.data(), arhs.data(), res);
		if (r != 0) {
			m_text->setText("Please, add more points or quit with Escape");
			update();
			return;
		}
		// phy = res[0] * raw + res[1]
		fix_area(res[0], res[1], m_h, old_area[TopY], old_area[BottomY], new_area[TopY], new_area[BottomY]);

		// TopX, TopY, BottomX, BottomY
        for (int i = 0; i < m_rotation; ++i) new_area.append(new_area.takeFirst());

		command = "xinput set-int-prop \"%1\" \"Wacom Tablet Area\" 32 %2 %3 %4 %5";
		command = command.arg(m_device).arg(new_area[TopX]).arg(new_area[TopY]).arg(new_area[BottomX]).arg(new_area[BottomY]);

		cout << "> " << command << endl;
		pro.start(command); pro.waitForFinished();
		cout << pro.readAllStandardOutput();
        cout << pro.readAllStandardError() << flush;

		m_borliMode = true;
		m_curveMode = true;
		m_drawRuler = true;
		m_text->setText("Distortion calibration\nWith a ruler, make a strait line");
		m_state = 2;
		clearAll();
		update();

	} else if (m_state == 2) {

		QVector<QVector<double>> values(4);

		for (int b : {TopX, TopY, BottomX, BottomY}) {
			values[b] << 0.0 << 0.0 << 0.0 << 0.0 << 1.0 << 0.0;

			// take only the last curve
			for (int j = 0; j < m_curves.size(); ++j) {
				if (m_curves[j].border == b) {
					values[b][0] = pixelToUnit(b, m_borderLimits[b].pos);
					for (int i = 0; i < 5; ++ i) {
						values[b][i + 1] = m_curves[j].poly[i];
					}
				}
			}
		}

		// TopX, TopY, BottomX, BottomY
        for (int i = 0; i < m_rotation; ++i) values.append(values.takeFirst());


		command = "xinput set-float-prop \"%1\" \"Wacom Border Distortion\"";
		command = command.arg(m_device);
		for (int b : {TopX, TopY, BottomX, BottomY}) {
            for (int i = 0; i < 6; ++ i) {
				command += QString(" %1").arg(values[b][i]);
			}
		}

		cout << "> " << command << endl;
		pro.start(command); pro.waitForFinished();
		cout << pro.readAllStandardOutput();
        cout << pro.readAllStandardError() << flush;

		m_state = 3;
		m_borliMode = false;
		m_text->setText("Test the result");
		clearAll();
		update();

	} else if (m_state == 3) {
		close();
	}
}

void CalibrationWidget::screenChanged()
{
	m_w = m_screen->size().width();
	m_h = m_screen->size().height();

	//qDebug() << m_w << m_h << rotation() << m_screen->orientation();
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
