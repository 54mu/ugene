/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2020 UniPro <ugene@unipro.ru>
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

#include "FindPatternMsaWidget.h"

#include <QFlags>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMovie>

#include <U2Algorithm/FindAlgorithmTask.h>

#include <U2Core/AppContext.h>
#include <U2Core/DNAAlphabet.h>
#include <U2Core/DNATranslation.h>
#include <U2Core/DocumentUtils.h>
#include <U2Core/Log.h>
#include <U2Core/ProjectModel.h>
#include <U2Core/TaskWatchdog.h>
#include <U2Core/TextUtils.h>
#include <U2Core/Theme.h>
#include <U2Core/U2DbiRegistry.h>
#include <U2Core/U2OpStatusUtils.h>
#include <U2Core/U2SafePoints.h>

#include <U2Formats/FastaFormat.h>

#include <U2Gui/DialogUtils.h>
#include <U2Gui/GUIUtils.h>
#include <U2Gui/ObjectViewModel.h>
#include <U2Gui/ShowHideSubgroupWidget.h>
#include <U2Gui/U2WidgetStateStorage.h>

#include <U2View/MSAEditorSequenceArea.h>

#include "FindPatternMsaTask.h"
#include "ov_msa/MaCollapseModel.h"

namespace U2 {

const int FindPatternMsaWidget::DEFAULT_RESULTS_NUM_LIMIT = 100000;
const int FindPatternMsaWidget::DEFAULT_REGEXP_RESULT_LENGTH_LIMIT = 10000;

const int FindPatternMsaWidget::REG_EXP_MIN_RESULT_LEN = 1;
const int FindPatternMsaWidget::REG_EXP_MAX_RESULT_LEN = 1000;
const int FindPatternMsaWidget::REG_EXP_MAX_RESULT_SINGLE_STEP = 20;

class PatternWalker {
public:
    PatternWalker(const QString &patternsString, int cursor = 0)
        : patternsString(patternsString.toLatin1()), cursor(cursor), current(-1) {
    }

    bool hasNext() const {
        return current < patternsString.size() - 1;
    }

    char next() {
        if (!hasNext()) {
            return 0;
        }
        current++;
        return patternsString[current];
    }

    bool isSequenceChar() const {
        CHECK(-1 != current, false);
        CHECK(current < patternsString.size(), false);
        return true;
    }

    /** moves current place to the previous */
    void removeCurrent() {
        CHECK(-1 != current, );
        CHECK(current < patternsString.size(), );
        patternsString.remove(current, 1);
        if (current < cursor) {
            cursor--;
        }
        current--;
    }

    bool isCorrect() const {
        if (!isSequenceChar()) {
            return true;
        }
        QChar c(patternsString[current]);
        return c.isLetter() ? c.isUpper() : c == '\n';
    }

    void setCurrent(char value) {
        CHECK(-1 != current, );
        CHECK(current < patternsString.size(), );
        patternsString[current] = value;
    }

    int getCursor() const {
        return cursor;
    }

