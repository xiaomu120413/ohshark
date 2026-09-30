# 原版 Wireshark 4.2.5 → HarmonyOS (arm64) 移植修复记录

状态：**全部验证通过（MCP 真机实点）**。菜单→菜单项→对话框→嵌套对话框→按钮 全链路触摸可用。

## 一、Qt-for-OHOS 平台插件修复（C:\Users\mu\Desktop\code\thirty\qtbase-dev）

1. `src/plugins/platforms/ohos/render/qohoswindowproxy.cpp`
   - `JsScopeData::onTouchEventFromArkUi`：原实现只把**非客户区**（浮动窗标题栏）触摸转给 Qt，
     客户区触摸被 ArkTS 窗口级 TouchEventFilterRegistry 直接丢弃 → QMenu/QDialog 全部点不动。
   - 修复：非 MainWindow 代理且触点在客户区时，构造 QOhosTouchEventTouchPointData，
     经 `jsWindowRef->owningQWindowRef().visitInQtThreadIfAlive`（JS线程→Qt线程）调用
     `QOhosPlatformIntegration::instance()->inputMethodEventHandler()->onTouchEventFromXComponent(...)`
     注入 Qt 触摸管道后 return；标题栏触摸仍走原非客户区路径（浮动窗拖动）。

2. `src/plugins/platforms/ohos/qohosinputmethodeventhandler.cpp`
   - `calculateTouchPointNormalPosition` / `makeWindowLocalPosition`：原来按整屏归一/未减
     frameMargins，导致所有触摸偏移约一个标题栏高度（≈70px）。修复：减去
     `platformWindow->geometry().topLeft() + frameMargins()` 后按 `geometry().size()` 归一。

3. `src/plugins/platforms/ohos/render/qnativenode.h`
   - `handleSurfaceEvent` / `registerParentXComponentCallbacks` 从 Q_SIGNALS 段移到普通
     public 方法段（它们在 .cpp 有定义，moc 生成冲突 signal body → duplicate symbol）。

4. `src/plugins/platforms/ohos/render/qohosview.cpp`
   - `showImmediate()`：子窗口/浮动窗 setParent 后调用
     `m_nativeNode->registerParentXComponentCallbacks(...)`（原本只有 registry 的
     surface-only 回调，子窗口收不到完整 XComponent 回调）。

5. `src/plugins/platforms/ohos/qohosplatformintegration.cpp`
   - 构造函数 `qInstallMessageHandler(ohsharkFileMessageHandler)`：Qt 日志落盘
     /data/storage/el2/base/files/qt_debug.log（临时诊断用，验证完成后可移除）。

6. `src/widgets/widgets/qmenu.cpp` — **Qt::Popup 显示与注册路径修复**
   - `QMenuPrivate::popup()` 在 `q->show()` 之后显式
     `QGuiApplicationPrivate::activatePopup(menuWindow)`：
     qohos 平台窗口创建即 visible，`QWindowPrivate::setVisible` 走"platform window 已同步"
     早退分支，popup_list 永不注册 → activePopupWidget 恒空 → 触摸/鼠标事件从不重定向进菜单。
     activatePopup 自带去重，hide 路径（hide_helper→closePopup）照常移除。
   - 诊断埋点：`[menu.press]` / `[menu.release]`（pos/global/rect/actionAt/currentAction）、
     `[menu.popup]` + `[menu.item]`（菜单原点、每个 action 的矩形）。

7. `src/widgets/widgets/qmenubar.cpp`
   - `QMenuBarPrivate::popupAction()` 诊断埋点 `[bar.popup]`：菜单项文本 + 全局矩形。

8. `src/widgets/kernel/qwidget.cpp`
   - `QWidgetPrivate::show_sys()` 诊断埋点 `[dlg.show]`：顶层 Qt::Dialog 出现时的
     类名/标题/原点/尺寸。

9. `src/widgets/kernel/qwidgetwindow.cpp`
   - `QWidgetWindow::handleMouseEvent` 埋点 `[ww.mouse]`（type/pos/popup/hit 控件/窗口）。

## 二、Wireshark 侧适配（C:\Users\mu\Desktop\code\thirty\ws-win\wireshark-4.2.5）

1. `ui/qt/main.cpp`
   - `main_w` 创建后 `setWindowFlag(Qt::FramelessWindowHint, true)` + `showMaximized()`：
     系统标题栏（37vp）覆盖 Qt y=0 区域，菜单栏被遮 → 无边框全屏。
   - 导出 main() 包装：qputenv QT_QPA_PLATFORM_PLUGIN_PATH=.../libs/arm64、HOME、
     WIRESHARK_CONFIG_DIR / WIRESHARK_DATA_DIR；无文件参数时注入 sample.pcap
     （argv 必须以 NULL 结尾，否则 ws_log_parse_args 越界 SIGSEGV）。
2. `capture/capture_ifinfo.c`：sync_interface_list_open 失败（execve 被禁，dumpcap 不可执行）
   → 回退 pcap_findalldevs 构造接口列表。
