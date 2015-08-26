#ifndef WIDGET_HH
#define WIDGET_HH

#include <QWidget>
#include <QTabletEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QPoint>
#include <QPointF>

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

	int _sw, _sh;
	int _border_topX;
	int _border_topY;
	int _border_bottomX;
	int _border_bottomY;
	QVector<QPointF> _phy_hi_points;
	QVector<QPointF> _raw_hi_points;
	QVector<QPoint> _phy_points;
	QVector<QPoint> _raw_points;
};

#endif // WIDGET_HH