    QString getString() const {
        return patternsString;
    }

private:
    QByteArray patternsString;
    int cursor;
    int current;
};

#define SEARCH_MODE_SEQUENCES_DATA QVariant(1)
#define SEARCH_MODE_NAMES_DATA QVariant(2)

FindPatternMsaWidget::FindPatternMsaWidget(MSAEditor *msaEditor, bool isSearchInNamesMode)
    : msaEditor(msaEditor),
      currentResultIndex(-1),
      searchTask(nullptr),
      previousMaxResult(-1),
      savableWidget(this, GObjectViewUtils::findViewByName(msaEditor->getName())),
      setSelectionToTheFirstResult(true),
      isSearchInNamesMode(isSearchInNamesMode),
      algorithmSubgroup(nullptr),
      searchInSubgroup(nullptr),
      otherSettingsSubgroup(nullptr) {
    setupUi(this);
    setObjectName("FindPatternMsaWidget");

    progressMovie = new QMovie(":/core/images/progress.gif", QByteArray(), progressLabel);
    progressLabel->setObjectName("progressLabel");
    resultLabel->setObjectName("resultLabel");
    resultLabel->setFixedHeight(progressLabel->height());
    savableWidget.setRegionWidgetIds(QStringList() << editStart->objectName()
                                                   << editEnd->objectName());
    progressLabel->setMovie(progressMovie);

    setContentsMargins(0, 0, 0, 0);

    const DNAAlphabet *alphabet = msaEditor->getMaObject()->getAlphabet();
    isAmino = alphabet->isAmino();

    initLayout();
    connectSlots();

    checkState();
    if (lblErrorMessage->text().isEmpty()) {
        showHideMessage(currentPatternFieldText.isEmpty(), PleaseInputAtLeastOneSearchPatternTip);
    }

    FindPatternEventFilter *findPatternEventFilter = new FindPatternEventFilter(this);
    textPattern->installEventFilter(findPatternEventFilter);

    setFocusProxy(textPattern);

    connect(findPatternEventFilter, SIGNAL(si_enterPressed()), SLOT(sl_onEnterPressed()));
    connect(findPatternEventFilter, SIGNAL(si_shiftEnterPressed()), SLOT(sl_onShiftEnterPressed()));

    sl_onSearchPatternChanged();

    nextPushButton->setDisabled(true);
    prevPushButton->setDisabled(true);
    showCurrentResultAndStopProgress();
    setUpTabOrder();
    previousMaxResult = boxMaxResult->value();
    U2WidgetStateStorage::restoreWidgetState(savableWidget);
}

int FindPatternMsaWidget::getTargetMsaLength() const {
    return msaEditor->getAlignmentLen();
}

void FindPatternMsaWidget::setSearchInNamesMode(bool flag) {
    CHECK(isSearchInNamesMode != flag, )
    isSearchInNamesMode = flag;
    QVariant itemDataToActivate = isSearchInNamesMode ? SEARCH_MODE_NAMES_DATA : SEARCH_MODE_SEQUENCES_DATA;
    int indexToActivate = 0;
    for (int i = 0; i < searchContextComboBox->count(); i++) {
        if (searchContextComboBox->itemData(i) == itemDataToActivate) {
            indexToActivate = i;
            break;
        }
    }
    searchContextComboBox->setCurrentIndex(indexToActivate);
}

void FindPatternMsaWidget::showCurrentResultAndStopProgress() {
    progressMovie->stop();
    progressLabel->hide();
    resultLabel->show();
    updateCurrentResultLabel();
}

void FindPatternMsaWidget::initLayout() {
    lblErrorMessage->setStyleSheet("font: bold;");
    lblErrorMessage->setText("");
    initAlgorithmLayout();
    initRegionSelection();
    initResultsLimit();

    subgroupsLayout->setSpacing(0);

    algorithmSubgroup = new ShowHideSubgroupWidget(QObject::tr("Search algorithm"), QObject::tr("Search algorithm"), widgetAlgorithm, false);
    subgroupsLayout->addWidget(algorithmSubgroup);

    searchInSubgroup = new ShowHideSubgroupWidget(QObject::tr("Search in"), QObject::tr("Search in"), widgetSearchIn, false);
    subgroupsLayout->addWidget(searchInSubgroup);

    otherSettingsSubgroup = new ShowHideSubgroupWidget(QObject::tr("Other settings"), QObject::tr("Other settings"), widgetOther, false);
    subgroupsLayout->addWidget(otherSettingsSubgroup);

    searchContextComboBox->addItem(tr("Sequences"), SEARCH_MODE_SEQUENCES_DATA);
    searchContextComboBox->addItem(tr("Sequence Names"), SEARCH_MODE_NAMES_DATA);
    if (isSearchInNamesMode) {
        searchContextComboBox->setCurrentIndex(1);
    }

    updateLayout();

    layoutSearchButton->setAlignment(Qt::AlignTop);
    this->layout()->setAlignment(Qt::AlignTop);

    this->layout()->setMargin(0);
}

void FindPatternMsaWidget::initAlgorithmLayout() {
    boxAlgorithm->addItem(tr("Exact"), FindAlgorithmPatternSettings_Exact);
    if (!isAmino) {
        boxAlgorithm->addItem(tr("InsDel"), FindAlgorithmPatternSettings_InsDel);
        boxAlgorithm->addItem(tr("Substitute"), FindAlgorithmPatternSettings_Subst);
    }
    boxAlgorithm->addItem(tr("Regular expression"), FindAlgorithmPatternSettings_RegExp);

    layoutMismatch = new QHBoxLayout();

    lblMatch = new QLabel(tr("Should match"));

    spinMatch = new QSpinBox(this);
    spinMatch->setSuffix("%");    // Percentage value
    spinMatch->setMinimum(30);
    spinMatch->setMaximum(100);
    spinMatch->setSingleStep(1);
    spinMatch->setValue(100);
    spinMatch->setObjectName("spinBoxMatch");
    spinMatch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layoutMismatch->addWidget(lblMatch);
    layoutMismatch->addWidget(spinMatch);

    layoutAlgorithmSettings->addLayout(layoutMismatch);
    initMaxResultLenContainer();

    selectedAlgorithm = boxAlgorithm->itemData(boxAlgorithm->currentIndex()).toInt();
}

void FindPatternMsaWidget::initRegionSelection() {
    boxRegion->addItem(FindPatternMsaWidget::tr("Whole alignment"), RegionSelectionIndex_WholeSequence);
    boxRegion->addItem(FindPatternMsaWidget::tr("Custom columns region"), RegionSelectionIndex_CustomRegion);
    boxRegion->addItem(FindPatternMsaWidget::tr("Selected columns region"), RegionSelectionIndex_CurrentSelectedRegion);
    setRegionToWholeSequence();

    editStart->setValidator(new QIntValidator(1, msaEditor->getAlignmentLen(), editStart));
    editEnd->setValidator(new QIntValidator(1, msaEditor->getAlignmentLen(), editEnd));

    sl_onRegionOptionChanged(RegionSelectionIndex_WholeSequence);
}

void FindPatternMsaWidget::initResultsLimit() {
    boxMaxResult->setMinimum(1);
    boxMaxResult->setMaximum(INT_MAX);
    boxMaxResult->setValue(DEFAULT_RESULTS_NUM_LIMIT);
    boxMaxResult->setEnabled(true);
}

void FindPatternMsaWidget::initMaxResultLenContainer() {
    useMaxResultLenContainer = new QWidget();

    layoutRegExpLen = new QVBoxLayout();
    layoutRegExpLen->setContentsMargins(0, 0, 0, 0);
    layoutRegExpLen->setSpacing(3);
    layoutRegExpLen->setSizeConstraint(QLayout::SetMinAndMaxSize);
    useMaxResultLenContainer->setLayout(layoutRegExpLen);

    QHBoxLayout *layoutUseMaxResultLen = new QHBoxLayout();
    layoutUseMaxResultLen->setSpacing(10);
    layoutUseMaxResultLen->setSizeConstraint(QLayout::SetMinAndMaxSize);

    boxUseMaxResultLen = new QCheckBox();
    boxUseMaxResultLen->setObjectName("boxUseMaxResultLen");
    QLabel *labelUseMaxResultLen = new QLabel(tr("Results no longer than:"));
    labelUseMaxResultLen->setWordWrap(true);
    layoutUseMaxResultLen->addWidget(boxUseMaxResultLen, 0);
    layoutUseMaxResultLen->addWidget(labelUseMaxResultLen, 1);

    boxMaxResultLen = new QSpinBox();
    boxMaxResultLen->setObjectName("boxMaxResultLen");
    boxMaxResultLen->setMinimum(REG_EXP_MIN_RESULT_LEN);
    boxMaxResultLen->setMaximum(REG_EXP_MAX_RESULT_LEN);
    boxMaxResultLen->setSingleStep(REG_EXP_MAX_RESULT_SINGLE_STEP);
    boxMaxResultLen->setValue(REG_EXP_MAX_RESULT_LEN);
    boxMaxResultLen->setEnabled(false);
    connect(boxUseMaxResultLen, SIGNAL(toggled(bool)), boxMaxResultLen, SLOT(setEnabled(bool)));
    connect(boxUseMaxResultLen, SIGNAL(toggled(bool)), SLOT(sl_activateNewSearch()));
    connect(boxMaxResultLen, SIGNAL(valueChanged(int)), SLOT(sl_activateNewSearch()));

    layoutRegExpLen->addLayout(layoutUseMaxResultLen);
    layoutRegExpLen->addWidget(boxMaxResultLen);
    layoutAlgorithmSettings->addWidget(useMaxResultLenContainer);

    connect(msaEditor->getUI()->getCollapseModel(), SIGNAL(si_toggled()), SLOT(sl_collapseModelChanged()));
}

void FindPatternMsaWidget::connectSlots() {
    connect(boxAlgorithm, SIGNAL(currentIndexChanged(int)), SLOT(sl_onAlgorithmChanged(int)));
    connect(boxRegion, SIGNAL(currentIndexChanged(int)), SLOT(sl_onRegionOptionChanged(int)));
    connect(textPattern, SIGNAL(textChanged()), SLOT(sl_onSearchPatternChanged()));
    connect(editStart, SIGNAL(textChanged(QString)), SLOT(sl_onRegionValueEdited()));
    connect(editEnd, SIGNAL(textChanged(QString)), SLOT(sl_onRegionValueEdited()));
    connect(boxMaxResult, SIGNAL(valueChanged(int)), SLOT(sl_onMaxResultChanged(int)));
    connect(removeOverlapsBox, SIGNAL(stateChanged(int)), SLOT(sl_activateNewSearch()));
    connect(msaEditor->getMaObject(), SIGNAL(si_alignmentChanged(const MultipleAlignment &, const MaModificationInfo &)), this, SLOT(sl_onMsaModified()));
    connect(msaEditor->getMaObject(), SIGNAL(si_alphabetChanged(const MaModificationInfo &, const DNAAlphabet *)), this, SLOT(sl_onMsaModified()));
    connect(prevPushButton, SIGNAL(clicked()), SLOT(sl_prevButtonClicked()));
    connect(nextPushButton, SIGNAL(clicked()), SLOT(sl_nextButtonClicked()));
    connect(spinMatch, SIGNAL(valueChanged(int)), SLOT(sl_activateNewSearch()));

    auto sequenceArea = msaEditor->getUI()->getSequenceArea();
    connect(sequenceArea, SIGNAL(si_selectionChanged(const MaEditorSelection &, const MaEditorSelection &)), this, SLOT(sl_onSelectedRegionChanged(const MaEditorSelection &, const MaEditorSelection &)));

    connect(searchContextComboBox, SIGNAL(currentIndexChanged(int)), SLOT(sl_searchModeChanged()));
}

void FindPatternMsaWidget::sl_onAlgorithmChanged(int index) {
    int previousAlgorithm = selectedAlgorithm;
    selectedAlgorithm = boxAlgorithm->itemData(index).toInt();
    updatePatternText(previousAlgorithm);
    updateLayout();
    bool noValidationErrors = verifyPatternAlphabet();
    if (noValidationErrors) {
        sl_activateNewSearch();
    }
}

void FindPatternMsaWidget::sl_onRegionOptionChanged(int index) {
    if (boxRegion->itemData(index).toInt() == RegionSelectionIndex_WholeSequence) {
        editStart->hide();
        lblStartEndConnection->hide();
        editEnd->hide();
        regionIsCorrect = true;
        checkState();
        setRegionToWholeSequence();
    } else if (boxRegion->itemData(index).toInt() == RegionSelectionIndex_CustomRegion) {
        editStart->show();
        lblStartEndConnection->show();
        editEnd->show();
        editStart->setReadOnly(false);
        editEnd->setReadOnly(false);
        getSearchRegionFromUi(regionIsCorrect);
        checkState();
    } else if (boxRegion->itemData(index).toInt() == RegionSelectionIndex_CurrentSelectedRegion) {
        editStart->show();
        lblStartEndConnection->show();
        editEnd->show();
        sl_onSelectedRegionChanged(msaEditor->getSelection(), MaEditorSelection());
    }
}

void FindPatternMsaWidget::sl_onRegionValueEdited() {
    regionIsCorrect = true;
    // The values are not empty
    if (editStart->text().isEmpty()) {
        GUIUtils::setWidgetWarning(editStart, true);
        regionIsCorrect = false;
    } else if (editEnd->text().isEmpty()) {
        GUIUtils::setWidgetWarning(editEnd, true);
        regionIsCorrect = false;
    } else {
        bool ok = false;
        qint64 value1 = editStart->text().toLongLong(&ok);
        if (!ok || (value1 < 1)) {
            GUIUtils::setWidgetWarning(editStart, true);
            regionIsCorrect = false;
        }
        int value2 = editEnd->text().toLongLong(&ok);
        if (!ok || value2 < 1) {
            GUIUtils::setWidgetWarning(editEnd, true);
            regionIsCorrect = false;
        }
        if (value2 - value1 < 0) {
            GUIUtils::setWidgetWarning(editEnd, true);
            regionIsCorrect = false;
        }
    }

    if (regionIsCorrect) {
        GUIUtils::setWidgetWarning(editStart, false);
        GUIUtils::setWidgetWarning(editEnd, false);
    }
    boxRegion->setCurrentIndex(boxRegion->findData(RegionSelectionIndex_CustomRegion));
    checkState();
    if (regionIsCorrect) {
        sl_activateNewSearch();
    }
}

void FindPatternMsaWidget::updateLayout() {
    algorithmSubgroup->setVisible(!isSearchInNamesMode);
    searchInSubgroup->setVisible(!isSearchInNamesMode);
    otherSettingsSubgroup->setVisible(!isSearchInNamesMode);
    if (isSearchInNamesMode) {    // search in names
        return;
    }
    // Algorithm group
    if (selectedAlgorithm == FindAlgorithmPatternSettings_Exact) {
        useMaxResultLenContainer->hide();
        boxMaxResultLen->hide();
        spinMatch->hide();
        lblMatch->hide();
    }
    if (selectedAlgorithm == FindAlgorithmPatternSettings_InsDel) {
        useMaxResultLenContainer->hide();
        boxMaxResultLen->hide();
        enableDisableMatchSpin();
        lblMatch->show();
        spinMatch->show();
        QWidget::setTabOrder(boxAlgorithm, spinMatch);
    } else if (selectedAlgorithm == FindAlgorithmPatternSettings_Subst) {
        useMaxResultLenContainer->hide();
        boxMaxResultLen->hide();
        QWidget::setTabOrder(boxAlgorithm, spinMatch);
        enableDisableMatchSpin();
        lblMatch->show();
        spinMatch->show();
    } else if (selectedAlgorithm == FindAlgorithmPatternSettings_RegExp) {
        useMaxResultLenContainer->show();
        boxMaxResultLen->show();
        spinMatch->hide();
        lblMatch->hide();
        QWidget::setTabOrder(boxAlgorithm, boxUseMaxResultLen);
        QWidget::setTabOrder(boxUseMaxResultLen, boxMaxResultLen);
    }
}

void FindPatternMsaWidget::showHideMessage(bool show, MessageFlag messageFlag, const QString &additionalMsg) {
    if (show) {
        if (!messageFlags.contains(messageFlag)) {
            messageFlags.append(messageFlag);
        }
    } else {
        messageFlags.removeAll(messageFlag);
    }

    if (!messageFlags.isEmpty()) {
#ifndef Q_OS_MAC
        QString lineBreakShortcut = "Ctrl+Enter";
#else
        QString lineBreakShortcut = "Cmd+Enter";
#endif
        QString text = "";
        foreach (MessageFlag flag, messageFlags) {
            switch (flag) {
            case PatternIsTooLong: {
                QString message = tr("The value is longer than the search region."
                                     " Please input a shorter value or select another region!");
                text = tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::errorColorLabelHtmlStr()).arg(message);
                break;
            }
            case PatternAlphabetDoNotMatch: {
                QString message = tr("Warning: input value contains characters that"
                                     " do not match the active alphabet!");
                text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::warningColorLabelHtmlStr()).arg(message);
                GUIUtils::setWidgetWarning(textPattern, true);
                break;
            }
            case PatternsWithBadAlphabetInFile: {
                QString message = tr("Warning: file contains patterns that"
                                     " do not match the active alphabet! Those patterns were ignored ");
                text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::warningColorLabelHtmlStr()).arg(message);
                break;
            }
            case PatternsWithBadRegionInFile: {
                QString message = tr("Warning: file contains patterns that"
                                     " longer than the search region! Those patterns were ignored. Please input a shorter value or select another region! ");
                text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::warningColorLabelHtmlStr()).arg(message);
                break;
            }
            case PleaseInputAtLeastOneSearchPatternTip: {
                QString message = isSearchInNamesMode ?
                                      tr("Info: please input at least one pattern to search in the sequence names.") :
                                      tr("Info: please input at least one sequence pattern to search for.");

                message += " " + tr("Use Ctrl+Enter to input multiple patterns").arg(lineBreakShortcut);
                text = QString("<b><font color=%1>%2</font><br></br></b>").arg(Theme::infoColorLabelHtmlStr()).arg(message);
                break;
            }
            case AnnotationNotValidName: {
                QString message = tr("Warning: annotation name or annotation group name are invalid. ");
                text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::errorColorLabelHtmlStr()).arg(message);
                if (!additionalMsg.isEmpty()) {
                    text += tr("<b><font color=%1>%2</font></b>").arg(Theme::errorColorLabelHtmlStr()).arg(tr("Reason: "));
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::errorColorLabelHtmlStr()).arg(additionalMsg);
                }
                QString msg = tr(" Please input valid annotation names. ");
                text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::errorColorLabelHtmlStr()).arg(msg);
                break;
            }
            case AnnotationNotValidFastaParsedName: {
                QString message = tr("Warning: annotation names are invalid. ");
                text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::errorColorLabelHtmlStr()).arg(message);
                if (!additionalMsg.isEmpty()) {
                    text += tr("<b><font color=%1>%2</font></b>").arg(Theme::errorColorLabelHtmlStr()).arg(tr("Reason: "));
                    text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::errorColorLabelHtmlStr()).arg(additionalMsg);
                }
                QString msg = tr(" It will be automatically changed to acceptable name if 'Get annotations' button is pressed. ");
                text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::errorColorLabelHtmlStr()).arg(msg);
                break;
            }
            case NoPatternToSearch: {
                QString message = tr("Warning: there is no pattern to search. ");
                text += tr("<b><font color=%1>%2</font></b>").arg(Theme::errorColorLabelHtmlStr()).arg(message);
                QString msg = tr(" Please input a valid pattern ");
                text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::errorColorLabelHtmlStr()).arg(msg);
                break;
            }
            case SearchRegionIncorrect: {
                QString message = tr("Warning: search region values is not correct. ");
                text += tr("<b><font color=%1>%2</font></b>").arg(Theme::errorColorLabelHtmlStr()).arg(message);
                QString msg = tr(" Please input a valid region to search");
                text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::errorColorLabelHtmlStr()).arg(msg);
                break;
            }
            case PatternWrongRegExp: {
                QString message = tr("Warning: the input regular expression is invalid! ");
                text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::errorColorLabelHtmlStr()).arg(message);
                GUIUtils::setWidgetWarning(textPattern, true);
                break;
            }
            case SequenceIsTooBig: {
                text.clear();    // the search is blocked at all -- any other messages are meaningless
                QString message = tr("Warning: current sequence is too long to search in.");
                text += tr("<b><font color=%1>%2</font><br></br></b>").arg(Theme::errorColorLabelHtmlStr()).arg(message);
                break;
            }
            default:
                FAIL("Unexpected value of the error flag in show/hide error message for pattern!", );
            }
        }
        lblErrorMessage->setText(text);
    } else {
        lblErrorMessage->setText("");
    }
    bool hasNoErrors = messageFlags.isEmpty() || (messageFlags.size() == 1 && messageFlags.contains(PleaseInputAtLeastOneSearchPatternTip));
    if (hasNoErrors) {
        GUIUtils::setWidgetWarning(textPattern, false);
    }
}

