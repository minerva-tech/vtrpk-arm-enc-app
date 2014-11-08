#pragma warning(disable : 4995)

#include "ext_headers.h"

#include "pchbarrier.h"

#include "ext_qt_headers.h"
#include <QPolygon>
#include <QFile>
#include <qpaintengine.h>
#include "progress_dialog.h"

#include "qdswidget.h"
#include "logview.h"
#include "errors.h"
#include "defs.h"

#include "portconfig.h"

#include "comm.h"
#include "proto.h"

#include "initguid.h"

#include "minerva_iface_h.h"

DEFINE_GUID(CLSID_MinervaSource, 0x903ff29b, 0x94d, 0x402c, 0x9c, 0x59, 0xb3, 0x4d, 0x38, 0xe2, 0xdc, 0x54);
//DEFINE_GUID(CLSID_BouncingBall, 0xfd501041, 0x8ebe, 0x11ce, 0x81, 0x83, 0x00, 0xaa, 0x00, 0x57, 0x7d, 0xa1);

// {B20CB322-4C08-40AC-BD3E-B832FE602B3D}
DEFINE_GUID(IID_Minerva_Statistics, 0xb20cb322, 0x4c08, 0x40ac, 0xbd, 0x3e, 0xb8, 0x32, 0xfe, 0x60, 0x2b, 0x3d);


bool ROIs::selectPt(const Point& from)
{
	m_pt_sel[0] = m_pt_sel[1] = -1;
	bforeach(ROI& roi, m_rois) {
		m_pt_sel[0]++;
		m_pt_sel[1]=-1;
		bforeach(Point& to, roi) {
			m_pt_sel[1]++;
			if (from.dist2(to) <= selection_radius2) {
				log() << "Point selected at : [" << m_pt_sel[0] << "][" << m_pt_sel[1] << "]";
				return true;
			}
		}
	}
	m_pt_sel[0] = m_pt_sel[1] = -1;

	return false;
}

bool ROIs::selectRoi(const Point& pt)
{
	m_pt_sel[0] = m_pt_sel[1] = -1;

	bforeach(ROI& roi, m_rois) {
		QPolygon poly;

		m_pt_sel[0]++;

		if (roi.size())
			poly.setPoints(roi.size(), &roi.at(0).x);

		if (poly.containsPoint(QPoint(pt.x, pt.y), Qt::OddEvenFill))
			return true;
	}

	m_pt_sel[0] = -1;

	return false;
}

void ROIs::movePt(const Point& to)
{
	if (m_pt_sel[0]<0 || m_pt_sel[1]<0)
		return;

	if (m_rois.size() <= m_pt_sel[0])
		return;

	if (m_rois.at(m_pt_sel[0]).size() <= m_pt_sel[1])
		return;

	log() << "Point moved to : " << to.x << ", " << to.y;

	m_rois[m_pt_sel[0]][m_pt_sel[1]] = to;
}

void ROIs::deletePt()
{
	m_rois[m_pt_sel[0]].erase(m_rois.at(m_pt_sel[0]).begin() + m_pt_sel[1]);

	if (m_rois.at(m_pt_sel[0]).empty())
		m_rois.erase(m_rois.begin() + m_pt_sel[0]);
}

void ROIs::deleteRoi()
{
	m_rois.erase(m_rois.begin() + m_pt_sel[0]);
}

void ROIs::addPt(const Point& pt)
{
	if (m_rois.empty())
		m_rois.push_back(ROI());

	m_rois.back().push_back(pt);

	log() << "Point added : " << pt.x << ", " << pt.y;
}

void ROIs::finishRoi()
{
	if (m_rois.size() && m_rois.back().size())
		m_rois.push_back(ROI());
}

void ROIs::clearRois()
{
	m_rois.clear();
}

bool ROIs::selectedRoi() const
{
	return m_pt_sel[0]>=0;
}

void ROIs::invert()
{
    m_inverted = !m_inverted;
}

bool ROIs::selectedPt() const
{
	return m_pt_sel[0]>=0 && m_pt_sel[1]>=0;
}

