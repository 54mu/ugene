#ifndef _U2_GUI_TEST_H_
#define _U2_GUI_TEST_H_

#include <U2Core/global.h>
#include <QtCore/QTimer>
#include <U2Core/Task.h>

namespace U2 {

class GUITestIgnorable {
public:
    // not ignored test, ignored by all, ignored on windows platforms, ignored on linux platforms
    enum IgnoreStatus {NotIgnored=0x0, Ignored=0x1, IgnoredWindows=0x2, IgnoredLinux=0x4, IgnoredMac=0x8};
    enum IgnoreReason {Bug, System};

    GUITestIgnorable() : ignoreStatus(NotIgnored), ignoreMessage(""), ignoreReason(Bug) {}

    void setIgnored(int status, const QString& message = "") { ignoreStatus = status; ignoreMessage = message; ignoreReason = Bug;}
    void setReason(IgnoreReason _reason){ ignoreReason = _reason; }
    int getIgnoreStatus() const {return ignoreStatus; }
    QString getIgnoreMessage() const {return ignoreMessage; }

    bool isIgnored() const {
        bool ignored = ignoreStatus & Ignored;
        bool platformIgnore = false;

#ifdef _WIN32
        platformIgnore = (ignoreStatus & IgnoredWindows);
#endif

#ifdef __linux__
        platformIgnore = (ignoreStatus & IgnoredLinux);
#endif

#ifdef Q_OS_MAC
        platformIgnore = (ignoreStatus & IgnoredMac);
#endif

        return ignored || platformIgnore;
    }
    IgnoreReason getReason(){return ignoreReason;}

private:
    int ignoreStatus;
    QString ignoreMessage;
    IgnoreReason ignoreReason;
};

class U2TEST_EXPORT GUITest: public QObject, public GUITestIgnorable {
    Q_OBJECT
public:
    GUITest(const QString &_name = "", int _timeout=240000) : name(_name), timeout(_timeout) {}
    virtual ~GUITest(){}

    QString getName() const { return name; }
    int getTimeout() const { return timeout; }
    void setName(const QString &n) { name = n; }
    void setTimeout(int _timeout) { timeout = _timeout; }

    static const QString testDir;
    static const QString dataDir;
    static const QString screenshotDir;
    static const QString sandBoxDir;

    virtual void run(U2OpStatus &os) = 0;
private:
    GUITest(const GUITest&);
    GUITest& operator=(const GUITest&);

    QString name;
    int timeout;
public slots:
    void sl_fail();
};

typedef QList<GUITest*> GUITests;

} //U2

#endif
