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

#include <QtCore/qglobal.h>

#include <QApplication>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>

#include "GTTestsSanger.h"
#include "GTUtilsLog.h"
#include "GTUtilsTaskTreeView.h"
#include "GTUtilsProject.h"
#include "GTUtilsProjectTreeView.h"

#include <base_dialogs/MessageBoxFiller.h>
#include <base_dialogs/GTFileDialog.h>

#include <primitives/GTCheckBox.h>
#include <primitives/GTLineEdit.h>
#include <primitives/GTMenu.h>
#include <primitives/GTSpinBox.h>
#include <primitives/GTWidget.h>

#include "../../../runnables/ugene/plugins/external_tools/AlignToReferenceBlastDialogFiller.h"



namespace U2 {

namespace GUITest_common_scenarios_sanger {
using namespace HI;
GUI_TEST_CLASS_DEFINITION(test_0001) {
    GTLogTracer l;

    AlignToReferenceBlastDialogFiller::Settings settings;
    settings.referenceUrl = testDir + "_common_data/sanger/reference.gb";
    for (int i = 5; i <= 7; i++) {
        settings.readUrls << QString(testDir + "_common_data/sanger/sanger_%1.ab1").arg(i, 2, 10, QChar('0'));
    }
    settings.outAlignment = QFileInfo(sandBoxDir + "sanger_test_0001").absoluteFilePath();

    GTUtilsDialog::waitForDialog(os, new AlignToReferenceBlastDialogFiller(settings, os));
    GTMenu::clickMainMenuItem(os, QStringList() << "Tools" << "Sanger data analysis" << "Align Sanger reads to reference...");

    GTUtilsTaskTreeView::waitTaskFinished(os);

    GTUtilsLog::check(os, l);
}

GUI_TEST_CLASS_DEFINITION(test_0002) {
    class CheckerFiller : public Filler {
    public:
        CheckerFiller(HI::GUITestOpStatus &os, const AlignToReferenceBlastDialogFiller::Settings& settings)
            : Filler(os, "AlignToReferenceBlastDialog"),
              settings(settings)
        {}

        virtual void run() {
            QWidget* dialog = QApplication::activeModalWidget();
            CHECK_SET_ERR(dialog, "activeModalWidget is NULL");

            GTUtilsDialog::waitForDialog(os, new MessageBoxDialogFiller(os, QMessageBox::Ok));
            GTUtilsDialog::clickButtonBox(os, dialog, QDialogButtonBox::Ok);

            QLineEdit* reference = qobject_cast<QLineEdit*>(GTWidget::findWidget(os, "referenceLineEdit", dialog));
            CHECK_SET_ERR(reference, "referenceLineEdit is NULL");
            GTLineEdit::setText(os, reference, settings.referenceUrl);

            GTUtilsDialog::waitForDialog(os, new MessageBoxDialogFiller(os, QMessageBox::Ok));
            GTUtilsDialog::clickButtonBox(os, dialog, QDialogButtonBox::Ok);

            QWidget* addReadButton = GTWidget::findWidget(os, "addReadButton");
            CHECK_SET_ERR(addReadButton, "addReadButton is NULL");
            foreach (const QString& read, settings.readUrls) {
                GTUtilsDialog::waitForDialog(os, new GTFileDialogUtils(os, read));
                GTWidget::click(os, addReadButton);
                GTGlobals::sleep();
            }

            GTUtilsDialog::waitForDialog(os, new MessageBoxDialogFiller(os, QMessageBox::Ok));
            GTUtilsDialog::clickButtonBox(os, dialog, QDialogButtonBox::Ok);

            GTUtilsDialog::clickButtonBox(os, dialog, QDialogButtonBox::Cancel);
        }
    private:
        AlignToReferenceBlastDialogFiller::Settings settings;
    };

    GTLogTracer l;

    AlignToReferenceBlastDialogFiller::Settings settings;
    settings.referenceUrl = testDir + "_common_data/sanger/reference.gb";
    settings.readUrls << testDir + "_common_data/sanger/sanger_05.ab1";

    GTUtilsDialog::waitForDialog(os, new CheckerFiller(os, settings));
    GTMenu::clickMainMenuItem(os, QStringList() << "Tools" << "Sanger data analysis" << "Align Sanger reads to reference...");

    GTUtilsTaskTreeView::waitTaskFinished(os);

    GTUtilsLog::check(os, l);
}

GUI_TEST_CLASS_DEFINITION(test_0003) {
    GTLogTracer l;

    AlignToReferenceBlastDialogFiller::Settings settings;
    settings.referenceUrl = testDir + "_common_data/sanger/reference.gb";
    for (int i = 11; i <= 13; i++) {
        settings.readUrls << QString(testDir + "_common_data/sanger/sanger_%1.ab1").arg(i, 2, 10, QChar('0'));
    }
    settings.outAlignment = QFileInfo(sandBoxDir + "sanger_test_0003").absoluteFilePath();

    GTUtilsDialog::waitForDialog(os, new AlignToReferenceBlastDialogFiller(settings, os));
    GTMenu::clickMainMenuItem(os, QStringList() << "Tools" << "Sanger data analysis" << "Align Sanger reads to reference...");

    GTUtilsTaskTreeView::waitTaskFinished(os);
    GTUtilsLog::checkContainsError(os, l, "No read satisfy minimum identity criteria");
    GTUtilsProject::checkProject(os, GTUtilsProject::NotExists);

    settings.minIdentity = 30;

    GTUtilsDialog::waitForDialog(os, new AlignToReferenceBlastDialogFiller(settings, os));
    GTMenu::clickMainMenuItem(os, QStringList() << "Tools" << "Sanger data analysis" << "Align Sanger reads to reference...");

    GTUtilsTaskTreeView::waitTaskFinished(os);
    GTUtilsProjectTreeView::checkItem(os, "sanger_test_0003");
}

GUI_TEST_CLASS_DEFINITION(test_0004) {
    GTLogTracer l;

    AlignToReferenceBlastDialogFiller::Settings settings;
    settings.referenceUrl = testDir + "_common_data/sanger/reference.gb";
    for (int i = 18; i <= 20; i++) {
        settings.readUrls << QString(testDir + "_common_data/sanger/sanger_%1.ab1").arg(i, 2, 10, QChar('0'));
    }
    settings.outAlignment = QFileInfo(sandBoxDir + "sanger_test_0004").absoluteFilePath();
    settings.addResultToProject = false;

    GTUtilsDialog::waitForDialog(os, new AlignToReferenceBlastDialogFiller(settings, os));
    GTMenu::clickMainMenuItem(os, QStringList() << "Tools" << "Sanger data analysis" << "Align Sanger reads to reference...");

    GTUtilsTaskTreeView::waitTaskFinished(os);
    GTUtilsProject::checkProject(os, GTUtilsProject::NotExists);

    settings.addResultToProject = true;
    settings.outAlignment = QFileInfo(sandBoxDir + "sanger_test_0004_1").absoluteFilePath();

    GTUtilsDialog::waitForDialog(os, new AlignToReferenceBlastDialogFiller(settings, os));
    GTMenu::clickMainMenuItem(os, QStringList() << "Tools" << "Sanger data analysis" << "Align Sanger reads to reference...");

    GTUtilsTaskTreeView::waitTaskFinished(os);
    GTUtilsProjectTreeView::checkItem(os, "sanger_test_0004_4");

    GTUtilsLog::check(os, l);
}

} // namespace GUITest_common_scenarios_sanger

} // namespace U2

