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

#ifndef _U2_BWA_SUPPORT_WORKER_
#define _U2_BWA_SUPPORT_WORKER_

#include <U2Core/U2OpStatus.h>

#include <U2Lang/LocalDomain.h>
#include <U2Lang/WorkflowUtils.h>

class DnaAssemblyToRefTaskSettings;

namespace U2 {
namespace LocalWorkflow {

class BWAWorker : public BaseWorker {
    Q_OBJECT
public:
    BWAWorker(Actor *p);

    virtual void init();
    virtual Task *tick();
    virtual void cleanup();

private:
    IntegralBus *inChannel;
    IntegralBus *output;

private:
    DnaAssemblyToRefTaskSettings getSettings(U2OpStatus &os);

private slots:
    void sl_taskFinished();
}; // BWAWorker

class BWAWorkerFactory : public DomainFactory {
public:
    static const QString ACTOR_ID;

    BWAWorkerFactory() : DomainFactory(ACTOR_ID) {}
    static void init();
    virtual Worker *createWorker(Actor *a);
}; // BWAWorkerFactory

class BWAPrompter : public PrompterBase<BWAPrompter> {
    Q_OBJECT
public:
    BWAPrompter(Actor *p = NULL) : PrompterBase<BWAPrompter>(p) {}

protected:
    QString composeRichDoc();

}; // BWAPrompter

} // LocalWorkflow
} // U2

#endif // _U2_BWA_SUPPORT_WORKER_
