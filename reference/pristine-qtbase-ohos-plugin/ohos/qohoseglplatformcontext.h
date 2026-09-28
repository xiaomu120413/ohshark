// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSEGLPLATFORMCONTEXT_H
#define QOHOSEGLPLATFORMCONTEXT_H

#include <QtEglSupport/private/qeglplatformcontext_p.h>

QT_BEGIN_NAMESPACE

class QOhosEGLPlatformContext final : public QEGLPlatformContext
{
public:
    struct CreateInfo
    {
        QSurfaceFormat format = QSurfaceFormat::defaultFormat();
        EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        QPlatformOpenGLContext *optionalShareContext{nullptr};
        EGLConfig *optionalConfig = nullptr;
        QVariant optionalNativeHandle = QVariant{};
        Flags optionalFlags = Flags{};
    };

    explicit QOhosEGLPlatformContext(const CreateInfo &createInfo);

    void swapBuffers(QPlatformSurface *surface) override;

private:
    EGLSurface eglSurfaceForPlatformSurface(QPlatformSurface *surface) final;
    bool m_isDuringSwappingBuffers;
};

QT_END_NAMESPACE

#endif
