#ifndef DESKTOPVIEW_H
#define DESKTOPVIEW_H

#include <QGraphicsView>
#include <QGroupBox>
#include <QPushButton>
#include <QTouchEvent>

#include "videoitem.h"

typedef enum {
	ISP,
	CIF
} CAMERA_TYPE;

class DesktopView : public QGraphicsView
{
	Q_OBJECT

public:
	static DesktopView *desktopView;

	explicit DesktopView(QWidget *parent = 0);
	virtual ~DesktopView();

protected:
	//bool event(QEvent *event);

private:
	QGroupBox *groupBox;
	QPushButton *switchBtn;
	QPushButton *registerBtn;
	QPushButton *deleteBtn;

	QRect desktopRect;
	VideoItem *videoItem;

	CAMERA_TYPE cameraType;

	void initSwitchUi();
	void iniSignalSlots();

	int initRkfacial();
	void deinitRkfacial();

	static void paintBox(int left, int top, int right, int bottom);
	static void paintName(char *name, bool real);

	static void displayIsp(void *src_ptr, int src_fd, int src_fmt, int src_w, int src_h, int rotation);
	static void displayCif(void *src_ptr, int src_fd, int src_fmt, int src_w, int src_h, int rotation);

signals: 
	void updateVideo();

private slots:
	void cameraSwitch();
	void registerSlots();
	void deleteSlots();
};

#endif // DESKTOPVIEW_H
