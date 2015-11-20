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

#include "primitives/GTMainWindow.h"

#include <QApplication>
#include <QWindow>

namespace HI {

QWidget* GTMainWindow::mainWindowWidget = NULL;

#define GT_CLASS_NAME "GTMainWindow"

#define GT_METHOD_NAME "getMainWindow"
QMainWindow* GTMainWindow::getMainWindow(U2::U2OpStatus &os) {
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(GTMainWindow::getMainWindowAsWidget(os));
    GT_CHECK_RESULT(mainWindow != NULL, "QMainWindow is NULL", NULL);
    return mainWindow;
}
#undef GT_METHOD_NAME

#define GT_METHOD_NAME "getMainWindowAsWidget"
QWidget* GTMainWindow::getMainWindowAsWidget(U2::U2OpStatus &os) {
    if (mainWindowWidget == NULL){
        foreach(QWidget *widget, qApp->topLevelWidgets()){
            if(widget->inherits("QMainWindow")){
                mainWindowWidget = widget;
                break;
            }
        }
    }
    GT_CHECK_RESULT(mainWindowWidget!=NULL,"main window not found", NULL);
    return mainWindowWidget;
}
#undef GT_METHOD_NAME

#undef GT_CLASS_NAME

}