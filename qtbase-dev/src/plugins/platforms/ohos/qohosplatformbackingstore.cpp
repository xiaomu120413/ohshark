// Copyright (C) 2025 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qohosplatformbackingstore.h"
#include "qohosplatformscreen.h"
#include "qohosplatformwindow.h"
#include "render/qohossurface.h"
#include <native_buffer/native_buffer.h>
#include <hilog/log.h>
#include <native_window/buffer_handle.h>
#include <native_window/external_window.h>
#include <qarkui/vsync.h>
#include <qohosutils.h>
#include <render/qohosview.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>
#include <QtGui/private/qhighdpiscaling_p.h>
#include <QtCore/private/qohoslogger_p.h>
#include <QtCore/qendian.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <tuple>

QT_BEGIN_NAMESPACE

namespace
{

// The application's main Qt window (the one whose view type is MainWindow),
// or null when it cannot be found.
QWindow *mainQtWindowOrNull()
{
    const auto windows = QGuiApplication::allWindows();
    for (QWindow *candidate : windows) {
        auto *platformWindow = QOhosPlatformWindow::fromQWindowOrNull(candidate);
        if (platformWindow == nullptr)
            continue;
        auto *view = platformWindow->ownedViewOrNull();
        if (view != nullptr && view->viewType() == QOhosView::ViewType::MainWindow)
            return candidate;
    }
    return nullptr;
}

constexpr std::int32_t ohNativeWindowErrorCodeSuccess = 0;

std::uint64_t getNativeWindowBufferQueueSize(::OHNativeWindow *nativeWindow)
{
    std::int32_t bufferQueueSize;
    auto getBufferQueueSizeResult = ::OH_NativeWindow_NativeWindowHandleOpt(
        nativeWindow, ::NativeWindowOperation::GET_BUFFERQUEUE_SIZE, &bufferQueueSize);
    if (Q_UNLIKELY(getBufferQueueSizeResult != ohNativeWindowErrorCodeSuccess)) {
        qOhosReportFatalErrorAndAbort(
            "%s: failed to get buffer queue size with error code: %d",
            Q_FUNC_INFO, getBufferQueueSizeResult);
    }
    return bufferQueueSize;
}

std::size_t qImageBytesPerPixel(const QImage &image)
{
    const auto bitsPerPixel = image.depth();
    const auto bytesPerPixel = bitsPerPixel / 8;
    return bytesPerPixel;
}

QSpan<uchar> qImageScanLine(QImage &image, int y)
{
    return QSpan(image.scanLine(y), image.width() * qImageBytesPerPixel(image));
}

void copyImageRow(QSpan<const uchar> srcRow, QSpan<uchar> dstRow)
{
    const auto sizeToCopy = std::min(srcRow.size(), dstRow.size());
    std::memcpy(dstRow.data(), srcRow.data(), sizeToCopy);
}

void copyImage(
    QOhosPlatformBackingStore::QImageView srcImage, QImage &dstImage, const QRegion &region, const QPoint &dstOffset)
{
    const auto intersectedRegion =
        region.intersected(QRect({}, srcImage.size()))
        .intersected(QRect({}, dstImage.size()).translated(-dstOffset));

    for (const auto &rect : intersectedRegion) {
        const auto xSrc = rect.x() * srcImage.bytesPerPixel();
        const auto widthSrc = rect.width() * srcImage.bytesPerPixel();
        const auto xDst = (rect.x() + dstOffset.x()) * qImageBytesPerPixel(dstImage);
        const auto widthDst = rect.width() * qImageBytesPerPixel(dstImage);

        for (int row = 0; row < rect.height(); ++row) {
            const int srcY = rect.y() + row;
            const int dstY = srcY + dstOffset.y();
            const auto srcRow = srcImage.constScanLine(srcY).subspan(xSrc, widthSrc);
            auto dstRow = qImageScanLine(dstImage, dstY).subspan(xDst, widthDst);
            copyImageRow(srcRow, dstRow);
        }
    }
}

void debugDrawFlushedQRegion(QImage &dstImage, const QRegion &region)
{
    static const QColor debugBoxColor("#7F00FF00");

    QPainter painter(&dstImage);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    for (const QRect &rect : region)
        painter.fillRect(rect, debugBoxColor);
}

std::vector<::Region::Rect> makeOhosRegionRectsForFlush(
    const QRegion &region, const QPoint &rootWindowOffset, const QSize &dstImageSize)
{
    std::vector<::Region::Rect> rects;
    if (!region.isEmpty()) {
        std::transform(
            region.begin(), region.end(), std::back_inserter(rects),
            [&](const auto &qrect) {
                return ::Region::Rect{
                    .x = qrect.x() + rootWindowOffset.x(),
                    .y = (dstImageSize.height() - qrect.y()) + rootWindowOffset.y() - qrect.height(),
                    .w = static_cast<std::uint32_t>(qrect.width()),
                    .h = static_cast<std::uint32_t>(qrect.height()),
                };
            });
    }
    return rects;
}

std::function<void()> makeVSyncFrameRequestFunc(
    QtOhos::QThreadSafeRef<QWindow> qWindowRef, ::OHNativeWindow *nativeWindow,
    QOhosConsumer<QWindow *> qtThreadFlushFunc)
{
    auto sharedQtThreadFlushFunc = QtOhos::moveToSharedPtr(std::move(qtThreadFlushFunc));
    return QtOhos::evalInJsThread(
        [&](QtOhos::JsState &) {
            auto sharedFrameRequestFunc = QtOhos::makeProxyWithJsThreadDeleter(
                QtOhos::moveToSharedPtr(
                    QArkUi::makeVSyncFrameRequester(
                        nativeWindow,
                        [qWindowRef, sharedQtThreadFlushFunc]() {
                            qWindowRef.visitInQtThreadIfAlive(
                                [sharedQtThreadFlushFunc](QWindow &qWindow) {
                                    (*sharedQtThreadFlushFunc)(&qWindow);
                                });
                            })));

            return [sharedFrameRequestFunc]() {
                (*sharedFrameRequestFunc)();
            };
        },
        Q_FUNC_INFO);
}

QPoint extractNegativeOffset(const QPoint &offset)
{
    return QPoint(
        std::max(0, -offset.x()),
        std::max(0, -offset.y()));
}

} // namespace