void FindPatternMsaWidget::sl_onSearchPatternChanged() {
    QString patternFieldText = textPattern->toPlainText();
    if (patternFieldText == currentPatternFieldText) {
        return;
    }
    currentPatternFieldText = patternFieldText;
    validateSearchPatternAndStartNewSearch();
}

void FindPatternMsaWidget::validateSearchPatternAndStartNewSearch() {
    if (!isSearchInNamesMode) {
        setCorrectPatternsString();
        checkState();
    } else {
        regionIsCorrect = true;
    }
    enableDisableMatchSpin();

    // Show a warning if the pattern alphabet doesn't match, but do not block the "Search" button
    bool noValidationErrors = isSearchInNamesMode || verifyPatternAlphabet();

    bool isEmptyPatternHintShown = noValidationErrors && currentPatternFieldText.isEmpty();
    showHideMessage(isEmptyPatternHintShown, PleaseInputAtLeastOneSearchPatternTip);

    if (noValidationErrors && regionIsCorrect) {
        sl_activateNewSearch();
    }
}

void FindPatternMsaWidget::sl_onMaxResultChanged(int newMaxResult) {
    int resultsSize = visibleSearchResults.size();
    bool limitResult = !visibleSearchResults.isEmpty() && newMaxResult < resultsSize;
    bool widenResult = newMaxResult > previousMaxResult && resultsSize == previousMaxResult;
    bool prevSearchIsNotComplete = visibleSearchResults.isEmpty() && searchTask != nullptr;
    if (limitResult || widenResult || prevSearchIsNotComplete) {
        sl_activateNewSearch();
    }
}

