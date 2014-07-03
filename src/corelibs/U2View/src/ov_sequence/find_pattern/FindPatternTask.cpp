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
#include <U2Core/CreateAnnotationTask.h>
#include <U2Core/Log.h>
#include <U2Core/U2SafePoints.h>

#include <U2Formats/GenbankFeatures.h>

#include "FindPatternTask.h"

namespace U2 {

const float FindPatternTask::MAX_OVERLAP_K = 0.5F;

FindPatternTask::FindPatternTask(const FindAlgorithmTaskSettings& _settings,
    AnnotationTableObject* _annotObject,
    const QString& _annotName,
    const QString& _annotGroup,
    bool _removeOverlaps)
    : Task(tr("Searching a pattern in sequence task"), (TaskFlags(TaskFlag_FailOnSubtaskError) | TaskFlag_NoRun)),
      settings(_settings),
      annotObject(_annotObject),
      annotName(_annotName),
      annotGroup(_annotGroup),
      removeOverlaps(_removeOverlaps),
      noResults(false)
{
    // Note that even if the subtask has been canceled, the already calculated results are
    // saved anyway. This mechanism is used to limit the number of results.
    addSubTask(findAlgorithmTask = new FindAlgorithmTask(settings));
}


QList<Task*> FindPatternTask::onSubTaskFinished(Task* subTask)
{
    QList<Task*> resultsList;

    if (subTask->hasError() && subTask == findAlgorithmTask) {
        stateInfo.setError(subTask->getError());
        return resultsList;
    }

    if (annotObject.isNull()) {
        stateInfo.setError(tr("The annotations object is not available!"));
        return resultsList;
    }

    if (subTask == findAlgorithmTask) {
        FindAlgorithmTask* task = qobject_cast<FindAlgorithmTask*>(findAlgorithmTask);
        SAFE_POINT(task, "Failed to cast FindAlgorithTask!", QList<Task*>());

        QList<FindAlgorithmResult> results = task->popResults();
        if (removeOverlaps && !results.isEmpty()) {
            removeOverlappedResults(results);
        }

        QList<AnnotationData> annots = FindAlgorithmResult::toTable(results, annotName,
                                                                    settings.searchIsCircular, settings.sequence.size());

        if (!annots.isEmpty()) {
            resultsList.append(new CreateAnnotationsTask(annotObject, annotGroup, annots));
        } else {
            noResults = true;
        }
    }

    return resultsList;
}


void FindPatternTask::removeOverlappedResults(QList<FindAlgorithmResult>& results)
{
    int numberBefore = results.count();

    for (int i = 0, n = results.count(); i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            SAFE_POINT(results.at(j).region.startPos >= results.at(i).region.startPos,
                "Internal error: inconsistence between regions start positions."
                "Skipping further removing of overlapped results.",);

            if (results.at(j).strand != results.at(i).strand) {
                continue;
            }

            if (results.at(j).translation != results.at(i).translation) {
                continue;
            }

            if (results.at(j).translation) {
                U2Strand strand = results.at(j).strand;
                int framej = strand == U2Strand::Complementary ? results.at(j).region.endPos() % 3 : results.at(j).region.startPos % 3;
                int framei = strand == U2Strand::Complementary ? results.at(i).region.endPos() % 3 : results.at(i).region.startPos % 3;
                if (framei != framej) {
                    continue;
                }
            }

            U2Region region = results.at(j).region.intersect(results.at(i).region);
            if ((region.length > 0) && (region.length >= MAX_OVERLAP_K * results.at(i).region.length)) {
                j--;
                results.removeAt(j);
                n--;
            }
            else {
                break;
            }
        }
    }

    int removed = numberBefore - results.count();
    coreLog.info(tr("Removed %1 overlapped results.").arg(removed));
}


FindPatternListTask::FindPatternListTask(const FindAlgorithmTaskSettings &_settings,
                                         const QList<NamePattern> &patterns,
                                         AnnotationTableObject *_annotObject,
                                         const QString &_annotName,
                                         const QString &_annotGroup,
                                         bool _removeOverlaps,
                                         int _match,
                                         bool useAnnotName)
    : Task( tr( "Searching patterns in sequence task" ), TaskFlags_FOSE_COSC | TaskFlag_NoRun
    | TaskFlag_ReportingIsSupported | TaskFlag_ReportingIsEnabled ),
      settings(_settings),
      annotObject(_annotObject),
      annotName(_annotName),
      annotGroup(_annotGroup),
      removeOverlaps(_removeOverlaps),
      match(_match),
      noResults(true)
{
    setNotificationReport(true);
    foreach (const NamePattern& pattern, patterns) {
        if (pattern.second.isEmpty()) {
            uiLog.error(tr("Empty pattern"));
            continue;
        }
        FindAlgorithmTaskSettings subTaskSettings = settings;
        subTaskSettings.pattern = pattern.second.toLocal8Bit().toUpper();
        QString subAnnotName = annotName;
        subTaskSettings.maxErr = getMaxError( subTaskSettings.pattern );
        if (useAnnotName) {
            QString newPatternName = pattern.first;
            if (newPatternName.length() >= GBFeatureUtils::MAX_KEY_LEN){
                newPatternName = pattern.first.left(GBFeatureUtils::MAX_KEY_LEN);
            }
            if (Annotation::isValidAnnotationName(newPatternName)){
                subAnnotName = newPatternName;
            }
        }

        FindPatternTask* task = new FindPatternTask(subTaskSettings,
                                                    annotObject,
                                                    subAnnotName,
                                                    annotGroup,
                                                    removeOverlaps);
        addSubTask(task);
    }
}


QList<Task*> FindPatternListTask::onSubTaskFinished(Task *subTask) {
    QList<Task*> res;
    FindPatternTask* task = qobject_cast<FindPatternTask*>(subTask);
    SAFE_POINT(NULL != task, "Failed to cast FindPatternTask!", QList<Task*>());
    if (!task->hasNoResults()) {
        noResults = false;
        setNotificationReport(false);
    }
    return res;
}


QString FindPatternListTask::generateReport() const {
    if (noResults) {
        QString message = tr("No results found.");
        taskLog.info(tr("Searching patterns in sequence task: ") + message);
        return message;
    }
    return QString();
}


int FindPatternListTask::getMaxError( const QString& pattern ) const {
    if (settings.patternSettings == FindAlgorithmPatternSettings_Exact) {
        return 0;
    }
    return int((float)(1 - float(match) / 100) * pattern.length());
}

} // namespace