void ROIs::draw(QPixmap& pic) const
{
    if (m_inverted)
        pic.fill(QColor(255,0,0,72));
    else
        pic.fill(Qt::black);

	QPainter painter(&pic);
//	painter.begin(&pic);

    if (m_inverted)
        painter.setBrush(Qt::black);
    else
        painter.setBrush(QColor(255,0,0,72));

	bforeach(const ROI& roi, m_rois) {
		QPoint* pts = new QPoint[roi.size()];
		int i=0;
		bforeach(const Point& pt, roi) {
			pts[i].setX(pt.x);
			pts[i].setY(pt.y);
			i++;
		}
		painter.setPen(QPen(QBrush(Qt::red), 3));
		painter.drawPolygon(pts, roi.size());
		painter.setPen(QPen(QBrush(Qt::red), 10));
		painter.drawPoints(pts, roi.size());
		delete[] pts;
	}
	painter.end();
}

void ROIs::serialize(std::vector<uint8_t>& dst) const
{
	assert(m_rois.size() < 256);

    dst.push_back(m_inverted?1:0);

    dst.push_back(std::min(255U, m_rois.size())); // I hardly believe that someone will make more than 255 rois or points.

	bforeach(const ROI& roi, m_rois) {
		assert(roi.size() < 256);

        dst.push_back(std::min(255U, roi.size()));

		bforeach(const Point& pt, roi) {
			dst.push_back(pt.x & 0xff);
			dst.push_back((pt.x >> 8) & 0xff);
			dst.push_back(pt.y & 0xff);
			dst.push_back((pt.y >> 8) & 0xff);
		}
	}
}

template<typename IT>
inline uint8_t inc(IT& it, const IT& end)
{
    const uint8_t v = *it;
    it++;
    if (it == end)

}

#define inc(it) \
    it++; \
    if (it == end) { \
        return; \
    }

void ROIs::deserialize(const std::vector<uint8_t>::iterator& begin, const std::vector<uint8_t>::iterator& end)
{
	m_rois.clear();

	std::vector<uint8_t>::iterator it = begin;

    m_inverted = *it?true:false;
    inc(it);

    m_rois.resize(*it);
    inc(it);

	for (int i=0; i<m_rois.size(); i++) {
//        m_rois[i].resize(*it++);
        const size_t sz = *it;
        inc(it);

        for (int j=0; j<sz; j++) {
//			m_rois[i][j].x = (*it++) + ((*it++)<<8); // huh
//			m_rois[i][j].y = (*it++) + ((*it++)<<8);
            int x = *it;
            inc(it);
            x += *it<<8;
            inc(it);
            int y = *it;
            inc(it);
            y += *it<<8;
            inc(it);

            m_rois[i].push_back(Point(x, y));
		}
	}

    if (it != end)
        m_rois.clear();

//    assert(it == end);
}

/*
bool ROIs::findPt(const Point& from, Point* pt)
{
	bforeach(ROI& roi, m_rois)
		bforeach(Point& to, roi)
			if (from.dist2(to) <= selection_radius2) {
				pt = &to;
				return true;
			}

	return false;
}*/

QDSWidget::QDSWidget(QWidget * parent, Qt::WindowFlags f) :
		QWidget(parent, f),
		m_rect(100, 100, 200, 200),
		m_roi_map(g_roi_map_w, g_roi_map_h, QImage::Format_ARGB32)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAttribute(Qt::WA_NoSystemBackground, true);

	DS_OPT(CoInitialize(NULL));

	m_bmpDC = CreateCompatibleDC( GetDC( (HWND)winId() ) );

	m_roi_map.fill(0);

	buildGraph();
}

QDSWidget::~QDSWidget()
{
	RemoveFromRot(m_reg);

	m_pMb->Release();
	m_pWc->Release();
	m_pVmr->Release();
	m_pDecoder->Release();
	m_pControl->Release();
	m_pEvent->Release();
	m_pGraph->Release();

	CoUninitialize();
}

QPaintEngine* QDSWidget::paintEngine() const
{
    return NULL;
}

const QString QDSWidget::getStatString() const
{
    int fps = 0;
//    m_pQualProp->get_AvgFrameRate(&fps);

	DWORD value1, value2, value3, value4, value5;

	IMinerva_Stistics* pStat;
	DS_OPT(m_pDecoder->QueryInterface(IID_Minerva_Statistics, (void **)&pStat));
    pStat->Get_COM_Statistic(&value1, &value2, &value3, &value4, &value5);

    return	tr("Video: ") + QString().number(value2*8) +tr(" bit/sec") + "\t" + "\t"+
            tr("Telemetry:") + QString().number(value3*8) + tr(" bit/sec") + "\t"+"\t"+
            tr("Frame rate:") + QString().number(value5/100.0, 'f', 2) + tr(" Hz");

    pStat->Release();
}