3. 引擎编成 libohsharkcore.so（execve 全禁 → fork-per-run）。

## 三、构建/部署

- Widgets 构建：`cd qtbase-oh2 && PATH=qtbase-host/bin:$PATH
  ../mingw/mingw64/bin/cmake.exe --build . --target Widgets`（插件用 `--target qohos`）
- strip：DevEco SDK llvm-strip --strip-all → 拷 ohshark-qt-hap/entry/libs/arm64-v8a/
- 打包：build_app MCP → `HarmonyMarkdownWorkbench/scripts/sign-local.sh <unsigned> <signed>
  3QC0124C11000711 com.ohshark.qt` → hdc install -r → aa start -a QAbility -b com.ohshark.qt -m entry
- 一键脚本：`deploy-ohshark.sh`

## 四、已实测通过（MCP 实点, 2026-09-22）

- 主窗口：选包行 → 详情树 + 字节窗更新；显示过滤 dns → 22/900；进程稳定（前一构建已验，本轮回归通过）。
- 菜单链路：点菜单栏"捕获(C)" → 菜单弹出 popup=yes 注册成功 → 触摸点"Options…" 菜单项
  （press+release 完整送达, actionAt 命中）→ **CaptureOptionsDialog 弹出**（[dlg.show] 确认）。
- 对话框内触摸点"Manage Interfaces…" 按钮（hit=QPushButton）→ **ManageInterfacesDialog 弹出**
  （[dlg.show] class=ManageInterfacesDialog）。
- ManageInterfacesDialog "Cancel" 按钮触摸点击 → 对话框关闭（窗口列表确认消失）。
- CaptureOptionsDialog "Close" 按钮触摸点击 → 对话框关闭（窗口列表确认消失）。
- 主窗口回归：包列表/详情树/字节窗区域触摸全部到达正确窗口，进程存活，无 faultlog。
- 曾经怀疑的"对话框左侧静默区"：查明为设备上用户打开的浏览器窗口（browser0, zord 114）
  覆盖在应用上方，触摸正确地派发给浏览器 —— 系统行为正常，非本应用缺陷。

## 五、遗留说明

1. 诊断埋点（[ww.mouse]/[menu.*]/[bar.popup]/[dlg.show]/[ark.touch]/[xcomp.touch] +
   qInstallMessageHandler 落盘 qt_debug.log）保留在当前构建中，便于后续诊断；
   日志位于应用沙箱 /data/storage/el2/base/files/qt_debug.log，不影响功能。
   如需"干净构建"可移除这些 fprintf 再重编（修复代码必须保留，见上文第一节）。
2. Wireshark 4.2.5 的"捕获"菜单没有独立的"接口..."项——接口列表与
   "Manage Interfaces…" 入口在 Capture Options 对话框（Options… 菜单项）内。
3. 无 CAP_NET_RAW：接口列表为空属预期（dumpcap/execve 均被系统禁止，
   capture_ifinfo.c 已回退 pcap_findalldevs）。

## 六、2026-09-23 补充：系统持久化状态损坏事件与恢复

**现象**：布局完全错乱（菜单栏 0 宽、中心部件 -2000 偏移、childAt 全 null）、输入全部失效。
**根因**：OHOS 系统按 bundle 持久化窗口矩形。多轮 resize 实验后系统保存了损坏的中间状态
（721x384），此后每次启动平台用损坏值回写 Qt 几何，与 Wireshark 自身几何互相打架。
**恢复**：`hdc uninstall` + `install`（清除系统保存的窗口状态）→ 重推沙箱文件：
- sample.pcap → /data/app/el2/100/base/com.ohshark.qt/files/
- wsconf/profiles/Default/preferences（含 capture.no_interface_load: TRUE）
- wsdata/、home/ 目录
恢复后：输入、布局、菜单、对话框全部正常（MCP 实点验证：捕获菜单→Options…→
CaptureOptionsDialog 弹出→Close 按钮触摸关闭，进程稳定）。

**注意**：卸载重装或清除应用数据后必须重推上述沙箱文件，否则启动会弹 QMessageBox 错误框。

## 七、当前已知限制（截至 2026-09-23）

1. 菜单/对话框子窗口内容视觉被 ArkTS 系统子窗口的白色背景遮挡：
   - 事件层完全可用（菜单打开/选中/触发、对话框按钮可点）
   - 内容已正确组合进主窗口 buffer（buffer_dump 验证），但被系统子窗口背景盖住
   - 尝试过且无效：setWindowBackgroundColor('#00000000')（插件侧+页面侧）、
     页面/XComponent/Stack backgroundColor(Color.Transparent)、handlePaletteChange 覆盖
   - 出路：需要 ArkTS 子窗口支持真透明（查 setWindowTransparent 系统权限）或
     补齐 createSubWindowWithOptions 的 JS 实现给子窗口真 surface
2. 主窗口默认 750x550（Wireshark 默认尺寸），非全屏；运行中 setGeometry 全屏会触发
   平台几何事件风暴（布局错乱），须在系统状态干净时验证或平台侧修复
   setWindowGeometryFromOhos 的回写逻辑
