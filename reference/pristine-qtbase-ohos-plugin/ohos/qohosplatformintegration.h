// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSPLATFORMINTERATION_H
#define QOHOSPLATFORMINTERATION_H

#include <QtGui/qtguiglobal.h>


#include <accessibility/qohosaccessibilitytree.h>
#include <memory>
#include <qohosdisplayinfo.h>
#include <qohosscreenmanager.h>
#include <qpa/qplatformaccessibility.h>
#include <qpa/qplatformintegration.h>
#include <qpa/qplatformoffscreensurface.h>
#include <qpa/qplatforminputcontext.h>
#include <qpa/qplatformtheme.h>
#include <EGL/egl.h>
#include <QtCore/qpoint.h>
#include <QtCore/qscopedpointer.h>
#include <QTouchDevice>
#include <qohosplatformclipboard.h>
#include <render/qxcomponent.h>

QT_BEGIN_NAMESPACE
class QPlatformOffscreenSurface;
class QOffscreenSurface;
class QThread;
class QOhosInputMethodEventHandler;
class QOhosPlatformScreen;
class QOhosPlatformFontDatabase;
class QOhosPlatformNativeInterface;
class QOhosSystemLocale;
class QOhosPlatformServices;

class QOhosPlatformIntegration : public QPlatformIntegration
{

public:
    enum class WindowGeometryPersistencePolicy
    {
        Disabled,
        Enabled,
        FollowSystemSetting,
    };

    static QOhosPlatformIntegration *instance();

    QOhosPlatformIntegration(const QStringList &paramList);

    static void setDefaultDisplayMetrics(const QOhosDisplayInfo &m_displayInfo);

    Qt::WindowState defaultWindowState(Qt::WindowFlags flags) const override;
    virtual QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformWindow *createForeignWindow(QWindow *, WId) const override;
    virtual QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    QVariant styleHint(StyleHint hint) const override;
    QPlatformNativeInterface *nativeInterface() const override;
#ifndef QT_NO_OPENGL
    virtual QPlatformOpenGLContext
                        *createPlatformOpenGLContext(QOpenGLContext *context) const override;
#endif
    virtual QAbstractEventDispatcher *createEventDispatcher() const override;
    QPlatformOffscreenSurface
                        *createPlatformOffscreenSurface(QOffscreenSurface *surface) const override;

    virtual QPlatformFontDatabase *fontDatabase() const override;

#ifndef QT_NO_CLIPBOARD
    virtual QOhosPlatformClipboard *clipboard() const override;
#endif
#if QT_CONFIG(draganddrop)
    QPlatformDrag *drag() const override;
#endif

    virtual bool hasCapability(Capability cap) const override;

    QPlatformInputContext *inputContext() const override;
    void initialize() override;
    QPlatformServices *services() const override;

    QPlatformTheme *createPlatformTheme(const QString &name) const override;
    QStringList themeNames() const override;

    static QOhosSystemLocale *systemLocale();
    static void setSystemLocale(QOhosSystemLocale *systemLocale);

    static void setMainWindowGeometryPersistencePolicy(WindowGeometryPersistencePolicy policy);
    static WindowGeometryPersistencePolicy getMainWindowGeometryPersistencePolicy();

    QOhosInputMethodEventHandler *inputMethodEventHandler();
    QOhosScreenManager *screenManager() const;

    static void setDarkThemeEnabled(bool darkThemeEnabled);

#ifndef QT_NO_ACCESSIBILITY
    QPlatformAccessibility *accessibility() const override;
    std::shared_ptr<void> tryInitializeAccessibilityForQWindowWithXComponent(
        QWindow *qWindow, const std::function<QXComponentRender(QtOhos::JsState &)> &xComponentProvider);
#endif

    static QPoint getSyntheticQPointForContextMenuOnLongPress();

    void setContextMenuOnLongPressEnabled();
    bool isContextMenuOnLongPressEnabled() const;

private:
    std::shared_ptr<void> m_eglDisplayHandle;

    QThread *m_mainThread;
    std::unique_ptr<QOhosPlatformFontDatabase> m_ohosFDB;
    std::unique_ptr<QOhosPlatformServices> m_ohosPlatformServices;

    std::unique_ptr<QOhosScreenManager> m_screenManager;

    QScopedPointer<QOhosPlatformNativeInterface> m_ohosPlatformNativeInterface;

    QScopedPointer<QPlatformInputContext> m_platformInputContext;
    std::unique_ptr<QOhosInputMethodEventHandler> m_ohosInputMethodEventHandler;

#ifndef QT_NO_CLIPBOARD
    std::unique_ptr<QOhosPlatformClipboard> m_platformClipboard;
#endif
#if QT_CONFIG(draganddrop)
    std::unique_ptr<QPlatformDrag> m_drag;
#endif

    static QScopedPointer<QOhosSystemLocale> m_systemLocale;
    static WindowGeometryPersistencePolicy m_mainWindowPersistencePolicy;

#ifndef QT_NO_ACCESSIBILITY
    struct AccessibilityContext
    {
        std::unique_ptr<QPlatformAccessibility> accessibility;
        std::function<std::shared_ptr<void>(
            QOhos::AccessibilityNode::Id, const std::function<QXComponentRender(QtOhos::JsState &)> &)>
            tryInitializeAccessibilityProviderInJsThreadFunc;
        std::shared_ptr<void> accessibilityDataHandle;
    };
    static std::unique_ptr<AccessibilityContext> makeAccessibilityContext();
    std::unique_ptr<AccessibilityContext> m_accessibilityContext;
#endif

    std::shared_ptr<void> m_applicationStateTracker;

    bool m_isContextMenuOnLongPressEnabled = false;
};

QT_END_NAMESPACE

#endif // QOHOSPLATFORMINTERATION_H