namespace
{

// All live raster backing stores. Used to composite subwindow content into
// the shared main-window surface (see QOhosPlatformBackingStore::flushImmediate).
std::vector<QOhosPlatformBackingStore *> &rasterBackingStores()
{
    static std::vector<QOhosPlatformBackingStore *> stores;
    return stores;
}

QPoint sharedSurfaceOffsetFor(QWindow *window, ::OHNativeWindow *nativeWindow)
{
    auto *mainWindow = mainQtWindowOrNull();
    if (mainWindow == nullptr || window == mainWindow)
        return {};
    auto *mainPlatformWindow = QOhosPlatformWindow::fromQWindowOrNull(mainWindow);
    auto *mainSurface =
        mainPlatformWindow != nullptr ? mainPlatformWindow->ownedSurfaceOrNull() : nullptr;
    if (mainSurface == nullptr || mainSurface->nativeWindow() != nativeWindow)
        return {};
    auto *windowPlatformWindow = QOhosPlatformWindow::fromQWindowOrNull(window);
    if (windowPlatformWindow == nullptr)
        return {};
    // Both platform-window geometries are in native (physical) pixels. The
    // offset of this window's client area relative to the main window's
    // client area is exactly where its content must land in the shared
    // main-window buffer. (QWindow::devicePixelRatio() double-counts the
    // high-dpi factor here and framePosition() adds stub frame margins, so
    // deriving the offset from logical coordinates lands 2x off.)
    return windowPlatformWindow->geometry().topLeft()
        - mainPlatformWindow->geometry().topLeft();
}

} // namespace

