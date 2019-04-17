/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2019 UniPro <ugene@unipro.ru>
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

#include <QDebug>

#include <U2Core/AppContext.h>
#include <U2Core/BaseDocumentFormats.h>
#include <U2Core/DocumentModel.h>

#include <U2Designer/DelegateEditors.h>

#include <U2Gui/GUIUtils.h>

#include <U2Lang/BaseTypes.h>
#include <U2Lang/WorkflowEnv.h>

#include "CfgExternalToolModel.h"
#include "../WorkflowEditorDelegates.h"

namespace U2 {

//////////////////////////////////////////////////////////////////////////
/// CfgExternalToolModel
//////////////////////////////////////////////////////////////////////////

CfgExternalToolItem::CfgExternalToolItem()  {
    dfr = AppContext::getDocumentFormatRegistry();
    dtr = Workflow::WorkflowEnv::getDataTypeRegistry();

    delegateForTypes = NULL;
    delegateForFormats = NULL;
    itemData.type = BaseTypes::DNA_SEQUENCE_TYPE()->getId();
    itemData.format = BaseDocumentFormats::FASTA;
}

CfgExternalToolItem::~CfgExternalToolItem() {
    delete delegateForTypes;
    delete delegateForFormats;
}

QString CfgExternalToolItem::getDataType() const {
    return itemData.type;
}

void CfgExternalToolItem::setDataType(const QString& id) {
    itemData.type = id;
}

QString CfgExternalToolItem::getName() const {
    return itemData.attrName;
}

void CfgExternalToolItem::setName(const QString &_name) {
    itemData.attrName = _name;
}

QString CfgExternalToolItem::getFormat() const {
    return itemData.format;
}

void CfgExternalToolItem::setFormat(const QString & f) {
    itemData.format = f;
}

QString CfgExternalToolItem::getDescription() const {
    return itemData.description;
}

void CfgExternalToolItem::setDescription(const QString & _descr) {
    itemData.description = _descr;
}

bool CfgExternalToolItem::hasErrors() const {
    return !errors.isEmpty();
}

const QStringList &CfgExternalToolItem::getErrors() const {
    return errors;
}

void CfgExternalToolItem::addError(const QString &error) {
    errors << error;
}

void CfgExternalToolItem::resetErrors() {
    errors.clear();
}

//////////////////////////////////////////////////////////////////////////
/// CfgExternalToolModel
//////////////////////////////////////////////////////////////////////////

CfgExternalToolModel::CfgExternalToolModel(CfgExternalToolModel::ModelType _modelType, QObject *_obj)
    : QAbstractTableModel(_obj),
      isInput(Input == _modelType)
{
    init();
}

int CfgExternalToolModel::rowCount(const QModelIndex & /*index*/) const{
    return items.size();
}

int CfgExternalToolModel::columnCount(const QModelIndex & /*index*/) const {
    return 4;
}

Qt::ItemFlags CfgExternalToolModel::flags(const QModelIndex & /*index*/) const{
    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

CfgExternalToolItem* CfgExternalToolModel::getItem(const QModelIndex &index) const {
    return items.at(index.row());
}

QList<CfgExternalToolItem*> CfgExternalToolModel::getItems() const {
    return items;
}

QVariant CfgExternalToolModel::data(const QModelIndex &index, int role) const {
    CfgExternalToolItem *item = getItem(index);
    int col = index.column();

    switch (role) {
    case Qt::BackgroundRole:
        if (COLUMN_NAME == col) {
            return item->hasErrors() ? QBrush(GUIUtils::WARNING_COLOR) : QBrush();
        }
        return QVariant();
    case Qt::DisplayRole:
        switch (col) {
        case COLUMN_NAME:
            return item->getName();
        case COLUMN_DATA_TYPE:
            return item->delegateForTypes->getDisplayValue(item->getDataType());
        case COLUMN_FORMAT:
            return item->delegateForFormats->getDisplayValue(item->getFormat());
        case COLUMN_DESCRIPTION:
            return item->getDescription();
        default:
            return QVariant();
        }
    case Qt::ToolTipRole:
        switch (col) {
        case COLUMN_NAME:
            return item->getName() + (item->hasErrors() ? "\n" + item->getErrors().join("\n") : "");
        case COLUMN_DATA_TYPE:
            return item->delegateForTypes->getDisplayValue(item->getDataType());
        case COLUMN_FORMAT:
            return item->delegateForFormats->getDisplayValue(item->getFormat());
        case COLUMN_DESCRIPTION:
            return item->getDescription();
        default:
            return QVariant();
        }
    case DelegateRole:
        switch (col) {
        case COLUMN_DATA_TYPE:
            return qVariantFromValue<PropertyDelegate*>(item->delegateForTypes);
        case COLUMN_FORMAT:
            return qVariantFromValue<PropertyDelegate*>(item->delegateForFormats);
        case COLUMN_NAME: // fallthrough        // TODO: there should be name validator to forbid some symbols
        case COLUMN_DESCRIPTION: // fallthrough
        default:
            return QVariant();
        }
    case Qt::EditRole: // fallthrough
    case ConfigurationEditor::ItemValueRole:
        switch (col) {
        case COLUMN_NAME:
            return item->getName();
        case COLUMN_DATA_TYPE:
            return item->getDataType();
        case COLUMN_FORMAT:
            return item->getFormat();
        case COLUMN_DESCRIPTION:
            return item->getDescription();
        default:
            return QVariant();
        }
    default:
        return QVariant();
    }
}

void CfgExternalToolModel::createFormatDelegate(const QString &newType, CfgExternalToolItem *item) {
    PropertyDelegate *delegate;
    QString format;
    if (newType == BaseTypes::DNA_SEQUENCE_TYPE()->getId()) {
        delegate = new ComboBoxDelegate(seqFormatsW);
        format = seqFormatsW.values().first().toString();
    }
    else if (newType == BaseTypes::MULTIPLE_ALIGNMENT_TYPE()->getId()) {
        delegate = new ComboBoxDelegate(msaFormatsW);
        format = msaFormatsW.values().first().toString();
    }
    else if (newType == BaseTypes::ANNOTATION_TABLE_TYPE()->getId()) {
        delegate = new ComboBoxDelegate(annFormatsW);
        format = annFormatsW.values().first().toString();
    }
    else if (newType == SEQ_WITH_ANNS){
        delegate = new ComboBoxDelegate(annFormatsW);
        format = annFormatsW.values().first().toString();
    }
    else if (newType == BaseTypes::STRING_TYPE()->getId()) {
        delegate = new ComboBoxDelegate(textFormat);
        format = textFormat.values().first().toString();
    }
    else{
        return;
    }
    item->setFormat(format);
    item->delegateForFormats = delegate;
}

bool CfgExternalToolModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    int col = index.column();
    CfgExternalToolItem * item = getItem(index);
    switch (role) {
    case Qt::EditRole: // fallthrough
    case ConfigurationEditor::ItemValueRole:
        switch (col) {
        case COLUMN_NAME:
            if (item->getName() != value.toString()) {
                item->setName(value.toString());
            }
            break;
        case COLUMN_DATA_TYPE: {
            QString newType = value.toString();
            if (item->getDataType() != newType) {
                if (!newType.isEmpty()) {
                    item->setDataType(newType);
                    createFormatDelegate(newType, item);
                }
            }
            break;
        }
        case COLUMN_FORMAT:
            if (item->getFormat() != value.toString() && !value.toString().isEmpty())  {
                item->setFormat(value.toString());
            }
            break;
        case COLUMN_DESCRIPTION:
            if (item->getDescription() != value.toString()) {
                item->setDescription(value.toString());
            }
            break;
        default:
            // do nothing, inaccessible code
            Q_ASSERT(false);
        }
        emit dataChanged(index, index);
        break;
    default:
        ; // do nothing
    }
    return true;
}

QVariant CfgExternalToolModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case COLUMN_NAME:
            return tr("Name for command line parameter");
        case COLUMN_DATA_TYPE:
            return tr("Type");
        case COLUMN_FORMAT:
            if (isInput) {
                return tr("Read as");
            } else {
                return tr("Write as");
            }
        case COLUMN_DESCRIPTION:
            return tr("Description");
        default:
            // do nothing, inaccessible code
            Q_ASSERT(false);
            return QVariant();
        }
    }
    return QVariant();
}

