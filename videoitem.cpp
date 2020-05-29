#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <arpa/inet.h>
#include <QtWidgets>
#include <QPainter>
#include <QPaintEngine>
#include <QDateTime>

#include "videoitem.h"

VideoItem::VideoItem(const QRect &rect, QGraphicsItem *parent)
	: QGraphicsObject(parent)
{
	char *networkip;

	displayRect = rect;
	infoBox.infoRect.setRect(displayRect.x(), displayRect.height()*4/5, displayRect.width(), displayRect.height()/5);
	infoBox.titleRect = infoBox.infoRect.adjusted(10, 10, 0, 0);
	infoBox.ipRect = infoBox.titleRect.adjusted(40, 70, 0, 0);
	infoBox.timeRect = infoBox.ipRect.adjusted(0, 50, 0, 0);
	infoBox.nameRect = infoBox.timeRect.adjusted(0, 70, 0, 0);
	infoBox.title = tr("人脸识别");
	facial.real = false;
	facial.update = false;
	memset(facial.fullName, 0, NAME_LEN);
	memset(&video, 0, sizeof(struct VideoInfo));

	//initTimer();
}

VideoItem::~VideoItem()
{
	c_RkRgaDeInit();
}

void VideoItem::initTimer()
{
	if(NULL == timer)
		timer = new QTimer;

	timer->setSingleShot(false);
	timer->start(10); //ms
	connect(timer, SIGNAL(timeout()), this, SLOT(timerTimeOut()));
}

void VideoItem::timerTimeOut()
{
	if(video.buf)
		update();
}

void VideoItem::render(uchar *buf, RgaSURF_FORMAT format, int rotate,
				int width, int height, int pitch)
{

	mutex.lock();

#if 0
	//printf fps
	static QTime cb_time;
	static int cb_frames = 0;

	if (!cb_frames)
		cb_time.start();

	if(!(++cb_frames % 50))
		printf("+++++ %s FPS: %2.2f (%u frames in %ds)\n",
				__func__, cb_frames * 1000.0 / cb_time.elapsed(),
				cb_frames, cb_time.elapsed() / 1000);
#endif

	video.buf = buf;
	video.format = format;
	video.rotate = rotate;
	video.width = width;
	video.height = height;
	video.pitch = pitch;

	mutex.unlock();
}

QRectF VideoItem::boundingRect() const
{
	return QRectF(displayRect.x(), displayRect.y(), displayRect.width(), displayRect.height());
}

void VideoItem::updateSlots()
{
#if 1
		//printf fps
		static QTime u_time;
		static int u_frames = 0;
	
		if (!u_frames)
			u_time.start();
	
		if(!(++u_frames % 50))
			printf("***** %s FPS: %2.2f (%u frames in %ds)\n",
					__func__, u_frames * 1000.0 / u_time.elapsed(),
					u_frames, u_time.elapsed() / 1000);
#endif

	update();
}

void VideoItem::setBoxRect(int left, int top, int right, int bottom)
{
	mutex.lock();

	if(abs(facial.boxRect.left() - left) > MIN_POS_DIFF || abs(facial.boxRect.top() - top) > MIN_POS_DIFF
		|| abs(facial.boxRect.right() - right) > MIN_POS_DIFF || abs(facial.boxRect.bottom() - bottom) > MIN_POS_DIFF) {
		facial.boxRect.setCoords(left, top, right, bottom);
		facial.update = true;
	}

	mutex.unlock();
}

void VideoItem::setName(char *name, bool real)
{
	int len;

	mutex.lock();

	if(name) {
		len = strlen(name) > (NAME_LEN - 1) ? (NAME_LEN - 1) : strlen(name);
		if(strncmp(facial.fullName, name, len)) {
			memset(facial.fullName, 0, NAME_LEN);
			strncpy(facial.fullName, name, len);
			facial.update = true;
		}

		if(facial.real != real) {
			facial.real = real;
			facial.update = true;
		}
	} else {
		if(strlen(facial.fullName)) {
			memset(facial.fullName, 0, sizeof(NAME_LEN));
			facial.update = true;
		}
	}

	mutex.unlock();
}

static int getLocalIp(char *interface, char *ip, int ip_len)
{
	int sd;
	struct sockaddr_in sin;
	struct ifreq ifr;

	memset(ip, 0, ip_len);
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == sd) {
		qDebug("socket error: %s\n", strerror(errno));
		return -1;
	}

	strncpy(ifr.ifr_name, interface, IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ - 1] = 0;

	if (ioctl(sd, SIOCGIFADDR, &ifr) < 0) {
		qDebug("ioctl error: %s\n", strerror(errno));
		close(sd);
		return -1;
	}

	memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
	sprintf(ip, "%s", inet_ntoa(sin.sin_addr));

	close(sd);
	return 0;
}

static bool findName(char *fullName, char *name, int nameLen)
{
	bool blackList = false;
	char *end, *begin;

	memset(name, 0, nameLen);
	if(strlen(fullName)) {
		begin = strrchr(fullName, '/');
		end = strrchr(fullName, '.');

		if (begin && end && end > begin)
			memcpy(name, begin + 1, end - begin - 1);
		else
			strcpy(name, "unknown_user");

		if (strstr(fullName, "black_list"))
			blackList = true;
	} 

	return blackList;
}