3. 对话框触摸坐标有半比偏移（Qt 几何与 ArkTS 窗口位置的换算不一致），
   按钮实际可点但位置需要按 [ww.mouse] 日志反推

## 八、2026-09-23 下午：全屏/半布局攻坚记录（最终回退到稳定态）

**已确认的根因链（未完全修复，方向明确）：**
1. **半布局**（内容只占窗口左半）：`QOhosView::createForWindow` 用窗口**逻辑**几何创建
   native node，但 ArkUI 节点树单位是**物理像素**（PX metric）→ 节点只覆盖窗口一半。
   修复尝试：创建时 ×DPR（生效，img 从 750x550 → 1442x886 满幅）——但同时引发布局
   错乱（子部件 -1221 偏移、菜单栏 0 宽），级联不可控，已回退。
2. **布局错乱**（子部件大负偏移）：与 nodeAreaInfo 的 `globalRelativeOffsetPixels`
   作为 Qt 窗口 topLeft 直接相关（qohosfloatingwindow.cpp handleNodeResizeEvent）。
   该值含窗口在屏幕上的全局位置（如 20,20），被误当窗口本地几何使用时布局被打散。
   这是下一步的正确修复点：nodeAreaInfo 的 offset 需减去窗口自身的全局原点。
3. **全屏 setGeometry**：node 尺寸修好后图像能稳定 3120x1956 不再回翻，但同样触发
   布局错乱（同根因 2）。

**当前部署 = 稳定可用态**：小窗口 750x550 逻辑（1442x886 物理），布局正常、输入可用、
菜单/对话框全链路可用（MCP 实点验证）、文字渲染完整。内容占窗口左半（根因 1）。

**下一步修复顺序**（按依赖）：
1. 先修 handleNodeResizeEvent 的 offset 语义（根因 2，布局错乱的源头）
2. 再恢复 createForWindow 的 ×DPR 修复（根因 1，半布局）
3. 最后恢复应用侧全屏 setGeometry

## 九、2026-09-23 傍晚：三大渲染修复全部落地（按第八节顺序）

1. **handleNodeResizeEvent offset 语义修复**（qohosfloatingwindow.cpp）：
   不再用 node 的全局 rect 当窗口几何；改用 `m_view->viewGeometry()`（窗口真实客户区，
   物理px）/DPR 转逻辑。且**只对 MainWindow 做系统几何同步**——Popup/Dialog 是 Qt 权威
   几何，系统回写会因 getWindowProperties 的 vp/物理单位混用把菜单几何二次减半。
   效果：布局不再散架（负偏移/零宽消失），菜单保持 236x162。
2. **createForWindow ×DPR 修复**（render/qohosview.cpp）：native node 创建几何
   = 窗口逻辑×DPR（ArkUI 节点树是物理像素）。效果：内容铺满窗口（状态栏到真实底部，
   详情树长文本完整），不再只占左半。
3. **全屏恢复**（main.cpp）：show() 后 2s `setGeometry(availableGeometry())`。
   效果：窗口 3120x1956 全屏，内容满幅（当前为 1560x978 半分辨率渲染，2x 拉伸略糊，
   因平台窗口 DPR 上报 1.0 —— 已知质量项，不影响可用性）。

**全链路 MCP 实测（2026-09-23 17:5x）**：全屏布局正常 → 捕获菜单触摸打开
（236x162）→ Options… 触摸选中（contains=1, actionAt 命中）→ CaptureOptionsDialog
弹出 → 进程稳定。

**剩余已知项**：
- 渲染为半分辨率（QPlatformWindow::devicePixelRatio 上报 1.0，应返回屏幕 DPR）
- 菜单/对话框内容仍被 ArkTS 子窗口白背景遮挡（事件层完全可用）

## 十、2026-09-23 晚：白背景遮挡根治（EmbeddedWindow 路由）

**根因**：菜单/对话框走 SubWindow 路径创建 OHOS 系统子窗口，子窗口的不透明白背景
盖住组合进主窗口 buffer 的内容；所有透明 API 在该设备上均无效。

**修复（render/qohosview.cpp + qohosinputmethodeventhandler.cpp）**：
1. `determineViewTypeAndLogicalParent`：Qt::Popup/Qt::Dialog 窗口改走 **EmbeddedWindow**
   路径（渲染进父窗口的节点树/surface，完全不创建系统子窗口）。注意 windowType() 是
   标志组合（菜单报 9=Popup|Window，对话框报 3=Dialog|Window），必须用**位测试**而非
   等值比较；对话框判定时 transientParent 可能未设置，需 synthetic parent 兜底。
2. `createForWindow`：无直接 parent 的 popup 用 transientParent 的视图作为节点父级。
3. `onTouchEventFromXComponent`：触摸重定向——嵌入式 popup/dialog 的触摸以主窗口为
   目标到达，按物理坐标命中**最小包含窗口**重定向到对应的嵌入式窗口（菜单比对话框小，
   嵌套时优先命中内层）。