void FindPatternMsaWidget::setCorrectPatternsString() {
    QTextCursor cursorInTextEdit = textPattern->textCursor();

    if (selectedAlgorithm != FindAlgorithmPatternSettings_RegExp) {
        PatternWalker walker(textPattern->toPlainText(), cursorInTextEdit.position());
        // Delete all non-alphabet symbols.
        while (walker.hasNext()) {
            QChar character(walker.next());
            if (walker.isCorrect()) {
                continue;
            }
            if (character.isLetter()) {
                if (!character.isUpper()) {
                    walker.setCurrent(character.toUpper().toLatin1());
                }
            } else {
                if (character != '\n') {
                    walker.removeCurrent();
                }
            }
        }

        if (textPattern->toPlainText() != walker.getString()) {
            textPattern->setText(walker.getString());
            cursorInTextEdit.setPosition(walker.getCursor());
            textPattern->setTextCursor(cursorInTextEdit);
        }
    }
}

void FindPatternMsaWidget::setRegionToWholeSequence() {
    editStart->setText(QString::number(1));
    editEnd->setText(QString::number(msaEditor->getAlignmentLen()));
    regionIsCorrect = true;
    boxRegion->setCurrentIndex(boxRegion->findData(RegionSelectionIndex_WholeSequence));
}

