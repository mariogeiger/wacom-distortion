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
	m_borders = true;
	m_tolerance = 1.5;

	setWindowFlags(Qt::Window);
	setCursor(QCursor(Qt::CrossCursor));
	setResult(QDialog::Rejected);
}

CalibrationDialog::~CalibrationDialog()
{

}

void CalibrationDialog::clearAll()
{
	m_border_topX = -1;
	m_border_topY = -1;
	m_border_bottomX = -1;
	m_border_bottomY = -1;
	m_phy_points.clear();
	m_raw_points.clear();
	setCursor(QCursor(Qt::CrossCursor));
}

void CalibrationDialog::tabletEvent(QTabletEvent* event)
{
	if (event->type() == QEvent::TabletPress) {
		if (m_raw_points.size() == m_phy_points.size()) {
			m_phy_points << event->globalPosF();
			setCursor(QCursor(Qt::BlankCursor));
		} else if (m_raw_points.size() < m_phy_points.size()) {
			m_raw_points << event->globalPosF();
			setCursor(QCursor(Qt::CrossCursor));
			check_borders();
		}
		qDebug("Point added, %d in raw, %d in phy", m_raw_points.size(), m_phy_points.size());

		repaint();
	}
}

void CalibrationDialog::mousePressEvent(QMouseEvent* )
{

}

void CalibrationDialog::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	painter.drawText(rect(), Qt::AlignCenter, m_text);

	painter.translate(mapFromGlobal(QPoint(0,0)));

	painter.setPen(QPen(Qt::black, 0.5));
	painter.drawLine(m_border_topX, 0, m_border_topX, m_h);
	painter.drawLine(0, m_border_topY, m_w, m_border_topY);
	painter.drawLine(m_border_bottomX, 0, m_border_bottomX, m_h);
	painter.drawLine(0, m_border_bottomY, m_w, m_border_bottomY);

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
			repaint();
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
				check_borders();
			}
		}
		repaint();
	}
	if (event->key() == Qt::Key_Escape) {
		reject();
	}
	if (event->key() == Qt::Key_Return) {
		accept();
	}
}

void CalibrationDialog::moveEvent(QMoveEvent* )
{
	repaint();
}

void CalibrationDialog::check_borders()
{
	if (!m_borders) return;
	if (m_raw_points.isEmpty()) return;
	if (m_raw_points.size() != m_phy_points.size()) return;

	QVector<QPair<QPointF,QPointF>> raw_phi_topX, raw_phi_topY, raw_phi_bottomX, raw_phi_bottomY;
	for (int i = 0; i < m_raw_points.size(); ++i) {
		double x = m_phy_points[i].x();
		double y = m_phy_points[i].y();
		double cx = m_w - x;
		double cy = m_h - y;

		if (x < y && x < cx && x < cy)   raw_phi_topX << QPair<QPointF,QPointF>(m_raw_points[i], m_phy_points[i]);
		if (y < x && y < cx && y < cy)   raw_phi_topY << QPair<QPointF,QPointF>(m_raw_points[i], m_phy_points[i]);
		if (cx < cy && cx < x && cx < y) raw_phi_bottomX << QPair<QPointF,QPointF>(m_raw_points[i], m_phy_points[i]);
		if (cy < cx && cy < x && cy < y) raw_phi_bottomY << QPair<QPointF,QPointF>(m_raw_points[i], m_phy_points[i]);
	}

	std::sort(raw_phi_topX.begin(), raw_phi_topX.end(), [](const QPair<QPointF,QPointF>& a, const QPair<QPointF,QPointF>& b) -> bool {
		return a.second.x() > b.second.x();
	}); // du plus grand au plus petit
	std::sort(raw_phi_topY.begin(), raw_phi_topY.end(), [](const QPair<QPointF,QPointF>& a, const QPair<QPointF,QPointF>& b) -> bool {
		return a.second.y() > b.second.y();
	}); // du plus grand au plus petit
	std::sort(raw_phi_bottomX.begin(), raw_phi_bottomX.end(), [](const QPair<QPointF,QPointF>& a, const QPair<QPointF,QPointF>& b) -> bool {
		return a.second.x() < b.second.x();
	});
	std::sort(raw_phi_bottomY.begin(), raw_phi_bottomY.end(), [](const QPair<QPointF,QPointF>& a, const QPair<QPointF,QPointF>& b) -> bool {
		return a.second.y() < b.second.y();
	});

	m_border_topX = -1;
	m_border_topY = -1;
	m_border_bottomX = -1;
	m_border_bottomY = -1;


	for (auto& elem : raw_phi_topX) {
		if ((elem.first - elem.second).manhattanLength() <= m_tolerance) {
			m_border_topX = elem.second.x();
		} else {
			break;
		}
	}

	for (auto& elem : raw_phi_topY) {
		if ((elem.first - elem.second).manhattanLength() <= m_tolerance) {
			m_border_topY = elem.second.y();
		} else {
			break;
		}
	}

	for (auto& elem : raw_phi_bottomX) {
		if ((elem.first - elem.second).manhattanLength() <= m_tolerance) {
			m_border_bottomX = elem.second.x();
		} else {
			break;
		}
	}

	for (auto& elem : raw_phi_bottomY) {
		if ((elem.first - elem.second).manhattanLength() <= m_tolerance) {
			m_border_bottomY = elem.second.y();
		} else {
			break;
		}
	}

}
