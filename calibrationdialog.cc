#include "calibrationdialog.hh"
extern "C" {
#include "lmath.h"
}
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QMessageBox>
#include <algorithm>

CalibrationDialog::CalibrationDialog(QWidget *parent)
	: QDialog(parent)
{
	QList<QScreen*> screens = QGuiApplication::screens();
	if (screens.isEmpty()) {
		m_w = m_h = -1;
	} else {
		m_w = screens.first()->size().width();
		m_h = screens.first()->size().height();
	}

	clearAll();
	m_show_borders = true;
	m_lineMode = false;

	setWindowFlags(Qt::Window);
	setCursor(QCursor(Qt::CrossCursor));
	setResult(QDialog::Rejected);
	setMouseTracking(true);
}

CalibrationDialog::~CalibrationDialog()
{

}

void CalibrationDialog::clearAll()
{
	m_borders[TopX].pos = 0.1 * m_w;
	m_borders[TopX].limit = 0.4 * m_w;
	m_borders[TopX].horizontal = false;
	m_borders[TopX].state = 0;

	m_borders[TopY].pos = 0.1 * m_h;
	m_borders[TopY].limit = 0.4 * m_h;
	m_borders[TopY].horizontal = true;
	m_borders[TopY].state = 0;

	m_borders[BottomX].pos = 0.9 * m_w;
	m_borders[BottomX].limit = 0.6 * m_w;
	m_borders[BottomX].horizontal = false;
	m_borders[BottomX].state = 0;

	m_borders[BottomY].pos = 0.9 * m_h;
	m_borders[BottomY].limit = 0.6 * m_h;
	m_borders[BottomY].horizontal = true;
	m_borders[BottomY].state = 0;

	m_phy_points.clear();
	m_raw_points.clear();
	setCursor(QCursor(Qt::CrossCursor));

	m_curves.clear();
}

double CalibrationDialog::getPolyCoeff(int border, int i) const
{
	// x^4 x^3 x^2 x 1
	for (int j = m_curves.size() - 1; j >= 0; --j) {
		if (m_curves[j].border == border) {
			return m_curves[j].poly[i];
		}
	}
	return i == 3 ? 1.0 : 0.0;
}

void CalibrationDialog::tabletEvent(QTabletEvent* event)
{
	if (event->type() == QEvent::TabletPress) {
		m_tabletGlobalPosF = event->globalPosF();
	}
}

void CalibrationDialog::mousePressEvent(QMouseEvent* event)
{
	bool grab = false;
	for (BorderLimit& elem : m_borders) {
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

	if (m_lineMode) {
		if (event->buttons() & Qt::LeftButton) {
			Curve c;
			c.border = -1;
			c.pts.append(event->globalPos());
			m_curves.append(c);
		}
	} else {
		if (event->globalPos().x() == int(m_tabletGlobalPosF.x()) && event->globalPos().y() == int(m_tabletGlobalPosF.y())) {
			add_point(m_tabletGlobalPosF);
		} else {
			add_point(event->globalPos());
		}
	}
	update();
}

void CalibrationDialog::mouseMoveEvent(QMouseEvent* event)
{
	if (m_phy_points.size() != m_raw_points.size()) return;

	bool grab = false;
	if (m_show_borders) {
		for (BorderLimit& elem : m_borders) {
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

	if (m_lineMode) {
		if (event->buttons() & Qt::LeftButton) {
			m_curves.last().pts.append(event->globalPos());
			if (m_show_borders) fitCurves();
			update();
			return;
		}
	}

	if (m_show_borders) {
		bool ungrab = false;
		for (BorderLimit& elem : m_borders) {
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

void CalibrationDialog::mouseReleaseEvent(QMouseEvent* )
{
	for (BorderLimit& elem : m_borders) {
		if (elem.state == 2) {
			elem.state = 1;
			setCursor(QCursor(Qt::OpenHandCursor));
		}
	}

	if (m_lineMode) {
		if (!m_curves.isEmpty() && m_curves.last().pts.size() <= 1) m_curves.removeLast();
	}

	update();
}

void CalibrationDialog::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	painter.setPen(Qt::darkGray);
	painter.drawText(rect(), Qt::AlignCenter, m_text);

	painter.translate(mapFromGlobal(QPoint(0,0)));

	if (m_show_borders) {
		for (BorderLimit& elem : m_borders) elem.paint(&painter, m_w, m_h);
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

		if (m_show_borders && c.border != -1) {
			painter.setPen(Qt::blue);

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

void CalibrationDialog::closeEvent(QCloseEvent* event)
{
	int ans = QMessageBox::question(
				  this, "Skip the calibration",
				  "Do you want to skip this part of the calibration ?",
				  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);

	if (ans == QMessageBox::Yes) {
		QWidget::closeEvent(event);
	} else {
		event->ignore();
	}
}

void CalibrationDialog::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_F) {
		setWindowState(windowState() ^ Qt::WindowFullScreen);
	}
	if (event->key() == Qt::Key_Delete) {
		int ans = QMessageBox::question(
					  this, "Delete all points",
					  "Do you want to remove all the points ?",
					  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);

		if (ans == QMessageBox::Yes) {
			clearAll();
			update();
		}
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
		int ans = QMessageBox::question(
					  this, "Skip the calibration",
					  "Do you want to skip this part of the calibration ?",
					  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);

		if (ans == QMessageBox::Yes) {
			reject();
		}
	}
	if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
		accept();
	}
}

void CalibrationDialog::moveEvent(QMoveEvent* event)
{
	QWidget::moveEvent(event);
	update();
}

void CalibrationDialog::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
}

void CalibrationDialog::add_point(const QPointF& point)
{
	if (m_raw_points.size() == m_phy_points.size()) {
		m_phy_points << point;
		setCursor(QCursor(Qt::BlankCursor));
	} else if (m_raw_points.size() < m_phy_points.size()) {
		m_raw_points << point;
		setCursor(QCursor(Qt::CrossCursor));
	}
	//qDebug("Point added, %d in raw, %d in phy", m_raw_points.size(), m_phy_points.size());
}

void CalibrationDialog::fitCurves()
{
	for (int i = 0; i < m_curves.size(); ++i) {
		Curve& c = m_curves[i];
		if (c.pts.size() <= 3) continue;
		int nb[4] = {0, 0, 0, 0};
		for (int j = 0;j < c.pts.size(); ++j) {
			if (c.pts[j].x() < m_borders[TopX].pos) nb[TopX]++;
			if (c.pts[j].y() < m_borders[TopY].pos) nb[TopY]++;
			if (c.pts[j].x() > m_borders[BottomX].pos) nb[BottomX]++;
			if (c.pts[j].y() > m_borders[BottomY].pos) nb[BottomY]++;
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
				double d = pixelToUnit(c.border, m_borders[c.border].pos);
				double cons[10] = {
					4.*d*d*d,      3.*d*d,      2.*d,      1.0,   0.0,
					d*d*d*d,       d*d*d,       d*d,       d,     1.0
				};
				double crhs[2] = {1.0, d};
				least_squares_constraint(arhs.size(), 5, 2, a.data(), arhs.data(), cons, crhs, c.poly);
			}
		}
	}
}

void CalibrationDialog::BorderLimit::paint(QPainter* p, double w, double h)
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

void CalibrationDialog::BorderLimit::move(double new_pos)
{
	if (pos < limit && new_pos < limit) pos = new_pos;
	if (pos > limit && new_pos > limit) pos = new_pos;
}
