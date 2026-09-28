interface SubWindowNativeNodeCreateInfo {
  xComponentId: string
  surfaceXComponentId: string
  onAppear: () => void
  onDisAppear: () => void
}

@Entry
@Component
struct SubWindowNativeNode {
  @State private createInfo: SubWindowNativeNodeCreateInfo | undefined = undefined

  aboutToAppear(): void {
    const localStorage  = this.getUIContext().getSharedLocalStorage();
    if (localStorage ) {
      this.createInfo = localStorage.get<SubWindowNativeNodeCreateInfo>('createInfo');
    }
  }

  build() {
    Stack() {
      if (this.createInfo !== undefined) {
        // Render target (bottom): a SURFACE-type XComponent whose native
        // window is captured by the plugin and used as this window's own
        // backing-store target, so the window renders inside its own system
        // subwindow instead of the main window's shared surface.
        if (this.createInfo.surfaceXComponentId !== undefined) {
          XComponent({
            type: XComponentType.SURFACE,
            id: this.createInfo.surfaceXComponentId,
            libraryname: 'qohos'
          })
          .width('100%')
          .height('100%')
          .backgroundColor(Color.Transparent)
        }
        // Attach point + input (top): a NODE-type XComponent (upstream
        // default). SURFACE type never fires onAppear while the subwindow
        // is still hidden, deadlocking window creation.
        XComponent({
          type: XComponentType.NODE,
          id: this.createInfo.xComponentId,
          libraryname: 'qohos'
        })
        .onAppear(this.createInfo.onAppear)
        .onDisAppear(this.createInfo.onDisAppear)
        .width('100%')
        .height('100%')
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.Transparent)
  }
}