QOhosPlatformBackingStore::QOhosPlatformBackingStore(QWindow *window, const CreateInfo &createInfo)
    : QRasterBackingStore(window)
    , m_debugDrawFlushedRegion(createInfo.debugDrawFlushedRegion)
    , m_vsyncEnabled(createInfo.enableVsync)
    , m_flushFunc(
        std::make_shared<std::function<void(QWindow *)>>(
            [this](QWindow *qWindow) {
                flushImmediate(qWindow);
            }))
    , m_windowContextManager(createInfo.enableVsync, m_flushFunc)
{
    rasterBackingStores().push_back(this);
}

QOhosPlatformBackingStore::~QOhosPlatformBackingStore()
{
    auto &stores = rasterBackingStores();
    stores.erase(std::remove(stores.begin(), stores.end(), this), stores.end());
}

void QOhosPlatformBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    if (m_reinitializeContextManager) {
        m_windowContextManager = WindowContextManager(m_vsyncEnabled, m_flushFunc);
        m_reinitializeContextManager = false;
    }

    auto *platformWindow = QOhosPlatformWindow::fromQWindowOrNull(window);
    auto *surface = platformWindow != nullptr ? platformWindow->ownedSurfaceOrNull() : nullptr;
    auto bounds = region.translated(offset).boundingRect() & m_image.rect();

    // ohshark diag: which window and native surface each raster flush targets
    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
        "[rbs.flush] win=%{public}p surface=%{public}p native=%{public}p bounds=[%{public}d,%{public}d %{public}dx%{public}d]",
        (void *)window, (void *)surface,
        surface ? (void *)surface->nativeWindow() : nullptr,
        bounds.x(), bounds.y(), bounds.width(), bounds.height());

    if (bounds.isEmpty())
        return;

    if (surface == nullptr) {
        qOhosPrintfDebug("Window %p has no surface", window);
        return;
    }

    m_windowContextManager
        .getOrCreateWindowContext(window, surface->nativeWindow())
        .flushData().updateDirtyRegionAndScheduleFlush(region, offset);
}

void QOhosPlatformBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    m_reinitializeContextManager = (size != m_requestedSize);
    QRasterBackingStore::resize(size, staticContents);
}

QImage::Format QOhosPlatformBackingStore::format() const
{
    return QOhosSurface::mapNativeBufferFormatToQImageFormatOrFail(QOhosSurface::bufferFormat);
}

QOhosPlatformBackingStore::QImageView::QImageView(const QImage &srcImage, const QRect &subRect)
    : m_srcImage(srcImage)
    , m_subRect(subRect)
{
    auto imageRect = QRect(QPoint{}, m_srcImage.size());
    if (!imageRect.contains(m_subRect)) {
        qOhosReportFatalErrorAndAbort(
            "image rect: (%d, %d, %d, %d) does not contain sub-rect: (%d, %d, %d, %d)",
            imageRect.x(), imageRect.y(), imageRect.width(), imageRect.height(),
            m_subRect.x(), m_subRect.y(), m_subRect.width(), m_subRect.height());
    }

    constexpr auto minAcceptedImageDepth = 8;
    if (srcImage.depth() < minAcceptedImageDepth)
        qOhosReportFatalErrorAndAbort("QImageView is not supported for <8bpp QImages");
}

std::size_t QOhosPlatformBackingStore::QImageView::bytesPerPixel() const
{
    return qImageBytesPerPixel(m_srcImage);
}

QSpan<const uchar> QOhosPlatformBackingStore::QImageView::constScanLine(int i) const
{
    Q_ASSERT(i >= 0 && i < m_subRect.height());
    auto srcScanLine = QSpan(
        m_srcImage.constScanLine(m_subRect.y() + i), m_srcImage.bytesPerLine());
    return srcScanLine.subspan(
        m_subRect.x() * bytesPerPixel(), bytesPerLine());
}

std::size_t QOhosPlatformBackingStore::QImageView::bytesPerLine() const
{
    return bytesPerPixel() * m_subRect.width();
}

QSize QOhosPlatformBackingStore::QImageView::size() const
{
    return m_subRect.size();
}

QOhosPlatformBackingStore::BufferRegionHandler::BufferRegionHandler(::OHNativeWindow *nativeWindow)
    : m_bufferQueueSize(getNativeWindowBufferQueueSize(nativeWindow))
{
}

