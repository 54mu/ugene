/**
* UGENE - Integrated Bioinformatics Tools.
* Copyright (C) 2008-2016 UniPro <ugene@unipro.ru>
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

#include <U2Core/L10n.h>
#include <U2Core/LoadDocumentTask.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/U2SafePoints.h>

#include <U2Designer/DelegateEditors.h>

#include <U2Lang/ActorPrototypeRegistry.h>
#include <U2Lang/BaseActorCategories.h>
#include <U2Lang/BasePorts.h>
#include <U2Lang/BaseSlots.h>
#include <U2Lang/BaseTypes.h>
#include <U2Lang/WorkflowEnv.h>

#include "AlignToReferenceBlastWorker.h"

#include "blast/BlastAllSupport.h"

#include "align_worker_subtasks/BlastReadsSubTask.h"
#include "align_worker_subtasks/FormatDBSubTask.h"
#include "align_worker_subtasks/ComposeResultSubTask.h"


namespace U2 {
namespace LocalWorkflow {

const QString AlignToReferenceBlastWorkerFactory::ACTOR_ID("align-to-reference-blast");
namespace {
    const QString OUT_PORT_ID = "out";
    const QString REF_ATTR_ID = "reference";

    const QString ALGORITHM = "algorithm";
}

/************************************************************************/
/* AlignToReferenceBlastWorkerFactory */
/************************************************************************/
AlignToReferenceBlastWorkerFactory::AlignToReferenceBlastWorkerFactory()
: DomainFactory(ACTOR_ID)
{

}

Worker * AlignToReferenceBlastWorkerFactory::createWorker(Actor *a) {
    return new AlignToReferenceBlastWorker(a);
}

void AlignToReferenceBlastWorkerFactory::init() {
    QList<PortDescriptor*> ports;
    {
        Descriptor inDesc(BasePorts::IN_SEQ_PORT_ID(), AlignToReferenceBlastPrompter::tr("Input sequence"), AlignToReferenceBlastPrompter::tr("Input sequence."));
        Descriptor outDesc(OUT_PORT_ID, AlignToReferenceBlastPrompter::tr("Aligned data"), AlignToReferenceBlastPrompter::tr("Aligned data."));

        QMap<Descriptor, DataTypePtr> inType;
        inType[BaseSlots::DNA_SEQUENCE_SLOT()] = BaseTypes::DNA_SEQUENCE_TYPE();

        QMap<Descriptor, DataTypePtr> outType;
        outType[BaseSlots::DNA_SEQUENCE_SLOT()] = BaseTypes::DNA_SEQUENCE_TYPE();
        outType[BaseSlots::ANNOTATION_TABLE_SLOT()] = BaseTypes::ANNOTATION_TABLE_TYPE();
        outType[BaseSlots::MULTIPLE_ALIGNMENT_SLOT()] = BaseTypes::MULTIPLE_ALIGNMENT_TYPE();

        ports << new PortDescriptor(inDesc, DataTypePtr(new MapDataType(ACTOR_ID + "-in", inType)), true /*input*/);
        ports << new PortDescriptor(outDesc, DataTypePtr(new MapDataType(ACTOR_ID + "-out", outType)), false /*input*/, true /*multi*/);
    }
    QList<Attribute*> attributes;
    {
        Descriptor refDesc(REF_ATTR_ID, AlignToReferenceBlastPrompter::tr("Reference URL"), AlignToReferenceBlastPrompter::tr("A URL to the file with a reference sequence."));
        attributes << new Attribute(refDesc, BaseTypes::STRING_TYPE(), true);
    }

    QMap<QString, PropertyDelegate*> delegates;
    {
        delegates[REF_ATTR_ID] = new URLDelegate("", "", false, false, false);
    }

    // temp double worker
    Descriptor desc(ACTOR_ID, AlignToReferenceBlastWorker::tr("Align to Reference with BLAST"),
        AlignToReferenceBlastWorker::tr("Align input sequences (e.g. Sanger reads) to the reference sequence."));
    ActorPrototype *proto = new IntegralBusActorPrototype(desc, ports, attributes);
    proto->setEditor(new DelegateEditor(delegates));
    proto->setPrompter(new AlignToReferenceBlastPrompter(NULL));
    proto->addExternalTool(ET_BLASTALL);
    WorkflowEnv::getProtoRegistry()->registerProto(BaseActorCategories::CATEGORY_ALIGNMENT(), proto);

    DomainFactory *localDomain = WorkflowEnv::getDomainRegistry()->getById(LocalDomainFactory::ID);
    localDomain->registerEntry(new AlignToReferenceBlastWorkerFactory());
}

