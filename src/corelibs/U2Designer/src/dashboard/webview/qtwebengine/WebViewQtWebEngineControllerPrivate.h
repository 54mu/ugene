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

#ifndef _U2_WEB_VIEW_QT_WEB_ENGINE_CONTROLLER_PRIVATE_H_
#define _U2_WEB_VIEW_QT_WEB_ENGINE_CONTROLLER_PRIVATE_H_

#include "dashboard/webview/WebViewControllerPrivate.h"

namespace U2 {

class JavaScriptAgent;
class U2WebChannel;

class WebViewWriter : public QObject {
    Q_OBJECT
public:
    WebViewWriter(const QString &pageUrl);

    void write(const QString &data);

private:
    const QString pageUrl;
};

class WebViewQtWebEngineControllerPrivate : public WebViewControllerPrivate {
    Q_OBJECT
public:
    WebViewQtWebEngineControllerPrivate(U2WebView *webView);

    void loadPage(const QString &pageUrl);
    void savePage(const QString &pageUrl);

    void registerJavaScriptAgent(JavaScriptAgent *agent);
    void runJavaScript(const QString &script);
    void runJavaScript(const QString &script, WebViewCallback callback);

private:
    void init();

    U2WebChannel *channel;
};

}    // namespace U2

#endif    // _U2_WEB_VIEW_QT_WEB_ENGINE_CONTROLLER_PRIVATE_H_