std::optional<QRegion> QOhosPlatformBackingStore::BufferRegionHandler::mergeRegionForBufferHandle(
    ::BufferHandle *bufferHandle,
    QRegion region) const
{
    const auto it = m_buffersToFlushSequenceIds.find(bufferHandle);
    if (it == m_buffersToFlushSequenceIds.end())
        return {};

    std::uint64_t numberOfRegions = m_flushSequenceId - it->second;
    if (numberOfRegions > m_bufferQueueSize)
        return {};

    for (auto it = m_lastFlushedRegions.cend() - numberOfRegions; it != m_lastFlushedRegions.cend(); ++it)
        region = region.united(*it);

    return region;
}

void QOhosPlatformBackingStore::BufferRegionHandler::storeRegionForBufferHandle(
    ::BufferHandle *bufferHandle,
    const QRegion &region)
{
    m_buffersToFlushSequenceIds[bufferHandle] = m_flushSequenceId;

    if (m_buffersToFlushSequenceIds.size() > m_bufferQueueSize) {
        m_buffersToFlushSequenceIds.clear();
        m_lastFlushedRegions.clear();
        m_flushSequenceId = 0;
        return;
    }

    m_lastFlushedRegions.push_back(region);
    if (m_lastFlushedRegions.size() > m_bufferQueueSize)
        m_lastFlushedRegions.pop_front();

    ++m_flushSequenceId;
}

QOhosPlatformBackingStore::FlushData::FlushData(std::function<void()> flushRequestFunc)
    : m_flushRequestFunc(std::move(flushRequestFunc))
{
}

std::pair<QRegion, QPoint> QOhosPlatformBackingStore::FlushData::fetchAndReset()
{
    return std::make_pair(
        std::exchange(m_mergedRegionForFlush, QRegion {}),
        std::exchange(m_lastWindowOffset, QPoint {}));
}

void QOhosPlatformBackingStore::FlushData::updateDirtyRegionAndScheduleFlush(
    const QRegion &region, const QPoint &rootWindowOffset)
{
    m_mergedRegionForFlush = m_mergedRegionForFlush.united(region);
    m_lastWindowOffset = rootWindowOffset;
    m_flushRequestFunc();
}

QOhosPlatformBackingStore::WindowContext::WindowContext(
    ::OHNativeWindow *nativeWindow, std::function<void()> flushRequestFunc)
    : m_bufferRegionHandler(std::make_unique<BufferRegionHandler>(nativeWindow))
    , m_flushData(std::move(flushRequestFunc))
{
}

QOhosPlatformBackingStore::BufferRegionHandler &
QOhosPlatformBackingStore::WindowContext::bufferRegionHandler()
{
    return *m_bufferRegionHandler;
}

QOhosPlatformBackingStore::FlushData &QOhosPlatformBackingStore::WindowContext::flushData()
{
    return m_flushData;
}

QOhosPlatformBackingStore::WindowContextManager::WindowContextManager(
    bool vsyncEnabled,
    std::shared_ptr<std::function<void(QWindow *)>> flushImmediateFunc)
    : m_flushFunc(flushImmediateFunc)
    , m_vsyncEnabled(vsyncEnabled)
{
}

QOhosPlatformBackingStore::WindowContext &
QOhosPlatformBackingStore::WindowContextManager::getOrCreateWindowContext(
    QWindow *window,
    ::OHNativeWindow *nativeWindow)
{
    auto handlerIter = m_windowContexts.find(window);
    if (handlerIter == m_windowContexts.end()) {

        std::function<void()> flushFunc;

        if (m_vsyncEnabled) {
            auto weakQtFlushFunc = QtOhos::makeWeakPtr(m_flushFunc);
            auto qWindowRef = QtOhos::makeQThreadSafeRef(window);
            flushFunc = makeVSyncFrameRequestFunc(
                qWindowRef, nativeWindow,
                [weakQtFlushFunc](QWindow *qWindow) {
                    auto sharedQtThreadFlushFunc = weakQtFlushFunc.lock();
                    if (sharedQtThreadFlushFunc)
                        (*sharedQtThreadFlushFunc)(qWindow);
                });
        } else {
            flushFunc = [window, flushFunc = m_flushFunc]() {
                (*flushFunc)(window);
            };
        }

        std::tie(handlerIter, std::ignore) = m_windowContexts.emplace(
            window, std::make_unique<WindowContext>(
                nativeWindow, std::move(flushFunc)));
    }
    return *(handlerIter->second);
}

