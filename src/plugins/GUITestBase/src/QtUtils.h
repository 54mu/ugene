#ifndef _U2_QT_UTILS_H_
#define _U2_QT_UTILS_H_

#include <U2Core/global.h>
#include <U2Core/Log.h>
#include <U2Gui/MainWindow.h>
#include <U2Core/AppContext.h>
#include <QtGui>
#include <QtCore/QThread>
#include <U2Core/U2OpStatus.h>

namespace U2 {

#define CHECK_SET_ERR(condition, errorMessage) \
	CHECK_SET_ERR_RESULT(condition, errorMessage, )

#define CHECK_SET_ERR_RESULT(condition, errorMessage, result) \
	CHECK_EXT(condition, if (!os.hasError()) {os.setError(errorMessage);}, result)

class QtUtils {
public:

    //mouse primitives
	static void moveTo(U2OpStatus &os, const QString &widgetName, const QPoint &pos = QPoint());
	static void mouseClick(U2OpStatus &os, const QString &widgetName, Qt::MouseButton button, const QPoint &pos = QPoint());
	static void mouseDbClick(U2OpStatus &os, const QString &widgetName, const QPoint &pos = QPoint());
	static void mousePress(U2OpStatus &os, const QString &widgetName, Qt::MouseButton button,const QPoint &pos = QPoint());
	static void mouseRelease(U2OpStatus &os, const QString &widgetName, Qt::MouseButton button, const QPoint &pos = QPoint());

	static void mouseClick(QWidget *w, Qt::MouseButton button, const QPoint &pos = QPoint());
	static void mouseDbClick(QWidget *w, const QPoint &pos = QPoint());
	static void mousePress(QWidget *w, Qt::MouseButton button, const QPoint &pos = QPoint());
	static void mouseRelease(QWidget *w, Qt::MouseButton button, const QPoint &pos = QPoint());

    //keyboard
	static Qt::Key asciiToKey(char ascii);
	static void keyPress(U2OpStatus &os, const QString &widgetName, int key, Qt::KeyboardModifiers modifiers = 0, const QString &text = "");
	static void keyRelease(U2OpStatus &os, const QString &widgetName, int key, Qt::KeyboardModifiers modifiers = 0);
	static void keyClick(U2OpStatus &os, const QString &widgetName, int key, Qt::KeyboardModifiers modifiers = 0, const QString &text = "");
	static void keySequence(U2OpStatus &os, const QString &widgetName, const QString &sequence, Qt::KeyboardModifiers modifiers = 0);

	static void keyPress(QWidget *w, int key, Qt::KeyboardModifiers modifiers = 0, const QString &text = "");
	static void keyRelease(QWidget *w, int key, Qt::KeyboardModifiers modifiers = 0);
	static void keyClick(QWidget *w, int key, Qt::KeyboardModifiers modifiers = 0, const QString &text = "");
	static void keySequence(QWidget *w, const QString &sequence, Qt::KeyboardModifiers modifiers = 0);

    //menu
	static void expandTopLevelMenu(U2OpStatus &os, const QString &menuName, const QString &parentMenu);
	static void clickMenu(U2OpStatus &os, const QString &menuName, const QString &parentMenu);
	static void clickContextMenu(U2OpStatus &os, const QString &menuName);
	static void contextMenu(U2OpStatus &os, const QString &widgetName, const QPoint &pos = QPoint());

    //tree model
	static void mouseClickOnItem(U2OpStatus &os, const QString &widgetName, Qt::MouseButton button, const QPoint &pos = QPoint());
	static void mouseDbClickOnItem(U2OpStatus &os, const QString &widgetName, const QPoint &pos = QPoint());
	static void mousePressOnItem(U2OpStatus &os, const QString &widgetName, Qt::MouseButton button, const QPoint &pos = QPoint());
	static void mouseReleaseOnItem(U2OpStatus &os, const QString &widgetName, Qt::MouseButton button, const QPoint &pos = QPoint());
	static void contextMenuOnItem(U2OpStatus &os, const QString &widgetName, const QPoint &pos = QPoint());
	static QPoint getItemPosition(U2OpStatus &os, const QString &itemName, const QString &treeName);
	static bool isItemExists(U2OpStatus &os, const QString &itemName, const QString &treeName);
	static void expandTreeItem(U2OpStatus &os, const QString &itemName, const QString &treeName);

	static QWidget *findWidgetByName(U2OpStatus &os, const QString &widgetName, const QString &parentName = "");
	static QWidget *findWidgetByTitle(U2OpStatus &os, const QString &widgetTitle);
	static bool isWidgetExists(const QString &widgetName);
	static QMenu   *getContextMenu();
	static QDialog *getActiveDialog();
	static QWidget *getWidgetInFocus();

	static void sendEvent(QObject *obj, QEvent *e);
	static void sleep(int msec);

    //wait for functions
	static bool waitForWidget(U2OpStatus &os, const QString& widgetName, bool show);
	static bool waitForTreeItem(U2OpStatus &os, const QString &itemName, const QString &treeName, bool show);
	static bool waitForMenuWithAction(const QString &actionName);
	static bool waitForTask(Task *t);

private:
	class Waiter: public QThread {
	public:
		static void await(int mseconds) {
			msleep(mseconds);
		}
	};
};

} // U2

#endif