**实测（MCP 截图+点击，2026-09-23 18:3x）**：
- 捕获菜单：OCR 读出 Options…/Capture Filters…/Ctrl+K/Refresh Interfaces F5（文字可见）
- Options… 触摸选中 → CaptureOptionsDialog 弹出：OCR 读出 Link-layer/Promiscuous/
  Snaplen/Buffer/Monitor Mode/Capture Filter 列头、Manage Interfaces… 按钮、
  Enable promiscuous mode 复选框、Close/Help 按钮（内容全部可见）
- Close 按钮触摸（hit=QPushButton）→ 对话框关闭 → 主窗口恢复交互
- 子窗口数量 0（不再创建任何系统子窗口），进程稳定

**至此用户三类抱怨全部解决**：点击可用（全链路触摸）、渲染正常（全屏+内容满幅+
菜单/对话框可见）、无残留弹窗。

## 十一、2026-09-23 深夜：全屏黑边修复 + 几何反馈循环

**问题**：用户截图看到全屏窗口右侧/下方大片纯黑——backing store 只有半分辨率
（QPlatformWindow::devicePixelRatio() 基类返回 1.0），且内容不拉伸。

**修复**：
1. `QOhosPlatformWindow::devicePixelRatio()` 重写：返回屏幕真实 DPR（2.0）——
   backing store 恢复全分辨率渲染。
2. 禁用 handleNodeResizeEvent 的所有系统→Qt 几何回写：此前主窗口同步形成
   resize 反馈循环（Qt setGeometry 全屏 → 系统报旧矩形 → Qt 缩回 → 循环），
   窗口永远停在持久化的小尺寸。应用是自管几何的无边框全屏程序，回写不需要。
3. `geometryControlledBySystem` 分支不再吞掉 Qt 的 setGeometry（原实现直接
   return，应用的全屏请求被忽略）。

**结果（实测）**：窗口 3120x1956 全屏、无黑边、内容横跨全宽
（包列表列头 No./Time/Destination/Length 到 x=2080+，右侧行内容可见，
状态栏在底部），文字大而清晰。捕获菜单打开、菜单项触摸选中触发（Start 项
命中，actionAt 与视觉有约一项偏移——backing store 为 4x 尺寸 6240x3912，
被窗口缓冲裁剪显示，剩余的视觉/触摸精确对齐为后续打磨项）。

## 十二、2026-09-24：backing store 尺寸收敛（最终）

**诊断**：beginPaint 日志显示 `requested=3120x1956 handleDpr=2.00` → img=6240x3912。
原因：该 Qt 配置下 QWidget/QWindow 传给平台 setGeometry 的矩形**已是原生像素**
（Qt 6 toNativePixels 在 QWindow 层完成，平台窗口几何=原生），backing store 的
requested 尺寸也已是原生全尺寸；我在 QOhosPlatformWindow::devicePixelRatio()
重写返回屏幕 DPR 2.0 造成**二次放大**（4x）。早先的"半分辨率"实际是窗口没到全屏
（几何反馈循环）时的表象，不是 DPR 缺失。

**修复**：撤销 devicePixelRatio 重写（回到基类 1.0）→ img=3120x1956 精确原生全分辨率。

**最终实测（2026-09-24 15:3x，MCP 截图+点击）**：
- 主窗口全屏 3120x1956、原生分辨率、无黑边（像素探针全有内容）
- 状态栏 "Packets: 900 · Displayed: 900 (100.0%)" / "Profile: Default" 在屏幕底部
- 捕获菜单文字可见（Options…/Ctrl+K/Capture Filters…/Refresh Interfaces F5）
- Options… 触摸选中（contains=1，位置对齐）→ 对话框弹出
- 对话框全部内容可见（Link-layer/Promiscuous/Snaplen/Buffer/Monitor Mode/
  Capture Filter 列头、Enable promiscuous mode、Manage Interfaces…、Close|Help）
- Close 按钮触摸命中（hit=QPushButton）
- 已知小瑕疵：对话框组合位置偏右下（非居中）——Qt 居中计算用的几何与组合偏移
  的单位换算仍有差异，不影响可见性与可点性

## 十三、2026-09-24 早晨：隔夜稳定性验证

- 应用进程跨夜存活（pid 42676），全屏原生分辨率渲染保持正常，无黑边。
- 早晨实测（uinput 注入触摸）：捕获菜单打开 → Options… 选中（位置精确对齐）→
  CaptureOptionsDialog 弹出且可见（OCR 读出 Input/Output/Options 组标题）→
  Close 按钮命中（hit=QPushButton）→ 对话框关闭 → 主界面恢复交互。
- 注意：MCP click 工具的 tap 注入隔夜后失效（uitest 问题），uinput 注入正常——
  真机物理触摸等价于 uinput，应用本身输入无回归。

## 十四、2026-09-24 下午：SubWindow 创建死锁（ets SURFACE 型 onAppear 断链）

- 现象：装饰窗口版点击菜单 → `[bar.popup]` 触发 → `[subwin] creation begin` →
  QtMainThread FUTEX 永久挂死（JS 线程 EVENTPOLL 空闲）。
