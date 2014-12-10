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

#include "DnaAssemblyTask.h"
#include <QtCore/QFileInfo>

namespace U2 {

DnaAssemblyToReferenceTask::DnaAssemblyToReferenceTask( const DnaAssemblyToRefTaskSettings& s, TaskFlags _flags, bool _justBuildIndex )
 : Task("DnaAssemblyToRefTask", _flags), settings(s), justBuildIndex(_justBuildIndex) {
}

void DnaAssemblyToReferenceTask::setUpIndexBuilding(const QStringList& indexExtensions) {
    settings.prebuiltIndex = isPrebuiltIndex(settings.refSeqUrl.getURLString(), indexExtensions);
    if(!settings.prebuiltIndex) {
        int extensionSeparatorPos = settings.refSeqUrl.getURLString().lastIndexOf('.');
        if(extensionSeparatorPos > 0) {
            QString baseIndexName = settings.refSeqUrl.getURLString().left(extensionSeparatorPos);
            if(isPrebuiltIndex(baseIndexName, indexExtensions)) {
                settings.prebuiltIndex = true;
                settings.refSeqUrl = GUrl(baseIndexName);
            }
        }
    }
}

bool DnaAssemblyToReferenceTask::isPrebuiltIndex(const QString& baseFileName, const QStringList& indexExtensions) {
    foreach(const QString& curIndexExtension, indexExtensions) {
        QString indexFilePath = baseFileName + curIndexExtension;
        QFileInfo fileInfo(indexFilePath);
        if(!fileInfo.exists()) {
            return false;
        }
    }
    return true;
}

QVariant DnaAssemblyToRefTaskSettings::getCustomValue( const QString& optionName, const QVariant& defaultVal ) const {
    if (customSettings.contains(optionName)) {
        return customSettings.value(optionName);
    } else {
        return defaultVal;
    }    
}

bool DnaAssemblyToRefTaskSettings::hasCustomValue(const QString & name) const {
    return customSettings.contains(name);
}

void DnaAssemblyToRefTaskSettings::setCustomValue( const QString& optionName, const QVariant& val ) {
    customSettings.insert(optionName,val);
}

QList<GUrl> DnaAssemblyToRefTaskSettings::getShortReadUrls() const
{
    QList<GUrl> res;
    foreach (const ShortReadSet& set, shortReadSets) {
        res.append(set.url);
    }

    return res;
}

void DnaAssemblyToRefTaskSettings::setCustomSettings( const QMap<QString, QVariant>& settings ) {
    customSettings = settings;
}
} // U2

