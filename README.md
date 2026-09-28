# OhShark — Wireshark 4.2.5 × HarmonyOS (arm64) 移植

把真正的上游 Wireshark 4.2.5（原版 Qt Widgets GUI，非自绘界面）移植到
HarmonyOS NEXT arm64 真机（HUAWEI MateBook Pro，设备 ID `3QC0124C11000711`）
并做到完全可用。本仓库是整个移植工作区的**白名单式 git 仓库**：只跟踪
修改过的源码、脚本与文档；上游全量源码树、构建产物、工具链一律不入库
（详见 `.gitignore`）。

**完整修复历史与深层根因分析见 [PORTING-FIXES.md](PORTING-FIXES.md)。**

## 当前状态（2026-09-28）

| 功能 | 状态 |
|---|---|
| 主窗口渲染（装饰窗口+标题栏+三键） | ✅ 可用（系统标题栏，showMaximized） |
| 字体（262 系统字体，无豆腐块） | ✅ |
| 主界面 1× 正确比例（DPR 双重计数已修） | ✅ |
| 菜单/对话框=独立系统子窗口（1× 渲染，全部条目+快捷键列） | ✅ 实测通过 |
| 菜单条目点击 → 触发动作 → 弹出对话框 → 对话框按钮交互 | ✅ 全链路实测通过 |
| 触控板/鼠标 | ✅ 事件管线实测（DispatchMouseEvent）；物理手感待人工确认 |
| 触摸拖拽滚动（数据包列表/树/字节窗格） | ✅ QScroller + physicalSize 兜底修复 |
| 数据包行点击选中 / 滚动条整页点击 | ✅ 实测 |
| 滚轮/触控板双指滚动 | ❌ 平台级阻断（axis 事件被 ArkUI 手势管线消费、
  MMI 监听需系统权限），用触摸拖拽滚动替代 |
| 抓包（dumpcap） | ⚠️ 真机无 root 受限，回退 pcap_findalldevs，主要用 sample.pcap 回放 |

## 仓库结构与工作区

```
thirty/                                ← 工作区根 = 本仓库
├── qtbase-dev/                        ← Qt 源码树（跟踪修改部分）
│   ├── src/plugins/platforms/ohos/    ← ★ 移植核心：OHOS 平台插件（整体跟踪）
│   ├── src/harmonyos/templates/       ← HAP 工程 ArkTS 模板（含修改）
│   ├── mkspecs/ohos-clang/            ← 自建 OHOS 交叉编译 mkspec
│   └── src/{gui,widgets}/…            ← 修改过的 6 个 Qt 模块文件
├── qtbase-ohos/                       ← 上游 Qt 安装前缀（仅原始 ets 模板作参考）
├── ohshark-qt-hap/                    ← 鸿蒙 HAP 工程（ets 页面 + cpp + 资源；
│                                         libs/ 与 build/ 不入库）
├── ws-win/                            ← Wireshark 源码（仅跟踪 main.cpp、
│                                         capture_ifinfo.c 与构建脚本）
├── tools/sign-local.sh                ← HAP 本地签名脚本（材料在 local-sign/，不入库）
├── *.sh / *.ps1                       ← 构建、部署、验证（OCR/像素探针）脚本
├── PORTING-FIXES.md                   ← 分节修复记录（根因分析）
└── README.md
```

- `reference/`：上游原始代码参考（来自 qt-harmonyos-src-5.12.12 快照）——
  `pristine-qtbase-ohos-plugin/` 是未修改的 OHOS 平台插件，
  `pristine-qtbase-modified-files/` 是本仓库修改过的 6 个 Qt 文件的原始版。
  想看移植改动：`git diff --no-index reference/pristine-qtbase-ohos-plugin/ohos
  qtbase-dev/src/plugins/platforms/ohos/`

工作区内**不入库**的大目录：`qtbase-oh2`（Qt 构建树）、`qtbase-host`
（主机工具链）、`mingw`、`ohshark-qt-hap/entry/libs`（部署的 .so）、
`local-sign/`（签名材料）、`HarmonyMarkdownWorkbench`（独立嵌套仓库）。
构建产物（`entry/build`、签名 HAP、验证截图、调试日志）与整个
`pc2b-ohos-thirdparty` 依赖移植工作区均已清理；Wireshark 依赖前缀
（glib/pcap/zlib/gcrypt/gpgerr/cares/speexdsp/qt5compat 等）已就位于
`ws-win/` 下，上游原始参考在 `reference/`。

## 构建 → 部署 → 验证 流水线

### 1. 编译 Qt OHOS 平台插件（及改过的 Qt 模块）

