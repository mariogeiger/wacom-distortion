#include "calibrationdialog.hh"
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
	m_borders[0].pos = 0.1 * m_w;
	m_borders[0].limit = 0.4 * m_w;
	m_borders[0].horizontal = false;
	m_borders[0].state = 0;

	m_borders[1].pos = 0.1 * m_h;
	m_borders[1].limit = 0.4 * m_h;
	m_borders[1].horizontal = true;
	m_borders[1].state = 0;

	m_borders[2].pos = 0.9 * m_w;
	m_borders[2].limit = 0.6 * m_w;
	m_borders[2].horizontal = false;
	m_borders[2].state = 0;

	m_borders[3].pos = 0.9 * m_h;
	m_borders[3].limit = 0.6 * m_h;
	m_borders[3].horizontal = true;
	m_borders[3].state = 0;

	m_phy_points.clear();
	m_raw_points.clear();
	setCursor(QCursor(Qt::CrossCursor));
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
	for (Border& elem : m_borders) {
		if (elem.state == 1) {
			elem.state = 2;
			grab = true;
		}
	}

	if (grab) {
		update();
		return;
	}

	if (event->globalPos() == m_tabletGlobalPosF.toPoint()) {
		add_point(m_tabletGlobalPosF);
	} else {
		add_point(event->globalPos());
	}
	update();
}

void CalibrationDialog::mouseMoveEvent(QMouseEvent* event)
{
	bool grab = false;
	for (Border& elem : m_borders) {
		if (elem.state == 2) {
			elem.move(elem.horizontal ? event->globalY() : event->globalX());
			grab = true;
		}
	}
	if (grab) {
		update();
		return;
	}

	for (Border& elem : m_borders) {
		if (std::abs((elem.horizontal ? event->globalY() : event->globalX()) - elem.pos) <= 5) {
			elem.state = 1;
		} else if (elem.state == 1) {
			elem.state = 0;
		}
	}
	update();
}

void CalibrationDialog::mouseReleaseEvent(QMouseEvent* )
{
	for (Border& elem : m_borders) {
		if (elem.state == 2) {
			elem.state = 1;
		}
	}
	update();
}

void CalibrationDialog::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	painter.drawText(rect(), Qt::AlignCenter, m_text);

	painter.translate(mapFromGlobal(QPoint(0,0)));

	if (m_show_borders) {
		for (Border& elem : m_borders) elem.paint(&painter, m_w, m_h);
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
		int ans = QMessageBox::question(
					  this, "Delete all points",
					  "Do you want to remove all the points ?",
					  QMessageBox::Yes, QMessageBox::Cancel);

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
		update();
	}
	if (event->key() == Qt::Key_Escape) {
		reject();
	}
	if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
		accept();
	}
}

void CalibrationDialog::moveEvent(QMoveEvent* )
{
	update();
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
	qDebug("Point added, %d in raw, %d in phy", m_raw_points.size(), m_phy_points.size());
}

void CalibrationDialog::Border::paint(QPainter* p, double w, double h)
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

void CalibrationDialog::Border::move(double new_pos)
{
	if (pos < limit && new_pos < limit) pos = new_pos;
	if (pos > limit && new_pos > limit) pos = new_pos;
}
