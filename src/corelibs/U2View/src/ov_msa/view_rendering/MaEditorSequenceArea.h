/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2017 UniPro <ugene@unipro.ru>
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

#ifndef _U2_MA_EDITOR_SEQUENCE_AREA_
#define _U2_MA_EDITOR_SEQUENCE_AREA_

#include <QColor>
#include <QTimer>
#include <QPainter>
#include <QWidget>

#include <U2Core/U2Region.h>
#include <U2Core/MultipleAlignment.h>

#include <U2Gui/GScrollBar.h>

#include "MaEditorSelection.h"
#include "../MaEditor.h"
#include "../MsaEditorUserModStepController.h"

class QRubberBand;

namespace U2 {

#define SETTINGS_ROOT QString("msaeditor/")
#define SETTINGS_COLOR_NUCL     "color_nucl"
#define SETTINGS_COLOR_AMINO    "color_amino"
#define SETTINGS_COLOR_RAW      "color_raw"
#define SETTINGS_HIGHLIGHT_NUCL     "highlight_nucl"
#define SETTINGS_HIGHLIGHT_AMINO    "highlight_amino"
#define SETTINGS_HIGHLIGHT_RAW      "highlight_raw"
#define SETTINGS_COPY_FORMATTED "copyformatted"

class GScrollBar;
class MaEditor;
class MaEditorWgt;
class SequenceAreaRenderer;

class MaModificationInfo;
class MsaColorScheme;
class MsaColorSchemeFactory;
class MsaHighlightingScheme;
class MsaHighlightingSchemeFactory;

class U2VIEW_EXPORT MaEditorSequenceArea : public QWidget {
    Q_OBJECT
    friend class SequenceAreaRenderer;

public:
    MaEditorSequenceArea(MaEditorWgt* ui, GScrollBar* hb, GScrollBar* vb);
    virtual ~MaEditorSequenceArea();

    MaEditor *getEditor() const;

    QSize getCanvasSize(const QList<int> &seqIdx, const U2Region &region) const;

    int getFirstVisibleBase() const;
    int getLastVisibleBase(bool countClipped) const;
    int getNumVisibleBases() const;
    int getDisplayableRowsCount() const;

    /*
     * Returns count of sequences that are drawn on the widget by taking into account
     * collapsed rows.
     */
    int getNumDisplayableSequences() const;

    bool isAlignmentEmpty() const;

    bool isPosInRange(int position) const;
    bool isSeqInRange(int rowNumber) const;
    bool isInRange(const QPoint &point) const;
    QPoint boundWithVisibleRange(const QPoint &point) const;

    bool isVisible(const QPoint& p, bool countClipped) const;
    bool isPositionVisible(int pos, bool countClipped) const;
    bool isRowVisible(int rowNumber, bool countClipped) const;

    const MaEditorSelection &getSelection() const;

    void updateSelection(const QPoint &newMousePos);

    // update selection when collapsible model changed
    void updateSelection();

    virtual void setSelection(const MaEditorSelection& sel, bool newHighlightSelection = false);

    virtual void moveSelection(int dx, int dy, bool allowSelectionResize = false);

    virtual void adjustReferenceLength(U2OpStatus& os) {}

    U2Region getSelectedRows() const;

    QString getCopyFormatedAlgorithmId() const;
    void setCopyFormatedAlgorithmId(const QString& algoId);

    virtual void deleteCurrentSelection();

    /**
     * Shifts currently selected region to @shift.
     * If @shift > 0, the region is moved to the right and "true" is returned.
     * If @shift <= 0, the region is moved to the left only for the available number
     * of columns (i.e. the columns with gaps). The returned value specifies
     * whether the region was actually moved in this case.
     */
    bool shiftSelectedRegion(int shift);

    void centerPos(const QPoint &point);
    void centerPos(int pos);

    void setFont(const QFont& f);

    void onVisibleRangeChanged();

    bool isAlignmentLocked() const;

    void drawVisibleContent(QPainter &painter);

    bool drawContent(QPainter &painter, const QRect &area);
    bool drawContent(QPainter &painter, const QRect &area, int xStart, int yStart);
    bool drawContent(QPainter &painter, const U2Region &region, const QList<int> &seqIdx);
    bool drawContent(QPainter &painter, const U2Region &region, const QList<int> &seqIdx, int xStart, int yStart);

    bool drawContent(QPainter &painter);
    bool drawContent(QPixmap &pixmap);
    bool drawContent(QPixmap &pixmap, const U2Region &region, const QList<int> &seqIdx);

    void highlightCurrentSelection();

    QString exportHighlighting(int startPos, int endPos, int startingIndex, bool keepGaps, bool dots, bool transpose);

    MsaColorScheme *getCurrentColorScheme() const;
    MsaHighlightingScheme *getCurrentHighlightingScheme() const;
    bool getUseDotsCheckedState() const;

    QAction *getReplaceCharacterAction() const;
    const QAction * const getRemoveSAction() const;

public slots:
    void sl_changeColorSchemeOutside(const QString &id);
    void sl_delCurrentSelection();
    void sl_cancelSelection();

protected slots:
    void sl_changeCopyFormat(const QString& alg);
    void sl_changeColorScheme();
    void sl_fillCurrentSelectionWithGaps();

    virtual void sl_alignmentChanged(const MultipleAlignment &ma, const MaModificationInfo &modInfo);

    void sl_completeUpdate();
    void sl_completeRedraw();

    void sl_triggerUseDots();
    void sl_useDots();

