#include "widget.hh"
extern "C"
{
#include "lmath.h"
}
#include <cmath>
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QMessageBox>
#include <iostream>

Widget::Widget(QWidget *parent)
	: QWidget(parent)
{
	QList<QScreen*> screens = QGuiApplication::screens();
	_sw = screens.first()->size().width();
	_sh = screens.first()->size().height();
	qDebug("Screen dimension %dx%d", _sw, _sh);

	_border_topX = -1;
	_border_topY = -1;
	_border_bottomX = -1;
	_border_bottomY = -1;

	_timer_pressure.setSingleShot(true);
	setCursor(QCursor(Qt::CrossCursor));
}

Widget::~Widget()
{

}

/* Quand on survole avec le stylet il n'y a pas d'event déclanché
 *
 * Quand on presse ca déclanche un tabletEvent puis un mousePressEvent puis plein de tabletEvent
 *
 */
void Widget::tabletEvent(QTabletEvent* event)
{
	switch (event->type()) {
	case QEvent::TabletEnterProximity:
		qDebug("stylus : type = TabletEnterProximity");
		break;
	case QEvent::TabletLeaveProximity:
		qDebug("stylus : type = TabletLeaveProximity");
		break;
	case QEvent::TabletMove:
		qDebug("stylus : type = TabletMove");
		break;
	case QEvent::TabletPress:
		qDebug("stylus : type = TabletPress");
		break;
	case QEvent::TabletRelease:
		qDebug("stylus : type = TabletRelease");
		break;
	default:
		qDebug("stylus : type = Other");
		break;
	}

	if (event->pressure() == 0.0) return;

	if (!_timer_pressure.isActive()) {
		if (event->pointerType() == QTabletEvent::Eraser) {
			click_border(event->globalX(), event->globalY());
			qDebug("stylus : Border changed");
		} else {
			if (_raw_points.size() == _phy_points.size()) {
				_phy_points << event->globalPosF();
				setCursor(QCursor(Qt::BlankCursor));
			} else if (_raw_points.size() < _phy_points.size()) {
				_raw_points << event->globalPosF();
				setCursor(QCursor(Qt::CrossCursor));
			}
			qDebug("stylus : Point added, %d in raw, %d in phy", _raw_points.size(), _phy_points.size());
		}
		repaint();
	} else {
		qDebug("stylus : Ignored");
	}
	_timer_pressure.start(200);
}

void Widget::mousePressEvent(QMouseEvent* event)
{
	if (_timer_pressure.isActive()) {
		qDebug("mouse : Ignored");
		return;
	}

	if (event->button() == Qt::LeftButton) {
		click_border(event->globalX(), event->globalY());
		qDebug("mouse : Border changed");
	} else {
		if (_raw_points.size() == _phy_points.size()) {
			_phy_points << event->globalPos();
		} else if (_raw_points.size() < _phy_points.size()) {
			_raw_points << event->globalPos();
		}
		qDebug("mouse : Point added, %d in raw, %d in phy", _raw_points.size(), _phy_points.size());
	}
	repaint();
}

void Widget::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	painter.drawText(rect(), Qt::AlignCenter,
					 "Click to set a border\n"
					 "Tap in any border to add a calibation point then tap in the target\n"
					 "you can move the window, the points will follow\n"
					 "F - fullscreen\n"
					 "backspace - remove last point\n"
					 "Delete - reset all\n"
					 "Esc - quit (the calibration setting are shown at exit)");

	painter.translate(mapFromGlobal(QPoint(0,0)));

	painter.setPen(QPen(Qt::black, 0.5));
	painter.drawLine(_border_topX, 0, _border_topX, _sh);
	painter.drawLine(0, _border_topY, _sw, _border_topY);
	painter.drawLine(_border_bottomX, 0, _border_bottomX, _sh);
	painter.drawLine(0, _border_bottomY, _sw, _border_bottomY);

	for (int i = 0; i < _raw_points.size(); ++i) {
		painter.setPen(Qt::black);
		painter.drawLine(_raw_points[i], _phy_points[i]);
		painter.setPen(QPen(Qt::red, 2.5));
		painter.drawPoint(_phy_points[i]);
		painter.setPen(QPen(Qt::blue, 2.5));
		painter.drawPoint(_raw_points[i]);
	}

	painter.setPen(Qt::red);
	if (_phy_points.size() > _raw_points.size()) {
		QPointF p = _phy_points.last();

		painter.setPen(QPen(Qt::red, 2.5));
		painter.drawPoint(p);

		QRectF r;
		r.setSize(QSizeF(10, 10));
		r.moveCenter(p);
		painter.setPen(QPen(Qt::blue, 1.5));
		painter.drawEllipse(r);
	}
}