- 根因：把 `SubWindowNativeNode.ets` 的 XComponent 改成 `XComponentType.SURFACE`
  后，SURFACE 型 XComponent 在**尚未显示的子窗口**里不触发 `onAppear`；
  而子窗口创建链的 promise 正是靠 onAppear → registry 取 XComponent →
  resultConsumer → 唤醒 Qt 主线程（`runInSlaveThreadAndWaitForContinue` 无超时）。
- 修复：ets 回退 NODE 型（上游模板原样）。NODE 型锚点在隐藏窗口中照常触发
  onAppear，创建链 30ms 内完成（`[subwin] onAppear fired` → `[pw.exposed]`）。
- 教训：SURFACE 型 ets XComponent 的生命周期回调依赖 surface 分配，
  隐藏窗口中不可依赖。

## 十五、2026-09-24 晚：子窗口独立渲染架构（双 XComponent + registry 捕获）

问题链（逐层揭开）：
1. 菜单内容先合成进主窗口 buffer（composite 路径），但**不透明/白色的
   系统子窗口浮在上面把它盖住**——像素探针确认子窗口矩形内是纯白面板。
2. 该设备（API 26）上 C-API 子 XComponent 永不分配 surface
   （OnSurfaceCreated 回调 window=nil），`OH_ArkUI_SurfaceHolder_Create`
   恒返回 nil——无法给节点树自有表面。
3. 唯一能拿到 OHNativeWindow 的途径 = ArkTS ets XComponent（主窗口正是
   这么工作的，见 registry 的 `g_parentSurface`）。

最终架构：
- `SubWindowNativeNode.ets` 放**双 XComponent**：
  - NODE 型（id=`__nnSubWindow_WIID_x`）：挂载锚点 + 输入 + onAppear
    （上游角色，保证创建链）；
  - SURFACE 型（id 加 `_surf` 后缀）：纯渲染目标，100%×100% 透明背景。
- `QXComponentRegistry::Init` 识别 `_surf` 后缀 id → 注册捕获回调，
  surface 到达时存进 `g_capturedSurfaces[id]`。
- `QNativeNode::adoptOwnSurfaceHolder`（带 15×120ms 重试）优先尝试
  ArkUI SurfaceHolder，失败则按 sub/float 的 `_surf` id 从 registry
  采纳表面 → `handleSurfaceEvent(SurfaceCreated)` → 该窗口直接 flush
  到自己的子窗口表面，1:1 原生分辨率（`buf=472x324` = `img=472x324`）。
- 节点树**不再 attach** 到子窗口的 NODE XComponent（详见十七）。
- 效果：菜单以独立系统子窗口渲染，全部条目 + Ctrl+K/F5 快捷键列
  1:1 可见（OCR 验证），不依赖主窗口合成、不受主窗口裁剪。

## 十六、2026-09-24 深夜：DPR 双重计数（本轮最重大修复）

- 现象：菜单渲染 2× 放大且只剩左上角一个条目；顺藤摸瓜发现**主窗口
  自始至终是 2× 缩放**——packet details/bytes 面板从未显示过（被裁掉），
  之前一直误判为"正常"。
- Qt 单位模型（qtbase-dev 源码实证）：
  - `QWindow::devicePixelRatio()` = `platformWindow->devicePixelRatio() ×
    QHighDpiScaling::factor()`；
  - `QBackingStore::resize(widgetLogicalSize)` → 平台收到
    `size × deviceIndependentToNativeFactor(=hd 2)` = 原生尺寸；
  - `QRasterBackingStore::beginPaint` → 图像 = 请求尺寸 ×
    `window()->handle()->devicePixelRatio()`；
  - 控件实际绘制比例 = `QBackingStorePrivate::backingStoreDevicePixelRatio()`
    = `QWindow::devicePixelRatio()`。
- 根因：`QOhosPlatformWindow::devicePixelRatio()` 曾返回
  `screen()->devicePixelRatio()`=2（本身已含 hd 因子）→
  `QWindow::devicePixelRatio()`=4 → 图像 2× 原生 + 内容 4× 逻辑 →
  flush 时裁剪到原生 buffer = 左上角四分之一画面 2× 放大。
- 修复：**平台窗口 `devicePixelRatio()` 返回 1.0**（hd 因子 2 独自承担
  逻辑↔原生换算）。修复后：`img=3120x1886 = buf`（1:1），`winDpr=2.00`
  处处一致；主窗口首见 packet details 面板，菜单 1:1 全条目。
- composite 偏移同轮修复：`sharedSurfaceOffsetFor` 改为两个平台窗口
  `geometry()`（原生-原生）直接相减，不再经 framePosition/DPR 换算
  （stub frameMargins + 双重 DPR 曾把偏移放大 2 倍并错位 70px 标题栏）。

## 十七、2026-09-28：子窗口输入链修复（点击菜单条目攻坚）

