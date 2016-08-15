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

#include <QMessageBox>
#include <QPushButton>

#include "ui_SaveSelectedSequenceFromMSADialog.h"

#include <U2Core/BaseDocumentFormats.h>
#include <U2Core/L10n.h>

#include <U2Gui/HelpButton.h>
#include <U2Gui/SaveDocumentController.h>

#include "SaveSelectedSequenceFromMSADialogController.h"


namespace U2 {

SaveSelectedSequenceFromMSADialogController::SaveSelectedSequenceFromMSADialogController(QWidget* p)
    : QDialog(p),
      saveController(NULL),
      ui(new Ui_SaveSelectedSequenceFromMSADialog())
{
    ui->setupUi(this);
    new HelpButton(this, ui->buttonBox, "18220445");
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Export"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));

    trimGapsFlag = false;
    addToProjectFlag = true;

    initSaveController();
}
SaveSelectedSequenceFromMSADialogController::~SaveSelectedSequenceFromMSADialogController(){
    delete ui;
}

void SaveSelectedSequenceFromMSADialogController::accept() {
    if (saveController->getSaveFileName().isEmpty()) {
        QMessageBox::critical(this, L10N::errorTitle(), tr("File name is empty!"));
        return;
    }

    url = saveController->getSaveFileName();
    format = saveController->getFormatIdToSave();
    trimGapsFlag = ui->trimGapsRB->isChecked();
    addToProjectFlag = ui->addToProjectBox->isChecked();

    QDialog::accept();
}

void SaveSelectedSequenceFromMSADialogController::initSaveController() {
    SaveDocumentControllerConfig config;
    config.defaultFormatId = BaseDocumentFormats::FASTA;
    config.fileDialogButton = ui->fileButton;
    config.fileNameEdit = ui->fileNameEdit;
    config.formatCombo = ui->formatCombo;
    config.parentWidget = this;

    DocumentFormatConstraints formatConstraints;
    formatConstraints.supportedObjectTypes << GObjectTypes::SEQUENCE;
    formatConstraints.addFlagToExclude(DocumentFormatFlag_SingleObjectFormat);
    formatConstraints.addFlagToSupport(DocumentFormatFlag_SupportWriting);

    saveController = new SaveDocumentController(config, formatConstraints, this);
}

}
