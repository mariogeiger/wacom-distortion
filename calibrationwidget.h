#ifndef CALIBRATIONWIDGET_H
#define CALIBRATIONWIDGET_H

#include <QWidget>

/*         Top Y
 *    +--------------+
 * Top|              |
 *  X |              | Bottom X
 *    |              |
 *    +--------------+
 *        Bottom Y
 *
 *
 *
 *     +-----------------------------+
 *     |                      |      |
 *     |                      |      |
 *     |                      |      |
 *     |                      |      |<-- Screen border
 *     |                      |      |
 *     |                      |      |
 *     |    Axis of Unit      |      |
 * <---1----------------------|------0
 *     |                      |      |
 *     +-----------------------------+
 *                            ^
 *                BorderLimit |
 *
 */

class CalibrationWidget : public QWidget
{
	Q_OBJECT
public:
	explicit CalibrationWidget(const QString& dev, QWidget *parent = 0);
	~CalibrationWidget();

	enum Border {
		TopX = 0,
		TopY = 1,
		BottomX = 2,
		BottomY = 3
	};

	inline void setDevice(const QString& dev) { m_device = dev; }

private:
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);
	virtual void paintEvent(QPaintEvent* event);
	virtual void keyPressEvent(QKeyEvent* event);


	void fitCurves();
	void clearAll();
	void nextStep();

	inline double wh(int border) const {
		return border % 2 == 0 ? m_w : m_h;
	}
	inline double xy(int border, const QPointF& point) const {
		return border % 2 == 0 ? point.x() : point.y();
	}
	inline double yx(int border, const QPointF& point) const {
		return border % 2 == 0 ? point.y() : point.x();
	}
	bool isInBorder(int border, const QPointF& point) const {
		return (border < 2) ? xy(border, point) < m_borderLimits[border].pos
							: xy(border, point) > m_borderLimits[border].pos;
	}
	double pixelToUnit(int border, double pixel) const {
		return (border < 2) ? pixel / wh(border)
							: 1.0 - pixel / wh(border);
	}
	double unitToPixel(int border, double unit) const {
		return (border < 2) ? unit * wh(border)
							: (1.0 - unit) * wh(border);
	}

	double m_w, m_h;

	QVector<QPointF> m_phy_points;
	QVector<QPointF> m_raw_points;
	bool m_borliMode;
	QString m_text;

	struct BorderLimit {
		double pos;
		double limit;
		bool horizontal;
		int state;

		void paint(QPainter* p, double w, double h);
		void move(double new_pos);
	} m_borderLimits[4];

	bool m_curveMode;
	struct Curve {
		QList<QPointF> pts;
		int border;

		// comments holds for TopX border
		double ab[2]; // phy_x = a*y + b; y in pixels, phy_x [0,1] unit
		double poly[5]; // order 4 polynomial phy_x = Poly(raw_x)
	};

	QList<Curve> m_curves;

	QVector<double> m_area;
	QString m_device;
	int m_state;
};

#endif // CALIBRATIONWIDGET_H