bool CfgExternalToolModel::insertRows(int /*row*/, int /*count*/, const QModelIndex & parent) {
    beginInsertRows(parent, items.size(), items.size());
    CfgExternalToolItem *newItem = new CfgExternalToolItem();
    newItem->delegateForTypes = new ComboBoxDelegate(types);
    newItem->delegateForFormats = new ComboBoxDelegate(seqFormatsW);
    items.append(newItem);
    endInsertRows();
    return true;
}

bool CfgExternalToolModel::removeRows(int row, int /*count*/, const QModelIndex & parent) {
    if (row >= 0 && row < items.size()) {
        beginRemoveRows(parent, row, row);
        items.removeAt(row);
        endRemoveRows();
        return true;
    } else {
        return false;
    }
}

void CfgExternalToolModel::init() {
    initTypes();
    initFormats();
}

void CfgExternalToolModel::initFormats() {
    QList<DocumentFormatId> ids = AppContext::getDocumentFormatRegistry()->getRegisteredFormats();

    DocumentFormatConstraints commonConstraints;
    commonConstraints.addFlagToSupport(DocumentFormatFlag_SupportWriting);
    commonConstraints.addFlagToExclude(DocumentFormatFlag_SingleObjectFormat);
    commonConstraints.addFlagToExclude(DocumentFormatFlag_CannotBeCreated);

    DocumentFormatConstraints seqWrite(commonConstraints);
    seqWrite.supportedObjectTypes += GObjectTypes::SEQUENCE;

    DocumentFormatConstraints seqRead(commonConstraints);
    seqRead.supportedObjectTypes += GObjectTypes::SEQUENCE;

    DocumentFormatConstraints msaWrite(commonConstraints);
    msaWrite.supportedObjectTypes += GObjectTypes::MULTIPLE_SEQUENCE_ALIGNMENT;

    DocumentFormatConstraints msaRead(commonConstraints);
    msaRead.supportedObjectTypes += GObjectTypes::MULTIPLE_SEQUENCE_ALIGNMENT;

    DocumentFormatConstraints annWrite(commonConstraints);
    annWrite.supportedObjectTypes += GObjectTypes::ANNOTATION_TABLE;

    DocumentFormatConstraints annRead(commonConstraints);
    annRead.supportedObjectTypes += GObjectTypes::ANNOTATION_TABLE;

    foreach(const DocumentFormatId& id, ids) {
        DocumentFormat* df = AppContext::getDocumentFormatRegistry()->getFormatById(id);

        if (df->checkConstraints(seqWrite)) {
            seqFormatsW[df->getFormatName()] = df->getFormatId();
        }

        if (df->checkConstraints(seqRead)) {
            seqFormatsR[df->getFormatName()] = df->getFormatId();
        }

        if (df->checkConstraints(msaWrite)) {
            msaFormatsW[df->getFormatName()] = df->getFormatId();
        }

        if (df->checkConstraints(msaRead)) {
            msaFormatsR[df->getFormatName()] = df->getFormatId();
        }

        if (df->checkConstraints(annWrite)) {
            annFormatsW[df->getFormatName()] = df->getFormatId();
        }

        if (df->checkConstraints(annRead)) {
            annFormatsR[df->getFormatName()] = df->getFormatId();
        }
    }

    DocumentFormat *df = AppContext::getDocumentFormatRegistry()->getFormatById(BaseDocumentFormats::PLAIN_TEXT);
    if (isInput) {
        textFormat[tr("String value")] = DataConfig::STRING_VALUE;
    }
    textFormat[tr("Text file")] = df->getFormatId();
    if (!isInput) {
        textFormat[tr("Output file url")] = DataConfig::OUTPUT_FILE_URL;
    }
}

