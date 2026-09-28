// OhShark — upstream Wireshark 4.2.5 on HarmonyOS, native ArkTS UI.
//
// Why ArkTS and not Qt: the qohos Qt platform plugin on this device breaks
// physical touch routing, blanks the surface on any popup, and mis-renders on
// resize. A native UI gets system-guaranteed input and rendering; the engine is
// unchanged — the real tshark from the cross-compiled Wireshark, reached via
// the libohsharkapi NAPI bridge (fork per run, see cpp/ohshark_api.cpp).
//
// Columns are tshark's own _ws.col.* fields, so the packet list shows exactly
// what Wireshark on the PC shows. The details pane is built from the same
// engine's `-T ek -x` dissection tree; the bytes pane from its raw hex.

import ohsharkapi from 'libohsharkapi.so';
import { fileIo } from '@kit.CoreFileKit';

interface SharkResult {
  rc: number;
  out: string;
  err: string;
}

class PacketRow {
  no: number = 0;
  time: string = '';
  src: string = '';
  dst: string = '';
  proto: string = '';
  len: string = '';
  info: string = '';
  color: string = '#ffffff';
}

class DetailRec {
  layersText: string = '';
  rawHex: string = '';
}

const STATS_MODES: string[] = ['io,phs', 'conv,tcp', 'conv,udp', 'endpoints,ip', 'expert'];
const CHIPS: string[] = ['all', 'icmp', 'arp', 'dns', 'tcp', 'tls'];

@Entry
@Component
struct AnalyzerPage {
  @State rows: PacketRow[] = [];
  @State details: string = '';
  @State hexStr: string = '';
  @State status: string = 'Loading engine…';
  @State engineLine: string = 'engine: —';
  @State filter: string = '';
  @State ifaceLabel: string = 'iface: any';
  @State openLabel: string = 'Open…';
  @State statsLabel: string = 'Stats';
  @State liveLabel: string = 'Live';
  @State selectedNo: number = -1;
  @State listTitle: string = 'No.  Time    Source                Destination           Protocol  Length  Info';

  private filesDir: string = '';
  private sampleFile: string = '';
  private liveFile: string = '';
  private currentFile: string = '';
  private detailCache: Record<string, DetailRec> = {};
  private lastLiveFrame: number = 0;
  private liveBusy: boolean = false;
  private statsIdx: number = -1;
  private ifaceIdx: number = -1;
  private ifaces: string[] = [];
  private openIdx: number = -1;
  private liveTimer: number = -1;

  aboutToAppear(): void {
    const ctx = this.getUIContext().getHostContext();
    this.filesDir = ctx !== undefined ? ctx.filesDir : '';
    this.sampleFile = this.filesDir + '/sample.pcap';
    this.liveFile = this.filesDir + '/live.pcap';
    this.engineLine = 'engine: ' + ohsharkapi.version();
    ohsharkapi.writeSample(this.sampleFile);
    ohsharkapi.interfaces().then((res: SharkResult) => {
      const lines = res.out.split('\n');
      const names: string[] = [];
      for (const l of lines) {
        const dot = l.indexOf('.');
        if (dot <= 0) {
          continue;
        }
        let name = l.substring(dot + 1);
        const tab = name.indexOf('\t');
        if (tab > 0) {
          name = name.substring(0, tab);
        }
        name = name.trim();
        if (name.length > 0) {
          names.push(name);
        }
      }
      if (names.length > 0) {
        this.ifaces = names;
        this.engineLine = this.engineLine + '  |  ' + names.length.toString() + ' interfaces';
      }
    });
    this.decode(this.sampleFile, '');
    // Live tail: a privileged dumpcap run outside the sandbox (via hdc) writes
    // live.pcap; we dissect new frames every few seconds. This is the closest
    // HarmonyOS allows a normal app to live capture.
    this.liveTimer = setInterval(() => {
      this.liveTick();
    }, 3000);
  }

  aboutToDisappear(): void {
    if (this.liveTimer >= 0) {
      clearInterval(this.liveTimer);
      this.liveTimer = -1;
    }
  }

  private colorFor(proto: string): string {
    const p = proto.toUpperCase();
    if (p.includes('DNS') || p.includes('ARP')) {
      return '#fdf3c0';
    }
    if (p.includes('TLS') || p.includes('SSL')) {
      return '#f8d7d7';
    }
    if (p.includes('HTTP')) {
      return '#d9f0d9';
    }
    if (p.includes('ICMP')) {
      return '#ecd8f0';
    }
    if (p.includes('TCP')) {
      return '#d6e9f8';
    }
    if (p.includes('UDP')) {
      return '#d9f2e6';
    }
    return '#ffffff';
  }

