#ifndef WIDGET_HH
#define WIDGET_HH

#include <QWidget>
#include <QTabletEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMoveEvent>
#include <QPoint>
#include <QPointF>
#include <QTimer>

class Widget : public QWidget
{
	Q_OBJECT

public:
	Widget(QWidget *parent = 0);
	~Widget();

private:
	virtual void tabletEvent(QTabletEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void paintEvent(QPaintEvent* event);
	virtual void closeEvent(QCloseEvent* event);
	virtual void keyPressEvent(QKeyEvent* event);
	virtual void moveEvent(QMoveEvent* event);

	QVector<double> find_polynomial(double d, QVector<double> raw, QVector<double> phy);
	void calibrate();

	int _sw, _sh;
	int _border_topX;
	int _border_topY;
	int _border_bottomX;
	int _border_bottomY;
	QVector<QPointF> _phy_points;
	QVector<QPointF> _raw_points;
	QTimer _timer_pressure;

	bool _mouse_fake;
};

#endif // WIDGET_HH