void QDSWidget::buildGraph()
{
	DS_OPT(CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&m_pGraph));

	DS_OPT(m_pGraph->QueryInterface(IID_IMediaControl, (void **)&m_pControl));
    DS_OPT(m_pGraph->QueryInterface(IID_IMediaEvent, (void **)&m_pEvent));
    DS_OPT(m_pGraph->QueryInterface(IID_IMediaFilter, (void **)&m_pMFilter));

    DS_OPT(CoCreateInstance(CLSID_VideoMixingRenderer9, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&m_pVmr));
//    DS_OPT(CoCreateInstance(CLSID_VideoMixingRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&m_pVmr));

	DS_OPT(m_pGraph->AddFilter(m_pVmr, NULL));

    if (!m_pVmr) {
        log() << "Fatal error: VMR9 cannot be created.";
        return;
    }

	IVMRFilterConfig9* fc;
	DS_OPT(m_pVmr->QueryInterface(IID_IVMRFilterConfig9, (void**)&fc));
	DS_OPT(fc->SetRenderingMode(VMR9Mode_Windowless));

	DS_OPT(fc->SetNumberOfStreams(2));
    DS_OPT(m_pMFilter->SetSyncSource(NULL));
	fc->Release();

	IEnumPins* pevmr = NULL;
	DS_OPT(m_pVmr->EnumPins(&pevmr));

	IPin *ppvmr = NULL;
	while (pevmr->Next(1, &ppvmr, NULL) == S_OK) {
		PIN_DIRECTION pd;
		DS_OPT(ppvmr->QueryDirection(&pd));

		if (pd == PINDIR_INPUT) {
			break;
		}
		
		ppvmr->Release();
	}

    DS_OPT(m_pVmr->QueryInterface(IID_IVMRWindowlessControl9, (void**)&m_pWc));
	DS_OPT(m_pWc->SetVideoClippingWindow((HWND)winId()));

    DS_OPT(m_pVmr->QueryInterface(IID_IVMRMixerBitmap9, (void**)&m_pMb));

//	DS_OPT(m_pGraph->AddSourceFilter(L"C:\\Video\\men_50m_1.avi", L"Source", &src));

    DS_OPT(CoCreateInstance(CLSID_MinervaSource, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&m_pDecoder));

    if (!m_pDecoder) {
        log() << "Fatal error: Source filter cannot be created.";
        return;
    }
	DS_OPT(m_pGraph->AddFilter(m_pDecoder, NULL));
	IBaseFilter* src = m_pDecoder;

    IMinerva_Stistics* pStat;
    DS_OPT(m_pDecoder->QueryInterface(IID_Minerva_Statistics, (void **)&pStat));
    pStat->Set_COM_Instance(&Comm::instance());

	IEnumPins* pe = NULL;
	DS_OPT(src->EnumPins(&pe));

	IPin *pp = NULL;
	while (pe->Next(1, &pp, NULL) == S_OK) {
		PIN_DIRECTION pd;
		DS_OPT(pp->QueryDirection(&pd));

		if (pd == PINDIR_OUTPUT) {
			IMediaFilter *mf;
			IFilterGraph2* fg2;
			DS_OPT(m_pGraph->QueryInterface(IID_IMediaFilter,  (void**)&mf));
			DS_OPT(m_pGraph->QueryInterface(IID_IFilterGraph2, (void**)&fg2));
//			HRESULT hr = m_pGraph->Connect(pp, ppvmr);
//			DS_OPT(m_pGraph->Render(pp)));
//			DS_OPT(m_pGraph->RenderFile(L"C:\\Video\\men_50m_1.avi", NULL)));
//			HRESULT hr = fg2->RenderEx(pp, AM_RENDEREX_RENDERTOEXISTINGRENDERERS, NULL);
			HRESULT hr = fg2->ConnectDirect(pp, ppvmr, NULL);
			ppvmr->Release();
			while (pevmr->Next(1, &ppvmr, NULL) == S_OK) {
				PIN_DIRECTION pd;
				DS_OPT(ppvmr->QueryDirection(&pd));

				if (pd == PINDIR_INPUT) {
					break;
				}

				ppvmr->Release();
			}

			mf->SetSyncSource(NULL);
			fg2->Release();
		}

		pp->Release();
	}

	pevmr->Release();
	ppvmr->Release();
	pe->Release();

	AddToRot(m_pGraph, &m_reg);

//	DS_OPT(m_pGraph->RenderFile(L"C:\\Video\\men_50m_1.avi", NULL)));

	DS_OPT(m_pVmr->QueryInterface(IID_IQualProp, (void**)&m_pQualProp));

    log() << "Play";

	DS_OPT(m_pControl->Run());
}