- 现象：菜单渲染完美但触摸全部穿透到主窗口，菜单零输入。
- 逐层根因（四层，全部实证于 hilog）：
  1. **节点树 attach 即拦截**：把节点树 attach 到子窗口的 NODE 型 ets
     XComponent 后，命中测试落在节点树上（STACK-TOUCH-EVENT 21 次），
     XComponent 的 DispatchTouchEvent 永不触发。主窗口之所以正常，是
     因为它的树 attach 在 SURFACE 型 XComponent 下（行为不同）且悬浮
     未挂载。→ 修复：SubWindow/FloatWindow 不再 setParent 到子窗口
     XComponent（渲染走自有表面，根本不需要 attach）。
  2. stack 节点补 `NODE_HIT_TEST_BEHAVIOR = ARKUI_HIT_TEST_MODE_NONE`。
  3. **非 focusable 子窗口输入穿透**：`setWindowFocusable(false)`
     （disableWindowFocusableBeforeLoadContentHack，QMenu 带
     WindowDoesNotAcceptFocus 触发）→ 子窗口的 ArkTS 窗口级事件过滤
     从不触发，触摸直达主窗口。→ 修复：loadContent 完成后立即
     `setWindowFocusable(true)`（展示时用 focusOnShow=false 防抢焦点）。
     修复后两个窗口的事件过滤都收到事件（46+46）。
  4. **鼠标路径**：node-API 鼠标处理器注册在 HIT_TEST NONE 的节点上，
     永不触发 → `enableNativeNodeApiMouseEvents` 默认改 false，改走
     XComponent `DispatchMouseEvent`（对触控板待真机实测）。
- 触摸数据流（修复后）：子窗口 ArkTS 过滤 → `onTouchEventFromArkUi`
  （非 MainWindow 分支）→ Qt 线程 → `onTouchEventFromXComponent(menu, …)`，
  归一化坐标 `calculateTouchPointNormalPosition`（物理像素 - 平台几何
  原生 origin）÷ 原生尺寸。
- **当前未决**：菜单 QWindow 的平台几何在显示后被同步成主窗口全屏值
  （`pgeom=[0,70 3120x1885]`，创建时是 `[322,116 472x324]`），
  归一化坐标随之错误（normal=0.135,0.054，应为 0.212,0.398）。
  怀疑 `handleNodeResizeEvent`/`viewGeometry()` 对 SubWindow 的
  drawableRect 语义（子窗口无装饰时 drawableRect 可能为空/误用主窗口值）。
  这是菜单条目点击的最后一步。

## 十八、2026-09-28：触摸反向劫持修复——菜单/对话框全链路打通（终）

上一节未决问题的真相：菜单 QWindow 几何其实**从未**被改成全屏——那是因为
DPR 修复后几何本来就正确了。真正的最后一环是 `onTouchEventFromXComponent`
里的"嵌入式弹窗触摸重定向"**反向劫持**：触摸经子窗口 ArkTS 事件过滤已正确
送达菜单 QWindow，重定向却把它改回主窗口（旧架构兼容逻辑，检查
`candidate->geometry() × QWindow::devicePixelRatio()`，DPR=4 时代矩形全错）。

修复：`targetWindow->type() & (Qt::Popup | Qt::Dialog)` 时跳过重定向——
触摸已正确寻址，只有从主窗口 XComponent 进来的触摸才需要重定向。

诊断手段升级（全部从文件日志转 hilog）：`[fw.setGeometry]`、`[geo.fromOhos]`、
`[xcomp.touch]`、`[touch.redirect]`、`[touch.map]`（含 pgeom/margins/qgeom）、
`[ww.touch]`、`[ww.mouse]`、`[menu.press]`、`[menu.release]`——逐层定位：
触摸过滤 ✓ → 归一化坐标 ✓（normal=0.212,0.398）→ QWSI ✓ → QWidgetWindow
✓ → 合成鼠标 press/release ✓ → QMenu::mousePress/ReleaseEvent ✓
→ actionAt==currentAction ✓ → **action 触发**。

### 全链路实测结果（截图 OCR + uinput 注入）

1. 菜单条点击 → Capture 菜单独立子窗口（1× 全条目：Options…Ctrl+K、
   Capture Filters…、Refresh Interfaces F5）。
2. "Options…" 点击 → action 触发、菜单关闭 → **Capture Options 对话框**
   以独立装饰子窗口弹出（系统标题栏 "Wireshark · Capture Options"，
   OCR 全读：Interface/Traffic/Link-layer Header/Promiscuous/Snaplen 列、
   Enable promiscuous mode…、Manage Interfaces…、Compile BPFs、
   Start/Close/Help）。
3. 对话框 Close 按钮点击 → 对话框关闭、主界面恢复交互。
4. 主窗口三键实测：还原（1560x943→1560x980 状态切换）、
   最小化（窗口收起、桌面露出、aa start 可恢复）、关闭（同机制）。
5. 鼠标事件链路：`uinput -M` 注入 move → `[ww.mouse] type=5` 到达
   主窗口 hit=QWidget，坐标换算正确——触控板（同 DispatchMouseEvent
   路径）事件管线打通，待物理触控板手感实测。

