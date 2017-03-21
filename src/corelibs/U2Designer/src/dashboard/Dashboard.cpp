/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2016 UniPro <ugene@unipro.ru>
 * http://ugene.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QMessageBox>

#include <QtWebSockets/QWebSocketServer>
#include <QtWebChannel/QWebChannel>

#include <QDesktopServices>

#if (QT_VERSION < 0x050500) //Qt 5
#include <U2Gui/WebSocketClientWrapper.h>
#include <U2Gui/WebSocketTransport.h>
#endif // endif

#include <QClipboard>

#include <U2Core/AppContext.h>
#include <U2Core/AppSettings.h>
#include <U2Core/UserApplicationsSettings.h>
#include <U2Core/GUrlUtils.h>
#include <U2Core/ProjectModel.h>
#include <U2Core/Task.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/U2SafePoints.h>

#include <U2Gui/MainWindow.h>

#include <U2Lang/WorkflowSettings.h>
#include <U2Lang/URLAttribute.h>
#include <U2Lang/URLContainer.h>
#include <U2Lang/WorkflowUtils.h>

#include "Dashboard.h"

namespace U2 {

static const QString REPORT_SUB_DIR("report/");
static const QString DB_FILE_NAME("dashboard.html");
static const QString SETTINGS_FILE_NAME("settings.ini");
static const QString OPENED_SETTING("opened");
static const QString NAME_SETTING("name");

/************************************************************************/
/* Dashboard */
/************************************************************************/
Dashboard::Dashboard(const WorkflowMonitor *monitor, const QString &_name, QWidget *parent)
    : QWebEngineView(parent), loaded(false), name(_name), opened(true), _monitor(monitor), initialized(false), workflowInProgress(true)
{
    connect(this, SIGNAL(loadFinished(bool)), SLOT(sl_loaded(bool)));
    connect(_monitor, SIGNAL(si_report()), SLOT(sl_serialize()));
    connect(_monitor, SIGNAL(si_dirSet(const QString &)), SLOT(sl_setDirectory(const QString &)));
    connect(_monitor, SIGNAL(si_taskStateChanged(Monitor::TaskState)), SLOT(sl_workflowStateChanged(Monitor::TaskState)));

    dashboardPageController = new DashboardPageController(this);
    connect(_monitor, SIGNAL(si_progressChanged(int)), dashboardPageController, SLOT(sl_progressChanged(int)));
    connect(_monitor, SIGNAL(si_taskStateChanged(U2::Workflow::Monitor::TaskState)), dashboardPageController, SLOT(sl_taskStateChanged(U2::Workflow::Monitor::TaskState)));
    connect(_monitor, SIGNAL(si_workerInfoChanged(const QString &, const U2::Workflow::Monitor::WorkerInfo &)),
        dashboardPageController, SLOT(sl_workerInfoChanged(const QString &, const U2::Workflow::Monitor::WorkerInfo &)));
    connect(_monitor, SIGNAL(si_updateProducers()), dashboardPageController, SLOT(sl_workerStatsUpdate()));
    connect(_monitor, SIGNAL(si_newOutputFile(const U2::Workflow::Monitor::FileInfo &)),
        dashboardPageController, SLOT(sl_newOutputFile(const U2::Workflow::Monitor::FileInfo &)));
    connect(_monitor, SIGNAL(si_logChanged(U2::Workflow::Monitor::LogEntry)), dashboardPageController, SLOT(sl_onLogChanged(U2::Workflow::Monitor::LogEntry)));

    setContextMenuPolicy(Qt::NoContextMenu);
    loadUrl = "qrc:///U2Designer/html/Dashboard_webengine.html";
    loadDocument();
    setObjectName("Dashboard");
}

Dashboard::Dashboard(const QString &dirPath, QWidget *parent)
    : QWebEngineView(parent), loaded(false), dir(dirPath), opened(true), _monitor(NULL), initialized(false), workflowInProgress(false)
{
    dashboardPageController = new DashboardPageController(this);
    connect(this, SIGNAL(loadFinished(bool)), SLOT(sl_loaded(bool)));
    setContextMenuPolicy(Qt::NoContextMenu);
    loadUrl = dir + REPORT_SUB_DIR + DB_FILE_NAME;
    loadSettings();
    saveSettings();
}

Dashboard::~Dashboard() {
}

void Dashboard::onShow() {
    CHECK(!loaded, );
    loadDocument();
}

void Dashboard::sl_setDirectory(const QString &value) {
    dir = value;
    U2OpStatus2Log os;
    saveSettings();
}

void Dashboard::sl_workflowStateChanged(Monitor::TaskState state) {
    workflowInProgress = (state == Monitor::RUNNING) || (state == Monitor::RUNNING_WITH_PROBLEMS);
    if (!workflowInProgress) {
        emit si_workflowStateChanged(workflowInProgress);
    }
}

void Dashboard::setClosed() {
    opened = false;
    U2OpStatus2Log os;
    saveSettings();
}

QString Dashboard::directory() const {
    return dir;
}

QString Dashboard::getName() const {
    return name;
}

void Dashboard::setName(const QString &value) {
    name = value;
    saveSettings();
}

void Dashboard::loadDocument() {
    loaded = true;
#if (QT_VERSION < 0x050500) //Qt 5.7 TODO: recheck local files loadUrl
    server = new QWebSocketServer(QStringLiteral("UGENE Standalone Server"), QWebSocketServer::NonSecureMode, this);
    port = 12346;
    while (!server->listen(QHostAddress::LocalHost, port)) {//TODO: need more useful solution
        port++;
    }

    clientWrapper = new WebSocketClientWrapper(server, this);

    QWebEnginePage *pg = new QWebEnginePage(parentWidget());
    QUrl abs;
    if(!loadUrl.startsWith("qrc")){
        abs = QUrl("file://"+loadUrl);
    }else{
        abs = QUrl(loadUrl);
    }
    pg->load(abs);
    setPage(pg);

    channel = new QWebChannel(this);

    QObject::connect(clientWrapper, &WebSocketClientWrapper::clientConnected,
        channel, &QWebChannel::connectTo);

    channel->registerObject(QString("agent"), dashboardPageController->getAgent());

#else
    QWebEnginePage *pg = new QWebEnginePage(parentWidget());
    QUrl abs;
    if(!loadUrl.startsWith("qrc")){
        abs = QUrl("file://"+loadUrl);
    }else{
        abs = QUrl(loadUrl);
    }
    setPage(pg);
    pg->load(abs);

    channel = new QWebChannel(page());
    page()->setWebChannel(channel);
    channel->registerObject(QString("agent"), dashboardPageController->getAgent());
#endif
}

void Dashboard::sl_loaded(bool ok) {
    CHECK(!initialized, );
    SAFE_POINT(ok, "Loaded with errors", );
    initialized = true;

#if (QT_VERSION < 0x050500)
    page()->runJavaScript("installWebChannel(true," + QString((NULL != monitor()) ? "true" : "false") + "," + QString::number(port) + ")");
#else
    page()->runJavaScript("installWebChannel(false," + QString((NULL != monitor()) ? "true" : "false") + ")");
#endif

    if (NULL != monitor()) {
        connect(monitor(), SIGNAL(si_newProblem(Problem,int)), dashboardPageController, SLOT(sl_newProblem(Problem,int)));
        if (!monitor()->getProblems().isEmpty()) {
            foreach (Problem problem, monitor()->getProblems()) {
                dashboardPageController->sl_newProblem(problem, 0);//TODO: fix count of problems
            }
        }

        connect(monitor(), SIGNAL(si_runStateChanged(bool)), SLOT(sl_runStateChanged(bool)));
        dashboardPageController->setDataReady();
        dashboardPageController->sl_pageLoaded();
    }
    if (!WorkflowSettings::isShowLoadButtonHint()) {
        page()->runJavaScript("hideLoadBtnHint()");
    }
}

void Dashboard::sl_serialize() {
    QCoreApplication::processEvents();
    QString reportDir = dir + REPORT_SUB_DIR;
    QDir d(reportDir);
    if (!d.exists(reportDir)) {
        bool created = d.mkpath(reportDir);
        if (!created) {
            coreLog.error(tr("Can not create a directory: ") + reportDir);
            return;
        }
    }
    serialize();
    saveSettings();
}

void Dashboard::serialize() {
    connect(this, SIGNAL(si_serializeContent(const QString&)), this, SLOT(sl_serializeContent(const QString&)));
    page()->toHtml([this](const QString& result) mutable {emit si_serializeContent(result);});
}

void Dashboard::saveSettings() {
    QSettings s(dir + REPORT_SUB_DIR + SETTINGS_FILE_NAME, QSettings::IniFormat);
    s.setValue(OPENED_SETTING, opened);
    s.setValue(NAME_SETTING, name);
    s.sync();
}

void Dashboard::loadSettings() {
    QSettings s(dir + REPORT_SUB_DIR + SETTINGS_FILE_NAME, QSettings::IniFormat);
    opened = true;
    name = s.value(NAME_SETTING).toString();
}

const QPointer<const WorkflowMonitor> Dashboard::monitor() const {
    return _monitor;
}

void Dashboard::sl_runStateChanged(bool paused) {
    QString script = paused ? "pauseTimer()" : "startTimer()";
    page()->runJavaScript(script);
}

void Dashboard::loadSchema() {
    QString url = dir + REPORT_SUB_DIR + WorkflowMonitor::WORKFLOW_FILE_NAME;
    emit si_loadSchema(url);
}

void Dashboard::initiateHideLoadButtonHint() {
    WorkflowSettings::setShowLoadButtonHint(false);
    emit si_hideLoadBtnHint();
}

bool Dashboard::isWorkflowInProgress() {
    return workflowInProgress;
}

void Dashboard::sl_hideLoadBtnHint() {
    page()->runJavaScript("hideLoadBtnHint()");
}

void Dashboard::sl_serializeContent(const QString& content) {
    QString fileName = dir + REPORT_SUB_DIR + DB_FILE_NAME;
    QFile file(fileName);
    bool opened = file.open(QIODevice::WriteOnly);
    if (!opened) {
        coreLog.error(tr("Can not open a file for writing: ") + fileName);
        return;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << content;
    stream.flush();
    file.close();
}

DashboardPageController* Dashboard::getController(){
    return dashboardPageController;
}

/************************************************************************/
/* DashboardPageController */
/************************************************************************/
const QString DashboardPageController::STATE_RUNNING = "RUNNING";
const QString DashboardPageController::STATE_RUNNING_WITH_PROBLEMS = "RUNNING_WITH_PROBLEMS";
const QString DashboardPageController::STATE_FINISHED_WITH_PROBLEMS = "FINISHED_WITH_PROBLEMS";
const QString DashboardPageController::STATE_FAILED = "FAILED";
const QString DashboardPageController::STATE_SUCCESS = "SUCCESS";
const QString DashboardPageController::STATE_CANCELED = "CANCELED";

DashboardPageController::DashboardPageController(Dashboard *parent)
    : QObject(parent),
      progress(0),
      state(STATE_RUNNING),
      isPageLoaded(false),
      isWebChannelInitialized(false),
      isDataReady(false),
      agent(new DashboardJsAgent(parent)),
      monitor(parent->monitor())
{

}

void DashboardPageController::sl_progressChanged(int _progress){
    progress = _progress;
    if(isPageLoaded){
        emit agent->si_progressChanged(progress);
    }
}
void DashboardPageController::sl_taskStateChanged(U2::Workflow::Monitor::TaskState _state){
    QString stateStr;
    if (Monitor::RUNNING == _state) {
        stateStr = STATE_RUNNING;
    } else if (Monitor::RUNNING_WITH_PROBLEMS == _state) {
        stateStr = STATE_RUNNING_WITH_PROBLEMS;
    } else if (Monitor::FINISHED_WITH_PROBLEMS == _state) {
        stateStr = STATE_FINISHED_WITH_PROBLEMS;
    } else if (Monitor::FAILED == _state) {
        stateStr = STATE_FAILED;
    } else if (Monitor::SUCCESS == _state) {
        stateStr = STATE_SUCCESS;
    } else {
        stateStr = STATE_CANCELED;
    }
    state = stateStr;
    if (isPageLoaded) {
        emit agent->si_taskStateChanged(stateStr);
    }
}

void DashboardPageController::sl_newProblem(const Problem &info, int count){
    QJsonObject infoJS;
    infoJS["actor"] = info.actor;
    infoJS["type"] = info.type;
    infoJS["message"] = info.message;
    infoJS["count"] = count;
    QJsonDocument doc(infoJS);
    QString strJson(doc.toJson(QJsonDocument::Compact));
    if(isPageLoaded){
        emit agent->si_newProblem(strJson);
    }else{
        problems.append(strJson);
    }
}

void DashboardPageController::sl_workerInfoChanged(const QString &actorId, const U2::Workflow::Monitor::WorkerInfo &info) {
    SAFE_POINT(NULL != monitor, "WorkflowMonitor is NULL", );
    QJsonObject infoJS;
    infoJS["actorId"] = actorId;
    infoJS["actor"] = monitor->actorName(actorId);
    infoJS["timeMks"] = info.timeMks;
    infoJS["countOfProducedData"] = monitor->getDataProduced(actorId);
    QJsonDocument doc(infoJS);
    QString strJson(doc.toJson(QJsonDocument::Compact));
    if(isPageLoaded){
        emit agent->si_workerStatsInfoChanged(strJson);
    }else{
        infos.append(strJson);
    }
}

void DashboardPageController::sl_workerStatsUpdate(){
    SAFE_POINT(NULL != monitor, "WorkflowMonitor is NULL", );
    QJsonArray workersStatisticsInfo;
    QMap<QString, Monitor::WorkerInfo> infos = monitor->getWorkersInfo();
    foreach (const QString &actorId, infos.keys()) {
        Monitor::WorkerInfo &info = infos[actorId];
        QJsonObject infoJS;
        infoJS["actorId"] = actorId;
        infoJS["actor"] = monitor->actorName(actorId);
        infoJS["timeMks"] = info.timeMks;
        infoJS["countOfProducedData"] = monitor->getDataProduced(actorId);
        workersStatisticsInfo.append(infoJS);
    }
    QJsonDocument doc(workersStatisticsInfo);
    QString strJson(doc.toJson(QJsonDocument::Compact));
    if(isPageLoaded){
        emit agent->si_workerStatsUpdate(strJson);
    }else{
        workersStatisticsInfos.append(strJson);
    }
}

void DashboardPageController::sl_newOutputFile(const U2::Workflow::Monitor::FileInfo &info) {
    SAFE_POINT(NULL != monitor, "WorkflowMonitor is NULL", );
    QJsonObject infoJS;
    infoJS["actor"] = monitor->actorName(info.actor);
    infoJS["url"] = info.url;
    infoJS["openBySystem"] = info.openBySystem;
    QJsonDocument doc(infoJS);
    QString strJson(doc.toJson(QJsonDocument::Compact));
    if(isPageLoaded){
        emit agent->si_newOutputFile(strJson);
    }else{
        fileInfos.append(strJson);
    }
}

void DashboardPageController::sl_onLogChanged(U2::Workflow::Monitor::LogEntry entry){
    QJsonObject entryJS;
    entryJS["toolName"] = entry.toolName;
    entryJS["actorName"] = entry.actorName;
    entryJS["runNumber"] = entry.runNumber;
    entryJS["logType"] = entry.logType;
    entryJS["lastLine"] = entry.lastLine;
    QJsonDocument doc(entryJS);
    QString strJson(doc.toJson(QJsonDocument::Compact));
    if(isPageLoaded){
        emit agent->si_onLogChanged(strJson);
    }else{
        logEntries.append(strJson);
    }
}

void DashboardPageController::sl_pageLoaded(){
    if(isWebChannelInitialized && isDataReady){
        initData();
    }
}
void DashboardPageController::initData(){
    emit agent->si_progressChanged(progress);
    emit agent->si_taskStateChanged(state);
    foreach (QString problem, problems) {
        emit agent->si_newProblem(problem);
    }

    foreach (QString info, infos) {
        emit agent->si_workerStatsInfoChanged(info);
    }
    foreach (QString workersStatisticsInfo, workersStatisticsInfos) {
        emit agent->si_workerStatsUpdate(workersStatisticsInfo);
    }
    foreach (QString fileInfo, fileInfos) {
        emit agent->si_newOutputFile(fileInfo);
    }
    foreach (QString entry, logEntries) {
        emit agent->si_onLogChanged(entry);
    }
    isPageLoaded = true;
}

DashboardJsAgent* DashboardPageController::getAgent(){
    return agent;
}
void DashboardPageController::setWebChannelInitialized(){
    isWebChannelInitialized = true;
}
void DashboardPageController::setDataReady(){
    isDataReady = true;
}
/************************************************************************/
/* DashboardJsAgent */
/************************************************************************/
DashboardJsAgent::DashboardJsAgent(Dashboard* parent)
    : QObject(parent),
      monitor(parent->monitor())
{
    //set language
    lang = AppContext::getAppSettings()->getUserAppsSettings()->getTranslationFile();
    if(lang.isEmpty()){
        lang = "en";
    }else {
        lang = lang.split("_")[1];
    }
    fillWorkerParamsInfo();
}

void DashboardJsAgent::sl_onJsError(const QString& errorMessage) {
    coreLog.error(errorMessage);
}

void DashboardJsAgent::sl_checkETsLog(){

}

void DashboardJsAgent::sl_webChannelInitialized(){
    Dashboard* dashboard = qobject_cast<Dashboard*>(parent());
    if(dashboard->monitor() != NULL){
        dashboard->getController()->setWebChannelInitialized();
        dashboard->getController()->sl_pageLoaded();
    }
}

void DashboardJsAgent::openUrl(const QString &relative) {
    QString url = absolute(relative);
    QVariantMap hints;
    hints[ProjectLoaderHint_OpenBySystemIfFormatDetectionFailed] = true;
    Task *t = AppContext::getProjectLoader()->openWithProjectTask(url, hints);
    if (t) {
        AppContext::getTaskScheduler()->registerTopLevelTask(t);
    }
}

void DashboardJsAgent::openByOS(const QString &relative) {
    QString url = absolute(relative);
    if (!QFile::exists(url)) {
        QMessageBox::critical((QWidget*)AppContext::getMainWindow()->getQMainWindow(), tr("Error"), tr("The file does not exist"));
        return;
    }
    QDesktopServices::openUrl(QUrl("file:///" + url));
}

QString DashboardJsAgent::absolute(const QString &url) {
    if (QFileInfo(url).isAbsolute()) {
        return url;
    }
    return qobject_cast<Dashboard*>(parent())->directory() + url;
}

void DashboardJsAgent::loadSchema(){
    qobject_cast<Dashboard*>(parent())->loadSchema();
}

void DashboardJsAgent::setClipboardText(const QString &text){
    QApplication::clipboard()->setText(text);
}

void DashboardJsAgent::hideLoadButtonHint() {
    Dashboard* dashboard = qobject_cast<Dashboard*>(parent());
    SAFE_POINT(NULL != dashboard, "NULL dashboard!", );
    dashboard->initiateHideLoadButtonHint();
}

QString DashboardJsAgent::getLang(){
    return lang;
}

QString DashboardJsAgent::getWorkersParamsInfo(){
    return workersParamsInfo;
}

bool DashboardJsAgent::getShowHint(){
    return WorkflowSettings::isShowLoadButtonHint();
}

//Worker parametes initialization
void DashboardJsAgent::fillWorkerParamsInfo(){
    CHECK(monitor,);
    QJsonArray localWorkersParamsInfo;
    QList<Monitor::WorkerParamsInfo> workersParamsList = monitor->getWorkersParameters();
    foreach (Monitor::WorkerParamsInfo workerInfo, workersParamsList) {
        QJsonObject workerInfoJS;
        workerInfoJS["workerName"] = workerInfo.workerName;
        workerInfoJS["actor"] = workerInfo.actor->getLabel();
        QJsonArray parameters;
        foreach (Attribute* parameter, workerInfo.parameters) {
            QJsonObject parameterJS;
            parameterJS["name"] = parameter->getDisplayName();
            QVariant paramValueVariant = parameter->getAttributePureValue();
            if (paramValueVariant.canConvert< QList<Dataset> >()) {
                QList<Dataset> sets = paramValueVariant.value< QList<Dataset > >();
                foreach (const Dataset &set, sets) {
                    QString paramName = parameter->getDisplayName();
                    if (sets.size() > 1) {
                        paramName += ": <i>" + set.getName() + "</i>";
                    }
                    parameterJS["name"] = paramName;
                    QStringList urls;
                    foreach (URLContainer *c, set.getUrls()) {
                        urls << c->getUrl();
                    }
                    parameterJS["value"] = urls.join(";");
                    parameterJS["isDataset"] = true;
                    parameters.append(parameterJS);
                }
            }else{
                parameterJS["value"] = WorkflowUtils::getStringForParameterDisplayRole(paramValueVariant);
                UrlAttributeType type = WorkflowUtils::isUrlAttribute(parameter, workerInfo.actor);
                if (type == NotAnUrl || QString::compare(paramValueVariant.toString(), "default", Qt::CaseInsensitive) == 0) {
                    parameterJS["isUrl"] = false;
                }else{

                    parameterJS["isUrl"] = true;
                    if(type == InputDir || type == OutputDir) {
                        parameterJS["type"] = "Dir";
                    }
                }
                parameters.append(parameterJS);
            }
        }
        workerInfoJS["parameters"] = parameters;
        localWorkersParamsInfo.append(workerInfoJS);
        QJsonDocument doc(localWorkersParamsInfo);
        workersParamsInfo = doc.toJson(QJsonDocument::Compact);
    }
}
/************************************************************************/
/* DashboardWidget */
/************************************************************************/
DashboardWidget::DashboardWidget(const QString &_container, Dashboard *parent)
: QObject(parent), dashboard(parent), container(_container)
{

}

/************************************************************************/
/* LoadDashboardsTask */
/************************************************************************/
ScanDashboardsDirTask::ScanDashboardsDirTask()
: Task(tr("Scan dashboards directory"), TaskFlag_None)
{

}

void ScanDashboardsDirTask::run() {
    QDir outDir(WorkflowSettings::getWorkflowOutputDirectory());
    CHECK(outDir.exists(), );

    QFileInfoList dirs = outDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    foreach (const QFileInfo &info, dirs) {
        QString dirPath = info.absoluteFilePath() + "/";
        DashboardInfo dbi(dirPath);
        if (isDashboardDir(dirPath, dbi)) {
            result << dbi;
            if (dbi.opened) {
                openedDashboards << dirPath;
            }
        }
    }
}

bool ScanDashboardsDirTask::isDashboardDir(const QString &dirPath, DashboardInfo &info) {
    QDir dir(dirPath + REPORT_SUB_DIR);
    CHECK(dir.exists(), false);
    CHECK(dir.exists(DB_FILE_NAME), false);
    CHECK(dir.exists(SETTINGS_FILE_NAME), false);

    QSettings s(dirPath + REPORT_SUB_DIR + SETTINGS_FILE_NAME, QSettings::IniFormat);
    info.opened = s.value(OPENED_SETTING).toBool();
    info.name = s.value(NAME_SETTING).toString();
    return true;
}

QStringList ScanDashboardsDirTask::getOpenedDashboards() const {
    return openedDashboards;
}

QList<DashboardInfo> ScanDashboardsDirTask::getResult() const {
    return result;
}

/************************************************************************/
/* RemoveDashboardsTask */
/************************************************************************/
RemoveDashboardsTask::RemoveDashboardsTask(const QList<DashboardInfo> &_dashboards)
: Task(tr("Remove dashboards"), TaskFlag_None), dashboards(_dashboards)
{

}
void RemoveDashboardsTask::run() {
    foreach (const DashboardInfo &info, dashboards) {
        U2OpStatus2Log os;
        GUrlUtils::removeDir(info.path, os);
    }
}

/************************************************************************/
/* DashboardInfo */
/************************************************************************/
DashboardInfo::DashboardInfo() {

}

DashboardInfo::DashboardInfo(const QString &dirPath, bool _opened)
: path(dirPath), opened(_opened)
{
    dirName = QDir(path).dirName();
}

bool DashboardInfo::operator==(const DashboardInfo &other) const {
    return path == other.path;
}

} // ns

