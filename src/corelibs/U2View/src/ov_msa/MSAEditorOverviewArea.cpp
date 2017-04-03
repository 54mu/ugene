/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2017 UniPro <ugene@unipro.ru>
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

#include "MSAEditorOverviewArea.h"

#include "MSAEditor.h"
#include "MSAEditorSequenceArea.h"
#include "Overview/MaSangerOverview.h"
#include "Overview/MaSimpleOverview.h"
#include "Overview/MaGraphOverview.h"
#include "Overview/MaOverviewContextMenu.h"

#include <QVBoxLayout>
#include <QActionGroup>


namespace U2 {

const QString MSAEditorOverviewArea::OVERVIEW_AREA_OBJECT_NAME  = "msa_overview_area";

MSAEditorOverviewArea::MSAEditorOverviewArea(MaEditorWgt *ui)
    : MaEditorOverviewArea(ui, OVERVIEW_AREA_OBJECT_NAME)
{
    simpleOverview = new MaSimpleOverview(ui);

    simpleOverview->setObjectName(OVERVIEW_AREA_OBJECT_NAME + QString("_simple"));

    addOverview(simpleOverview);
    addOverview(graphOverview);

    connect(ui, SIGNAL(customContextMenuRequested(QPoint)), SLOT(sl_onContextMenuRequested(QPoint)));
    connect(ui->getSequenceArea(), SIGNAL(si_highlightingChanged()),
            simpleOverview, SLOT(sl_highlightingChanged()));
    connect(ui->getSequenceArea(), SIGNAL(si_highlightingChanged(addOverview)),
            graphOverview, SLOT(sl_highlightingChanged()));
    connect(ui->getEditor(), SIGNAL(si_referenceSeqChanged(qint64)),
            graphOverview, SLOT(sl_highlightingChanged()));
    connect(ui->getEditor(), SIGNAL(si_referenceSeqChanged(qint64)),
            simpleOverview, SLOT(sl_highlightingChanged()));

    contextMenu = new MaOverviewContextMenu(simpleOverview, graphOverview);

    connect(contextMenu, SIGNAL(si_graphTypeSelected(MaGraphOverviewDisplaySettings::GraphType)),
            graphOverview, SLOT(sl_graphTypeChanged(MaGraphOverviewDisplaySettings::GraphType)));
    connect(contextMenu, SIGNAL(si_colorSelected(QColor)),
            graphOverview, SLOT(sl_graphColorChanged(QColor)));
    connect(contextMenu, SIGNAL(si_graphOrientationSelected(MaGraphOverviewDisplaySettings::OrientationMode)),
            graphOverview, SLOT(sl_graphOrientationChanged(MaGraphOverviewDisplaySettings::OrientationMode)));
    connect(contextMenu, SIGNAL(si_calculationMethodSelected(MaGraphCalculationMethod)),
            graphOverview, SLOT(sl_calculationMethodChanged(MaGraphCalculationMethod)));

    setMaximumHeight( graphOverview->FIXED_HEIGHT + simpleOverview->FIXED_HEIGTH + 5 );
}

bool MSAEditorOverviewArea::isOverviewWidget(QWidget *wgt) const {
    if (MaEditorOverviewArea::isOverviewWidget(wgt) || wgt == simpleOverview) {
        return true;
    }
    return false;
}

void MSAEditorOverviewArea::sl_onContextMenuRequested(const QPoint &p) {
    if (geometry().contains(p)) {
        contextMenu->exec(QCursor::pos());
    }
}

} // namespace