  private indent(n: number): string {
    let s = '';
    for (let i = 0; i < n; i++) {
      s += '    ';
    }
    return s;
  }

  // ArkTS type narrowing turns `typeof v === 'object'` on Object-typed values
  // into `never`, so structure checks go through untyped helpers instead.
  private isRecord(v: Object): boolean {
    return typeof v === 'object' && !Array.isArray(v);
  }

  private valueToString(v: Object): string {
    try {
      const s = JSON.stringify(v);
      if (s === null) {
        return '';
      }
      if (s.length >= 2 && s.startsWith('"') && s.endsWith('"')) {
        return s.substring(1, s.length - 1);
      }
      return s;
    } catch (e) {
      return '';
    }
  }

  private walkLayers(rec: Record<string, Object>, depth: number, out: string[]): void {
    const keys = Object.keys(rec).sort();
    for (const k of keys) {
      if (k.endsWith('_raw')) {
        continue;
      }
      const v: Object = rec[k];
      if (this.isRecord(v)) {
        out.push(this.indent(depth) + k);
        this.walkLayers(v as Record<string, Object>, depth + 1, out);
      } else {
        out.push(this.indent(depth) + k + ': ' + this.valueToString(v));
      }
    }
  }

  private hexDump(hex: string): string {
    let out = '';
    for (let i = 0; i + 1 < hex.length; i += 32) {
      const rowHex = hex.substring(i, i + 32);
      let line = i.toString(16).padStart(8, '0') + '  ';
      let ascii = '';
      for (let j = 0; j + 1 < rowHex.length + 1; j += 2) {
        const pair = rowHex.substring(j, j + 2);
        if (pair.length < 2) {
          break;
        }
        line += pair + ' ';
        const code = parseInt(pair, 16);
        ascii += code >= 32 && code < 127 ? String.fromCharCode(code) : '.';
      }
      while (line.length < 8 + 2 + 16 * 3 + 1) {
        line += ' ';
      }
      out += line + ' ' + ascii + '\n';
    }
    return out;
  }

  private ekField(rec: Record<string, Object>, key: string): string {
    const keys = Object.keys(rec);
    for (const k of keys) {
      const v: Object = rec[k];
      if (this.isRecord(v)) {
        const sub = v as Record<string, Object>;
        const sv: Object | undefined = sub[key];
        if (sv !== undefined && !this.isRecord(sv)) {
          return this.valueToString(sv);
        }
      }
    }
    return '';
  }

  // Parses tshark `-T ek -x` output and fills the per-frame details cache.
  private parseEk(payload: string): void {
    const lines = payload.split('\n');
    for (const line of lines) {
      const t = line.trim();
      if (t.length < 2 || t.startsWith('{"index"')) {
        continue;
      }
      let parsed: object | null = null;
      try {
        parsed = JSON.parse(t);
      } catch (e) {
        continue;
      }
      if (parsed === null) {
        continue;
      }
      const doc = parsed as Record<string, Object>;
      const layersObj: Object | undefined = doc['layers'];
      if (layersObj === undefined) {
        continue;
      }
      const layers = layersObj as Record<string, Object>;
      const frameNo = this.ekField(layers, 'frame_frame_number');
      if (frameNo.length === 0) {
        continue;
      }
      const rec = new DetailRec();
      const out: string[] = [];
      this.walkLayers(layers, 0, out);
      rec.layersText = out.join('\n');

      // Longest top-level *_raw holds the whole frame.
      let bestKey = '';
      let bestLen = -1;
      for (const k of Object.keys(layers)) {
        if (k.endsWith('_raw')) {
          const v: Object = layers[k];
          if (this.isRecord(v)) {
            continue;
          }
          const s = this.valueToString(v);
          if (s.length > bestLen && s.length % 2 === 0) {
            bestLen = s.length;
            bestKey = k;
          }
        }
      }
      if (bestKey.length > 0) {
        rec.rawHex = this.hexDump(this.valueToString(layers[bestKey]));
      }
      this.detailCache[frameNo] = rec;
    }
  }

