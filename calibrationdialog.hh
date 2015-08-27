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
		return _phy_points;
	}
	const QVector<QPointF>& getRawPoints() const {
		return _raw_points;
	}
	double getBorderTopX() const { return _border_topX; }
	double getBorderTopY() const { return _border_topY; }
	double getBorderBottomX() const { return _border_bottomX; }
	double getBorderBottomY() const { return _border_bottomY; }
	double getScreenWidth() const { return _sw; }
	double getScreenHeight() const { return _sh; }
	void setCreateBorders(bool on) { _create_borders = on; }
	void setText(const QString& text) { _text = text; }
	void clearAll();

private:
	virtual void tabletEvent(QTabletEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void paintEvent(QPaintEvent* event);
	virtual void closeEvent(QCloseEvent* event);
	virtual void keyPressEvent(QKeyEvent* event);
	virtual void moveEvent(QMoveEvent* event);

	void click_border(int x, int y);

	int _sw, _sh;
	int _border_topX;
	int _border_topY;
	int _border_bottomX;
	int _border_bottomY;
	QVector<QPointF> _phy_points;
	QVector<QPointF> _raw_points;
	QTimer _timer_pressure;
	bool _create_borders;
	QString _text;
};

#endif // CALIBRATIONDIALOG_HH
