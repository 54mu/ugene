/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2014 UniPro <ugene@unipro.ru>
 * http://ugene.unipro.ru
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

#include <U2Core/AppContext.h>
#include <U2Core/CMDLineCoreOptions.h>
#include <U2Core/CMDLineRegistry.h>
#include <U2Core/CredentialsStorage.h>
#include <U2Core/Log.h>
#include <U2Core/ProjectModel.h>
#include <U2Core/U2DbiRegistry.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/AppSettings.h>
#include <U2Core/UserApplicationsSettings.h>
#include <U2Core/GUrlUtils.h>
#include <U2Core/U2SafePoints.h>
#include <U2Core/U2DbiUtils.h>

#include <U2Formats/MysqlDbiUtils.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QThread>

namespace U2 {

static const QString SESSION_TMP_DBI_ALIAS("session");

U2DbiRegistry::U2DbiRegistry(QObject *parent) : QObject(parent), lock(QMutex::Recursive) {
    pool = new U2DbiPool(this);
    sessionDbiConnection = NULL;
    sessionDbiInitDone = false;
}

void U2DbiRegistry::initSessionDbi(TmpDbiRef& tmpDbiRef) {
    sessionDbiInitDone = true;

    U2OpStatus2Log os;
    sessionDbiConnection = new DbiConnection(tmpDbiRef.dbiRef, os);

    tmpDbiRef.nUsers++;
}

U2DbiRegistry::~U2DbiRegistry() {
    if (sessionDbiConnection != NULL) {
        U2DbiRef ref = sessionDbiConnection->dbi->getDbiRef();
        delete sessionDbiConnection;
        U2OpStatus2Log os;
        detachTmpDbi(SESSION_TMP_DBI_ALIAS, os);
    }

    coreLog.trace("Deallocating U2DbiRegistry");
    for (int i = 0; i < tmpDbis.size(); i++) {
        TmpDbiRef& ref = tmpDbis[i];
        coreLog.trace(QString("BUG: tmp DBI was not deallocated: %1 %2 [%3]").arg(ref.alias).arg(ref.dbiRef.dbiId).arg(ref.nUsers));
        ref.nUsers = 1;
    }
    foreach(const TmpDbiRef& ref, tmpDbis) {
        U2OpStatus2Log os;
        detachTmpDbi(ref.alias, os);
    }
    qDeleteAll(factories.values());
}

QList<U2DbiRef> U2DbiRegistry::listTmpDbis() const {
    QList<U2DbiRef> res;
    foreach(const TmpDbiRef& ref, tmpDbis) {
        res << ref.dbiRef;
    }
    return res;
}

U2DbiRef U2DbiRegistry::attachTmpDbi(const QString& alias, U2OpStatus& os, const U2DbiFactoryId &factoryId) {
    QMutexLocker m(&lock);

    for (int i = 0; i < tmpDbis.size(); i++) {
        TmpDbiRef& ref = tmpDbis[i];
        if (ref.alias == alias) {
            ref.nUsers++;
            return ref.dbiRef;
        }
    }

    coreLog.trace("Allocating a tmp dbi with alias: " + alias);
    U2DbiRef dbiRef = allocateTmpDbi(alias, os, factoryId);
    CHECK_OP(os, U2DbiRef());

    coreLog.trace("Allocated tmp dbi: " + dbiRef.dbiId);
    TmpDbiRef tmpDbiRef = TmpDbiRef(alias, dbiRef, 1);

    if (alias == SESSION_TMP_DBI_ALIAS && !sessionDbiInitDone) { //once used -> cache connection
        initSessionDbi(tmpDbiRef);
    }

    tmpDbis << tmpDbiRef;

    return dbiRef;
}

void U2DbiRegistry::detachTmpDbi(const QString& alias, U2OpStatus& os) {
    QMutexLocker l(&lock);

    int i = 0;
    bool found = false;
    for (;i < tmpDbis.size(); i++) {
        TmpDbiRef& ref = tmpDbis[i];
        if (ref.alias == alias) {
            found = true;
            ref.nUsers--;
            if (ref.nUsers > 0) {
                return;
            }
            break;
        }
    }
    if (!found) {
        coreLog.error(tr("The tmp dbi is not found: %1").arg(alias));
        return;
    }

    const TmpDbiRef& tmpDbiRef = tmpDbis.at(i);

    coreLog.trace("Deallocating a tmp dbi " + tmpDbiRef.dbiRef.dbiId + " with alias: " + tmpDbiRef.alias);
    deallocateTmpDbi(tmpDbiRef, os);

    tmpDbis.removeAt(i);
}

U2DbiRef U2DbiRegistry::getSessionTmpDbiRef(U2OpStatus &os) {
    TmpDbiHandle dh(SESSION_TMP_DBI_ALIAS, os);
    CHECK_OP(os, U2DbiRef());

    return dh.getDbiRef();
}

QString U2DbiRegistry::shutdownSessionDbi(U2OpStatus &os) {
    QMutexLocker l(&lock);
    CHECK_EXT(sessionDbiInitDone, os.setError("Session dbi is not initialized"), "");
    CHECK_EXT(NULL != sessionDbiConnection, os.setError("No session dbi connection"), "");
    CHECK_EXT(NULL != sessionDbiConnection->dbi, os.setError("No opened dbi"), "");

    QString url = sessionDbiConnection->dbi->getDbiRef().dbiId;
    delete sessionDbiConnection;
    sessionDbiConnection = NULL;
    return url;
}

void U2DbiRegistry::deallocateTmpDbi(const TmpDbiRef& ref, U2OpStatus& os) {
    QMutexLocker l(&lock);

    pool->closeAllConnections(ref.dbiRef, os);
    if (QFile::exists(ref.dbiRef.dbiId)) {
        QFile::remove(ref.dbiRef.dbiId);
    }
}

namespace {
    QString createNewDatabase(const QString &alias, U2OpStatus &os) {
        QString tmpDirPath = AppContext::getAppSettings()->getUserAppsSettings()->getCurrentProcessTemporaryDirPath();
        return GUrlUtils::prepareTmpFileLocation(tmpDirPath, alias, "ugenedb", os);
    }

