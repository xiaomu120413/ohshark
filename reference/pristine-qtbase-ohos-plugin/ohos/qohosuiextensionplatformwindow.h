// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSUIEXTENSIONPLATFORMWINDOW_H
#define QOHOSUIEXTENSIONPLATFORMWINDOW_H

#include <QtCore/private/qohoscommon_p.h>
#include <QtCore/qglobal.h>
#include <memory>
#include <qohosplatformwindow.h>
#include <render/qnativenode.h>
#include <render/qohoswindowproxy.h>
#include <render/qxcomponent.h>
#include <tuple>

QT_BEGIN_NAMESPACE

class QOhosUiExtensionPlatformWindow : public QOhosPlatformWindow
{
public:
    enum class ViewType
    {
        UiExtensionRoot,
        SubWindow,
        EmbeddedWindow,
    };

    class ViewTypeInfo
    {
    public:
        static ViewTypeInfo createForUiExtensionRoot(const std::string &qAbilityInstanceId);
        static ViewTypeInfo createForSubWindowOrFail(QWindow *logicalParent);
        static ViewTypeInfo createForEmbeddedWindowOrFail(QWindow *logicalParent);

        ViewType viewType() const;
        QWindow *logicalParentOrFail() const;
        QWindow *logicalParentOrNull() const;
        std::string qAbilityInstanceIdOrFail() const;

    private:
        ViewTypeInfo(ViewType viewType, QWindow *optLogicalParent, QOhosOptional<std::string> optQAbilityInstanceId);
        ViewType m_viewType;
        QWindow *m_optLogicalParent;
        QOhosOptional<std::string> m_optQAbilityInstanceId;
    };

    explicit QOhosUiExtensionPlatformWindow(QWindow *window);

    void initialize() override;

    void setGeometry(const QRect &geometry) override;
    void setVisible(bool visible) override;

    QOhosSurface *ownedSurfaceOrNull() const override;
    QOhosView *ownedViewOrNull() const override;

    WId winId() const override;

protected:
    void showAsUiExtensionRoot(const std::string &qAbilityInstanceId);
    void showAsEmbeddedWindow(QWindow *logicalParent);
    void showAsSubWindow(QWindow *logicalParent);
    QOhosOptional<std::string> tryGetQAbilityInstanceId() const;

    virtual bool isUiExtensionRootWindow() const = 0;
    virtual ViewTypeInfo determineViewTypeInfo() const = 0;
    virtual void onWindowShow(ViewType viewType) = 0;

private:
    struct JsScopeData
    {
        std::shared_ptr<QXComponentNode> uiExtensionRootNode;
        QOhosOptional<std::string> optQAbilityInstanceId;
        std::vector<std::shared_ptr<void>> callbackReceivers;
    };

    void handleUiExtensionRootWindowRectChangeEventFromOhos();
    void handleWindowFocusStateChangedFromOhos(bool focused);
    void handleSubWindowEventFromOhos(QOhosWindowProxy::WindowEvent event);
    void handleSubWindowRectChangeEventFromOhos(QOhosWindowProxy::RectChangeOptions event);
    ViewType viewType() const;

    std::shared_ptr<QNativeNode> m_nativeNode;
    std::shared_ptr<JsScopeData> m_jsScopeData;
    QPointer<QWindow> m_optLogicalParent;
    QObject m_qObjecContext;
    std::shared_ptr<QOhosWindowProxy> m_ohosWindowProxy;
};

class QOhosInstanceUiExtensionPlatformWindow : public QOhosUiExtensionPlatformWindow
{
public:
    explicit QOhosInstanceUiExtensionPlatformWindow(QWindow *window);

protected:
    bool isUiExtensionRootWindow() const override;
    ViewTypeInfo determineViewTypeInfo() const override;
    void onWindowShow(ViewType viewtype) override;
};

class QOhosBundleUiExtensionPlatformWindow : public QOhosUiExtensionPlatformWindow
{
public:
    using QAbilityQWindowBindingKey = QtOhos::TypedId<std::string, struct QAbilityQWindowBindingIdTag>;

    struct QAbilityQWindowBinding
    {
        QOhosOptional<std::string> qAbilityInstanceId;
        QPointer<QWindow> qWindow;
    };

    explicit QOhosBundleUiExtensionPlatformWindow(QWindow *qWindow);

    static void setQAbilityQWindowBindingKeyForQWindow(
        QWindow *qWindow, const std::string &qAbilityQWindowBindingKeyStr);
    static void handleInstanceStarted(
        const QAbilityQWindowBindingKey &qAbilityQWindowBindingKey,
        const std::string &qAbilityInstanceId);

protected:
    bool isUiExtensionRootWindow() const override;
    ViewTypeInfo determineViewTypeInfo() const override;
    void onWindowShow(ViewType viewtype) override;

private:
    class Context
    {
    public:
        Context();

        void setQAbilityQWindowBindingKeyForQWindow(
            QWindow *qWindow, const QAbilityQWindowBindingKey &qAbilityQWindowBindingKey);
        bool hasInstanceStarted(const QAbilityQWindowBindingKey &qAbilityQWindowBindingKey) const;
        QOhosOptional<std::string> tryFindQAbilityInstanceIdForQWindow(QWindow *qWindow) const;
        QOhosOptional<QWindow *> tryFindQWindowForQAbilityQWindowBindingKey(
            const QAbilityQWindowBindingKey &qAbilityQWindowBindingKey) const;
        void addStartedInstanceOrFail(
            const QAbilityQWindowBindingKey &qAbilityQWindowBindingKey, const std::string &qAbilityInstanceId);

    private:
        std::map<QAbilityQWindowBindingKey, QAbilityQWindowBinding> m_qAbilityQWindowBindings;
    };

    static void checkQWindowMayBeShownAsUiExtensionRootOrFail(QWindow *qWindow);
    static Context &contextInstance();
};

QT_END_NAMESPACE

#endif
