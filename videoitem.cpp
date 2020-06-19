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
	int r = 153, g = 51, b = 250, a = 100;

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

	int len = infoBox.infoRect.width() * infoBox.infoRect.height();
	int bgra = ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
	infoBoxBuf = (int *)malloc(len * sizeof(int));
	if(infoBoxBuf) {
		for(int i = 0; i < len; i++)
			infoBoxBuf[i] = bgra;
	}

	initTimer();
}

VideoItem::~VideoItem()
{
	c_RkRgaDeInit();
	timer->stop();
	if(infoBoxBuf) {
		free(infoBoxBuf);
		infoBoxBuf = NULL;
	}
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

void VideoItem::initTimer()
{
	timer = new QTimer;
	timer->setSingleShot(false);
	timer->start(3000); //ms
	connect(timer, SIGNAL(timeout()), this, SLOT(timerTimeOut()));
}

void VideoItem::timerTimeOut()
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

	mutex.lock();
	getLocalIp("eth0", ip, 20);
	mutex.unlock();
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

	if(cb_time.elapsed()/1000 >= 10) {
		cb_frames = 0;
		cb_time.restart();
	}

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
#if 0 
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
				int width, int height, int pitch, int x, int y, rga_info_t *info)
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

	rga_set_rect(&info->rect, x, y, width - x, height - y,
				 pitch * 8 / bpp, height, format);
	return 0;
}

static int rgaDrawImage(uchar *src, RgaSURF_FORMAT src_format,
				int src_x, int src_y, int src_width, int src_height, int src_pitch,
				uchar *dst, RgaSURF_FORMAT dst_format, int dst_x, int dst_y,
				int dst_width, int dst_height, int dst_pitch, int rotate, unsigned int blend)
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

	if (rgaPrepareInfo(src, src_format, src_width, src_height,
			 src_pitch, src_x, src_y, &srcInfo) < 0)
		return -1;

	if (rgaPrepareInfo(dst, dst_format, dst_width, dst_height,
			 dst_pitch, dst_x, dst_y, &dstInfo) < 0)
		return -1;

	srcInfo.rotation = rotate;
	if(blend)
		srcInfo.blend = blend;

	return c_RkRgaBlit(&srcInfo, &dstInfo, NULL);
}

bool VideoItem::drawInfoBox(QPainter *painter)
{
	int flags;
	QFont font;
	bool blackList = false;
	char name[NAME_LEN];

	if(facial.boxRect.isEmpty())
		return blackList;

	flags = Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap;
	painter->setPen(QPen(Qt::white, 2));

#ifdef QT_FB_DRM_RGB565
	painter->setBrush(QColor(153, 51, 250, 100));
	painter->setClipRect(boundingRect());
	painter->drawRect(infoBox.infoRect);
#endif

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
	bool blackList = false;
	static int printf_cnt = 1;

	if(!painter->paintEngine())
		return;

	Q_UNUSED(option);
	Q_UNUSED(widget);

	mutex.lock();

#if 0
	//printf fps
	static QTime paint_time;
	static int paint_frames = 0;
	if (!paint_frames)
		paint_time.start();

	if(paint_time.elapsed()/1000 >= 10) {
		paint_frames = 0;
		paint_time.restart();
	}

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
	if(printf_cnt) {
		qDebug("image->format: %d", image->format());
		printf_cnt--;
	}

#ifdef QT_FB_DRM_RGB565
	rgaDrawImage(video.buf, video.format, 0, 0, video.width, video.height, video.pitch,
					image->bits(), RK_FORMAT_RGB_565, 0, 0, image->width(),
					image->height(), image->bytesPerLine(), video.rotate, 0);
#else
	rgaDrawImage(video.buf, video.format, 0, 0, video.width, video.height, video.pitch,
					image->bits(), RK_FORMAT_BGRA_8888, 0, 0, image->width(),
					image->height(), image->bytesPerLine(), video.rotate, 0);

	if(!facial.boxRect.isEmpty() && infoBoxBuf) {
		//unsigned int blend = 0xFF0105;
		unsigned int blend = 0xFF0405;
		rgaDrawImage((uchar *)infoBoxBuf, RK_FORMAT_BGRA_8888, 0, 0,
						infoBox.infoRect.width(), infoBox.infoRect.height(),
						infoBox.infoRect.width() * 4, image->bits(),
						RK_FORMAT_BGRA_8888, infoBox.infoRect.x(), infoBox.infoRect.y(),
						image->width(), image->height(), image->bytesPerLine(), 0, blend);
	}
#endif

	blackList = drawInfoBox(painter);
	drawBox(painter, blackList);

	video.buf = NULL;

	mutex.unlock();
}
