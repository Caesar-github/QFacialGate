#ifndef VIDEOITEM_H
#define VIDEOITEM_H

#include <QGraphicsObject>
#include <QMutex>
#include <QImage>

#include <rga/rga.h>
#include <rga/RgaApi.h>
#include <rkfacial/rkfacial.h>
#include "snapshotthread.h"

#ifndef NAME_LEN
#define NAME_LEN 256
#endif

#define MIN_POS_DIFF 10

struct VideoInfo
{
	uchar *buf;
	RgaSURF_FORMAT format;
	int rotate;
	int width;
	int height;
};

struct FacialInfo
{
	QRect boxRect;
	char fullName[NAME_LEN];
	bool real;
	enum user_state state;
};

struct InfoBox
{
	QRectF infoRect;
	QRectF titleRect;
	QRectF ipRect;
	QRectF timeRect;
	QRectF nameRect;
	QRectF snapshotRect;
	QString title;
};

#ifdef BUILD_TEST
struct TestBox
{
	QRectF irDetectRect;
	QRectF irLivenessRect;
	QRectF rgbAlignRect;
	QRectF rgbExtractRect;
	QRectF rgbLandmarkRect;
	QRectF rgbSearchRect;
};

struct TestInfo
{
	bool valid;
	struct TestBox testBox;
	struct test_result testResult;
};
#endif

class VideoItem : public QGraphicsObject
{
	Q_OBJECT

public:
	VideoItem(const QRect &rect, QGraphicsItem *parent = 0);
	virtual ~VideoItem();

	QRectF boundingRect() const override;

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0) override;


	void render(uchar *buf, RgaSURF_FORMAT format, int rotate,
			int width, int height);

	bool setBoxRect(int left, int top, int right, int bottom);

	void setUserInfo(struct user_info *info, bool real);

#ifdef BUILD_TEST
	void setTesIntfo(struct test_result *testResult);
#endif

private:
	QRect displayRect;
	QTimer *timer;
	struct VideoInfo video;
	struct FacialInfo facial;
	struct InfoBox infoBox;
	int *infoBoxBuf;
	char ip[20];

	RgaSURF_FORMAT rgaFormat;
	unsigned int blend;

	QImage defaultSnapshot;
	SnapshotThread *snapshotThread;

	QMutex mutex;

#ifdef BUILD_TEST
	struct TestInfo testInfo;
	void initTestInfo();
	void drawTestInfo(QPainter *painter);
#endif

	void drawInfoBox(QPainter *painter, QImage *image);
	void drawBox(QPainter *painter);
	void drawSnapshot(QPainter *painter, QImage *image);

	void initTimer();

private slots:
	void timerTimeOut();
	void updateSlots();
};

#endif // VIDEOITEM_H
