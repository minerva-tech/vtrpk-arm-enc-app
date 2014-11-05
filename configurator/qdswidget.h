#ifndef DSWIDGET_H
#define DSWIDGET_H

#include <QWidget>
//#ifdef _WIN32
#include <Windows.h>
#include <boost/cstdint.hpp>

using std::vector;

const int selection_radius2 = 25;

struct Point {
	Point(int x_, int y_) : x(x_), y(y_) { }
	Point(const QPoint& pt) : x(pt.x()), y(pt.y()) { }

	int dist2(const Point& pt) const { return (x-pt.x)*(x-pt.x)+(y-pt.y)*(y-pt.y); }

	int x;
	int y;
};

class ROIs
{
public:
	typedef vector<Point> ROI;

    ROIs() : m_inverted(false) {m_pt_sel[0] = m_pt_sel[1] = -1;}

	bool selectPt(const Point& pt);
	bool selectRoi(const Point& pt);
	void movePt(const Point& to);
	void deletePt();
	void deleteRoi();
	void addPt(const Point& pt);
	void finishRoi();
	void clearRois();
    void invert();

	bool selectedRoi() const;
	bool selectedPt() const;

	void draw(QPixmap& pic) const;

	void serialize(std::vector<uint8_t>& dst) const;
	void deserialize(const std::vector<uint8_t>::iterator& begin, const std::vector<uint8_t>::iterator& end);

//	QImage& map() {return m_map;}

private:
//	bool findPt(const Point& from, Point* pt);

    bool        m_inverted;
	vector<ROI> m_rois;

//	QImage		m_map;

	int			m_pt_sel[2];
};

class QDSWidget : public QWidget
{
	Q_OBJECT

	static const int g_roi_map_w = 320;
	static const int g_roi_map_h = 128;

public:
    QDSWidget(QWidget * parent = 0, Qt::WindowFlags f = 0);
    virtual ~QDSWidget();

    virtual QPaintEngine* paintEngine() const;

    const QString getStatString() const;

protected:
	virtual void paintEvent(QPaintEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseDoubleClickEvent(QMouseEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);

public slots:
	void Play();
	void Stop();

private slots:
	void DownloadROI();
	void UploadROI();
	void ClearROI();

	void ChangeMarkerMode();

signals:
	void setStartChecked(bool);

private:
	void buildGraph();
	void setROI(HDC hDC);

	void showPopup(const QPoint& pt);

	ROIs                            m_rois;

	HRESULT AddToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) const;
	HRESULT RemoveFromRot(DWORD pdwRegister) const;

	struct IGraphBuilder            *m_pGraph;
	struct IMediaControl            *m_pControl;
	struct IMediaEvent              *m_pEvent;
	struct IMediaFilter             *m_pMFilter;
	struct IBaseFilter              *m_pVmr;
	struct IBaseFilter              *m_pDecoder;
	struct IVMRWindowlessControl9   *m_pWc;
	struct IQualProp                *m_pQualProp;
	struct IVMRMixerBitmap9         *m_pMb;

	HDC                             m_bmpDC;

	QRect                           m_rect;

	DWORD                           m_reg;

	QImage                          m_roi_map;
};
/*#else // #ifdef _WIN32

class QDSWidget : public QWidget
{
public:
    QDSWidget(QWidget * parent = 0, Qt::WindowFlags f = 0) {};
};

#endif // #ifdef _WIN32*/

#endif // DSWIDGET_H