void QDSWidget::Play()
{
    emit setStartChecked(true);

    Server::SendCommand(Server::Start);

    DS_OPT(m_pControl->Run());
}

void QDSWidget::Stop()
{
    emit setStartChecked(false);

    DS_OPT(m_pControl->Stop());

    Server::SendCommand(Server::Stop);
}

void QDSWidget::DownloadROI()
{
	Stop();

	assert(g_roi_map_w % 8 == 0);

	ProgressDialog progress(tr("Downloading ROI"), QString(), 0, 5, this);

    std::vector<uint8_t> roi = Client::GetROI(&progress);

	if (roi.empty()) {
		Play();
		return;
	}
/*
	for (int y=g_roi_map_h-1; y>=0; y--) { // flipped
        QRgb* line = (QRgb*)m_roi_map.scanLine(y);
        const int pos = (g_roi_map_h-1-y)*g_roi_map_w/8;
        for (int x=g_roi_map_w/8-1; x>=0; x--, line+=8) {
			uint8_t byte = roi.at(pos+x);
			for (int b=0; b<8; b++) {
				const int val = byte & 1 ? 255 : 0;
				line[b] = qRgba(val, 0, 0, val);
				byte >>= 1;
			}
		}
    }*/

	m_rois.deserialize(roi.begin()+g_roi_map_w*g_roi_map_h/8, roi.end());

/*	for (int y=0; y<g_roi_map_h; y++) { // direct order
		QRgb* line = (QRgb*)m_roi_map.scanLine(y);
		const int pos = y*g_roi_map_w/8;
		for (int x=0; x<g_roi_map_w/8; x++, line+=8) {
			uint8_t byte = roi.at(pos+x);
			for (int b=0; b<8; b++) {
				const int val = byte & 1 ? 255 : 0;
				line[7-b] = qRgba(val, 0, 0, val);
				byte >>= 1;
			}
		}
	}*/

	update();

	progress.close();

	Play();

    return;
}

void QDSWidget::UploadROI()
{
	assert(g_roi_map_w % 8 == 0);

    m_rois.finishRoi();

	std::vector<uint8_t> roi;
	roi.resize(g_roi_map_w * g_roi_map_h / 8);

	QPixmap pic(width()-1, height()-1);

	m_rois.draw(pic);

	QImage img = pic.toImage().scaled(g_roi_map_w, g_roi_map_h);

//	for (int y=g_roi_map_h-1; y>=0; y--) { // !!! flipped
    for (int y=0; y<g_roi_map_h; y++) { // !!! flipped
        const QRgb* line_v = (const QRgb*)img.scanLine(y);
        const QRgb* line_m = (const QRgb*)m_roi_map.scanLine(y);
        const int pos = y*g_roi_map_w/8;
//        const int pos = (g_roi_map_h - 1 - y)*g_roi_map_w/8;
//		for (int x=g_roi_map_w/8-1; x>=0; x--) {
        for (int x=0; x<g_roi_map_w/8; x++) {
			uint8_t byte = 0;
			for (int b=0; b<8; b++) {
                byte <<= 1;
//				byte |= (RGB_MASK & *line_v++) == 0 && (RGB_MASK & *line_m++) == 0 ? 0 : 1; // !! EPIC FAIL TWO INCREMENTS AND operator &&. COMPUTATION STOPS AFTER FIRST FAIL
                byte |= (RGB_MASK & *line_v) == 0 && (RGB_MASK & *line_m) == 0 ? 0 : 0x01;
                line_v++;
                line_m++;
			}
			roi[pos+x] = byte;
		}
	}

	m_rois.serialize(roi);

/*	for (int y=0; y<g_roi_map_h; y++) { // not flipped
		const QRgb* line_v = (const QRgb*)img.scanLine(y);
		const QRgb* line_m = (const QRgb*)m_roi_map.scanLine(y);
		const int pos = y*g_roi_map_w/8;
		for (int x=0; x<g_roi_map_w/8; x++) {
			uint8_t byte = 0;
			for (int b=0; b<8; b++) {
				byte <<= 1;
//				byte |= (RGB_MASK & *line_v++) == 0 && (RGB_MASK & *line_m++) == 0 ? 0 : 1; // !! EPIC FAIL TWO INCREMENTS AND operator &&. COMPUTATION STOPS AFTER FIRST FAIL
				byte |= (RGB_MASK & *line_v) == 0 && (RGB_MASK & *line_m) == 0 ? 0 : 1;
				line_v++;
				line_m++;
			}
			roi[pos+x] = byte;
		}
	}*/

	Stop();

	Server::SendROI(roi);

	Play();
}

