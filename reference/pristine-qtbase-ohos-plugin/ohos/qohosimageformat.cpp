// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qohosimageformat.h"

QT_BEGIN_NAMESPACE

QOhosOptional<::PIXEL_FORMAT> tryMapQtPixelFormatToOhosPixelFormat(QImage::Format format)
{
    switch (format) {
        case QImage::Format_RGBA8888:
            return makeQOhosOptional(::PIXEL_FORMAT::PIXEL_FORMAT_RGBA_8888);
        case QImage::Format_RGB888:
            return makeQOhosOptional(::PIXEL_FORMAT::PIXEL_FORMAT_RGB_888);
        case QImage::Format_Alpha8:
            return makeQOhosOptional(::PIXEL_FORMAT::PIXEL_FORMAT_ALPHA_8);
        default:
            return {};
    }
}

QT_END_NAMESPACE