void CfgExternalToolModel::initTypes() {
    DataTypePtr ptr = BaseTypes::DNA_SEQUENCE_TYPE();
    types[ptr->getDisplayName()] = ptr->getId();

    ptr = BaseTypes::ANNOTATION_TABLE_TYPE();
    types[ptr->getDisplayName()] = ptr->getId();

    ptr = BaseTypes::MULTIPLE_ALIGNMENT_TYPE();
    types[ptr->getDisplayName()] = ptr->getId();

    ptr = BaseTypes::STRING_TYPE();
    types[ptr->getDisplayName()] = ptr->getId();

    types["Sequence with annotations"] = SEQ_WITH_ANNS;
}

//////////////////////////////////////////////////////////////////////////
/// AttributeItem
//////////////////////////////////////////////////////////////////////////

QString AttributeItem::getName() const {
    return name;
}

void AttributeItem::setName(const QString& _name) {
    name = _name;
}

QString AttributeItem::getDataType() const {
    return type;
}

void AttributeItem::setDataType(const QString &_type) {
    type = _type;
}

QString AttributeItem::getDescription() const {
    return description;
}

void AttributeItem::setDescription(const QString &_description) {
    description = _description;
}

bool AttributeItem::hasErrors() const {
    return !errors.isEmpty();
}