    void sl_registerCustomColorSchemes();
    void sl_colorSchemeFactoryUpdated();
    void sl_setDefaultColorScheme();
    void sl_changeHighlightScheme();

    void sl_replaceSelectedCharacter();
    void sl_changeSelectionColor();
    virtual void sl_modelChanged();

private slots:
    void sl_hScrollBarActionPerfermed();

signals:
    void si_selectionChanged(const MaEditorSelection& current, const MaEditorSelection& prev);
    void si_selectionChanged(const QStringList& selectedRows);
    void si_highlightingChanged();
    void si_visibleRangeChanged(QStringList visibleSequences, int reqHeight);
    void si_visibleRangeChanged();
    void si_startMaChanging();
    void si_stopMaChanging(bool msaUpdated);
    void si_copyFormattedChanging(bool enabled);

protected:
    void setCursorPos(const QPoint& p);
    void setCursorPos(int x, int y);
    void setCursorPos(int pos);

    void resizeEvent(QResizeEvent *event);
    void paintEvent(QPaintEvent *event);
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);

    void keyPressEvent(QKeyEvent *);
    void keyReleaseEvent(QKeyEvent *);

    virtual void initRenderer() = 0;
    virtual void updateActions() = 0;
    virtual void drawBackground(QPainter& p);

    /**
     * Inserts a region consisting of gaps only before the selection. The inserted region width
     * is specified by @countOfGaps parameter if 0 < @countOfGaps, its height is equal to the
     * current selection's height.
     *
     * If there is no selection in MSA then the method does nothing.
     *
     * If -1 == @countOfGaps then the inserting region width is equal to
     * the selection's width. If 1 > @countOfGaps and -1 != @countOfGaps then nothing happens.
     */
    void insertGapsBeforeSelection( int countOfGaps = -1 );

    /**
     * Reverse operation for @insertGapsBeforeSelection( ),
     * removes the region preceding the selection if it consists of gaps only.
     *
     * If there is no selection in MSA then the method does nothing.
     *
     * @countOfGaps specifies maximum width of the removed region.
     * If -1 == @countOfGaps then count of removed gap columns is equal to
     * the selection width. If 1 > @countOfGaps and -1 != @countOfGaps then nothing happens.
     */
    void removeGapsPrecedingSelection( int countOfGaps = -1 );

    /*
     * Interrupts the tracking of MSA modifications caused by a region shifting,
     * also stops shifting. The method is used to keep consistence of undo/redo stack.
     */
    void cancelShiftTracking( );

    void drawAll();

    void updateColorAndHighlightSchemes();

    void initColorSchemes(MsaColorSchemeFactory* defaultColorSchemeFactory);

    void registerCommonColorSchemes();

    void initHighlightSchemes(MsaHighlightingSchemeFactory* hsf);

    MsaColorSchemeFactory * getDefaultColorSchemeFactory() const;
    MsaHighlightingSchemeFactory * getDefaultHighlightingSchemeFactory() const;

    virtual void getColorAndHighlightingIds(QString &csid, QString &hsid);
    void applyColorScheme(const QString &id);

    void processCharacterInEditMode(QKeyEvent *e);
    void processCharacterInEditMode(char newCharacter);
    void replaceChar(char newCharacter);
    virtual void insertChar(char ) {}
    void exitFromEditCharacterMode();
    virtual bool isCharacterAcceptable(const QString &text) const;
    virtual const QString &getInacceptableCharacterErrorMessage() const;

    void deleteOldCustomSchemes();

    virtual void updateCollapsedGroups(const MaModificationInfo &maModificationInfo);

protected:
    enum MaMode {
        ViewMode,
        ReplaceCharMode,
        InsertCharMode
    };

public:
    MaMode getModInfo();

protected:
    MaEditor*       editor;
    MaEditorWgt*    ui;

    MsaColorScheme*         colorScheme;
    MsaHighlightingScheme*  highlightingScheme;
    bool                    highlightSelection;

    GScrollBar*     shBar;
    GScrollBar*     svBar;
    QRubberBand*    rubberBand;

    SequenceAreaRenderer*   renderer;

    QPixmap*        cachedView;
    bool            completeRedraw;

    MaMode          maMode;
    QTimer          editModeAnimationTimer;
    QColor          selectionColor;

    bool                editingEnabled;
    bool                shifting;
    bool                selecting;
    Qt::MouseButton     prevPressedButton;
    QPoint              rubberBandOrigin; // global window coordinates
    QPoint              cursorPos; // mouse cursor position in alignment coordinates
    MaEditorSelection   selection; // selection with rows indexes in collapsible model coordinates
    MaEditorSelection   baseSelection; // selection with rows indexes in absolute coordinates

    int                 maVersionBeforeShifting;

    QAction*            useDotsAction;

    QList<QAction*>     colorSchemeMenuActions;
    QList<QAction* >    customColorSchemeMenuActions;
    QList<QAction* >    highlightingSchemeMenuActions;

    QAction*            replaceCharacterAction;
    QAction*            fillWithGapsinsSymAction;

    // The member is intended for tracking MSA changes (handling U2UseCommonUserModStep objects)
    // that does not fit into one method, e.g. shifting MSA region with mouse.
    // If the changing action fits within one method it's recommended using
    // the U2UseCommonUserModStep object explicitly.
    MsaEditorUserModStepController changeTracker;

    static const QChar emDash;
};

} // namespace

#endif // _U2_MA_EDITOR_SEQUENCE_AREA_

