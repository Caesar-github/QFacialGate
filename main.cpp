#include <QtWidgets/QtWidgets>
#include "desktopview.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	Q_INIT_RESOURCE(QFacialGate);

	DesktopView desktop;
	desktop.show();

	return a.exec();
}