static int rgaPrepareInfo(uchar *buf, RgaSURF_FORMAT format,
				int width, int height, int pitch, rga_info_t *info)
{
	int bpp;

	memset(info, 0, sizeof(rga_info_t));

	info->fd = -1;
	info->virAddr = buf;
	info->mmuFlag = 1;

	// TODO: Pass format directly
	switch (format) {
	case RK_FORMAT_YCbCr_420_SP:
		bpp = 12;
		break;
	case RK_FORMAT_RGB_565:
		bpp = 16;
		break;
	case RK_FORMAT_BGRA_8888:
		bpp = 32;
		break;
	default:
		return -1;
	}

	rga_set_rect(&info->rect, 0, 0, width, height,
				 pitch * 8 / bpp, height, format);
	return 0;
}

static int rgaDrawImage(uchar *src, RgaSURF_FORMAT src_format,
				int src_width, int src_height, int src_pitch,
				uchar *dst, RgaSURF_FORMAT dst_format,
				int dst_width, int dst_height, int dst_pitch,
				int rotate)
{
	static int rgaSupported = 1;
	static int rgaInited = 0;
	rga_info_t srcInfo;
	rga_info_t dstInfo;

	memset(&srcInfo, 0, sizeof(rga_info_t));
	memset(&dstInfo, 0, sizeof(rga_info_t));

	if (!rgaSupported)
		return -1;

	if (!rgaInited) {
		if (c_RkRgaInit() < 0) {
			rgaSupported = 0;
			return -1;
		}

		rgaInited = 1;
	}

	srcInfo.rotation = 4;
	if (rgaPrepareInfo(src, src_format, src_width, src_height,
			 src_pitch, &srcInfo) < 0)
		return -1;

	if (rgaPrepareInfo(dst, dst_format, dst_width, dst_height,
			 dst_pitch, &dstInfo) < 0)
		return -1;

	srcInfo.rotation = rotate;
	return c_RkRgaBlit(&srcInfo, &dstInfo, NULL);
}

bool VideoItem::drawInfoBox(QPainter *painter)
{
	int flags;
	QFont font;
	bool blackList = false;
	char name[NAME_LEN];
	char ip[20];

	if(facial.boxRect.isEmpty())
		return blackList;

	painter->setPen(QPen(Qt::white, 2));
	painter->setBrush(QColor(153, 51, 250, 100));
	painter->setClipRect(boundingRect());
	painter->drawRect(infoBox.infoRect);

	flags = Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap;

	getLocalIp("eth0", ip, 20);
	if(strlen(ip)) {
		font.setPixelSize(20);
		painter->setFont(font);
		painter->drawText(infoBox.ipRect, flags, QString(ip));
	}

	font.setPixelSize(40);
	font.setBold(true);
	painter->setFont(font);
	painter->drawText(infoBox.titleRect, flags, infoBox.title);

	QDateTime time = QDateTime::currentDateTime();
	QString date = time.toString("yyyy.MM.dd hh:mm:ss");
	//QString date = time.toString("yyyy.MM.dd hh:mm:ss ddd");
	font.setPixelSize(35);
	painter->setFont(font);
	painter->drawText(infoBox.timeRect, flags, date);

	blackList = findName(facial.fullName, name, NAME_LEN);
	if(strlen(name))
		painter->drawText(infoBox.nameRect, flags, QString(name));

	return blackList;
}

void VideoItem::drawBox(QPainter *painter, bool blackList)
{
	int w, h;

	if(facial.boxRect.isEmpty())
		return;

	w = facial.boxRect.right() - facial.boxRect.left();
	h = facial.boxRect.bottom() - facial.boxRect.top();

	if(!strlen(facial.fullName))
		painter->setPen(QPen(Qt::red, 4));
	else if(blackList)
		painter->setPen(QPen(Qt::black, 4));
	else if(facial.real)
		painter->setPen(QPen(Qt::green, 4));
	else
		painter->setPen(QPen(Qt::yellow, 4));

	painter->setBrush(QColor(255, 255, 255, 0));
	painter->drawRect(facial.boxRect.left(), facial.boxRect.top(), w, h);
}

void VideoItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
						QWidget *widget)
{
	bool blackList;

	if(!painter->paintEngine())
		return;

	Q_UNUSED(option);
	Q_UNUSED(widget);

	while(!video.buf)
		usleep(10000);

	mutex.lock();

#if 0
	//printf fps
	static QTime paint_time;
	static int paint_frames = 0;
	if (!paint_frames)
		paint_time.start();

	if(!(++paint_frames % 50))
		printf("----- %s FPS: %2.2f (%u frames in %ds)\n",
				__func__, paint_frames * 1000.0 / paint_time.elapsed(),
				paint_frames, paint_time.elapsed() / 1000);
#endif

	QImage *image = static_cast<QImage *>(painter->paintEngine()->paintDevice());
	if (image->isNull()) {
		mutex.unlock();
		return;
	}

	// TODO: Parse dst format
	//qDebug("image->format: %d", image->format());
#if 1
	//qt5base 0008 patch USE_RGB565
	rgaDrawImage(video.buf, video.format, video.width, video.height, video.pitch,
					image->bits(), RK_FORMAT_RGB_565, image->width(),
					image->height(), image->bytesPerLine(), video.rotate);
#else
	rgaDrawImage(video.buf, video.format, video.width, video.height, video.pitch,
					image->bits(), RK_FORMAT_BGRA_8888, image->width(),
					image->height(), image->bytesPerLine(), video.rotate);
#endif

	blackList = drawInfoBox(painter);
	drawBox(painter, blackList);

	video.buf = NULL;

	mutex.unlock();
	update();
}