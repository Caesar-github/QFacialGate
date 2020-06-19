#ifndef VIDEOITEM_H
#define VIDEOITEM_H

#include <rga/rga.h>
#include <rga/RgaApi.h>

#include <QGraphicsObject>
#include <QMutex>

#define NAME_LEN 256
#define MIN_POS_DIFF 10

struct VideoInfo
{
	uchar *buf;
	RgaSURF_FORMAT format;
	int rotate;
	int width;
	int height;
	int pitch;
};

struct FacialInfo
{
	QRect boxRect;
	char fullName[NAME_LEN];
	bool real;
	bool update;
};

struct InfoBox
{
	QRectF infoRect;
	QRectF titleRect;
	QRectF ipRect;
	QRectF timeRect;
	QRectF nameRect;
	QString title;
};

class VideoItem : public QGraphicsObject
{
	Q_OBJECT

public:
	VideoItem(const QRect &rect, QGraphicsItem *parent = 0);
	virtual ~VideoItem();

	QRectF boundingRect() const override;

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0) override;


	void render(uchar *buf, RgaSURF_FORMAT format, int rotate,
			int width, int height, int pitch);

	void setBoxRect(int left, int top, int right, int bottom);

	void setName(char *name, bool real);

private:
	QRect displayRect;
	QTimer *timer;
	struct VideoInfo video;
	struct FacialInfo facial;
	struct InfoBox infoBox;
	int *infoBoxBuf;
	char ip[20];

	QMutex mutex;

	bool drawInfoBox(QPainter *painter);
	void drawBox(QPainter *painter, bool blackList);

	void initTimer();

private slots:
	void timerTimeOut();
	void updateSlots();
};

#endif // VIDEOITEM_H
