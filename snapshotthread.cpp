#include <rkfacial/turbojpeg_decode.h>
#include "snapshotthread.h"

SnapshotThread::SnapshotThread()
{
	memset(fullName, 0, NAME_LEN);
	snapshot = NULL;
	width = 0;
	height = 0;
	bytes = 0;
}

bool SnapshotThread::setName(char *name)
{
	if(!name)
		return false;

	int len = strlen(name) > (NAME_LEN - 1) ? (NAME_LEN - 1) : strlen(name);
	if(strncmp(fullName, name, len)) {
		mutex.lock();

		if(snapshot) {
			turbojpeg_decode_put(snapshot);
			snapshot = NULL;
		}

		memset(fullName, 0, NAME_LEN);
		strncpy(fullName, name, len);

		mutex.unlock();
		return true;
	}

	return false;
}

int SnapshotThread::snapshotWidth(){
	return width;
}

int SnapshotThread::snapshotHeight()
{
	return height;
}

int SnapshotThread::snapshotBytesPerLine()
{
	return bytes;
}

char *SnapshotThread::snapshotBuf()
{
	if(isRunning())
		return NULL;

	return snapshot;
}

void SnapshotThread::run()
{
	mutex.lock();

	if(snapshot) {
		turbojpeg_decode_put(snapshot);
		snapshot = NULL;
	}

	snapshot = (char *)turbojpeg_decode_get(fullName, &width, &height, &bytes);
	if(!snapshot)
		qDebug("SnapshotThread::run: turbojpeg_decode_get failed");

	mutex.unlock();
}