  // Parses tshark `-T fields` output with tab separator into list rows.
  private parseColumns(payload: string, append: boolean): number {
    if (!append) {
      this.rows = [];
    }
    const lines = payload.split('\n');
    let count = 0;
    for (const line of lines) {
      if (line.trim().length === 0) {
        continue;
      }
      const f = line.split('\t');
      if (f.length < 6 || f[0].trim().length === 0) {
        continue;
      }
      const r = new PacketRow();
      r.no = parseInt(f[0]);
      r.time = f[1].length > 9 ? f[1].substring(0, 9) : f[1];
      r.src = f[2];
      r.dst = f[3];
      r.proto = f[4];
      r.len = f[5];
      r.info = f.length > 6 ? f[6] : '';
      r.color = this.colorFor(r.proto);
      this.rows.push(r);
      count++;
      if (this.rows.length > 20000) {
        this.rows.splice(0, this.rows.length - 20000);
      }
    }
    return count;
  }

  private decode(file: string, filter: string): void {
    this.currentFile = file;
    this.status = 'Dissecting…';
    const args: string[] = ['-r', file, '-n', '-T', 'fields', '-E', 'separator=/t',
      '-e', 'frame.number', '-e', 'frame.time_relative',
      '-e', '_ws.col.Source', '-e', '_ws.col.Destination', '-e', '_ws.col.Protocol',
      '-e', '_ws.col.Length', '-e', '_ws.col.Info'];
    if (filter.length > 0) {
      args.push('-Y', filter);
    }
    ohsharkapi.runTshark(args).then((res: SharkResult) => {
      if (res.out.trim().length === 0) {
        this.status = 'Dissection failed (rc=' + res.rc.toString() + '): ' +
          res.err.trim().substring(0, 160);
        return;
      }
      this.detailCache = {};
      this.parseColumns(res.out, false);
      this.status = this.rows.length.toString() + ' packets — asking engine for details…';
      const args2: string[] = ['-r', file, '-n', '-T', 'ek', '-x'];
      if (filter.length > 0) {
        args2.push('-Y', filter);
      }
      ohsharkapi.runTshark(args2).then((res2: SharkResult) => {
        this.parseEk(res2.out);
        this.status = this.rows.length.toString() +
          ' packets dissected by upstream Wireshark (engine in-process)';
        const first = this.rows.length > 0 ? this.rows[0] : undefined;
        if (first !== undefined) {
          this.selectRow(first);
        }
      });
    });
  }

  private liveTick(): void {
    if (this.liveLabel !== 'Live ■' || this.liveBusy) {
      return;
    }
    try {
      if (!fileIo.accessSync(this.liveFile)) {
        return;
      }
    } catch (e) {
      return;
    }
    this.liveBusy = true;
    const args: string[] = ['-r', this.liveFile, '-n', '-T', 'fields', '-E', 'separator=/t',
      '-e', 'frame.number', '-e', 'frame.time_relative',
      '-e', '_ws.col.Source', '-e', '_ws.col.Destination', '-e', '_ws.col.Protocol',
      '-e', '_ws.col.Length', '-e', '_ws.col.Info',
      '-Y', 'frame.number > ' + this.lastLiveFrame.toString()];
    ohsharkapi.runTshark(args).then((res: SharkResult) => {
      const added = this.parseColumns(res.out, true);
      const last = this.rows.length > 0 ? this.rows[this.rows.length - 1] : undefined;
      if (last !== undefined) {
        this.lastLiveFrame = last.no;
      }
      if (added > 0) {
        this.status = 'Live: +' + added.toString() + ' packets (frame ' +
          this.lastLiveFrame.toString() + ') — tailing live.pcap';
        const args2: string[] = ['-r', this.liveFile, '-n', '-T', 'ek', '-x',
          '-Y', 'frame.number > ' + Math.max(this.lastLiveFrame - added, 0).toString()];
        ohsharkapi.runTshark(args2).then((res2: SharkResult) => {
          this.parseEk(res2.out);
        });
      }
      this.liveBusy = false;
    });
  }

  private selectRow(r: PacketRow): void {
    this.selectedNo = r.no;
    const rec: DetailRec | undefined = this.detailCache[r.no.toString()];
    if (rec !== undefined) {
      this.details = rec.layersText;
      this.hexStr = rec.rawHex;
    } else {
      // Cache miss (frame decoded before details pass): fetch just this frame.
      this.details = 'fetching frame ' + r.no.toString() + '…';
      this.hexStr = '';
      ohsharkapi.runTshark(['-r', this.currentFile, '-n', '-T', 'ek', '-x',
        '-Y', 'frame.number == ' + r.no.toString()]).then((res: SharkResult) => {
        this.parseEk(res.out);
        const now: DetailRec | undefined = this.detailCache[r.no.toString()];
        if (now !== undefined) {
          this.details = now.layersText;
          this.hexStr = now.rawHex;
        }
      });
    }
  }