    QString getDatabaseFromCMDLine(U2OpStatus &os) {
        CMDLineRegistry *cmdlineReg = AppContext::getCMDLineRegistry();
        SAFE_POINT_EXT(NULL != cmdlineReg, os.setError("NULL cmdline registry"), "");
        if (!cmdlineReg->hasParameter(CMDLineCoreOptions::SESSION_DB)) {
            os.setError("The session database path is not supplied through the cmd line argument");
            return "";
        }
        return cmdlineReg->getParameterValue(CMDLineCoreOptions::SESSION_DB);
    }

    /**
     * Returns true if the path to the correct (openable) session database is
     * supplied with the cmdline argument
     */
    bool useDatabaseFromCMDLine(const QString &alias) {
        if (SESSION_TMP_DBI_ALIAS != alias) {
            return false;
        }

        CMDLineRegistry *cmdlineReg = AppContext::getCMDLineRegistry();
        SAFE_POINT(NULL != cmdlineReg, "NULL cmdline registry", false);
        if (!cmdlineReg->hasParameter(CMDLineCoreOptions::SESSION_DB)) {
            return false;
        }

        U2DbiRef ref;
        ref.dbiId = cmdlineReg->getParameterValue(CMDLineCoreOptions::SESSION_DB);
        ref.dbiFactoryId = DEFAULT_DBI_ID;

        // Check if the connection can be established
        U2OpStatus2Log os;
        DbiConnection con(ref, true, os);
        Q_UNUSED(con);
        if (os.hasError()) {
            return false;
        }
        return true;
    }

