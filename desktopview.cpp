#include <stdio.h>
#include <sys/time.h>

#include <QtWidgets>
#include <QTouchEvent>
#include <QList>

#include <rkfacial/rkfacial.h>
#include "savethread.h"
#include "desktopview.h"

#define CAMERA_WIDTH 1280
#define CAMERA_HEIGHT 720
#define SAVE_FRAMES 30

DesktopView *DesktopView::desktopView = nullptr;

bool DesktopView::event(QEvent *event)
{
	switch(event->type()) {
		case QEvent::TouchBegin:
		case QEvent::TouchUpdate:
		case QEvent::TouchEnd:
		{
			QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
			QList<QTouchEvent::TouchPoint> touchPoints = touchEvent->touchPoints();
			if(touchPoints.count() != 1)
				break;

			const QTouchEvent::TouchPoint &touchPoint = touchPoints.first();
			switch (touchPoint.state()) {
				case Qt::TouchPointStationary:
				case Qt::TouchPointReleased:
					// don't do anything if this touch point hasn't moved or has been released
					break;
				default:
				{
					QRectF rect = touchPoint.rect();
					if (rect.isEmpty())
						break;

					if(rect.y() > groupBox->height() && rect.y() < desktopRect.height()*4/5) {
						if(groupBox->isVisible())
							groupBox->setVisible(false);
						else
							groupBox->setVisible(true);
					}
					break;
				}
			}
			break;
		}

		default:
			break;
	}

	return QGraphicsView::event(event);
}

void DesktopView::cameraSwitch()
{
	if(cameraType == ISP) {
		videoItem->setBoxRect(0, 0, -1, -1);
		switchBtn->setText(tr("IR"));
		cameraType = CIF;

#ifdef TWO_PLANE
		display_switch(DISPLAY_VIDEO_IR);
#endif
	} else {
		switchBtn->setText(tr("RGB"));
		cameraType = ISP;

#ifdef TWO_PLANE
		display_switch(DISPLAY_VIDEO_RGB);
#endif
	}
}

void DesktopView::registerSlots()
{
	rkfacial_register();
}

void DesktopView::deleteSlots()
{
	rkfacial_delete();
}

void DesktopView::saveSlots()
{
	saving = true;
	saveBtn->setEnabled(false);
	switchBtn->setEnabled(false);
}

void DesktopView::initSwitchUi()
{
	QHBoxLayout *hLayout = new QHBoxLayout;

	groupBox = new QGroupBox();

	registerBtn = new QPushButton(tr("Register"));
	registerBtn->setFixedSize(160, 80);
	registerBtn->setStyleSheet("QPushButton{font-size:35px}");
	hLayout->addWidget(registerBtn);

	switchBtn = new QPushButton(tr("RGB"));
	switchBtn->setFixedSize(160, 80);
	switchBtn->setStyleSheet("QPushButton{font-size:35px}");
	hLayout->addWidget(switchBtn);

	saveBtn = new QPushButton(tr("Capture"));
	saveBtn->setFixedSize(160, 80);
	saveBtn->setStyleSheet("QPushButton{font-size:35px}");
	hLayout->addWidget(saveBtn);

	deleteBtn = new QPushButton(tr("Delete"));
	deleteBtn->setFixedSize(160, 80);
	deleteBtn->setStyleSheet("QPushButton{font-size:35px}");
	hLayout->addWidget(deleteBtn);

	groupBox->setLayout(hLayout);

	groupBox->setObjectName("groupBox");
	groupBox->setStyleSheet("#groupBox {background-color:rgba(10, 10, 10,100);}");
	groupBox->setWindowOpacity(0.5);
	//groupBox->setWindowFlags(Qt::FramelessWindowHint);
	groupBox->setGeometry(QRect(0, 0, desktopRect.width(), desktopRect.height()/10));
}

void DesktopView::iniSignalSlots()
{
	connect(switchBtn, SIGNAL(clicked()), this, SLOT(cameraSwitch()));
	connect(registerBtn, SIGNAL(clicked()), this, SLOT(registerSlots()));
	connect(deleteBtn, SIGNAL(clicked()), this, SLOT(deleteSlots()));
	connect(saveBtn, SIGNAL(clicked()), this, SLOT(saveSlots()));
	//connect(this, SIGNAL(updateVideo()), videoItem, SLOT(updateSlots()));
	//connect(this, SIGNAL(updateVideo()), videoItem, SLOT(updateSlots()), Qt::BlockingQueuedConnection);
}

static bool coordIsVaild(int left, int top, int right, int bottom)
{
	if(left < 0 || top < 0 || right < 0 || bottom < 0) {
		qDebug("%s: invalid rect(%d, %d, %d, %d)", __func__, left, top, right, bottom);
		return false;
	}

	if(left > right || top > bottom) {
		qDebug("%s: invalid rect(%d, %d, %d, %d)", __func__, left, top, right, bottom);
		return false;
	}

	return true;
}

