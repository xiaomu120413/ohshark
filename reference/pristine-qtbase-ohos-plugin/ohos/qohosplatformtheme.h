// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QOHOSPLATFORMTHEME_H
#define QOHOSPLATFORMTHEME_H

#include <QtCore/qfileinfo.h>
#include <QtCore/qhash.h>
#include <QtCore/qmap.h>
#include <QtGui/qicon.h>
#include <QtGui/qpalette.h>
#include <qpa/qplatformtheme.h>
#include <qpa/qplatformdialoghelper.h>


QT_BEGIN_NAMESPACE

class QOhosPlatformTheme : public QPlatformTheme
{
public:
    enum class ColorsTheme {
        Light,
        Dark,
    };

    static constexpr int defaultWheelScrollLines = 3;

    QOhosPlatformTheme();

    ColorsTheme colorsTheme() const;
    void setColorsTheme(ColorsTheme theme);

    QPlatformDialogHelper *createPlatformDialogHelper(DialogType type) const override;
    bool usePlatformNativeDialog(DialogType type) const override;
    QVariant themeHint(ThemeHint hint) const override;

    const QPalette *palette(Palette type) const override;
    const QFont *font(Font type) const override;
    void setWheelScrollLines(int wheelScrollLines);

    QPlatformSystemTrayIcon *createPlatformSystemTrayIcon() const override;

    QIcon fileIcon(const QFileInfo &fileInfo, QPlatformTheme::IconOptions iconOptions) const override;

private:
    ColorsTheme m_currentColorsTheme;
    QMap<ColorsTheme, QHash<Palette, QPalette>> m_themesPalettes;
    QHash<Font, QFont> m_fonts;
    int m_wheelScrollLines;
};

QT_END_NAMESPACE

#endif // QOHOSPLATFORMTHEME_H
