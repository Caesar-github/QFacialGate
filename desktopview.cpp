#include <stdio.h>
#include <sys/time.h>

#include <QtWidgets>
#include <rkfacial/rkfacial.h>

#include "desktopview.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define FACE_CNT 30000

DesktopView *DesktopView::desktopView = nullptr;

#if 0
bool DesktopView::event(QEvent *event)
{
	//qDebug("%s, +++++ tid(%lu)\n", __func__, pthread_self());
	switch(event->type()) {
		case QEvent::TouchBegin:
		case QEvent::TouchUpdate:
		case QEvent::TouchEnd:
			qDebug("-----event->type: %d", event->type());
			break;

		default:
			break;
	}

	return QGraphicsView::event(event);
}
#endif

void DesktopView::cameraSwitch()
{
	if(cameraType == ISP) {
		videoItem->setBoxRect(0, 0, -1, -1);
		videoItem->setName(NULL, false);
		switchBtn->setText(tr("IR"));
		cameraType = CIF;
	} else {
		switchBtn->setText(tr("RGB"));
		cameraType = ISP;
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

void DesktopView::initSwitchUi()
{
	QHBoxLayout *hLayout = new QHBoxLayout;

	groupBox = new QGroupBox();

	registerBtn = new QPushButton(tr("Register"));
	hLayout->addWidget(registerBtn);

	switchBtn = new QPushButton(tr("RGB"));
	hLayout->addWidget(switchBtn);

	deleteBtn = new QPushButton(tr("Delete"));
	hLayout->addWidget(deleteBtn);

	groupBox->setLayout(hLayout);

	groupBox->setObjectName("groupBox");
	groupBox->setStyleSheet("#groupBox {background-color:rgba(10, 10, 10,100);}");
	groupBox->setWindowOpacity(0.5);

	//switchGroupBox->setWindowFlags(Qt::FramelessWindowHint);
	groupBox->setGeometry(QRect(0, 0, desktopRect.width(), 30));
}

void DesktopView::iniSignalSlots()
{
	connect(switchBtn, SIGNAL(clicked()), this, SLOT(cameraSwitch()));
	connect(registerBtn, SIGNAL(clicked()), this, SLOT(registerSlots()));
	connect(deleteBtn, SIGNAL(clicked()), this, SLOT(deleteSlots()));
	//connect(this, SIGNAL(updateVideo()), videoItem, SLOT(updateSlots()));
	//connect(this, SIGNAL(updateVideo()), videoItem, SLOT(updateSlots()), Qt::BlockingQueuedConnection);
}

static bool coordIsVaild(int left, int top, int right, int bottom)
{
	if(left < 0 || top < 0 || right < 0 || bottom < 0) {
		qDebug("%s: invalid rect(%d, %d, %d, %d)", left, top, right, bottom);
		return false;
	}

	if(left > right || top > bottom) {
		qDebug("%s: invalid rect(%d, %d, %d, %d)", left, top, right, bottom);
		return false;
	}

	return true;
}

void DesktopView::paintBox(int left, int top, int right, int bottom)
{
	if(desktopView->cameraType == CIF)
		return;

	if(!coordIsVaild(left, top, right, bottom))
		return;

	if(!left && !top && !right && !bottom) {
		desktopView->videoItem->setBoxRect(0, 0, -1, -1);
		desktopView->videoItem->setName(NULL, false);
		return;
	}

	desktopView->videoItem->setBoxRect(left, top, right, bottom);
}

void DesktopView::paintName(char *name, bool real)
{
	if(desktopView->cameraType == CIF)
		return;

	desktopView->videoItem->setName(name, real);
}

void DesktopView::displayIsp(void *src_ptr, int src_fd, int src_fmt, int src_w, int src_h, int rotation)
{
	if(desktopView->cameraType != ISP)
		return;

	if (src_fmt != RK_FORMAT_YCbCr_420_SP) {
		qDebug("%s: src_fmt = %d", __func__, src_fmt);
		return;
	}

	//qDebug("%s, tid(%lu)\n", __func__, pthread_self());
	desktopView->videoItem->render(src_ptr, src_fmt, rotation,
						src_w, src_h, src_w * 3 / 2);
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

	desktopView->videoItem->render(src_ptr, src_fmt, rotation,
						src_w, src_h, src_w * 3 / 2);
	desktopView->scene()->update();
}

static 

int DesktopView::initRkfacial()
{
	set_isp_param(SCREEN_WIDTH, SCREEN_HEIGHT, displayIsp, true);
	set_cif_param(SCREEN_WIDTH, SCREEN_HEIGHT, displayCif);

	set_face_param(SCREEN_WIDTH, SCREEN_HEIGHT, FACE_CNT);

	register_rkfacial_paint_box(paintBox);
	register_rkfacial_paint_name(paintName);

	if(rkfacial_init() < 0) {
		qDebug("%s: rkfacial_init failed", __func__);
		return -1;
	}
	return 0;
}

void DesktopView::deinitRkfacial()
{
	rkfacial_exit();
}

DesktopView::DesktopView(QWidget *parent)
	: QGraphicsView(parent)
{
	desktopView = this;
	cameraType = ISP;

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
	scene->setSceneRect(scene->itemsBoundingRect());
	setScene(scene);

	initRkfacial();
}

DesktopView::~DesktopView()
{
	deinitRkfacial();
}