    U2DbiRef getDbiRef(const QString &alias, U2OpStatus &os,
        const U2DbiFactoryId &factoryId)
    {
        U2DbiRef res;
        res.dbiFactoryId = factoryId;
        if (useDatabaseFromCMDLine(alias)) {
            res.dbiId = getDatabaseFromCMDLine(os);
        } else {
            res.dbiId = createNewDatabase(alias, os);
        }
        return res;
    }
}

U2DbiRef U2DbiRegistry::allocateTmpDbi(const QString& alias, U2OpStatus& os,
    const U2DbiFactoryId &factoryId)
{
    QMutexLocker m(&lock);

    U2DbiRef res = getDbiRef(alias, os, factoryId);
    CHECK_OP(os, res);

    if ( SQLITE_DBI_ID == factoryId ) {
        // Create a tmp dbi file (the DbiConnection is opened with "bool create = true", and released)
        DbiConnection con(res, true, os);
        Q_UNUSED(con);
    }
    CHECK_OP(os, U2DbiRef());

    return res;
}

bool U2DbiRegistry::registerDbiFactory(U2DbiFactory *factory) {
    if (factories.contains(factory->getId())) {
        return false;
    }
    factories.insert(factory->getId(), factory);
    return true;
}


QList<U2DbiFactoryId> U2DbiRegistry::getRegisteredDbiFactories() const {
    return factories.keys();
}

U2DbiFactory *U2DbiRegistry::getDbiFactoryById(U2DbiFactoryId id) const {
    return factories.value(id, NULL);
}

//////////////////////////////////////////////////////////////////////////
// U2DbiPool

const int U2DbiPool::DBI_POOL_EXPIRATION_TIME_MSEC = 1800000;

U2DbiPool::U2DbiPool(QObject* p)
    : QObject(p)
{
    connect(&expirationTimer, SIGNAL(timeout()), SLOT(sl_checkDbiPoolExpiration()));
    expirationTimer.start(DBI_POOL_EXPIRATION_TIME_MSEC);
}

U2DbiPool::~U2DbiPool() {
    expirationTimer.stop();
    foreach (U2Dbi *dbi, suspendedDbis.values()) {
        U2OpStatus2Log os;
        deallocateDbi(dbi, os);
    }
}

U2Dbi * U2DbiPool::createDbi(const U2DbiRef &ref, bool create, U2OpStatus &os) {
    U2DbiFactory* dbiFactory = AppContext::getDbiRegistry()->getDbiFactoryById(ref.dbiFactoryId);
    CHECK_EXT(NULL != dbiFactory, os.setError(tr("Invalid database type: %1").arg(ref.dbiFactoryId)), NULL);
    U2Dbi *result = dbiFactory->createDbi();

    const QString url = dbiFactory->id2Url(ref.dbiId).getURLString();

    QHash<QString, QString> initProperties = getInitProperties(url, create);
    result->init(initProperties, QVariantMap(), os);
    CHECK_EXT(!os.hasError(), delete result, NULL);
    return result;
}

void U2DbiPool::deallocateDbi(U2Dbi *dbi, U2OpStatus &os) {
    SAFE_POINT(NULL != dbi, "Invalid DBI reference detected!", );
    dbi->shutdown(os);
    delete dbi;
    SAFE_POINT_OP(os, );
}

U2Dbi * U2DbiPool::getDbiFromPool(const U2DbiRef &ref) {
    U2Dbi *dbi = suspendedDbis.values(ref).first();
    suspendedDbis.remove(ref, dbi);

    if (dbiSuspendStartTime.contains(dbi)) {
        dbiSuspendStartTime.remove(dbi);
    } else {
        coreLog.error("Invalid DBI expiration time");
    }

    return dbi;
}

U2Dbi * U2DbiPool::openDbi(const U2DbiRef &ref, bool createDatabase, U2OpStatus &os) {
    CHECK_EXT(!ref.dbiId.isEmpty(), os.setError(tr("Invalid database id")), NULL);
    QMutexLocker m(&lock);
    Q_UNUSED(m);

    const QString id = getId(ref, os);
    CHECK_OP(os, NULL);
    U2Dbi* dbi = NULL;

    if (dbiById.contains(id)) {
        dbi = dbiById[id];
        int cnt = dbiCountersById[id];
        dbiCountersById[id] = cnt + 1;
    } else {
        if (suspendedDbis.contains(ref)) {
            // take initialized DBI from pool
            dbi = getDbiFromPool(ref);
        } else {
            // create new DBI
            dbi = createDbi(ref, createDatabase, os);
        }
        dbiById[id] = dbi;
        dbiCountersById[id] = 1;
    }
    return dbi;
}

void U2DbiPool::addRef(U2Dbi *dbi, U2OpStatus &os) {
    QMutexLocker m(&lock);

    const QString id = getId(dbi->getDbiRef(), os);
    SAFE_POINT_OP(os, );
    if (!dbiById.contains(id)) {
        os.setError(tr("DbiPool: DBI not found! Dbi ID: %1").arg(dbi->getDbiId()));
        return;
    }

    SAFE_POINT(dbiCountersById[id] > 0, "Invalid DBI reference counter value", );
    ++dbiCountersById[id];
}

void U2DbiPool::releaseDbi(U2Dbi* dbi, U2OpStatus& os) {
    QMutexLocker m(&lock);

    const QString id = getId(dbi->getDbiRef(), os);
    SAFE_POINT_OP(os, );

    if (!dbiById.contains(id)) {
        os.setError(tr("DbiPool: DBI not found! Dbi ID: %1").arg(dbi->getDbiId()));
        return;
    }
    int cnt = --dbiCountersById[id];
    if (cnt > 0) {
        return;
    } else if (SQLITE_DBI_ID != dbi->getDbiRef().dbiFactoryId) {
        dbiById.remove(id);
        dbiCountersById.remove(id);

        suspendedDbis.insert(dbi->getDbiRef(), dbi);
        dbiSuspendStartTime.insert(dbi, QDateTime::currentMSecsSinceEpoch());
    }
}

void U2DbiPool::sl_checkDbiPoolExpiration() {
    Project *proj = AppContext::getProject();

    // collect DBI references from all used documents
    QList<U2DbiRef> dbiRefsInUse;
    if (NULL != proj) {
        foreach (Document *doc, proj->getDocuments()) {
            const U2DbiRef ref = doc->getDbiRef();
            if (!dbiRefsInUse.contains(ref)) {
                dbiRefsInUse.append(ref);
            }
        }
    }

    const qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    foreach (U2Dbi *dbi, dbiSuspendStartTime.keys()) {
        SAFE_POINT(suspendedDbis.contains(dbi->getDbiRef(), dbi), "Unexpected DBI detected in pool", );

        if (suspendedDbis.count(dbi->getDbiRef()) == 1) {
            // hold at least one connection per database
            continue;
        }

        // destroy connection if it has lived too long or no documents use that database
        if (currentTime - dbiSuspendStartTime[dbi] >= DBI_POOL_EXPIRATION_TIME_MSEC || !dbiRefsInUse.contains(dbi->getDbiRef())) {
            suspendedDbis.remove(dbi->getDbiRef(), dbi);
            dbiSuspendStartTime.remove(dbi);

            U2OpStatus2Log os;
            deallocateDbi(dbi, os);
        }
    }
}

void U2DbiPool::closeAllConnections(const U2DbiRef& ref, U2OpStatus& os) {
    QMutexLocker m(&lock);
    int nActive = 0;

    const QStringList allConnectionsIds = getIds(ref, os);
    SAFE_POINT_OP(os, );

    foreach (const QString& id, allConnectionsIds) {
        U2Dbi* dbi = dbiById[id];
        deallocateDbi(dbi, os);

        dbiById.remove(id);
        nActive += dbiCountersById.value(id, 0);
        dbiCountersById.remove(id);
    }

    ioLog.trace(QString("DBIPool: closing all connections. Id: %1, active references: %2 ").arg(ref.dbiId).arg(nActive));
}

QHash<QString, QString> U2DbiPool::getInitProperties(const QString &url, bool create) {
    QHash<QString, QString> initProperties;

    initProperties[U2DbiOptions::U2_DBI_OPTION_URL] = url;

    if (create) {
        initProperties[U2DbiOptions::U2_DBI_OPTION_CREATE] = U2DbiOptions::U2_DBI_VALUE_ON;
    }

    Credentials credentials = AppContext::getCredentialsStorage()->getEntry(url);

    if (!credentials.login.isNull()) {
        initProperties[U2DbiOptions::U2_DBI_OPTION_LOGIN] = credentials.login;
    }

    if (!credentials.password.isNull()) {
        initProperties[U2DbiOptions::U2_DBI_OPTION_PASSWORD] = credentials.password;
    }

    return initProperties;
}

QString U2DbiPool::getId(const U2DbiRef& ref, U2OpStatus& os) const {
    const QString url = U2DbiUtils::ref2Url(ref);
    SAFE_POINT_EXT(!url.isEmpty(), os.setError(tr("Invalid dbi reference")), "");

    if (ref.dbiFactoryId == MYSQL_DBI_ID) {
        return url + "|" + QString::number((qint64)QThread::currentThread());
    } else {
        return url;
    }
}

QStringList U2DbiPool::getIds(const U2DbiRef& ref, U2OpStatus &os) const {
    U2DbiFactory* dbiFactory = AppContext::getDbiRegistry()->getDbiFactoryById(ref.dbiFactoryId);
    SAFE_POINT_EXT(NULL != dbiFactory, os.setError(tr("Invalid database type: %1").arg(ref.dbiFactoryId)), QStringList());
    const QString url = dbiFactory->id2Url(ref.dbiId).getURLString();
    QStringList result;

    if (ref.dbiFactoryId == MYSQL_DBI_ID) {
        foreach (const QString& id, dbiById.keys()) {
            if (id.left(id.length() - 1 - sizeof(qint64)) == url) {
                result << id;
            }
        }
    } else {
        if (dbiById.contains(url)) {
            result << url;
        }
    }

    return result;
}

} // namespace U2
