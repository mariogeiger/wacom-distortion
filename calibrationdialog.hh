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
#include <QShowEvent>
#include <QPoint>
#include <QPointF>
#include <QTimer>

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
 *     |    Axis of unit      |      |
 * <---1----------------------|------0
 *     |                      |      |
 *     +-----------------------------+
 *                            ^
 *                BorderLimit |
 *
 */

class CalibrationDialog : public QDialog
{
	Q_OBJECT

public:
	enum Border {
		TopX = 0,
		TopY = 1,
		BottomX = 2,
		BottomY = 3
	};

	CalibrationDialog(QWidget *parent = 0);
	~CalibrationDialog();

	inline const QVector<QPointF>& getPhysicalPoints() const {
		return m_phy_points;
	}
	inline const QVector<QPointF>& getRawPoints() const {
		return m_raw_points;
	}
	inline double getBorderLimit(int border) const {
		return m_borders[border].pos;
	}
	double getPolyCoeff(int border, int i) const;
	double getScreenWidth() const { return m_w; }
	double getScreenHeight() const { return m_h; }
	void setBorders(bool on) { m_show_borders = on; }
	void setText(const QString& text) { m_text = text; }
	void setLineMode(bool on) { m_lineMode = on; }
	void clearAll();

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
		return (border < 2) ? xy(border, point) < m_borders[border].pos
							: xy(border, point) > m_borders[border].pos;
	}
	double pixelToUnit(int border, double pixel) const {
		return (border < 2) ? pixel / wh(border)
							: 1.0 - pixel / wh(border);
	}
	double unitToPixel(int border, double unit) const {
		return (border < 2) ? unit * wh(border)
							: (1.0 - unit) * wh(border);
	}

private:
	virtual void tabletEvent(QTabletEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);
	virtual void paintEvent(QPaintEvent* event);
	virtual void closeEvent(QCloseEvent* event);
	virtual void keyPressEvent(QKeyEvent* event);
	virtual void moveEvent(QMoveEvent* event);
	virtual void showEvent(QShowEvent* event);

	void add_point(const QPointF& point);
	void fitCurves();

	double m_w, m_h;

	QVector<QPointF> m_phy_points;
	QVector<QPointF> m_raw_points;
	bool m_show_borders;
	QString m_text;

	QPointF m_tabletGlobalPosF;

	struct BorderLimit {
		double pos;
		double limit;
		bool horizontal;
		int state;

		void paint(QPainter* p, double w, double h);
		void move(double new_pos);
	} m_borders[4];

	bool m_lineMode;
	struct Curve {
		QList<QPointF> pts;
		int border;

		// comments holds for TopX border
		double ab[2]; // phy_x = a*y + b; y in pixels, phy_x [0,1] unit
		double poly[5]; // order 4 polynomial phy_x = Poly(raw_x)
	};

	QList<Curve> m_curves;
};

#endif // CALIBRATIONDIALOG_HH