bool FindPatternMsaWidget::verifyPatternAlphabet() {
    bool alphabetIsOk = checkAlphabet(textPattern->toPlainText().remove("\n"));
    showHideMessage(!alphabetIsOk, PatternAlphabetDoNotMatch);

    bool result = alphabetIsOk;

    if (selectedAlgorithm == FindAlgorithmPatternSettings_RegExp) {
        QString reText = textPattern->toPlainText();

        // Check that all symbols are ascii
        if (reText.contains(QRegularExpression(QStringLiteral("[^\\x{0000}-\\x{007F}]")))) {
            showHideMessage(true, PatternWrongRegExp);
            result = false;
        } else {
            QRegExp regExp(reText.toUtf8());
            if (regExp.isValid()) {
                showHideMessage(false, PatternWrongRegExp);
            } else {
                showHideMessage(true, PatternWrongRegExp);
                result = false;
            }
        }
    } else {
        showHideMessage(false, PatternWrongRegExp);
    }
    return result;
}

void FindPatternMsaWidget::sl_onMsaModified() {
    setRegionToWholeSequence();

    visibleSearchResults.clear();
    allSearchResults.clear();
    currentResultIndex = -1;

    checkState();
    verifyPatternAlphabet();
    sl_activateNewSearch(true);
}

void FindPatternMsaWidget::showTooLongSequenceError() {
    showHideMessage(true, SequenceIsTooBig);

    showHideMessage(false, AnnotationNotValidFastaParsedName);
    showHideMessage(false, AnnotationNotValidName);
    showHideMessage(false, PatternAlphabetDoNotMatch);
    showHideMessage(false, PatternsWithBadRegionInFile);
    showHideMessage(false, PatternsWithBadAlphabetInFile);
    showHideMessage(false, NoPatternToSearch);
    showHideMessage(false, SearchRegionIncorrect);
    GUIUtils::setWidgetWarning(textPattern, false);
}

void FindPatternMsaWidget::checkState() {
    // Disable the "Search" button if the pattern is empty
    //and pattern is not loaded from a file
    if (textPattern->toPlainText().isEmpty()) {
        showHideMessage(false, PatternAlphabetDoNotMatch);
        GUIUtils::setWidgetWarning(textPattern, false);
        return;
    }

    // Show warning if the region is not correct
    if (!regionIsCorrect) {
        showHideMessage(true, SearchRegionIncorrect);
        return;
    }

    // Show warning if the length of the pattern is greater than the search region length
    // Not for RegExp algorithm
    if (selectedAlgorithm != FindAlgorithmPatternSettings_RegExp) {
        bool regionOk = checkPatternRegion(textPattern->toPlainText());

        if (!regionOk) {
            GUIUtils::setWidgetWarning(textPattern, true);
            showHideMessage(true, PatternIsTooLong);
            return;
        } else {
            GUIUtils::setWidgetWarning(textPattern, false);
            showHideMessage(false, PatternIsTooLong);
        }
    }

    showHideMessage(false, AnnotationNotValidFastaParsedName);
    showHideMessage(false, AnnotationNotValidName);
    showHideMessage(false, PatternsWithBadRegionInFile);
    showHideMessage(false, PatternsWithBadAlphabetInFile);
    showHideMessage(false, NoPatternToSearch);
    showHideMessage(false, SearchRegionIncorrect);
    showHideMessage(false, SequenceIsTooBig);
}

