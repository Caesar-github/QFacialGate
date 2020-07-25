#include <stdio.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <arpa/inet.h>

#include <QtWidgets>
#include <QTouchEvent>
#include <QList>

#include <rkfacial/rkfacial.h>
#ifdef TWO_PLANE
#include <rkfacial/display.h>
#endif
#include "savethread.h"
#include "desktopview.h"

#define CAMERA_WIDTH 1280
#define CAMERA_HEIGHT 720
#define SAVE_FRAMES 30

DesktopView *DesktopView::desktopView = nullptr;

static int getLocalIp(char *interface, char *ip, int ip_len)
{
	int sd;
	struct sockaddr_in sin;
	struct ifreq ifr;

	memset(ip, 0, ip_len);
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == sd) {
		//qDebug("socket error: %s\n", strerror(errno));
		return -1;
	}

	strncpy(ifr.ifr_name, interface, IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ - 1] = 0;

	if (ioctl(sd, SIOCGIFADDR, &ifr) < 0) {
		//qDebug("ioctl error: %s\n", strerror(errno));
		close(sd);
		return -1;
	}

	memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
	sprintf(ip, "%s", inet_ntoa(sin.sin_addr));

	close(sd);
	return 0;
}

void DesktopView::initTimer()
{
	timer = new QTimer;
	timer->setSingleShot(false);
	timer->start(3000); //ms
	connect(timer, SIGNAL(timeout()), this, SLOT(timerTimeOut()));
}

void DesktopView::timerTimeOut()
{
#if 0
	static QTime t_time;
	static int cnt = 1;

	if(cnt) {
		t_time.start();
		cnt--;
	}

	qDebug("%s: %ds", __func__, t_time.elapsed() / 1000);
#endif
	char ip[MAX_IP_LEN];

	getLocalIp("eth0", ip, MAX_IP_LEN);
	if(!strcmp(ip, videoItem->getIp()))
		return;

	videoItem->setIp(ip);
	updateUi();
}

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
					bool flag;
					QRectF rect = touchPoint.rect();
					if (rect.isEmpty())
						break;

					bool settingFlag = rect.y() > groupBox->height()
						&& rect.y() < desktopRect.height()*4/5;
#ifdef BUILD_TEST
					bool testFlag = rect.x() > testGroupBox->width()
						|| (rect.y() < testGroupBox->y()
						|| rect.y() > testGroupBox->y() + testGroupBox->height());

					if(testGroupBox->isVisible())
						flag = settingFlag && testFlag;
					else