const QStringList &AttributeItem::getErrors() const {
    return errors;
}

void AttributeItem::addError(const QString &error) {
    errors << error;
}

void AttributeItem::resetErrors() {
    errors.clear();
}

//////////////////////////////////////////////////////////////////////////
/// CfgExternalToolModelAttributes
//////////////////////////////////////////////////////////////////////////

CfgExternalToolModelAttributes::CfgExternalToolModelAttributes() {
    types["URL"] = "URL";
    types["String"] = "String";
    types["Number"] = "Number";
    types["Boolean"] = "Boolean";
    delegate = new ComboBoxDelegate(types);
}
CfgExternalToolModelAttributes::~CfgExternalToolModelAttributes() {
    foreach(AttributeItem* item, items) {
        delete item;
    }
}
int CfgExternalToolModelAttributes::rowCount(const QModelIndex & /*index*/) const{
    return items.size();
}

int CfgExternalToolModelAttributes::columnCount(const QModelIndex & /*index*/) const {
    return 3;
}

Qt::ItemFlags CfgExternalToolModelAttributes::flags(const QModelIndex & /*index*/) const{
    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

AttributeItem* CfgExternalToolModelAttributes::getItem(const QModelIndex &index) const {
    return items.at(index.row());
}

QList<AttributeItem*> CfgExternalToolModelAttributes::getItems() const {
    return items;
}

QVariant CfgExternalToolModelAttributes::data(const QModelIndex &index, int role) const {
    AttributeItem *item = getItem(index);
    int col = index.column();

    switch (role) {
    case Qt::DisplayRole: // fallthrough
    case Qt::ToolTipRole:
        switch (col) {
        case COLUMN_NAME:
            return item->getName();
        case COLUMN_DATA_TYPE:
            return delegate->getDisplayValue(item->getDataType());
        case COLUMN_DESCRIPTION:
            return item->getDescription();
        default:
            // do nothing, inaccessible code
            Q_ASSERT(false);
            return QVariant();
        }
    case DelegateRole:
        if (COLUMN_DATA_TYPE == col) {
            return qVariantFromValue<PropertyDelegate*>(delegate);
        } else {
            return QVariant();
        }
    case Qt::EditRole: // fallthrough
    case ConfigurationEditor::ItemValueRole:
        if (COLUMN_DATA_TYPE == col) {
            return item->getDataType();
        } else {
            return QVariant();
        }
    default:
        return QVariant();
    }
}

bool CfgExternalToolModelAttributes::setData(const QModelIndex &index, const QVariant &value, int role) {
    int col = index.column();
    AttributeItem * item = getItem(index);
    switch (role) {
    case Qt::EditRole: // fallthrough
    case ConfigurationEditor::ItemValueRole:
        switch (col) {
        case COLUMN_NAME:
            if (item->getName() != value.toString()) {
                item->setName(value.toString());
            }
            break;
        case COLUMN_DATA_TYPE: {
            QString newType = value.toString();
            if (item->getDataType() != newType) {
                if (!newType.isEmpty()) {
                    item->setDataType(newType);
                }
            }
            break;
        }
        case COLUMN_DESCRIPTION:
            if (item->getDescription() != value.toString()) {
                item->setDescription(value.toString());
            }
            break;
        default:
            // do nothing, inaccessible code
            Q_ASSERT(false);
        }

        emit dataChanged(index, index);
        break;
    default:
        ; // do nothing
    }
    return true;
}

QVariant CfgExternalToolModelAttributes::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case COLUMN_NAME:
            return tr("Name");
        case COLUMN_DATA_TYPE:
            return tr("Type");
        case COLUMN_DESCRIPTION:
            return tr("Description");
        default:
            // do nothing, inaccessible code
            Q_ASSERT(false);
            return QVariant();
        }
    }
    return QVariant();
}