void FindPatternMsaWidget::enableDisableMatchSpin() {
    spinMatch->setDisabled(textPattern->toPlainText().isEmpty() || isAmino);
}

U2Region FindPatternMsaWidget::getSearchRegionFromUi(bool &isRegionIsCorrect) const {
    qint64 maxLen = msaEditor->getAlignmentLen();
    if (boxRegion->itemData(boxRegion->currentIndex()).toInt() == RegionSelectionIndex_WholeSequence) {
        isRegionIsCorrect = true;
        return U2Region(0, maxLen);
    }
    bool ok = false;
    qint64 value1 = editStart->text().toLongLong(&ok) - 1;
    if (!ok || value1 < 0) {
        isRegionIsCorrect = false;
        return U2Region();
    }

    int value2 = editEnd->text().toLongLong(&ok);
    if (!ok || value2 <= 0 || value2 > maxLen) {
        isRegionIsCorrect = false;
        return U2Region();
    }

    if (value1 > value2) {    // start > end
        value2 += maxLen;
    }

    isRegionIsCorrect = true;
    return U2Region(value1, value2 - value1);
}

int FindPatternMsaWidget::getMaxError(const QString &pattern) const {
    if (selectedAlgorithm == FindAlgorithmPatternSettings_Exact) {
        return 0;
    }
    return int((float)(1 - float(spinMatch->value()) / 100) * pattern.length());
}

QStringList FindPatternMsaWidget::getPatternsFromTextPatternField(U2OpStatus &os) const {
    QString inputText = textPattern->toPlainText().toLocal8Bit();
    QList<NamePattern> nameList = FastaFormat::getSequencesAndNamesFromUserInput(inputText, os);
    if (!nameList.isEmpty()) {
        QStringList result;
        foreach (const NamePattern &namePattern, nameList) {
            result << namePattern.second;
        }
        return result;
    }
    return inputText.split(QRegExp("\n"), QString::SkipEmptyParts);
}

#define FIND_PATTER_LAST_DIR "Find_msa_pattern_last_dir"

void FindPatternMsaWidget::startFindPatternInMsaTask(const QStringList &patterns) {
    currentSearchPatternList = patterns;
    CHECK(!patterns.isEmpty(), );

    if (selectedAlgorithm == FindAlgorithmPatternSettings_RegExp) {
        QRegExp regExp(textPattern->toPlainText());
        CHECK(regExp.isValid(), );
    }

    FindPatternMsaSettings settings;
    foreach (const QString &pattern, patterns) {
        settings.patterns << NamePattern("", pattern);
    }

    settings.msaObj = msaEditor->getMaObject();
    U2OpStatusImpl os;
    CHECK_OP_EXT(os, showTooLongSequenceError(), );    // suppose that if the sequence cannot be fetched from the DB, UGENE ran out of memory

    // Limit results number to the specified value
    settings.findSettings.maxResult2Find = boxMaxResult->value();
    previousMaxResult = settings.findSettings.maxResult2Find;

    // Region
    bool regionIsCorrectRef = false;
    U2Region region = getSearchRegionFromUi(regionIsCorrectRef);
    SAFE_POINT(regionIsCorrectRef, "Internal error: incorrect search region has been supplied."
                                   " Skipping the pattern search.", );
    settings.findSettings.searchRegion = region;

    // Algorithm settings
    settings.findSettings.patternSettings = static_cast<FindAlgorithmPatternSettings>(selectedAlgorithm);

    settings.findSettings.maxErr = 0;

    settings.findSettings.maxRegExpResultLength = boxUseMaxResultLen->isChecked() ?
                                                      boxMaxResultLen->value() :
                                                      DEFAULT_REGEXP_RESULT_LENGTH_LIMIT;

    // Creating and registering the task
    settings.removeOverlaps = removeOverlapsBox->isChecked();
    settings.findSettings.maxResult2Find = boxMaxResult->value();
    settings.matchValue = spinMatch->value();

    SAFE_POINT(searchTask == nullptr, "Search task is not nullptr", );
    nextPushButton->setDisabled(true);
    prevPushButton->setDisabled(true);

    searchTask = new FindPatternMsaTask(settings);
    connect(searchTask, SIGNAL(si_stateChanged()), SLOT(sl_findPatternTaskStateChanged()));
    startProgressAnimation();
    TaskWatchdog::trackResourceExistence(msaEditor->getMaObject(), searchTask);
    AppContext::getTaskScheduler()->registerTopLevelTask(searchTask);

    // Switch from the 'selected region' to 'custom region' because search task results will trigger selection updates.
    int boxRegionData = boxRegion->itemData(boxRegion->currentIndex()).toInt();
    if (boxRegionData == RegionSelectionIndex_CurrentSelectedRegion) {
        boxRegion->setCurrentIndex(boxRegion->findData(RegionSelectionIndex_CustomRegion));
    }
}

void FindPatternMsaWidget::sl_searchModeChanged() {
    isSearchInNamesMode = searchContextComboBox->currentData() == SEARCH_MODE_NAMES_DATA;
    updateLayout();
    validateSearchPatternAndStartNewSearch();
}

void FindPatternMsaWidget::sl_findPatternTaskStateChanged() {
    FindPatternMsaTask *findTask = static_cast<FindPatternMsaTask *>(sender());
    CHECK(nullptr != findTask, );
    if (findTask != searchTask) {
        return;
    }
    if (!findTask->isFinished() && !findTask->isCanceled() && !findTask->hasError()) {
        return;
    }
    allSearchResults.clear();
    const QList<FindPatternInMsaResult> &findTaskResultList = findTask->getResults();
    for (int i = 0; i < findTaskResultList.size(); i++) {
        const FindPatternInMsaResult &findTaskResult = findTaskResultList[i];
        for (int j = 0; j < findTaskResult.regions.length(); j++) {
            allSearchResults << FindPatternWidgetResult(findTaskResult.rowId, -1, findTaskResult.regions[j]);
        }
    }
    postProcessAllSearchResults();
    searchTask = nullptr;
}