void DesktopView::paintBox(int left, int top, int right, int bottom)
{
	bool ret;

	if(desktopView->cameraType == CIF)
		return;

	if(!coordIsVaild(left, top, right, bottom))
		return;

	if(!left && !top && !right && !bottom) {
		ret = desktopView->videoItem->setBoxRect(0, 0, -1, -1);
		goto update_paint;
	}

	ret = desktopView->videoItem->setBoxRect(left, top, right, bottom);

update_paint:
#ifdef TWO_PLANE
	if(ret) {
		desktopView->update();
		desktopView->scene()->update();
	}
#endif
	return;
}

void DesktopView::paintInfo(struct user_info *info, bool real)
{
	if(desktopView->cameraType == CIF)
		return;

	desktopView->videoItem->setUserInfo(info, real);
}

void DesktopView::saveFile(uchar *buf, int len, uchar *flag)
{
	if(!saving)
		return;

	if(saveFrames) {
		SaveThread *thread = new SaveThread(buf, len, flag, saveFrames);
		thread->start();
		saveFrames--;
	} else {
		saveFrames = SAVE_FRAMES;
		saving = false;
		saveBtn->setEnabled(true);
		switchBtn->setEnabled(true);
	}
}

void DesktopView::displayIsp(void *src_ptr, int src_fd, int src_fmt, int src_w, int src_h, int rotation)
{
	if(desktopView->cameraType != ISP)
		return;

	if (src_fmt != RK_FORMAT_YCbCr_420_SP) {
		qDebug("%s: src_fmt = %d", __func__, src_fmt);
		return;
	}

	desktopView->saveFile((uchar *)src_ptr, src_w * src_h * 3 / 2, "rgb");

	//qDebug("%s, tid(%lu)\n", __func__, pthread_self());
	desktopView->videoItem->render((uchar *)src_ptr, src_fmt, rotation,
						src_w, src_h);
	desktopView->update();
	desktopView->scene()->update();
}

void DesktopView::displayCif(void *src_ptr, int src_fd, int src_fmt, int src_w, int src_h, int rotation)
{
	if(desktopView->cameraType != CIF)
		return;

	if (src_fmt != RK_FORMAT_YCbCr_420_SP) {
		qDebug("%s: src_fmt = %d", __func__, src_fmt);
		return;
	}

	desktopView->saveFile((uchar *)src_ptr, src_w * src_h * 3 / 2, "ir");

	desktopView->videoItem->render((uchar *)src_ptr, src_fmt, rotation,
						src_w, src_h);
	desktopView->update();
	desktopView->scene()->update();
}

static int DesktopView::initRkfacial(int faceCnt)
{
#ifdef TWO_PLANE
	set_isp_param(CAMERA_WIDTH, CAMERA_HEIGHT, NULL, true);
	set_cif_param(CAMERA_WIDTH, CAMERA_HEIGHT, NULL);

	display_switch(DISPLAY_VIDEO_RGB);
	if (display_init(720, 1280)) {
		qDebug("%s: display_init failed", __func__);
		return -1;
	}
#else
	set_isp_param(CAMERA_WIDTH, CAMERA_HEIGHT, displayIsp, true);
	set_cif_param(CAMERA_WIDTH, CAMERA_HEIGHT, displayCif);
#endif

	set_face_param(CAMERA_WIDTH, CAMERA_HEIGHT, faceCnt);

	register_rkfacial_paint_box(paintBox);
	register_rkfacial_paint_info(paintInfo);

	if(rkfacial_init() < 0) {
		qDebug("%s: rkfacial_init failed", __func__);
		return -1;
	}
	return 0;
}

void DesktopView::deinitRkfacial()
{
	rkfacial_exit();

#ifdef TWO_PLANE
	display_exit();
#endif
}

DesktopView::DesktopView(int faceCnt, QWidget *parent)
	: QGraphicsView(parent)
{
	desktopView = this;
	cameraType = ISP;
	saveFrames = SAVE_FRAMES;
	saving = false;

#ifdef TWO_PLANE
	this->setStyleSheet("background: transparent");
#endif

	QDesktopWidget *desktopWidget = QApplication::desktop();
	desktopRect = desktopWidget->availableGeometry();
	qDebug("DesktopView Rect(%d, %d, %d, %d)", desktopRect.x(), desktopRect.y(),
		desktopRect.width(), desktopRect.height());

	resize(desktopRect.width(), desktopRect.height());
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setAttribute(Qt::WA_AcceptTouchEvents,true);

	videoItem = new VideoItem(desktopRect);
	videoItem->setZValue(0);

	initSwitchUi();
	iniSignalSlots();

	QGraphicsScene *scene = new QGraphicsScene(this);
	scene->setItemIndexMethod(QGraphicsScene::NoIndex);
	scene->addItem(videoItem);
	scene->addWidget(groupBox);
#ifdef ONE_PLANE
	groupBox->setVisible(false);
#endif
	scene->setSceneRect(scene->itemsBoundingRect());
	setScene(scene);

	initRkfacial(faceCnt);
}

DesktopView::~DesktopView()
{
	deinitRkfacial();
}