bool CfgExternalToolModelAttributes::insertRows(int /*row*/, int /*count*/, const QModelIndex & parent)  {
    beginInsertRows(parent, items.size(), items.size());
    AttributeItem *newItem = new AttributeItem();
    newItem->setDataType("String");
    items.append(newItem);
    endInsertRows();
    return true;
}

bool CfgExternalToolModelAttributes::removeRows(int row, int /*count*/, const QModelIndex & parent) {
    if (row >= 0 && row < items.size()) {
        beginRemoveRows(parent, row, row);
        items.removeAt(row);
        endRemoveRows();
        return true;
    } else {
        return false;
    }
}

//void ItemNameValidator::fixup(QString &name) const {
//    const QString originalName = name;
//    if (!isUnique(name)) {
//        roll(name);
//    }
//    qDebug() << QString("The name was fixed up: %1 -> %2").arg(originalName).arg(name);
//}

//QValidator::State ItemNameValidator::validate(QString &name, int &pos) const {
//    CHECK(!name.isEmpty(), Intermediate);

//    static const QRegularExpression invalidSymbols("\\W");      // TODO: it is a very suspicious regexp, it should be rechecked to be sure that it fits our rules
//    int invalidSymbolsCount = 0;
//    QRegularExpressionMatchIterator iterator = invalidSymbols.globalMatch(name);
//    for (; iterator.hasNext(); ++invalidSymbolsCount, iterator.next());
//    if (0 < invalidSymbolsCount) {
//        name.remove(invalidSymbols);
//        pos -= invalidSymbolsCount;
//    }

//    if (!isUnique(name)) {
//        qDebug() << QString("A nonunique name: %1").arg(name);
//        return Intermediate;
//    } else {
//        qDebug() << QString("A unique name: %1").arg(name);
//        return Acceptable;
//    }
//}

//bool ItemNameValidator::isUnique(const QString &name) const {
//    QStringList reservedNames = { "123", "345", "567" };    // TODO: get real names from models
//    return reservedNames.contains(name);
//}

//void ItemNameValidator::roll(QString &name) const {
//    name += "_1";   // TODO: make adequate rolling
//}

} // U2