/************************************************************************/
/* AlignToReferenceBlastPrompter */
/************************************************************************/
AlignToReferenceBlastPrompter::AlignToReferenceBlastPrompter(Actor *a)
: PrompterBase<AlignToReferenceBlastPrompter>(a)
{

}

QString AlignToReferenceBlastPrompter::composeRichDoc() {
    IntegralBusPort *input = qobject_cast<IntegralBusPort*>(target->getPort(BasePorts::IN_SEQ_PORT_ID()));
    SAFE_POINT(NULL != input, "No input port", "");
    Actor *producer = input->getProducer(BaseSlots::DNA_SEQUENCE_SLOT().getId());
    const QString unsetStr = "<font color='red'>"+tr("unset")+"</font>";
    const QString producerName = (NULL != producer) ? producer->getLabel() : unsetStr;
    const QString refLink = getHyperlink(REF_ATTR_ID, getURL(REF_ATTR_ID));
    return tr("Aligns each sequence from <u>%1</u> to the reference sequence from <u>%2</u>.").arg(producerName).arg(refLink);
}

/************************************************************************/
/* AlignToReferenceBlastWorker */
/************************************************************************/
AlignToReferenceBlastWorker::AlignToReferenceBlastWorker(Actor *a)
: BaseDatasetWorker(a, BasePorts::IN_SEQ_PORT_ID(), OUT_PORT_ID), referenceDoc(NULL)
{

}

void AlignToReferenceBlastWorker::cleanup() {
    delete referenceDoc;
    referenceDoc = NULL;
    BaseDatasetWorker::cleanup();
}

Task * AlignToReferenceBlastWorker::createPrepareTask(U2OpStatus &os) const {
    QString referenceUrl = getValue<QString>(REF_ATTR_ID);
    QVariantMap hints;
    hints[DocumentFormat::DBI_REF_HINT] = qVariantFromValue(context->getDataStorage()->getDbiRef());
    LoadDocumentTask *task = LoadDocumentTask::getDefaultLoadDocTask(referenceUrl, hints);
    if (NULL == task) {
        os.setError(tr("Can not read the reference file: ") + referenceUrl);
    }
    return task;
}

void AlignToReferenceBlastWorker::onPrepared(Task *task, U2OpStatus &os) {
    LoadDocumentTask *loadTask = qobject_cast<LoadDocumentTask*>(task);
    CHECK_EXT(NULL != loadTask, os.setError(L10N::internalError("Unexpected prepare task")), );

    QScopedPointer<Document> doc(loadTask->takeDocument());
    CHECK_EXT(!doc.isNull(), os.setError(tr("Can't read the file: ") + loadTask->getURLString()), );
    QList<GObject*> objects = doc->findGObjectByType(GObjectTypes::SEQUENCE);
    CHECK_EXT(!objects.isEmpty(), os.setError(tr("No reference sequence in the file: ") + loadTask->getURLString()), );
    CHECK_EXT(1 == objects.size(), os.setError(tr("More than one sequence in the reference file: ") + loadTask->getURLString()), );

    referenceDoc = doc.take();
    referenceDoc->setDocumentOwnsDbiResources(false);
    reference = context->getDataStorage()->getDataHandler(objects.first()->getEntityRef());
}

