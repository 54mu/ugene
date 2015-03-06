/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2015 UniPro <ugene@unipro.ru>
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

#include <U2View/MSAGraphOverview.h>

#include "GTTestsRegressionScenarios.h"
#include "GTUtilsAnnotationsTreeView.h"
#include "GTUtilsDocument.h"
#include "GTUtilsLog.h"
#include "GTUtilsMsaEditor.h"
#include "GTUtilsMsaEditorSequenceArea.h"
#include "GTUtilsProjectTreeView.h"
#include "GTUtilsOptionPanelMSA.h"
#include "GTUtilsOptionPanelSequenceView.h"
#include "GTUtilsProjectTreeView.h"
#include "GTUtilsTaskTreeView.h"
#include "GTUtilsWorkflowDesigner.h"

#include "api/GTAction.h"
#include "api/GTFile.h"
#include "api/GTFileDialog.h"
#include "api/GTMenu.h"
#include "api/GTMouseDriver.h"
#include "api/GTKeyboardDriver.h"
#include "api/GTTextEdit.h"
#include "api/GTWidget.h"

#include "runnables/qt/MessageBoxFiller.h"
#include "runnables/qt/PopupChooser.h"
#include "runnables/ugene/corelibs/U2Gui/ImportBAMFileDialogFiller.h"
#include "runnables/ugene/corelibs/U2Gui/CreateAnnotationWidgetFiller.h"
#include "runnables/ugene/corelibs/U2View/ov_msa/DeleteGapsDialogFiller.h"
#include "runnables/ugene/ugeneui/DocumentFormatSelectorDialogFiller.h"
#include "runnables/ugene/ugeneui/SequenceReadingModeSelectorDialogFiller.h"
#include "runnables/ugene/plugins/pcr/PrimersDetailsDialogFiller.h"