```bash
cd qtbase-oh2
export PATH="/c/Users/mu/Desktop/code/thirty/qtbase-host/bin:$PATH"
/c/Users/mu/Desktop/code/thirty/mingw/mingw64/bin/cmake.exe --build . --target qohos     # 平台插件
/c/Users/mu/Desktop/code/thirty/mingw/mingw64/bin/cmake.exe --build . --target Widgets  # 改过 Widgets 时
"/c/Program Files/Huawei/DevEco Studio/sdk/default/openharmony/native/llvm/bin/llvm-strip.exe" \
    -s plugins/platforms/libqohos.so lib/libQt6Widgets.so
cp plugins/platforms/libqohos.so ../ohshark-qt-hap/entry/libs/arm64-v8a/
cp lib/libQt6Widgets.so       ../ohshark-qt-hap/entry/libs/arm64-v8a/   # 按需
```

### 2. 编译 Wireshark GUI（改过 ws-win 源码时）

```bash
bash ws-win/ws-gui-build.sh
```

### 3. 打包 HAP

```bash
cd ohshark-qt-hap && ./hvigorw assembleHap --mode module -p product=default debuggable
```

### 4. 签名 + 安装 + 启动

```bash
cd ..
bash tools/sign-local.sh \
    ohshark-qt-hap/entry/build/default/outputs/default/entry-default-unsigned.hap \
    ohshark-qt-hap/entry-default-signed.hap 3QC0124C11000711 com.ohshark.qt
MSYS_NO_PATHCONV=1 hdc install -r ohshark-qt-hap/entry-default-signed.hap
MSYS_NO_PATHCONV=1 hdc shell "aa start -a QAbility -b com.ohshark.qt -m entry"
```

### 5. 真机验证（MCP 截图 + OCR + 触摸注入）

```bash
MSYS_NO_PATHCONV=1 hdc shell "snapshot_display -f /data/local/tmp/s.jpeg"
MSYS_NO_PATHCONV=1 hdc file recv /data/local/tmp/s.jpeg <本地路径>
powershell -ExecutionPolicy Bypass -File ocr-shot.ps1 <截图绝对路径>   # OCR
MSYS_NO_PATHCONV=1 hdc shell "uinput -T -m X Y X Y 300"              # 注入点击
MSYS_NO_PATHCONV=1 hdc shell "hilog -x" | grep OhShark               # 日志
```

> 验证纪律：**任何渲染/输入结论必须以截图 OCR + 像素探针 + 真实注入点击
> 为准**，不能只看日志。日志工具悬浮窗（com.huawei.log_tool）会盖在应用
> 上拦截触摸，测试前用 `aa start` 把应用提回前台。

## 关键架构决策（摘要）

1. **Qt High-DPI 单位契约**：`QOhosPlatformScreen::logicalDpi()/logicalBaseDpi()`
   比率（=系统 densityPixels=2）激活 QHighDpiScaling，因子 2 独自承担
   QWindow 逻辑坐标 ↔ 原生（物理）像素的换算；**平台窗口
   `devicePixelRatio()` 必须返回 1.0**。插件内任何额外 ×DPR/÷DPR 都会造成
   双重换算（曾导致主窗口长期 2× 缩放 + 下半屏截断、菜单 2× 放大裁剪）。
2. **平台插件全程使用原生像素**：`QPlatformWindow::setGeometry/geometry()`
   传/收原生矩形，Qt 在 QWindow 边界自动换算。触摸点也是物理像素，
   与 `geometry()` 直接相减。
3. **子窗口（菜单/对话框）独立渲染**：ets 页面放双 XComponent——
   NODE 型做挂载锚点+输入+onAppear（SURFACE 型在隐藏子窗口中永不触发
   onAppear，会死锁创建链）；SURFACE 型（id 后缀 `_surf`）做渲染目标，
   由 `QXComponentRegistry` 捕获其 OHNativeWindow，视图侧轮询采纳为
   自有表面后直接 flush。节点树**不** attach 到子窗口的 NODE XComponent
   （attach 会让节点树成为命中测试目标，XComponent 收不到任何输入）。
4. **输入路径**：主窗口走 ets XComponent `DispatchTouchEvent`；子窗口走
   ArkTS 窗口级事件过滤（`onTouchEventFromArkUi` 转发）。子窗口在
   loadContent 后必须 `setWindowFocusable(true)`（非 focusable 的子窗口
   输入直接穿透）。鼠标统一走 XComponent `DispatchMouseEvent`
   （node-API 鼠标路径因节点树 HIT_TEST NONE 永不触发，开关已置 false）。
5. **backing store**：软件渲染直写各窗口自有表面；主窗口表面 = ets
   SURFACE XComponent 的 surface（registry 全局捕获）；图像尺寸 =
   请求尺寸（原生）× 平台窗口 dpr(1.0) = 1:1。

## 已知未决问题

- 触摸事件多路径重复投递（窗口过滤 + XComponent 各一份）——Qt 状态机
  自行收敛，无可见副作用；可在 `filterEvent` 返回 true 消费掉以优化。
- 触控板物理手感需人工实测（事件管线已验证）。
