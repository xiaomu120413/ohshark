import { fileIo } from '@kit.CoreFileKit';

interface WindowNativeNodeCreateInfo {
  xComponentId: string
  onAppear: () => void
  onAttach: () => void
  onDisAppear: () => void
}

// The qohos native XComponent input path does not reliably deliver physical
// touch/trackpad events on this device, so the ArkTS layer records every touch
// and mouse event into a file in the app sandbox; the Qt side tails that file
// and re-injects the events (deduplicated against whatever arrives natively).
// Format per line: "<type> <x> <y>" with type 0=down/press 1=move 2=up/release.
function recordInput(filesDir: string, type: number, x: number, y: number): void {
  const path = filesDir + '/touch.fifo';
  try {
    const f = fileIo.openSync(path,
      fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.CREATE | fileIo.OpenMode.APPEND);
    const line = type.toString() + ' ' + x.toString() + ' ' + y.toString() + '\n';
    fileIo.writeSync(f.fd, line);
    fileIo.closeSync(f);
  } catch (e) {
    // Never let input logging break the page.
  }
}

@Entry
@Component
struct WindowNativeNode {
  @State private createInfo: WindowNativeNodeCreateInfo | undefined = undefined
  private filesDir: string = ''

  aboutToAppear(): void {
    const localStorage  = this.getUIContext().getSharedLocalStorage();
    if (localStorage ) {
      this.createInfo = localStorage.get<WindowNativeNodeCreateInfo>('createInfo');
    }
    try {
      this.filesDir = this.getUIContext().getHostContext()!.filesDir;
    } catch (e) {
      this.filesDir = '';
    }
  }

  build() {
    Stack() {
      if (this.createInfo !== undefined) {
        XComponent({
          type: XComponentType.SURFACE,
          id: this.createInfo.xComponentId,
          libraryname: 'qohos'
        })
        .onLoad(() => { if (this.createInfo) this.createInfo.onAttach(); })
        .onAppear(this.createInfo.onAppear)
        .onDisAppear(this.createInfo.onDisAppear)
        .focusOnTouch(true)
        .onTouch((event: TouchEvent) => {
          const t = event.touches.length > 0 ? event.touches[0] :
            (event.changedTouches.length > 0 ? event.changedTouches[0] : undefined);
          if (t === undefined || this.filesDir === '') {
            return;
          }
          let type = 1;
          if (event.type === TouchType.Down) {
            type = 0;
          } else if (event.type === TouchType.Up || event.type === TouchType.Cancel) {
            type = 2;
          }
          recordInput(this.filesDir, type, t.x, t.y);
        })
        .onMouse((event: MouseEvent) => {
          if (this.filesDir === '') {
            return;
          }
          let type = 1;
          if (event.action === MouseAction.Press) {
            type = 0;
          } else if (event.action === MouseAction.Release) {
            type = 2;
          } else if (event.action !== MouseAction.Move) {
            return;
          }
          recordInput(this.filesDir, type, event.x, event.y);
        })
        .width('100%')
        .height('100%')
      }
    }
    .width('100%')
    .height('100%')
  }
}