bool QOhosPlatformBackingStore::scroll(const QRegion &area, int dx, int dy)
{
    Q_GUI_EXPORT void qt_scrollRectInImage(QImage &img, const QRect &rect, const QPoint &offset);

    const qreal devicePixelRatio = m_image.devicePixelRatio();
    const QPoint delta(
        static_cast<int>(dx * devicePixelRatio),
        static_cast<int>(dy * devicePixelRatio));

    for (const QRect &rect : area) {
        qt_scrollRectInImage(
            m_image,
            QRect(rect.topLeft() * devicePixelRatio, rect.size() * devicePixelRatio), delta);
    }

    return true;
}

void QOhosPlatformBackingStore::beginPaint(const QRegion &region)
{
    // ohshark diag
    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
        "[bs.beginPaint] win=%{public}p requested=%{public}dx%{public}d img=%{public}dx%{public}d imgDpr=%{public}.2f handleDpr=%{public}.2f",
        (void *)window(), m_requestedSize.width(), m_requestedSize.height(),
        m_image.width(), m_image.height(),
        (float)m_image.devicePixelRatio(),
        window()->handle() ? (float)window()->handle()->devicePixelRatio() : -1.0);
    auto previousImageSize = m_image.size();
    QRasterBackingStore::beginPaint(region);
    bool imageWasResized = previousImageSize != m_image.size();
    if (imageWasResized)
        m_windowContextManager = WindowContextManager(m_vsyncEnabled, m_flushFunc);
}

