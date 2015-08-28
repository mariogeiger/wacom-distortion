#ifndef CALIBRATIONDIALOG_HH
#define CALIBRATIONDIALOG_HH

#include <QWidget>
#include <QDialog>
#include <QTabletEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMoveEvent>
#include <QPoint>
#include <QPointF>
#include <QTimer>

class CalibrationDialog : public QDialog
{
	Q_OBJECT

public:
	CalibrationDialog(QWidget *parent = 0);
	~CalibrationDialog();

	const QVector<QPointF>& getPhysicalPoints() const {
		return m_phy_points;
	}
	const QVector<QPointF>& getRawPoints() const {
		return m_raw_points;
	}
	double getBorderTopX() const { return m_border_topX; }
	double getBorderTopY() const { return m_border_topY; }
	double getBorderBottomX() const { return m_border_bottomX; }
	double getBorderBottomY() const { return m_border_bottomY; }
	double getScreenWidth() const { return m_w; }
	double getScreenHeight() const { return m_h; }
	void setCreateBorders(bool on) { m_borders = on; }
	void setText(const QString& text) { m_text = text; }
	void setTolerance(double t) { m_tolerance = t; }
	void clearAll();

private:
	virtual void tabletEvent(QTabletEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void paintEvent(QPaintEvent* event);
	virtual void closeEvent(QCloseEvent* event);
	virtual void keyPressEvent(QKeyEvent* event);
	virtual void moveEvent(QMoveEvent* event);

	void click_border(int x, int y);
	void check_borders();

	double m_w, m_h;
	double m_border_topX;
	double m_border_topY;
	double m_border_bottomX;
	double m_border_bottomY;
	QVector<QPointF> m_phy_points;
	QVector<QPointF> m_raw_points;
	bool m_borders;
	double m_tolerance;
	QString m_text;
};

#endif // CALIBRATIONDIALOG_HH