### 残留小项（不阻塞可用性）

- 触摸事件多路径重复投递（窗口过滤 + XComponent 各一份，46+46）——
  Qt 状态机自行收敛，无可见副作用；后续可在 filterEvent 返回 true 消费掉。
- 弹簧刀式 46 个重复 TouchBegin——同上。
- 触控板物理手感需人工实测。

## 十九、2026-09-28 晚：滚动三连修 + 沙箱恢复

### 1. 触摸拖拽滚动（QScroller）

- 现象：数据包列表触摸拖拽不滚动（桌面 Qt 滚动区不响应触摸拖拽）。
- 方案：`main.cpp` 在主窗口 show 后 `QScroller::grabGesture(viewport,
  LeftMouseButtonGesture)`（所有 QAbstractScrollArea 的 viewport——事件
  落在 viewport 子控件上，grab 必须挂在 viewport 而非 scrollArea 本身）。
- 实测：列表从第 2 行拖到第 556 行（含惯性），后续 715 行同样正常。

### 2. 拖拽失效的深层根因：physicalSize 为 NaN（pixelPerMeter=0）

- QScroller 拖拽阈值 `deltaPixel / ppm > dragStartDistance(米)`：
  ppm=0 时 0/0=NaN，NaN>0.005 恒 false，`moveStarted` 永不触发。
- ppm 来自 `QScreen::physicalDotsPerInch` → `physicalSize()`：
  设备未报物理尺寸，`mapPixelsToMillimeters(pixels, dpi=0)` =
  pixels/0 = **+inf**（不是 0！），`inf > 0` 的守卫挡不住它，
  physicalDotsPerInch = size/inf×25.4 = **0**。
- 修复：`QOhosPlatformScreen::physicalSize()` 兜底——报告值非有限或
  ≤0 时按"参考 96dpi × densityPixels"换算毫米（2x 屏 ≈192dpi）。
  ppm 从 0 → 2834，拖拽阈值（5mm≈14px）正常生效。

### 3. 滚轮/双指滚动：平台级阻断（未解决，已绕过）

四条路全部验证死路：
1. ArkTS `onAxisEvent`（XComponent/Stack/Scroll 包裹）——axis 事件只
   派发给 ArkUI 可滚动容器，普通组件回调不触发（Scroll 包裹还破坏
   Qt 全屏布局，已回退）。
2. NODE_ON_AXIS（节点树 axis handler）——节点树 hit-test 透明，
   axis 永不达；恢复 hit-test 会吞掉触摸（回归验证后回退）。
3. `OH_Input_AddAxisEventMonitorForAll`（进程级监听）——rc=201
   权限拒绝（仅系统应用）。
4. 签名 profile ACL 提升（apl=system_basic + allowed-acls）——
   设备不认自签 profile 的 ACL，仍 rc=201（已回退）。
- 事件确实到达 app 进程（InputKeyFlow axis-begin/end 可见），但被
  ArkUI 手势管线消费。绕过方案：触摸拖拽滚动（上）+ 滚动条整页点击。

### 4. 沙箱 sample.pcap 恢复流程

卸载重装清空 `/data/app/el2/100/base/<bundle>/files/`。恢复：
`hdc file send <本地> /data/local/tmp/sample.pcap`（本地路径必须用
Windows 格式，MSYS 路径会被当目录同步）→ `hdc shell cp` 进沙箱 →
重启应用。欢迎页"sample.pcap doesn't exist"即此因。日志工具悬浮窗
（常驻进程杀不掉）抢前台时 `aa start` 提回。

## 二十、2026-09-29：键盘链路修复 + 滚轮窗口过滤转发

### 1. 键盘（PageDown/字母/快捷键全通）

- 根因：`enableNativeNodeApiKeyEvents=true` 走 NODE_ON_KEY_EVENT 节点路径，
  但节点树永不获得 ArkUI 焦点（焦点在 ets XComponent 上）——按键到达
  app 进程（ImsaKit/InputKeyFlow 可见）却无人处理。
- 修复（两处）：
  1. `enableNativeNodeApiKeyEvents=false` → XComponentCallbackDispatcher
     改注册 `OH_NativeXComponent_RegisterKeyEventCallback`（ets XComponent
     级回调，与鼠标同构）；
  2. ets XComponent 加 `.focusable(true).defaultFocus(true)`——按键派发
     要求 ArkUI 焦点，`focusOnTouch(true)` 不足以获得初始焦点。
- 实测：PageDown 点击行后滚列表（1→30+ 行）；显示过滤框点击后打字
  "dns" 出现在框内；Esc 清除。

### 2. 滚轮：窗口鼠标过滤转发（等待物理触控板验证）

- 发现：`OH_NativeWindowManager_RegisterMouseEventFilter` 的
  `Input_MouseEvent` 携带 `MOUSE_ACTION_AXIS_*` 动作和
  `OH_Input_GetMouseEventAxisType/AxisValue` 滚轮数据——这是无系统权限
  拿滚轮的唯一通道。