#endif
						flag = settingFlag;

					if(flag) {
						if(groupBox->isVisible())
							groupBox->setVisible(false);
						else
							groupBox->setVisible(true);
#ifdef BUILD_TEST
						if(testGroupBox->isVisible())
							testGroupBox->setVisible(false);
						else
							testGroupBox->setVisible(true);
#endif
						updateUi();
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

static void setButtonFormat(QBoxLayout *layout, QPushButton *btn)
{
	btn->setFixedSize(160, 70);
	btn->setStyleSheet("QPushButton{font-size:35px}");
	layout->addWidget(btn);
}

#ifdef BUILD_TEST
void DesktopView::paintTestInfo(struct test_result *test)
{
	desktopView->videoItem->setTesIntfo(test);
	desktopView->collectBtn->setEnabled(true);
	desktopView->realBtn->setEnabled(true);
	desktopView->photoBtn->setEnabled(true);
	desktopView->testing = false;
}

void DesktopView::initTestUi()
{
	QVBoxLayout *vLayout = new QVBoxLayout;
	testGroupBox = new QGroupBox();

	collectBtn = new QPushButton(tr("开始采集"));
	setButtonFormat(vLayout, collectBtn);
	realBtn = new QPushButton(tr("测试真人"));
	setButtonFormat(vLayout, realBtn);
	photoBtn = new QPushButton(tr("测试照片"));
	setButtonFormat(vLayout, photoBtn);

	testGroupBox->setLayout(vLayout);
	testGroupBox->setObjectName("testGroupBox");
	testGroupBox->setStyleSheet("#testGroupBox {background-color:rgba(10, 10, 10,100);}");
	testGroupBox->setWindowOpacity(0.5);
	testGroupBox->setGeometry(QRect(0, desktopRect.height()/3, desktopRect.width()/3  - 20, desktopRect.height()/4));

	connect(collectBtn, SIGNAL(clicked()), this, SLOT(saveAllSlots()));
	connect(realBtn, SIGNAL(clicked()), this, SLOT(saveFakeSlots()));
	connect(photoBtn, SIGNAL(clicked()), this, SLOT(saveRealSlots()));

	testing = false;
	register_get_test_callback(paintTestInfo);
}

static void setSaveIrFlag(bool real, bool fake)
{
	save_ir_real(real);
	save_ir_fake(fake);
}

void DesktopView::saveAllSlots()
{
	if(testing)
		return;

	bool equal = collectBtn->text().compare("开始采集", Qt::CaseSensitive);

	if(!equal) {
		setSaveIrFlag(true, true);
		collectBtn->setText(tr("结束采集"));
		realBtn->setEnabled(false);
		photoBtn->setEnabled(false);
	} else {
		setSaveIrFlag(false, false);
		collectBtn->setText(tr("开始采集"));
		realBtn->setEnabled(true);
		photoBtn->setEnabled(true);
	}
}

void DesktopView::saveRealSlots()
{
	setSaveIrFlag(true, false);
	rockface_start_test();

	testing = true;
	collectBtn->setEnabled(false);
	realBtn->setEnabled(false);
	photoBtn->setEnabled(false);
}

void DesktopView::saveFakeSlots()
{
	setSaveIrFlag(false, true);
	rockface_start_test();

	testing = true;
	collectBtn->setEnabled(false);
	realBtn->setEnabled(false);
	photoBtn->setEnabled(false);
}
#endif

void DesktopView::updateUi()
{
	desktopView->update();
	desktopView->scene()->update();
}

void DesktopView::cameraSwitch()
{
	if(cameraType == ISP) {
		videoItem->setBoxRect(0, 0, -1, -1);
		switchBtn->setText(tr("IR"));
		cameraType = CIF;

#ifdef TWO_PLANE
		display_switch(DISPLAY_VIDEO_IR);
		updateUi();
#endif
	} else {
		switchBtn->setText(tr("RGB"));
		cameraType = ISP;

#ifdef TWO_PLANE
		display_switch(DISPLAY_VIDEO_RGB);
		updateUi();
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

void DesktopView::initUi()
{
	QHBoxLayout *hLayout = new QHBoxLayout;

	groupBox = new QGroupBox();

	registerBtn = new QPushButton(tr("Register"));
	setButtonFormat(hLayout, registerBtn);
	switchBtn = new QPushButton(tr("RGB"));
	setButtonFormat(hLayout, switchBtn);
	saveBtn = new QPushButton(tr("Capture"));
	setButtonFormat(hLayout, saveBtn);
	deleteBtn = new QPushButton(tr("Delete"));
	setButtonFormat(hLayout, deleteBtn);

	groupBox->setLayout(hLayout);
	groupBox->setObjectName("groupBox");
	groupBox->setStyleSheet("#groupBox {background-color:rgba(10, 10, 10,100);}");
	groupBox->setWindowOpacity(0.5);
	//groupBox->setWindowFlags(Qt::FramelessWindowHint);
	groupBox->setGeometry(QRect(0, 0, desktopRect.width(), desktopRect.height()/9 - 40));
}

void DesktopView::iniSignalSlots()
{
	connect(switchBtn, SIGNAL(clicked()), this, SLOT(cameraSwitch()));
	connect(registerBtn, SIGNAL(clicked()), this, SLOT(registerSlots()));
	connect(deleteBtn, SIGNAL(clicked()), this, SLOT(deleteSlots()));
	connect(saveBtn, SIGNAL(clicked()), this, SLOT(saveSlots()));
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
		desktopView->updateUi();
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
	desktopView->updateUi();
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
	desktopView->updateUi();
}

static int DesktopView::initRkfacial(int faceCnt)
{
#ifdef TWO_PLANE
	set_isp_param(CAMERA_WIDTH, CAMERA_HEIGHT, NULL, true);
	set_cif_param(CAMERA_WIDTH, CAMERA_HEIGHT, NULL);
	set_isp_rotation(270);

	display_switch(DISPLAY_VIDEO_RGB);
	if (display_init(desktopRect.width(), desktopRect.height())) {
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

	initUi();
	iniSignalSlots();

	QGraphicsScene *scene = new QGraphicsScene(this);
	scene->setItemIndexMethod(QGraphicsScene::NoIndex);
	scene->addItem(videoItem);
	scene->addWidget(groupBox);
	groupBox->setVisible(false);

#ifdef BUILD_TEST
	initTestUi();
	scene->addWidget(testGroupBox);
	testGroupBox->setVisible(false);
#endif

	scene->setSceneRect(scene->itemsBoundingRect());
	setScene(scene);

	initTimer();
	initRkfacial(faceCnt);
}

DesktopView::~DesktopView()
{
	deinitRkfacial();
	timer->stop();
}
