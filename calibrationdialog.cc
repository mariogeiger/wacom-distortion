#include "calibrationdialog.hh"
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>

CalibrationDialog::CalibrationDialog(QWidget *parent)
	: QDialog(parent)
{
	QList<QScreen*> screens = QGuiApplication::screens();
	_sw = screens.first()->size().width();
	_sh = screens.first()->size().height();

	clearAll();
	_create_borders = true;

	setWindowFlags(Qt::Window);

	_timer_pressure.setSingleShot(true);
	setCursor(QCursor(Qt::CrossCursor));
	setResult(QDialog::Rejected);
}

CalibrationDialog::~CalibrationDialog()
{

}

void CalibrationDialog::clearAll()
{
	_border_topX = -1;
	_border_topY = -1;
	_border_bottomX = -1;
	_border_bottomY = -1;
	_phy_points.clear();
	_raw_points.clear();
}

/* Quand on survole avec le stylet il n'y a pas d'event déclanché
 *
 * Quand on presse ca déclanche un tabletEvent puis un mousePressEvent puis plein de tabletEvent
 *
 */
void CalibrationDialog::tabletEvent(QTabletEvent* event)
{
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

void CalibrationDialog::mousePressEvent(QMouseEvent* event)
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

void CalibrationDialog::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	painter.drawText(rect(), Qt::AlignCenter, _text);

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

void CalibrationDialog::closeEvent(QCloseEvent* )
{
	// check number of points ??
}

void CalibrationDialog::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_F) {
		setWindowState(windowState() ^ Qt::WindowFullScreen);
	}
	if (event->key() == Qt::Key_Delete) {
		clearAll();
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
		setResult(QDialog::Rejected);
		close();
	}
	if (event->key() == Qt::Key_Return) {
		setResult(QDialog::Accepted);
		close();
	}
}

void CalibrationDialog::moveEvent(QMoveEvent* )
{
	repaint();
}

void CalibrationDialog::click_border(int x, int y)
{
	if (!_create_borders) return;

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