void FindPatternMsaWidget::postProcessAllSearchResults() {
    visibleSearchResults.clear();
    resortResultsByViewState();
    showCurrentResultAndStopProgress();
    nextPushButton->setDisabled(visibleSearchResults.isEmpty());
    prevPushButton->setDisabled(visibleSearchResults.isEmpty());
    if (!visibleSearchResults.isEmpty()) {
        checkState();
        correctSearchInCombo();
        if (setSelectionToTheFirstResult) {
            currentResultIndex = 0;
            selectCurrentResult();
        }
    }
}

bool FindPatternMsaWidget::checkAlphabet(const QString &pattern) {
    const DNAAlphabet *alphabet = msaEditor->getMaObject()->getAlphabet();
    if (selectedAlgorithm == FindAlgorithmPatternSettings_RegExp) {
        return true;
    }
    return TextUtils::fits(alphabet->getMap(), pattern.toLocal8Bit().data(), pattern.size());
}

bool FindPatternMsaWidget::checkPatternRegion(const QString &pattern) {
    int maxError = getMaxError(pattern);
    qint64 patternLength = pattern.length();
    qint64 minMatch = patternLength - maxError;
    SAFE_POINT(minMatch > 0, "Search pattern length is greater than max error value!", false);

    bool isCorrect = false;
    U2Region searchRegion = getSearchRegionFromUi(isCorrect);

    SAFE_POINT(searchRegion.length > 0 && isCorrect,
               "Incorrect region length when enabling/disabling the pattern search button.",
               false);

    return minMatch <= searchRegion.length;
}

void FindPatternMsaWidget::sl_onSelectedRegionChanged(const MaEditorSelection &currentSelection, const MaEditorSelection &prev) {
    int boxRegionData = boxRegion->itemData(boxRegion->currentIndex()).toInt();
    bool isSearchInSelectionOn = boxRegionData == RegionSelectionIndex_CurrentSelectedRegion;
    if (isSearchInSelectionOn && findCurrentResultIndexFromSelection() == -1) {
        currentResultIndex = -1;
        if (currentSelection.isEmpty()) {
            editStart->setText(QString::number(1));
            editEnd->setText(QString::number(msaEditor->getAlignmentLen()));
        } else {
            QRect selectionRect = currentSelection.toRect();
            U2Region firstRegion = U2Region(selectionRect.topLeft().rx(), selectionRect.width());
            editStart->setText(QString::number(firstRegion.startPos + 1));
            editEnd->setText(QString::number(firstRegion.endPos()));
        }
        // Select the option again. Reason: setting text makes RegionSelectionIndex_CurrentRegion activated
        boxRegion->setCurrentIndex(boxRegion->findData(RegionSelectionIndex_CurrentSelectedRegion));
        regionIsCorrect = true;
    } else if (!isResultSelected()) {
        // reset result position on non-compatible selection change.
        currentResultIndex = -1;
    }
    checkState();
    updateCurrentResultLabel();
}

void FindPatternMsaWidget::updatePatternText(int previousAlgorithm) {
    // Save a previous state.
    if (FindAlgorithmPatternSettings_RegExp == previousAlgorithm) {
        patternRegExp = textPattern->toPlainText();
    } else {
        patternString = textPattern->toPlainText();
    }

    // Set a new state.
    if (FindAlgorithmPatternSettings_RegExp == selectedAlgorithm) {
        textPattern->setText(patternRegExp);
    } else {
        textPattern->setText(patternString);
    }
    setCorrectPatternsString();
}

void FindPatternMsaWidget::sl_activateNewSearch(bool activatedByOutsideChanges) {
    setSelectionToTheFirstResult = !activatedByOutsideChanges;
    U2OpStatusImpl os;
    QStringList newPatterns = getPatternsFromTextPatternField(os);
    CHECK_OP(os, )
    stopCurrentSearchTask();
    if (isSearchInNamesMode) {
        runSearchInSequenceNames(newPatterns);
    } else {
        startFindPatternInMsaTask(newPatterns);
    }
}

void FindPatternMsaWidget::runSearchInSequenceNames(const QStringList &patterns) {
    currentSearchPatternList = patterns;
    allSearchResults.clear();

    const MultipleAlignment &multipleAlignment = msaEditor->getMaObject()->getMultipleAlignment();
    U2Region wholeRowRegion(0, msaEditor->getAlignmentLen());
    QSet<int> resultRowIndexSet;
    foreach (const QString &pattern, currentSearchPatternList) {
        if (pattern.isEmpty()) {
            continue;
        }
        QString lowerCasePattern = pattern.toLower();
        for (int i = 0, n = multipleAlignment->getNumRows(); i < n; i++) {
            const MultipleAlignmentRow &row = multipleAlignment->getRow(i);
            if (row->getName().toLower().contains(lowerCasePattern)) {
                resultRowIndexSet << i;
            }
        }
    }
    foreach (int rowIndex, resultRowIndexSet) {
        const MultipleAlignmentRow &row = multipleAlignment->getRow(rowIndex);
        allSearchResults << FindPatternWidgetResult(row->getRowId(), -1, wholeRowRegion);
    }
    postProcessAllSearchResults();
}

void FindPatternMsaWidget::sl_prevButtonClicked() {
    int nResults = visibleSearchResults.size();
    CHECK(nResults > 0, );
    if (currentResultIndex == -1 || !isResultSelected()) {
        currentResultIndex = getNextOrPrevResultIndexFromSelection(false);
    } else {
        currentResultIndex = (currentResultIndex - 1 + nResults) % nResults;
    }
    selectCurrentResult();
}