namespace U2 {

namespace GUITest_regression_scenarios {

GUI_TEST_CLASS_DEFINITION(test_4008) {
//    1. Open "samples/CLUSTALW/COI.aln".
//    2. Use Alignment Viewer context menu -> View.
//    Expected: "Show offsets" option is enabled.
//    3. Open "_common_data/clustal/big.aln".
//    4. Use Alignment Viewer context menu -> View.
//    Expected: "Show offsets" option is enabled.
//    Current: "Show offsets" option is disabled.

    GTFileDialog::openFile(os, dataDir + "samples/CLUSTALW/", "COI.aln");

    GTUtilsDialog::waitForDialog(os, new PopupChecker(os, QStringList() << "MSAE_MENU_VIEW" << "show_offsets",
                                                            PopupChecker::IsEnabled | PopupChecker::IsChecable));
    GTUtilsMSAEditorSequenceArea::callContextMenu(os);
    GTGlobals::sleep();

    GTFileDialog::openFile(os, testDir + "_common_data/clustal/", "big.aln");
    GTUtilsTaskTreeView::waitTaskFinished(os);

    GTGlobals::sleep();
    GTUtilsDialog::waitForDialog(os, new PopupChecker(os, QStringList() << "MSAE_MENU_VIEW" << "show_offsets",
                                                            PopupChecker::IsEnabled | PopupChecker::IsChecable));
    GTUtilsMSAEditorSequenceArea::callContextMenu(os);
}

GUI_TEST_CLASS_DEFINITION(test_4011){
    GTLogTracer l;
//    1. Open WD
    GTUtilsWorkflowDesigner::openWorkflowDesigner(os);
//    2. Open sample "Align sequences with MUSCLE"
    GTUtilsWorkflowDesigner::addSample(os, "Align sequences with MUSCLE");
//    3. Align attached file
    GTUtilsWorkflowDesigner::click(os, "Read alignment");
    GTUtilsWorkflowDesigner::setDatasetInputFile(os, testDir + "_common_data/regression/4011", "human_T1.aln");
    GTUtilsWorkflowDesigner::runWorkflow(os);
//    Current state:
//    Runtime error occured(x86 version of UGENE)
//    Windows hangs(x64 version)
    l.checkMessage("Nothing to write");
}

GUI_TEST_CLASS_DEFINITION(test_4010) {
//    1. Open "samples/FASTA/human_T1.fa".
    GTFileDialog::openFile(os, dataDir + "samples/FASTA", "human_T1.fa");

//    2. Open the PCR OP tab.
    GTUtilsOptionPanelSequenceView::openTab(os, GTUtilsOptionPanelSequenceView::InSilicoPcr);

//    3. Enter the forward primer: AAGGAAAAAATGCT.
    GTUtilsOptionPanelSequenceView::setForwardPrimer(os, "AAGGAAAAAATGCT");

//    4. Enter the reverse primer: AGCATTTTTTCCTT.
    GTUtilsOptionPanelSequenceView::setReversePrimer(os, "AGCATTTTTTCCTT");

//    5. Click the Primers Details dialog.
//    Expected: the primers are whole dimers, 14 red lines.
    class Scenario : public CustomScenario {
        void run(U2::U2OpStatus &os) {
            QWidget *dialog = QApplication::activeModalWidget();
            CHECK_SET_ERR(NULL != dialog, "Active modal widget is NULL");

            GTTextEdit::containsString(os, GTWidget::findExactWidget<QTextEdit *>(os, "textEdit"), "||||||||||||||");
            GTUtilsDialog::clickButtonBox(os, dialog, QDialogButtonBox::Ok);
        }
    };

    GTUtilsDialog::waitForDialog(os, new PrimersDetailsDialogFiller(os, new Scenario));
    GTUtilsOptionPanelSequenceView::showPrimersDetails(os);
}

GUI_TEST_CLASS_DEFINITION(test_4026) {
    //1. Open "samples/Genbank/sars.gb".
    //Expected: there are a lot of annotations in the panoramic and details views.
    GTFileDialog::openFile(os, dataDir + "samples/Genbank/sars.gb");

    //2. Close the MDI window.
    GTKeyboardDriver::keyClick(os, 'w', GTKeyboardDriver::key["ctrl"]);

    //3. Double click the sequence in the project.
    GTUtilsProjectTreeView::doubleClickItem(os, "NC_004718");

    //Expected: there is the same amount of annotations in the panoramic and details views.
    //Actual: annotations are now shown in the views. Their locations and qualifier names are deleted.
    QString value = GTUtilsAnnotationsTreeView::getQualifierValue(os, "evidence", "5'UTR");
    CHECK_SET_ERR("not_experimental" == value, QString("Unexpected qualifier value"));
}

GUI_TEST_CLASS_DEFINITION(test_4030) {
    //1. Open "samples/CLUSTALW/COI.aln".
    GTFileDialog::openFile(os, dataDir + "samples/CLUSTALW/COI.aln");

    //2. Open the "Statistics" tab.
    GTUtilsOptionPanelMsa::openTab(os, GTUtilsOptionPanelMsa::Statistics);

    //3. Edit the alignment.
    GTUtilsMSAEditorSequenceArea::selectArea(os, QPoint(1, 1), QPoint(1, 1));
    GTKeyboardDriver::keyClick(os, GTKeyboardDriver::key["space"]);

    //Expected state: hint about reference sequence is hidden
    QWidget *label = GTWidget::findWidget(os, "refSeqWarning");
    CHECK_SET_ERR(!label->isVisible(), "Label is shown");
}

GUI_TEST_CLASS_DEFINITION(test_4034) {
    //1. Open "samples/Genbank/murine.gb".
    //2. Ctrl + N.
    //There are two check boxes for switching the location type. Usually, when you choose between several actions (switch them), GUI shows you radio buttons, not check boxes.
    //Check boxes are used to switch on/off an option but not to choose between options. In this dialog, you even can't switch off the check box when you click it. 
    //It is a wrong behavior for this graphic primitive
    //Solution: replace the check boxes with radio buttons.
    GTFileDialog::openFile(os, dataDir+"samples/Genbank/", "murine.gb");

    GTUtilsDialog::waitForDialog(os, new CreateAnnotationWidgetFiller(os, true, "exon", "annotation", "200..300",
        sandBoxDir + "ann_test_4034.gb"));
    GTUtilsDialog::waitForDialog(os, new PopupChooser(os, QStringList() << "ADV_MENU_ADD" << "create_annotation_action"));
    GTWidget::click(os, GTWidget::findWidget(os, "ADV_single_sequence_widget_0"), Qt::RightButton);
}

GUI_TEST_CLASS_DEFINITION(test_4036) {
//     1. Open "_common_data/clustal/gap_column.aln".
//     2. MSA sequence area context menu -> Edit -> Remove columns of gaps.
//     3. Choose "Remove all gap-only columns".
//     4. Click "Remove".
//     UGENE 1.16-dev: it take ~15 minutes to remove gaps.
//     UGENE 1.15.1: it takes ~5 seconds to remove gaps.

    GTFileDialog::openFile(os, testDir + "_common_data/clustal/", "gap_column.aln");
    GTUtilsTaskTreeView::waitTaskFinished(os);

    GTUtilsDialog::waitForDialog(os, new PopupChooser(os, QStringList() << MSAE_MENU_EDIT << "remove_columns_of_gaps"));
    GTUtilsDialog::waitForDialog(os, new DeleteGapsDialogFiller(os, 1));

    GTUtilsMSAEditorSequenceArea::callContextMenu(os);
    GTUtilsTaskTreeView::waitTaskFinished(os);
}

GUI_TEST_CLASS_DEFINITION(test_4045) {
    //1. Open "murine.gb"
    //2. Find ORFs
    //3. Unload "murine.gb"
    //4. Open "murine.gb"
    //Current state: SAFE_POINT is triggered
    GTLogTracer logTracer;
    GTFileDialog::openFile(os, dataDir+"samples/Genbank/", "murine.gb");

    class OkClicker : public Filler {
    public:
        OkClicker(U2OpStatus& _os) : Filler(_os, "ORFDialogBase"){}
        virtual void run() {
            QWidget *w = QApplication::activeWindow();
            CHECK(NULL != w, );
            QDialogButtonBox *buttonBox = w->findChild<QDialogButtonBox*>(QString::fromUtf8("buttonBox"));
            CHECK(NULL != buttonBox, );

            QPushButton *button = buttonBox->button(QDialogButtonBox::Ok);
            CHECK(NULL != button, );
            GTWidget::click(os, button);
        }
    };

    GTUtilsDialog::waitForDialog(os, new OkClicker(os));
    GTWidget::click(os, GTAction::button(os, "Find ORFs"));
    GTUtilsTaskTreeView::waitTaskFinished(os);

    GTUtilsDocument::unloadDocument(os, "murine.gb");
    GTUtilsTaskTreeView::waitTaskFinished(os);
    GTUtilsDocument::loadDocument(os, "murine.gb");
    GTUtilsTaskTreeView::waitTaskFinished(os);
    GTUtilsLog::check(os, logTracer);
}

GUI_TEST_CLASS_DEFINITION(test_4059) {
    GTLogTracer l;
    //1. Open "_common_data/text/text.txt".
    GTUtilsDialog::waitForDialog(os, new DocumentFormatSelectorDialogFiller(os, "Plain text"));
    GTFileDialog::openFile(os, testDir + "_common_data/text/text.txt");

    //2. Delete the "Text" object.
    GTUtilsProjectTreeView::click(os, "text");
    GTKeyboardDriver::keyClick(os, GTKeyboardDriver::key["delete"]);

    //3. Unload the document.
    MessageBoxDialogFiller *filler = new MessageBoxDialogFiller(os, "No");
    GTUtilsDialog::waitForDialog(os, filler);
    GTUtilsDocument::unloadDocument(os, "text.txt", false);
    GTUtilsTaskTreeView::waitTaskFinished(os);

    //4. Load the document.
    GTMouseDriver::moveTo(os, GTUtilsProjectTreeView::getItemCenter(os, "text.txt"));
    GTKeyboardDriver::keyClick(os, GTKeyboardDriver::key["enter"]);

    //Expected: no safe points triggered.
    CHECK_SET_ERR(!l.hasError(), "Errors in log");
}

GUI_TEST_CLASS_DEFINITION(test_4064) {
//    1. Copy "_common_data/bam/scerevisiae.bam" and "_common_data/bam/scerevisiae.bam.bai" to separate folder
//    2. Rename "scerevisiae.bam.bai" to "scerevisiae.bai"
//    3. Open "scerevisiae.bam" in UGENE
//    Expected state: "Import BAM file" dialog appeared - there is no "Index is not available" warning message.

    GTFile::copy(os, testDir + "_common_data/bam/scerevisiae.bam", sandBoxDir + "test_4064.bam");

    class CustomImportBAMDialogFiller : public Filler {
    public:
        CustomImportBAMDialogFiller(U2OpStatus &os, bool warningExistence)
            : Filler(os, "Import BAM File"),
              warningExistence(warningExistence) {}
        virtual void run() {
            QWidget* dialog = QApplication::activeModalWidget();
            CHECK_SET_ERR(dialog, "activeModalWidget is NULL");

            QLabel *label = qobject_cast<QLabel*>(GTWidget::findWidget(os,"indexNotAvailableLabel",dialog));
            CHECK_SET_ERR(label != NULL, "indexNotAvailableLabel not found");
            CHECK_SET_ERR(label->isVisible() == warningExistence, "Warning message is shown");

            QDialogButtonBox* box = qobject_cast<QDialogButtonBox*>(GTWidget::findWidget(os, "buttonBox", dialog));
            QPushButton* button = box->button(QDialogButtonBox::Cancel);
            CHECK_SET_ERR(button !=NULL, "cancel button is NULL");
            GTWidget::click(os, button);
        }
    private:
        bool warningExistence;
    };

    GTUtilsDialog::waitForDialog(os, new CustomImportBAMDialogFiller(os, true));
    GTFileDialog::openFile(os, sandBoxDir, "test_4064.bam");

    GTFile::copy(os, testDir + "_common_data/bam/scerevisiae.bam.bai", sandBoxDir + "test_4064.bai");
    GTUtilsDialog::waitForDialog(os, new CustomImportBAMDialogFiller(os, false));
    GTFileDialog::openFile(os, sandBoxDir, "test_4064.bam");
}

GUI_TEST_CLASS_DEFINITION(test_4065) {
/* 1. Open _common_data/scenarios/_regression/4065/example_bam.bam
 * 2. Check log for error: "No bam index given, preparing sequential import"
*/
    GTLogTracer l;
    GTUtilsDialog::waitForDialog(os, new ImportBAMFileFiller(os, sandBoxDir + "/test_4065.ugenedb"));
    GTFileDialog::openFile(os, testDir + "_common_data/scenarios/_regression/4065/example_bam.bam");
    GTUtilsTaskTreeView::waitTaskFinished(os);

    bool hasMessage = l.checkMessage("No bam index given");
    CHECK_SET_ERR(false == hasMessage , "Error message is found. Bam index file not found.");
}

GUI_TEST_CLASS_DEFINITION(test_4070) {
//    1. Open file "_common_data/scenarios/msa/ma.aln".
    GTFileDialog::openFile(os, testDir + "_common_data/scenarios/msa", "ma.aln");

//    2. Switch on collapsing mode.
    GTUtilsMsaEditor::toggleCollapsingMode(os);

//    3. Expand "Conocephalus_discolor" group.
    GTUtilsMsaEditor::toggleCollapsingGroup(os, "Conocephalus_discolor");

//    Expected state: the overview is calculated and shown.
    GTUtilsTaskTreeView::waitTaskFinished(os);

    QWidget *graphOverview = GTUtilsMsaEditor::getGraphOverview(os);
    CHECK_SET_ERR(NULL != graphOverview, "Graph overview widget is NULL");

    bool colorFound = false;
    for (int i = 0; i < graphOverview->width() && !colorFound; i++) {
        for (int j = 0; j < graphOverview->height() && !colorFound; j++) {
            if (QColor(0, 0, 0) == GTUtilsMsaEditor::getGraphOverviewPixelColor(os, QPoint(i, j))) {
                colorFound = true;
            }
        }
    }

    CHECK_SET_ERR(colorFound, "The overview doesn't contain white color");
}
GUI_TEST_CLASS_DEFINITION(test_4122) {
/* 1. Open "data/samples/Genbank/murine.gb".
 * 2. Search any existing pattern.
 * 3. Try to create annotations from the search results to the existing annotations table.
 *   Expected state: annotations are created.
 *   Current state: annotations are not created, a safe point is triggered
*/
    GTFileDialog::openFile(os, dataDir + "samples/Genbank", "murine.gb");
    GTUtilsOptionPanelSequenceView::openTab(os, GTUtilsOptionPanelSequenceView::Search);
    GTUtilsOptionPanelSequenceView::enterPattern(os, "GAGTTCTGAACACCCGGC");
    GTUtilsOptionPanelSequenceView::clickGetAnnotation(os);
    GTUtilsTaskTreeView::waitTaskFinished(os);

    QTreeWidgetItem* annotationTable = GTUtilsAnnotationsTreeView::findItem(os, "NC_001363 features [murine.gb] *");
    CHECK_SET_ERR(NULL != annotationTable, "AnnotationTable (NC_001363 features [murine.gb]) is NULL or not changed");
    GTMouseDriver::moveTo(os, GTUtilsAnnotationsTreeView::getItemCenter(os, "NC_001363 features [murine.gb] *"));
    GTMouseDriver::click(os);

    QTreeWidgetItem* newItem = GTUtilsAnnotationsTreeView::findItem(os, "Misc_ Feature",annotationTable);
    CHECK_SET_ERR(NULL != newItem, "New annotation is NULL or not created");
    GTMouseDriver::moveTo(os, GTUtilsAnnotationsTreeView::getItemCenter(os, "Misc_ Feature"));
    GTMouseDriver::click(os);

}

} // namespace GUITest_regression_scenarios

} // namespace U2