void QOhosPlatformBackingStore::flushImmediate(QWindow *window)
{
    auto *platformWindow = QOhosPlatformWindow::fromQWindowOrNull(window);
    auto *surface = platformWindow != nullptr ? platformWindow->ownedSurfaceOrNull() : nullptr;

    if (surface == nullptr) {
        qOhosPrintfDebug("Window %p has no surface", window);
        return;
    }

    bool isRootWindow = window == this->window();

    ::OHNativeWindow *nativeWindow = surface->nativeWindow();

    // Subwindow / floating-window surfaces fall back to the main window's
    // OHNativeWindow because their NODE-type ArkTS XComponent never allocates
    // one. Content flushed through that shared surface must be shifted by the
    // window's position within the main window so it lands on screen where
    // the window actually is, instead of overwriting the main window's origin.
    QPoint sharedSurfaceOffset = sharedSurfaceOffsetFor(window, nativeWindow);

    // ohshark diag
    {
        const QRect qg = window->geometry();
        const QRect pg = platformWindow != nullptr ? platformWindow->geometry() : QRect();
        const qreal winDpr = window->devicePixelRatio();
        const qreal scrDpr = window->screen() != nullptr ? window->screen()->devicePixelRatio() : -1.0;
        const qreal hdFactor = QHighDpiScaling::factor(window->screen());
        auto *diagMain = mainQtWindowOrNull();
        const auto bufGeom = QOhosSurface::tryGetBufferGeometryForWindow(nativeWindow);
        OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
            "[rbs.flushImm] win=%{public}p root=%{public}d framePos=%{public}d,%{public}d sharedOff=%{public}d,%{public}d img=%{public}dx%{public}d "
            "qgeom=[%{public}d,%{public}d %{public}dx%{public}d] pgeom=[%{public}d,%{public}d %{public}dx%{public}d] winDpr=%{public}.2f scrDpr=%{public}.2f hd=%{public}.2f mainDpr=%{public}.2f buf=%{public}dx%{public}d",
            (void *)window, int(isRootWindow),
            window->framePosition().x(), window->framePosition().y(),
            sharedSurfaceOffset.x(), sharedSurfaceOffset.y(),
            m_image.width(), m_image.height(),
            qg.x(), qg.y(), qg.width(), qg.height(),
            pg.x(), pg.y(), pg.width(), pg.height(),
            (float)winDpr, (float)scrDpr, (float)hdFactor,
            (float)(diagMain ? diagMain->devicePixelRatio() : -1.0),
            bufGeom ? bufGeom->width() : -1, bufGeom ? bufGeom->height() : -1);
    }

    auto &windowContext = m_windowContextManager.getOrCreateWindowContext(window, nativeWindow);
    QRegion region;
    QPoint rootWindowOffset;
    std::tie(region, rootWindowOffset) = windowContext.flushData().fetchAndReset();

    if (region.isEmpty())
        return;

    // A window that renders through the shared main-window surface must not
    // touch the buffer queue itself: a second context on the same
    // OHNativeWindow stalls on buffer fences and breaks presentation for
    // both windows (observed as a black main window). Instead, ask the main
    // window to repaint; its flush composites this window's image (see the
    // registry loop below) and presents once, on its own context.
    if (!sharedSurfaceOffset.isNull()) {
        if (auto *mainWindow = mainQtWindowOrNull()) {
            QWindowSystemInterface::handleExposeEvent(
                mainWindow, QRegion(QRect(QPoint(0, 0), mainWindow->size())));
            mainWindow->requestUpdate();
        }
        return;
    }

    auto &bufferRegionHandler = m_windowContextManager
        .getOrCreateWindowContext(window, nativeWindow).bufferRegionHandler();

    auto srcImageRect = isRootWindow
        ? QRect({}, m_image.size())
        : QRect(rootWindowOffset, platformWindow->geometry().size()).intersected(QRect({}, m_image.size()));
    if (srcImageRect.isEmpty()) {
        qOhosPrintfDebug("Cannot get source image rect, ignore flush call");
        return;
    }

    QImageView srcImage = QImageView(m_image, srcImageRect);

    const auto rootWindowRegionToFlush = region
        .translated(rootWindowOffset)
        .intersected(srcImageRect)
        .translated(-rootWindowOffset);

    surface->paintOnNativeWindowSurface(
        [&](QImage &dstImage, ::BufferHandle *bufferHandle) {
            if (srcImage.bytesPerPixel() != qImageBytesPerPixel(dstImage)) {
                qOhosReportFatalErrorAndAbort(
                    "%s: bytes per pixel in src and dst image mismatch. Image formats are not the same.", Q_FUNC_INFO);
            }

            const auto& mergedRegionOpt = bufferRegionHandler.mergeRegionForBufferHandle(bufferHandle, rootWindowRegionToFlush);
            const auto negativeOffset = extractNegativeOffset(rootWindowOffset);
            const auto dstImageOffset = negativeOffset + sharedSurfaceOffset;
            const auto requestedRegion = mergedRegionOpt.value_or(QRegion(QRect(negativeOffset, srcImage.size())));
            const auto sourceRegionToFlush = requestedRegion.translated(-negativeOffset);
            copyImage(srcImage, dstImage, sourceRegionToFlush, dstImageOffset);

            // The main window owns the shared surface. Subwindow content is
            // composited into it at the subwindow's position, so every main
            // window flush must also re-draw the currently visible subwindows
            // — otherwise this flush would present a buffer without them and
            // the menus/dialogs would visually disappear.
            if (sharedSurfaceOffset.isNull() && window == this->window()) {
                for (auto *otherStore : rasterBackingStores()) {
                    if (otherStore == this)
                        continue;
                    QWindow *otherWindow = otherStore->window();
                    if (otherWindow == nullptr || !otherWindow->isVisible())
                        continue;
                    auto *otherPlatformWindow = QOhosPlatformWindow::fromQWindowOrNull(otherWindow);
                    auto *otherSurface =
                        otherPlatformWindow != nullptr ? otherPlatformWindow->ownedSurfaceOrNull() : nullptr;
                    if (otherSurface == nullptr || otherSurface->nativeWindow() != nativeWindow)
                        continue;
                    if (otherStore->m_image.isNull())
                        continue;
                    const QPoint otherOffset = sharedSurfaceOffsetFor(otherWindow, nativeWindow);
                    // ohshark diag
                    OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
                        "[rbs.composite] other=%{public}p off=%{public}d,%{public}d img=%{public}dx%{public}d",
                        (void *)otherWindow, otherOffset.x(), otherOffset.y(),
                        otherStore->m_image.width(), otherStore->m_image.height());
                    // ohshark diag: bounding box of dark (content) pixels in
                    // the full image, to locate where the window content is
                    // actually painted.
                    {
                        const QImage &im = otherStore->m_image;
                        int minX = INT_MAX, minY = INT_MAX, maxX = -1, maxY = -1;
                        for (int y = 0; y < im.height(); y += 2) {
                            const QRgb *line = reinterpret_cast<const QRgb *>(im.constScanLine(y));
                            for (int x = 0; x < im.width(); x += 2) {
                                const QRgb px = line[x];
                                if (qAlpha(px) > 32 && qGray(px) < 220) {
                                    if (x < minX) minX = x;
                                    if (y < minY) minY = y;
                                    if (x > maxX) maxX = x;
                                    if (y > maxY) maxY = y;
                                }
                            }
                        }
                        OH_LOG_Print(LOG_APP, LOG_INFO, 0x0500, "OhShark",
                            "[menu.contentBB] img=%{public}dx%{public}d format=%{public}d bb=[%{public}d,%{public}d %{public}dx%{public}d]",
                            im.width(), im.height(), int(im.format()),
                            minX, minY,
                            minX == INT_MAX ? 0 : maxX - minX + 1,
                            minY == INT_MAX ? 0 : maxY - minY + 1);
                    }
                    {
                        static int dumpCount = 0;
                        if (dumpCount < 3) {
                            dumpCount++;
                            QImage dump = otherStore->m_image;
                            if (dump.format() != QImage::Format_ARGB32)
                                dump = dump.convertToFormat(QImage::Format_ARGB32);
                            dump.save(QStringLiteral("/data/storage/el2/base/files/menu_dump_%1.png")
                                          .arg(dumpCount));
                        }
                    }
                    // The raster image is larger than the window's native
                    // size (the backing store scales by the platform-window
                    // devicePixelRatio on top of the already-native
                    // requested size); the painted content occupies the
                    // top-left native-sized region only.
                    const QSize otherNativeSize =
                        otherPlatformWindow != nullptr
                            ? otherPlatformWindow->geometry().size()
                            : otherStore->m_image.size();
                    QImageView otherSrc(
                        otherStore->m_image,
                        QRect(QPoint{}, otherNativeSize)
                            .intersected(QRect(QPoint{}, otherStore->m_image.size())));
                    if (otherSrc.bytesPerPixel() != qImageBytesPerPixel(dstImage))
                        continue;
                    copyImage(otherSrc, dstImage, QRegion(QRect({}, otherSrc.size())), otherOffset);
                    {
                        static int bufDumpCount = 0;
                        if (bufDumpCount < 2) {
                            bufDumpCount++;
                            QImage bufDump = dstImage.copy(
                                QRect(otherOffset.x(), otherOffset.y(),
                                      qMin(otherSrc.size().width(), dstImage.width() - otherOffset.x()),
                                      qMin(otherSrc.size().height(), dstImage.height() - otherOffset.y())));
                            bufDump.save(QStringLiteral("/data/storage/el2/base/files/buffer_dump_%1.png")
                                             .arg(bufDumpCount));
                        }
                    }
                }
            }

            if (m_debugDrawFlushedRegion)
                debugDrawFlushedQRegion(dstImage, rootWindowRegionToFlush);

            return makeOhosRegionRectsForFlush(
                mergedRegionOpt.value_or(QRegion()),
                (isRootWindow ? QPoint{} : rootWindowOffset) + sharedSurfaceOffset, dstImage.size());
        },
        [&](::BufferHandle *bufferHandle) {
            bufferRegionHandler.storeRegionForBufferHandle(bufferHandle, rootWindowRegionToFlush);
        });

    if (isRootWindow) {
        auto *view = platformWindow->ownedViewOrNull();
        if (view != nullptr)
            view->handleSurfaceContentsUpdated();
    }
}

QT_END_NAMESPACE