void QDSWidget::ClearROI()
{
	m_rois.clearRois();
    m_roi_map.fill(0);
    HWND hWnd = (HWND)winId();
    HDC hDC = GetDC(hWnd);
    setROI(hDC);  // todo: FIX IT, DO IT VIA REPAINT()
}

void QDSWidget::ChangeMarkerMode()
{
	IVMRMixerControl9* mc;
	DS_OPT(m_pVmr->QueryInterface(IID_IVMRMixerControl9, (void**)&mc));

	float alpha = 0.0f;

	mc->GetAlpha(1, &alpha);

	alpha += 1.0f;
	
	mc->SetAlpha(1, alpha > 1.01f ? 0.f : alpha);

	mc->Release();
}

/*
void QDSWidget::DownloadROI()
{
	Stop();
	QFile f_roi("roi.bin");
	f_roi.open(QIODevice::ReadOnly);

	assert(g_roi_map_w % 8 == 0);
	assert(f_roi.size() == g_roi_map_w*g_roi_map_h/8);

	const QByteArray ba = f_roi.readAll();

	for (int y=0; y<g_roi_map_h; y++) {
		QRgb* line = (QRgb*)m_roi_map.scanLine(y);
		const int pos = y*g_roi_map_w/8;
		for (int x=0; x<g_roi_map_w/8; x++, line+=8) {
			uint8_t byte = ba.at(pos+x);
			for (int b=0; b<8; b++) {
				const int val = byte & 1 ? 255 : 0;
				line[7-b] = qRgba(val, 0, 0, val);
				byte >>= 1;
			}
		}
	}

	update();
}

void QDSWidget::UploadROI()
{
	QPixmap pic(width()-1, height()-1);

	m_rois.draw(pic);

	QImage img = pic.toImage().scaled(g_roi_map_w, g_roi_map_h);

	assert(g_roi_map_w % 8 == 0);

	QByteArray ba;
	ba.resize(g_roi_map_w*g_roi_map_h/8);

	for (int y=0; y<g_roi_map_h; y++) {
		const QRgb* line_v = (const QRgb*)img.scanLine(y);
		const QRgb* line_m = (const QRgb*)m_roi_map.scanLine(y);
		const int pos = y*g_roi_map_w/8;
		for (int x=0; x<g_roi_map_w/8; x++) {
			uint8_t byte = 0;
			for (int b=0; b<8; b++) {
				byte <<= 1;
//				byte |= (RGB_MASK & *line_v++) == 0 && (RGB_MASK & *line_m++) == 0 ? 0 : 1; // !! EPIC FAIL TWO INCREMENTS AND operator &&. COMPUTATION STOPS AFTER FIRST FAIL
				byte |= (RGB_MASK & *line_v) == 0 && (RGB_MASK & *line_m) == 0 ? 0 : 1; // !! EPIC FAIL TWO INCREMENTS AND operator &&. COMPUTATION STOPS AFTER FIRST FAIL
				line_v++;
				line_m++;
			}
			ba[pos+x] = byte;
		}
	}

	Stop();
	QFile f_roi("roi.bin");
	f_roi.open(QIODevice::WriteOnly | QIODevice::Truncate);

	f_roi.write(ba);
}
*/
void QDSWidget::paintEvent(QPaintEvent* event)
{
    RECT pos;

	pos.top = 0;
    pos.left = 0;
    pos.bottom = height() - 1;
    pos.right = width() - 1;

    HWND hWnd = (HWND)winId();
    HDC hDC = GetDC(hWnd);

	setROI(hDC);

    m_pWc->SetVideoPosition(NULL, &pos);
    m_pWc->RepaintVideo(hWnd, hDC);

	ReleaseDC(hWnd, hDC);
}

