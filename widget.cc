#include "widget.hh"
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QMessageBox>

Widget::Widget(QWidget *parent)
	: QWidget(parent)
{
	QList<QScreen*> screens = QGuiApplication::screens();
	_sw = screens.first()->size().width();
	_sh = screens.first()->size().height();
	qDebug("screen dimention %dx%d", _sw, _sh);

	_border_topX = -1;
	_border_topY = -1;
	_border_bottomX = -1;
	_border_bottomY = -1;
}

Widget::~Widget()
{

}

void Widget::tabletEvent(QTabletEvent* event)
{
	if (_phy_hi_points.size() == _raw_hi_points.size()) {
		_phy_hi_points << QPointF(event->hiResGlobalX(), event->hiResGlobalY());
		_phy_points << event->globalPos();
	} else if (_raw_hi_points.size() < _phy_hi_points.size()) {
		_raw_hi_points << QPointF(event->hiResGlobalX(), event->hiResGlobalY());
		_raw_points << event->globalPos();
	}
	repaint();
}

void Widget::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton) {
		int x = event->globalX();
		int y = event->globalY();
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
	} else {
		if (_phy_hi_points.size() == _raw_hi_points.size()) {
			_phy_hi_points << event->globalPos();
			_phy_points << event->globalPos();
		} else if (_raw_hi_points.size() < _phy_hi_points.size()) {
			_raw_hi_points << event->globalPos();
			_raw_points << event->globalPos();
		}
	}
	repaint();
	qDebug("%d in phy and %d in raw", _phy_hi_points.size(), _raw_hi_points.size());
}

void Widget::paintEvent(QPaintEvent*)
{
	QPainter painter(this);

	painter.translate(mapFromGlobal(QPoint(0,0)));

	painter.setPen(Qt::red);
	painter.drawLine(_border_topX, 0, _border_topX, _sh);
	painter.drawLine(0, _border_topY, _sw, _border_topY);
	painter.drawLine(_border_bottomX, 0, _border_bottomX, _sh);
	painter.drawLine(0, _border_bottomY, _sw, _border_bottomY);

	painter.setPen(Qt::black);
	for (int i = 0; i < _raw_hi_points.size(); ++i) {
		painter.drawLine(_raw_points[i], _phy_points[i]);
	}

	painter.setPen(Qt::red);
	if (_phy_hi_points.size() > _raw_hi_points.size()) {
		painter.drawPoint(_phy_points.last());
	}
}

void Widget::closeEvent(QCloseEvent* event)
{
	int result = QMessageBox::question(
				this, "Apply calibration", "Apply the calibration ?", QMessageBox::Yes, QMessageBox::No);

	if (result == QMessageBox::Yes) {
		if (_border_topX > 0) {
			QVector<double> raw, phy;
			for (int i = 0; i < _raw_hi_points.size(); ++i) {
				if (_raw_points[i].x() < _border_topX) {
					raw << _raw_hi_points[i].x();
					phy << _phy_hi_points[i].x();
				}
			}


		}
	}
}
