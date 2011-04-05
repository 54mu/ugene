/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2011 UniPro <ugene@unipro.ru>
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

#include <QtGui/QAction>
#include <QtGui/QMenu>
#include <QtGui/QMessageBox>

#include <U2Core/AppContext.h>
#include <U2Core/BaseDocumentFormats.h>
#include <U2Core/DocumentModel.h>
#include <U2Core/DocumentUtils.h>
#include <U2Core/GUrlUtils.h>
#include <U2Gui/MainWindow.h>
#include <U2Gui/ObjectViewModel.h>
#include <U2Algorithm/MSAAlignAlgRegistry.h>
#include <U2Core/ProjectModel.h>
#include <U2Core/DNASequenceObject.h>
#include <U2Core/MAlignmentObject.h>
#include <U2Core/GObjectSelection.h>
#include <U2Core/LoadDocumentTask.h>
#include <U2Core/SaveDocumentTask.h>
#include <U2Core/AddDocumentTask.h>
#include <U2Gui/OpenViewTask.h>
#include <U2Core/MSAUtils.h>
#include <U2Core/DNATranslation.h>


#include "MSAAlignUtils.h"
#include "MSAAlignDialog.h"


namespace U2 {

MSAAlignFileTask::MSAAlignFileTask( const MSAAlignTaskSettings& s, bool view)
: Task("MSAAlignMultiTask", TaskFlags_NR_FOSCOE), settings(s), alignTask(NULL), 
addDocumentTask(NULL), loadDocumentTask(NULL), doc(NULL), openView(view), obj(NULL)
{

}

void MSAAlignFileTask::prepare() {
    DocumentFormatConstraints c;
    c.checkRawData = true;
    c.supportedObjectTypes += GObjectTypes::MULTIPLE_ALIGNMENT;
    c.rawData = BaseIOAdapters::readFileHeader(settings.resultFileName);
    QList<DocumentFormatId> formats = AppContext::getDocumentFormatRegistry()->selectFormats(c);
    if (formats.isEmpty()) {
        stateInfo.setError(  tr("input_format_error") );
        return;
    }

    DocumentFormatId alnFormat = formats.first();
    loadDocumentTask=
        new LoadDocumentTask(alnFormat,
        settings.resultFileName,
        AppContext::getIOAdapterRegistry()->getIOAdapterFactoryById(BaseIOAdapters::url2io(settings.resultFileName)));
    addSubTask(loadDocumentTask);
}

Task::ReportResult MSAAlignFileTask::report() {
    return ReportResult_Finished;
}

QList<Task*> MSAAlignFileTask::onSubTaskFinished( Task* subTask ) {
    QList<Task*> res;
    if (subTask->hasErrors() || isCanceled()) {
        return res;
    }

    if(subTask==loadDocumentTask){
        doc = loadDocumentTask->getDocument()->clone();
        assert(doc != NULL);
        assert(doc->getObjects().length() == 1);
        obj = qobject_cast<MAlignmentObject*>(doc->getObjects().first());
        assert(obj != NULL);
        QString algName = settings.algName;
        MSAAlignAlgorithmEnv* env= AppContext::getMSAAlignAlgRegistry()->getAlgorithm(algName);
        assert(env);
        if (env == NULL) {
            setError(QString("Algorithm %1 is not found").arg(algName));
            return res;
        }
        alignTask = env->getTaskFactory()->createTaskInstance(obj, settings);
        res.append(alignTask);        
    } else if(subTask == alignTask){
        saveDocumentTask = new SaveDocumentTask(doc, 
            AppContext::getIOAdapterRegistry()->getIOAdapterFactoryById(BaseIOAdapters::url2io(settings.resultFileName)),
            settings.resultFileName);
        res.append(saveDocumentTask);
    } else if(subTask==saveDocumentTask){
        Project* proj = AppContext::getProject();
        if (proj == NULL) {
            res.append(AppContext::getProjectLoader()->openProjectTask(doc->getURLString(), false));
        } else {
            bool docAlreadyInProject=false;
            foreach(Document* d, proj->getDocuments()){
                if(doc->getURL() == d->getURL()){
                    docAlreadyInProject=true;
                }
            }
            if (docAlreadyInProject) {
                res.append(new LoadUnloadedDocumentAndOpenViewTask(doc));
            } else {
                // Add document to project
                res.append(new AddDocumentTask(doc));
                res.append(new LoadUnloadedDocumentAndOpenViewTask(doc));
            }
        }
    }

    return res;
}

const MAlignmentObject* MSAAlignFileTask::getAlignResult() {
    if ( doc == NULL ) {
        return NULL;
    }

    QList<GObject*> objs = doc->getObjects();
    if ( objs.size() == 0 ) {
        return NULL;
    }

    return qobject_cast<MAlignmentObject*> (objs.first());
}

//////////////////////////////////////////////////////////////////////////
/// TranslateMSA2AminoTask


TranslateMSA2AminoTask::TranslateMSA2AminoTask( MAlignmentObject* obj )
: Task("TranslateMSA2AminoTask", TaskFlags_FOSCOE), maObj(obj)
{
    assert(maObj != NULL);
    assert(maObj->getAlphabet()->isNucleic());

    translations = AppContext::getDNATranslationRegistry()->lookupTranslation(maObj->getAlphabet(), DNATranslationType_NUCL_2_AMINO);

}

void TranslateMSA2AminoTask::run()
{
    assert(!translations.isEmpty());

    if (translations.isEmpty()) {
        setError(tr("Unable to find suitable translation for %1").arg(maObj->getGObjectName()));
        return;
    }

    DNATranslation* transl = translations.first();

    QList<DNASequence> lst = MSAUtils::ma2seq(maObj->getMAlignment(), true);
    MAlignment resultMa(maObj->getMAlignment().getName(),transl->getDstAlphabet()) ;

    foreach (const DNASequence& dna, lst) {    
        QByteArray buf;
        buf.reserve(dna.length() / 3);
        transl->translate(dna.seq.constData(), dna.length(), buf.data(), buf.length());
        buf.replace("*","X");
        MAlignmentRow row(dna.getName(), buf);  
        resultMa.addRow(row);
    }

    maObj->setMAlignment(resultMa);

}


//////////////////////////////////////////////////////////////////////////
/// MSAAlignMultiTask

MSAAlignMultiTask::MSAAlignMultiTask( MAlignmentObject* obj, const MSAAlignTaskSettings& s )
: Task ("MSAAlignMultiTask",TaskFlags_FOSCOE), settings(s), maObj(obj)
{
    setMaxParallelSubtasks(1);  
}

void MSAAlignMultiTask::prepare()
{
    if (settings.useAminoMode == true) {
        assert(maObj->getAlphabet()->isNucleic());
        bufMA = maObj->getMAlignment();
        addSubTask(new TranslateMSA2AminoTask(maObj));
    }

    QString algName = settings.algName;
    MSAAlignAlgorithmEnv* env = AppContext::getMSAAlignAlgRegistry()->getAlgorithm(algName);
    assert(env);
    if (env == NULL) {
        setError(tr("Multiple alignment algorithm %1 is not found").arg(algName));
        return;
    }
    Task* alignTask = env->getTaskFactory()->createTaskInstance(maObj, settings);
    addSubTask(alignTask);

}

Task::ReportResult MSAAlignMultiTask::report()
{

    return ReportResult_Finished;
}

void MSAAlignMultiTask::run()
{
    if ( hasErrors() || isCanceled() ) {
        return;
    }
    
    if (bufMA.isEmpty()) {
        return;
    }
    
    // TODO: perform back translation
    

}



} // U2