  private applyFilter(f: string): void {
    this.filter = f;
    this.decode(this.currentFile.length > 0 ? this.currentFile : this.sampleFile, f);
  }

  private chipClick(c: string): void {
    this.applyFilter(c === 'all' ? '' : c);
  }

  private toggleLive(): void {
    if (this.liveLabel === 'Live ■') {
      this.liveLabel = 'Live';
      this.status = 'Live tail stopped at frame ' + this.lastLiveFrame.toString();
      return;
    }
    try {
      if (!fileIo.accessSync(this.liveFile)) {
        this.status = 'No live.pcap yet — start a privileged capture from the PC with: ' +
          'hdc shell "cd /data/local/tmp/ohshark; LD_LIBRARY_PATH=/data/local/tmp/ohshark/lib ' +
          'nohup ./bin/dumpcap -i any -w /data/app/el2/100/base/com.ohshark.qt/files/live.pcap" ' +
          'then chmod 644 that file';
        return;
      }
    } catch (e) {
      this.status = 'live.pcap not accessible';
      return;
    }
    this.liveLabel = 'Live ■';
    this.status = 'Live tailing live.pcap…';
  }

  private cycleIface(): void {
    if (this.ifaces.length === 0) {
      return;
    }
    this.ifaceIdx++;
    if (this.ifaceIdx >= this.ifaces.length) {
      this.ifaceIdx = -1;
    }
    const name = this.ifaceIdx >= 0 ? this.ifaces[this.ifaceIdx] : undefined;
    this.ifaceLabel = name === undefined ? 'iface: any' : 'iface: ' + name;
  }

  private startCapture(): void {
    this.status = 'Live capture on ' + this.ifaceLabel +
      ' needs CAP_NET_RAW, which HarmonyOS denies normal apps — use Live (external privileged dumpcap) instead';
  }

  private cycleStats(): void {
    this.statsIdx = (this.statsIdx + 1) % STATS_MODES.length;
    const mode = STATS_MODES[this.statsIdx];
    this.statsLabel = 'Stats: ' + mode;
    this.status = 'Computing ' + mode + '…';
    ohsharkapi.runTshark(['-r', this.currentFile.length > 0 ? this.currentFile : this.sampleFile,
      '-q', '-z', mode]).then((res: SharkResult) => {
      this.details = res.out;
      this.hexStr = '';
      this.status = 'Statistics ' + mode + ' (tshark -z) rc=' + res.rc.toString();
    });
  }

  private followStream(): void {
    if (this.selectedNo < 0) {
      this.status = 'Select a TCP packet first';
      return;
    }
    ohsharkapi.runTshark(['-r', this.currentFile, '-T', 'fields', '-e', 'tcp.stream',
      '-Y', 'frame.number == ' + this.selectedNo.toString()]).then((res: SharkResult) => {
      const stream = res.out.trim();
      if (stream.length === 0) {
        this.status = 'Selected packet is not TCP';
        return;
      }
      this.status = 'Following tcp stream ' + stream + '…';
      ohsharkapi.runTshark(['-r', this.currentFile, '-q',
        '-z', 'follow,tcp,ascii,' + stream]).then((res2: SharkResult) => {
        this.details = res2.out;
        this.hexStr = '';
        this.status = 'Followed tcp stream ' + stream;
      });
    });
  }

  private openNext(): void {
    const names: string[] = [];
    try {
      const list = fileIo.listFileSync(this.filesDir);
      for (const f of list) {
        if (f.endsWith('.pcap') || f.endsWith('.pcapng')) {
          names.push(this.filesDir + '/' + f);
        }
      }
    } catch (e) {
      this.status = 'cannot list sandbox dir';
      return;
    }
    if (names.length === 0) {
      this.status = 'No .pcap in sandbox yet — push one with: hdc file send <file> ' +
        '/data/app/el2/100/base/com.ohshark.qt/files/';
      return;
    }
    this.openIdx = (this.openIdx + 1) % names.length;
    const f = names[this.openIdx];
    if (f === undefined) {
      return;
    }
    const base = f.substring(f.lastIndexOf('/') + 1);
    this.openLabel = 'Open: ' + base;
    this.decode(f, this.filter);
  }