void QDSWidget::setROI(HDC hDC)
{
	DeleteDC(m_bmpDC);
	m_bmpDC = CreateCompatibleDC(hDC);

// todo: Massive resources leak below. pic.toWinHBITMAP() produces object which is never deleted. And bmpDC is also never deleted.
	QPixmap pic(width() - 1, height() - 1);

	m_rois.draw(pic);

	QPainter painter(&pic);
	painter.drawImage(0, 0, m_roi_map.scaled(width()-1, height()-1));
	painter.end();

	const HBITMAP bmp = pic.toWinHBITMAP(QPixmap::Alpha);

	SelectObject(m_bmpDC, bmp);

	_VMR9AlphaBitmap bitmap;

	bitmap.dwFlags = VMR9AlphaBitmap_hDC | VMR9AlphaBitmap_SrcColorKey;
	bitmap.hdc = m_bmpDC;
	bitmap.pDDS = NULL;
	bitmap.rSrc.top = 0;
	bitmap.rSrc.left = 0;
	bitmap.rSrc.bottom = height() - 1;
	bitmap.rSrc.right = width() - 1;
	bitmap.rDest.top = 0;
	bitmap.rDest.left = 0;
	bitmap.rDest.bottom = 1.0f;
	bitmap.rDest.right = 1.0f;
	bitmap.fAlpha = 0.5f;
	bitmap.clrSrcKey = 0x000000;
	bitmap.dwFilterMode = MixerPref9_AnisotropicFiltering;//MixerPref9_GaussianQuadFiltering;

	DS_OPT(m_pMb->SetAlphaBitmap(&bitmap));

	OAFilterState state;
	if (m_pControl->GetState(0, &state)==S_OK && state == State_Stopped)
		m_pControl->StopWhenReady();
}

void QDSWidget::showPopup(const QPoint& pt)
{
	QMenu menu;

    QAction* invert = menu.addAction(tr("Invert ROI"));

	QAction* finish = menu.addAction(tr("Finish ROI"));

	QAction* del = m_rois.selectRoi(pt) ? menu.addAction(tr("Delete ROI")) : NULL;

	QAction* act = menu.exec(mapToGlobal(pt));

    if (act == invert)
        m_rois.invert();
    else if (act == finish)
		m_rois.finishRoi();
	else if (m_rois.selectedRoi() && act == del)
		m_rois.deleteRoi();
}

void QDSWidget::mousePressEvent(QMouseEvent* event)
{
/*	m_rect.setLeft(event->pos().x());
	m_rect.setTop(event->pos().y());
	m_track = true;*/

	if (event->buttons() & Qt::RightButton)
		if (m_rois.selectPt(event->pos()))
			m_rois.deletePt();
		else
			showPopup(event->pos()); // yeah, i know about customContextMenuRequested signal, and it's connection to some slot, i just don't understand why is it necessary right here.

	if (event->buttons() & Qt::LeftButton)
		if (!m_rois.selectPt(event->pos()))
			m_rois.addPt(event->pos());

	HWND hWnd = (HWND)winId();
	HDC hDC = GetDC(hWnd);
	setROI(hDC);
	ReleaseDC(hWnd, hDC);
}

void QDSWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (event->buttons() & Qt::LeftButton && m_rois.selectedPt()) {
		m_rois.movePt(Point(event->x(), event->y()));

		HWND hWnd = (HWND)winId();
		HDC hDC = GetDC(hWnd);
		setROI(hDC);
		ReleaseDC(hWnd, hDC);
	}
	/*
	if (m_track) {
		m_rect.setRight(event->pos().x());
		m_rect.setBottom(event->pos().y());

		HWND hWnd = (HWND)winId();
		HDC hDC = GetDC(hWnd);
		setROI(hDC);
		ReleaseDC(hWnd, hDC);
	}*/
}

void QDSWidget::mouseReleaseEvent(QMouseEvent* event)
{
}

void QDSWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	m_rois.finishRoi();
}

HRESULT QDSWidget::AddToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) const
{
    IMoniker * pMoniker = NULL;
    IRunningObjectTable *pROT = NULL;

    if (FAILED(GetRunningObjectTable(0, &pROT))) 
    {
        return E_FAIL;
    }
    
    const size_t STRING_LENGTH = 256;

    WCHAR wsz[STRING_LENGTH];

	StringCchPrintfW(
        wsz, STRING_LENGTH, 
        L"FilterGraph %08x pid %08x", 
        (DWORD_PTR)pUnkGraph, 
        GetCurrentProcessId()
        );

    HRESULT hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if (SUCCEEDED(hr)) 
    {
        hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pUnkGraph,
            pMoniker, pdwRegister);
        pMoniker->Release();
    }
    pROT->Release();

    return hr;
}

HRESULT QDSWidget::RemoveFromRot(DWORD pdwRegister) const
{
    IRunningObjectTable *pROT;
    if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) {
        pROT->Revoke(pdwRegister);
        pROT->Release();
    }

	return S_OK;
}