void FindPatternMsaWidget::sl_nextButtonClicked() {
    int nResults = visibleSearchResults.size();
    CHECK(nResults > 0, );
    if (currentResultIndex == -1 || !isResultSelected()) {
        currentResultIndex = getNextOrPrevResultIndexFromSelection(true);
    } else {
        currentResultIndex = (currentResultIndex + 1) % nResults;
    }
    selectCurrentResult();
}

void FindPatternMsaWidget::selectCurrentResult() {
    CHECK(currentResultIndex >= 0 && currentResultIndex < visibleSearchResults.length(), );
    const FindPatternWidgetResult &result = visibleSearchResults[currentResultIndex];
    MaEditorSequenceArea *seqArea = msaEditor->getUI()->getSequenceArea();
    MaEditorSelection selection(result.region.startPos, result.viewRowIndex, result.region.length, 1);
    seqArea->setSelection(selection);
    seqArea->centerPos(selection.topLeft());
    updateCurrentResultLabel();
}

void FindPatternMsaWidget::sl_onEnterPressed() {
    nextPushButton->click();
}

void FindPatternMsaWidget::sl_onShiftEnterPressed() {
    prevPushButton->click();
}

void FindPatternMsaWidget::sl_collapseModelChanged() {
    resortResultsByViewState();
    updateCurrentResultLabel();
}

void FindPatternMsaWidget::stopCurrentSearchTask() {
    if (searchTask != nullptr) {
        if (!searchTask->isCanceled() && searchTask->getState() != Task::State_Finished) {
            searchTask->cancel();
        }
        searchTask = nullptr;
    }
    visibleSearchResults.clear();
    nextPushButton->setDisabled(true);
    prevPushButton->setDisabled(true);
    showCurrentResultAndStopProgress();
}

void FindPatternMsaWidget::correctSearchInCombo() {
    if (boxRegion->itemData(boxRegion->currentIndex()).toInt() == RegionSelectionIndex_CurrentSelectedRegion) {
        boxRegion->setCurrentIndex(boxRegion->findData(RegionSelectionIndex_CustomRegion));
    }
}

void FindPatternMsaWidget::setUpTabOrder() const {
    QWidget::setTabOrder(nextPushButton, boxAlgorithm);
    QWidget::setTabOrder(boxRegion, editStart);
    QWidget::setTabOrder(editStart, editEnd);
    QWidget::setTabOrder(editEnd, removeOverlapsBox);
    QWidget::setTabOrder(removeOverlapsBox, boxMaxResult);
}

void FindPatternMsaWidget::startProgressAnimation() {
    resultLabel->setText(tr("Results:"));
    progressLabel->show();
    progressMovie->start();
}

struct SearchResultsComparator {
    bool operator()(const FindPatternWidgetResult &r1, const FindPatternWidgetResult &r2) const {
        return r1.viewRowIndex != r2.viewRowIndex ? r1.viewRowIndex < r2.viewRowIndex : r1.region.startPos < r2.region.startPos;
    }
};

void FindPatternMsaWidget::resortResultsByViewState() {
    MaCollapseModel *collapseModel = msaEditor->getUI()->getCollapseModel();
    visibleSearchResults.clear();
    for (int i = 0; i < allSearchResults.size(); i++) {
        FindPatternWidgetResult &result = allSearchResults[i];
        result.viewRowIndex = collapseModel->getViewRowIndexByMaRowId(result.rowId);
        if (result.viewRowIndex >= 0) {
            visibleSearchResults << result;
        }
    }
    qSort(visibleSearchResults.begin(), visibleSearchResults.end(), SearchResultsComparator());
    currentResultIndex = findCurrentResultIndexFromSelection();
}

int FindPatternMsaWidget::findCurrentResultIndexFromSelection() const {
    const MaEditorSelection &selection = msaEditor->getSelection();
    if (visibleSearchResults.isEmpty() || selection.isEmpty() || selection.height() != 1) {
        return -1;
    }
    U2Region selectedXRegion = selection.getXRegion();
    for (int i = 0; i < visibleSearchResults.size(); i++) {
        const FindPatternWidgetResult &result = visibleSearchResults[i];
        if (result.viewRowIndex == selection.y() && result.region == selectedXRegion) {
            return i;
        }
    }
    return -1;
}

int FindPatternMsaWidget::getNextOrPrevResultIndexFromSelection(bool isNext) {
    if (visibleSearchResults.isEmpty()) {
        return -1;
    }
    const MaEditorSelection &selection = msaEditor->getSelection();
    if (selection.isEmpty()) {
        return 0;
    }
    int resultIndex = 0;
    for (; resultIndex < visibleSearchResults.size(); resultIndex++) {
        FindPatternWidgetResult &result = visibleSearchResults[resultIndex];
        if (result.viewRowIndex >= selection.y() && result.region.startPos >= selection.x()) {
            break;
        }
    }
    return isNext ? (resultIndex == visibleSearchResults.size() ? 0 : resultIndex) : resultIndex - 1;
}

bool FindPatternMsaWidget::isResultSelected() const {
    const MaEditorSelection &selection = msaEditor->getSelection();
    if (selection.height() != 1 || currentResultIndex < 0 || currentResultIndex >= visibleSearchResults.size()) {
        return false;
    }
    const FindPatternWidgetResult &result = visibleSearchResults[currentResultIndex];
    return selection.y() == result.viewRowIndex && result.region == selection.getXRegion();
}

void FindPatternMsaWidget::updateCurrentResultLabel() {
    QString currentResultText = visibleSearchResults.isEmpty() || currentResultIndex < 0 ? "-" : QString::number(currentResultIndex + 1);
    if (visibleSearchResults.isEmpty()) {
        resultLabel->setText(tr("No results"));
    } else {
        resultLabel->setText(tr("Results: %1/%2").arg(currentResultText).arg(visibleSearchResults.size()));
    }
}

FindPatternWidgetResult::FindPatternWidgetResult(qint64 rowId, int viewRowIndex, const U2Region &region)
    : rowId(rowId), viewRowIndex(viewRowIndex), region(region) {
}

}    // namespace U2