  build() {
    Column() {
      // Toolbar
      Row({ space: 6 }) {
        Button(this.ifaceLabel).fontSize(14).height(36)
          .onClick(() => this.cycleIface())
        Button('▶ Start').fontSize(14).height(36)
          .onClick(() => this.startCapture())
        Button(this.liveLabel).fontSize(14).height(36)
          .onClick(() => this.toggleLive())
        Button(this.openLabel).fontSize(14).height(36)
          .onClick(() => this.openNext())
        Button('Reload').fontSize(14).height(36)
          .onClick(() => this.applyFilter(this.filter))
        Button(this.statsLabel).fontSize(14).height(36)
          .onClick(() => this.cycleStats())
        Button('Follow').fontSize(14).height(36)
          .onClick(() => this.followStream())
        TextInput({ placeholder: 'display filter, e.g. icmp || arp', text: this.filter })
          .fontSize(14).height(36).layoutWeight(1)
          .onChange((v: string) => {
            this.filter = v;
          })
          .onSubmit(() => this.applyFilter(this.filter))
      }
      .width('100%').padding(6)

      // Quick filter chips
      Row({ space: 6 }) {
        Text('Quick filter:').fontSize(13).padding({ left: 6 })
        ForEach(CHIPS, (c: string) => {
          Button(c).fontSize(13).height(30)
            .backgroundColor(c === 'all' && this.filter.length === 0 ? '#8fce6f' :
              (c === this.filter ? '#8fce6f' : '#e4e4e4'))
            .fontColor('#111')
            .onClick(() => this.chipClick(c))
        }, (c: string) => c)
      }
      .width('100%').padding({ bottom: 4 })

      // Column header
      Text(this.listTitle).fontSize(12).fontFamily('monospace')
        .width('100%').padding({ left: 8, right: 8, top: 3, bottom: 3 })
        .backgroundColor('#e8e8e8').fontColor('#333')

      // Packet list
      List({ space: 0 }) {
        ForEach(this.rows, (r: PacketRow) => {
          ListItem() {
            Row() {
              Text(r.no.toString()).fontSize(12).fontFamily('monospace').width(46)
              Text(r.time).fontSize(12).fontFamily('monospace').width(72)
              Text(r.src).fontSize(12).fontFamily('monospace').width(150)
              Text(r.dst).fontSize(12).fontFamily('monospace').width(150)
              Text(r.proto).fontSize(12).fontFamily('monospace').width(70)
              Text(r.len).fontSize(12).fontFamily('monospace').width(50)
              Text(r.info).fontSize(12).fontFamily('monospace').layoutWeight(1)
                .maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis })
            }
            .width('100%')
            .backgroundColor(r.no === this.selectedNo ? '#a8d4ff' : r.color)
            .padding({ left: 8, right: 8, top: 2, bottom: 2 })
            .onClick(() => this.selectRow(r))
          }
        }, (r: PacketRow) => r.no.toString())
      }
      .layoutWeight(5).width('100%').backgroundColor('#ffffff')
      .scrollBar(BarState.Auto)
      .divider({ strokeWidth: 1, color: '#f0f0f0', startMargin: 8, endMargin: 8 })

      // Details pane
      Column() {
        Text('Packet Details').fontSize(12).fontColor('#333')
          .padding({ left: 8, top: 4 }).width('100%').backgroundColor('#e8e8e8')
        Scroll() {
          Text(this.details.length > 0 ? this.details : '(select a packet)')
            .fontSize(12).fontFamily('monospace').fontColor('#111')
            .textAlign(TextAlign.Start).width('100%').padding(6)
        }
        .scrollBar(BarState.Auto)
        .align(Alignment.TopStart)
        .layoutWeight(1).width('100%')
      }
      .layoutWeight(3).width('100%').backgroundColor('#ffffff')

      // Bytes pane
      Column() {
        Text('Packet Bytes').fontSize(12).fontColor('#333')
          .padding({ left: 8, top: 4 }).width('100%').backgroundColor('#e8e8e8')
        Scroll() {
          Text(this.hexStr.length > 0 ? this.hexStr : '')
            .fontSize(12).fontFamily('monospace').fontColor('#111')
            .width('100%').padding(6)
        }
        .scrollBar(BarState.Auto)
        .align(Alignment.TopStart)
        .layoutWeight(1).width('100%')
      }
      .layoutWeight(2).width('100%').backgroundColor('#ffffff')

      // Status bar
      Column() {
        Text(this.status).fontSize(12).fontColor('#222').width('100%').padding({ left: 8 })
        Text(this.engineLine + '  |  packets: ' + this.rows.length.toString())
          .fontSize(12).fontColor('#555').width('100%').padding({ left: 8, bottom: 3 })
      }
      .width('100%').backgroundColor('#e8e8e8')
    }
    .width('100%').height('100%').backgroundColor('#f0f0f0')
  }
}