void Widget::closeEvent(QCloseEvent* )
{
	calibrate();
}

void Widget::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_F) {
		setWindowState(windowState() ^ Qt::WindowFullScreen);
	}
	if (event->key() == Qt::Key_Delete) {
		_phy_points.clear();
		_raw_points.clear();
		_border_topX = -1;
		_border_topY = -1;
		_border_bottomX = -1;
		_border_bottomY = -1;
		repaint();
	}
	if (event->key() == Qt::Key_Backspace) {
		// p : xxxx
		// r : xxx
		if (_phy_points.size() > 0) {
			if (_phy_points.size() == _raw_points.size()) {
				_raw_points.removeLast();
			} else {
				_phy_points.removeLast();
			}
		}
		repaint();
	}
	if (event->key() == Qt::Key_Escape) {
		close();
	}
}

void Widget::moveEvent(QMoveEvent* )
{
	repaint();
}

void Widget::click_border(int x, int y)
{
	int cx = _sw - x;
	int cy = _sh - y;
	int smallest = qMin(qMin(x, y), qMin(cx, cy));

	if (x == smallest) {
		_border_topX = x;
	}
	if (y == smallest) {
		_border_topY = y;
	}
	if (cx == smallest) {
		_border_bottomX = x;
	}
	if (cy == smallest) {
		_border_bottomY = y;
	}
}

QVector<double> Widget::find_polynomial(double d, QVector<double> raw, QVector<double> phy)
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

void Widget::calibrate()
{
	QVector<double> values;
	for (int i = 0; i < 4; ++i) {
		values << 0.0 << 0.0 << 0.0 << 1.0 << 0.0;
	}

	if (_border_topX > 0) {
		QVector<double> raw, phy;
		for (int i = 0; i < _raw_points.size(); ++i) {
			if (_raw_points[i].x() < _border_topX) {
				raw << _raw_points[i].x() / (double)_sw;
				phy << _phy_points[i].x() / (double)_sw;
			}
		}
		double d = _border_topX / (double)_sw;
		QVector<double> poly = find_polynomial(d, raw, phy);

		if (poly.size() == 4) {
			values[0] = d;
			for (int i = 0; i < 4; ++i) values[1+i] = poly[i];
		}
	}

	if (_border_topY > 0) {
		QVector<double> raw, phy;
		for (int i = 0; i < _raw_points.size(); ++i) {
			if (_raw_points[i].y() < _border_topY) {
				raw << _raw_points[i].y() / (double)_sh;
				phy << _phy_points[i].y() / (double)_sh;
			}
		}
		double d = _border_topY / (double)_sh;
		QVector<double> poly = find_polynomial(d, raw, phy);

		if (poly.size() == 4) {
			values[5] = d;
			for (int i = 0; i < 4; ++i) values[6+i] = poly[i];
		}
	}

	if (_border_bottomX > 0) {
		QVector<double> raw, phy;
		for (int i = 0; i < _raw_points.size(); ++i) {
			if (_raw_points[i].x() > _border_bottomX) {
				raw << 1.0 - _raw_points[i].x() / (double)_sw;
				phy << 1.0 - _phy_points[i].x() / (double)_sw;
			}
		}
		double d = 1.0 - _border_bottomX / (double)_sw;
		QVector<double> poly = find_polynomial(d, raw, phy);

		if (poly.size() == 4) {
			values[10] = d;
			for (int i = 0; i < 4; ++i) values[11+i] = poly[i];
		}
	}

	if (_border_bottomY > 0) {
		QVector<double> raw, phy;
		for (int i = 0; i < _raw_points.size(); ++i) {
			if (_raw_points[i].y() > _border_bottomY) {
				raw << 1.0 - _raw_points[i].y() / (double)_sh;
				phy << 1.0 - _phy_points[i].y() / (double)_sh;
			}
		}
		double d = 1.0 - _border_bottomY / (double)_sh;
		QVector<double> poly = find_polynomial(d, raw, phy);

		if (poly.size() == 4) {
			values[15] = d;
			for (int i = 0; i < 4; ++i) values[16+i] = poly[i];
		}
	}

	std::cout << "xinput set-float-prop <device> \"Wacom Border Distortion\"";
	for (int i = 0; i < values.size(); ++i) std::cout << " " << values[i];
	std::cout << std::endl;
}