- 实现：`QArkUi::MouseEvent` 增加 axisType/axisValue 字段；
  `onMouseEventFromArkUi` 对 AXIS_* 动作换算窗口本地逻辑坐标后调
  `onAxisEventFromArkUi` → Qt wheel。
- 遗憾：`uinput -M -s` 注入的滚轮事件不经过窗口过滤（只在 ArkUI ITK
  里出现，PanVelocity=0 疑似注入缺 delta），远程无法验证；
  真实触控板双指滚动待人工实测。代码路径已就绪。

### 3. 鼠标右键：注入按钮事件不触发 XComponent DispatchMouseEvent
  （只有 move 触发），窗口过滤的客户端区按键事件只转发非客户区——
  真实触控板/外接鼠标的右键待人工实测；触摸长按可出上下文菜单。

## 二十一、2026-09-29：启动竞态 crash（setQWindow 二次覆盖 abort）

- 现象：偶发启动崩溃，faultlog: `LastFatalMessage:[NAPI] Crash occurred on
  ProcessAsyncHandle`，符号化（重建未 strip .so）定位到
  `QAbilityPeerImpl::setQWindow` 的"overwriting previously set qwindow"致命断言。
- 日志链：`tryCreate: MainWindow, preCreated proxy=(nil)`（预建代理跨库
  dlsym 存取竞态未就绪）→ fallback `createForExistingMainWindow` →
  `makeWindowProxyDataForExistingMainWindowInJsThread` → setQWindow，而该
  peer 在预建流程中已被 set 过一次 → abort。
- 修复：覆盖改为警告+替换（预建半途与 fallback 的第二窗口才是活的）。
- 验证：11 次连续 launch/force-stop 循环零新增 crash；菜单→Options 对话框
  →Close→渲染 全部回归通过。

## 二十二、2026-09-29：点击详情面板渲染损坏（damage 区域 y 翻转）

- 现象（用户报告"点击详情就没了"）：点击 packet detail 树任意行 →
  详情区被巨大蓝色块覆盖 + 其余行消失（灰色空底）。
- 三层逐层取证（图像/buffer 转储对比）：
  1. backing store 图像：**正确**（Frame 1/Linux/IP/UDP/DNS 全渲染）；
  2. 拷贝后的原生 buffer：**正确**（与图像一致）；
  3. 屏幕显示：**错误**——呈现的是中间帧（选中蓝块）。
- 根因：`makeOhosRegionRectsForFlush` 把 damage 矩形做了 **y 翻转**
  （`dstHeight - y - h`，OpenGL 表面惯例）。软件渲染的 buffer 是
  top-left 原点——翻转后的 damage 指向镜像区域（详情树的 flush
  [116,1000 3000x882] 被报为 y≈3 顶部），render service 认为
  实际变更区"未损坏"，继续显示旧 buffer 的中间帧。
  全窗口 flush（[0,0 3120x1886]）翻转后仍覆盖全屏，所以初始渲染
  和大多数操作从不暴露此 bug；只有小区域 flush（详情树选中
  重绘）才触发。
- 修复：damage 矩形直接用 top-left 原点（去掉翻转）。
- 验证：详情树 6 行分别点击全部正常（选中行文字+高亮完整，
  状态栏字段联动）；拖拽滚动（556 行）、菜单、Options 对话框、
  Close 回归全过。
- 附带：QScroller 抓取排除 ProtoTree/ByteViewTabWidget（树视图
  点击选中优先于拖拽滚动——判别实验证明 scroller 非根因但排除
  仍属合理）。

## 二十三、2026-09-30：运行期无父窗口误判主窗口 → 注册表二次消费崩溃

- 用户报告："详情点击就关闭了窗口"。faultlog：14:41 crash，进程存活
  90 秒（非启动崩溃），栈在 makeWindowProxyDataForExistingMainWindowInJsThread。
- 根因链：determineViewTypeAndLogicalParent 的兜底分支把**任何**无父、
  无 transientParent、无 tag 的窗口判为 MainWindow。启动时正确（它就是
  主窗口），但运行期创建的此类窗口（无合成父的 tooltip、辅助窗口、
  脱离视图）会走 tryCreate 的 fallback createForExistingMainWindow →
  setQWindow（已改警告）→ takeNodeXComponentFromRegistryOrFail ——
  主 XComponent 注册表条目启动时已被消费（tryTake 是移除式）→
  **abort → 进程死亡 → 用户看到"窗口关闭"**。
- 修复：兜底判 MainWindow 前检查已存在 MainWindow 视图——存在则改走
  SubWindow(syntheticParent)（菜单/对话框同款已验证路径）。启动首窗
  无其他窗口 → 仍判 MainWindow ✓。
- 诊断：[viewtype] 日志转 hilog（每次窗口定型记录类型/父/tag），再触发
  即可定位具体窗口。
- 验证：详情多行点击/展开箭头/双击/悬停/鼠标点击/菜单/对话框/Close/
  拖拽滚动 全部存活，重启后详情渲染完整。