Task * AlignToReferenceBlastWorker::createTask(const QList<Message> &messages) const {
    QList<SharedDbiDataHandler> reads;
    foreach (const Message &message, messages) {
        QVariantMap data = message.getData().toMap();
        if (data.contains(BaseSlots::DNA_SEQUENCE_SLOT().getId())) {
            reads << data[BaseSlots::DNA_SEQUENCE_SLOT().getId()].value<SharedDbiDataHandler>();
        }
    }
    return new AlignToReferenceBlastTask(getValue<QString>(REF_ATTR_ID), reference, reads, context->getDataStorage());
}

QVariantMap AlignToReferenceBlastWorker::getResult(Task *task, U2OpStatus &os) const {
    AlignToReferenceBlastTask *alignTask = qobject_cast<AlignToReferenceBlastTask*>(task);
    CHECK_EXT(NULL != alignTask, os.setError(L10N::internalError("Unexpected task")), QVariantMap());
    QVariantMap result;
    result[BaseSlots::MULTIPLE_ALIGNMENT_SLOT().getId()] = qVariantFromValue<SharedDbiDataHandler>(alignTask->getAlignment());
    result[BaseSlots::DNA_SEQUENCE_SLOT().getId()] = qVariantFromValue<SharedDbiDataHandler>(reference);
    result[BaseSlots::ANNOTATION_TABLE_SLOT().getId()] = qVariantFromValue<SharedDbiDataHandler>(alignTask->getAnnotations());
    return result;
}

MessageMetadata AlignToReferenceBlastWorker::generateMetadata(const QString &datasetName) const {
    SAFE_POINT(NULL != referenceDoc, L10N::nullPointerError("Reference sequence document"), BaseDatasetWorker::generateMetadata(datasetName));
    return MessageMetadata(referenceDoc->getURLString(), datasetName);
}

/************************************************************************/
/* AlignToReferenceBlastTask */
/************************************************************************/
AlignToReferenceBlastTask::AlignToReferenceBlastTask(const QString& refUrl,
                                                     const SharedDbiDataHandler &reference,
                                                     const QList<SharedDbiDataHandler> &reads,
                                                     DbiDataStorage *storage)
    : Task(tr("Align to reference"), TaskFlags_NR_FOSE_COSC),
      referenceUrl(refUrl),
      reference(reference),
      reads(reads),
      formatDbSubTask(NULL),
      blastTask(NULL),
      composeSubTask(NULL),
      storage(storage)
{
}

void AlignToReferenceBlastTask::prepare() {
    formatDbSubTask = new U2::Workflow::FormatDBSubTask(referenceUrl, reference, storage);
    addSubTask(formatDbSubTask);
}

QList<Task*> AlignToReferenceBlastTask::onSubTaskFinished(Task *subTask) {
    QList<Task*> result;
    CHECK(subTask != NULL, result);
    CHECK(!subTask->isCanceled() && !subTask ->hasError(), result);

    if (subTask == formatDbSubTask) {
        QString dbPath = formatDbSubTask->getResultPath();
        blastTask = new BlastReadsSubTask(dbPath, reads, reference, storage);
        result << blastTask;
    } else if (subTask == blastTask) {
        composeSubTask = new ComposeResultSubTask(reference, reads, blastTask->getBlastSubtasks(), storage);
        composeSubTask->setSubtaskProgressWeight(0.5f);
        result << composeSubTask;
    }
    return result;
}

SharedDbiDataHandler AlignToReferenceBlastTask::getAlignment() const {
    CHECK(NULL != composeSubTask, SharedDbiDataHandler());
    return composeSubTask->getAlignment();
}

SharedDbiDataHandler AlignToReferenceBlastTask::getAnnotations() const {
    CHECK(NULL != composeSubTask, SharedDbiDataHandler());
    return composeSubTask->getAnnotations();
}

} // LocalWorkflow
} // U2
