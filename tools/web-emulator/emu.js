// NullMenu v3 を WebAssembly で動かす小さな「本体」
// ・2つのコアは、asyncify で1本の流れのまま交互に動かす
// ・再起動は、WebAssembly を作り直して(メモリもまっさら)RTC の値だけ引き継ぐ
class NullMenuEmu {
  constructor(bytes, hooks) {
    this.module = (bytes instanceof WebAssembly.Module) ? bytes : new WebAssembly.Module(bytes);
    this.hooks = hooks || {};
    this.w = 468; this.h = 468;
    this.imu = true; this.mic = false;
    this.rtc = [0, 0xFFFFFFFF, 0];
    this.clock = null;                  // テスト用: 決まった時計(ミリ秒を返す関数)
    this.restartPending = false;
    this.bright = 190;
    this.logBuf = '';
  }
  now() { return this.clock ? this.clock() : performance.now(); }

  boot(reason) {                        // 電源を入れる / 再起動
    const self = this;
    this.ctx = 'main';
    this.pending = null;
    this.rewinding = false;
    this.suspended = { main: false, worker: false };
    this.workerStarted = false;
    this.mainEntry = 'web_setup';
    this.setupDone = false;
    this.restartPending = false;
    const env = {
      js_now: () => self.now() - self.t0,
      js_bright: (v) => { self.bright = v; self.hooks.bright && self.hooks.bright(v); },
      js_yield: () => self._suspend('switch'),
      js_frame: () => self._suspend('frame'),
      js_restart: () => { self._saveRtc(); self.restartPending = true; throw new RestartSignal(); },
    };
    const wasi = {
      fd_write: (fd, iovs, n, nwritten) => {
        const dv = new DataView(self.mem.buffer); let total = 0, s = '';
        for (let i = 0; i < n; ++i) {
          const p = dv.getUint32(iovs + i * 8, true), l = dv.getUint32(iovs + i * 8 + 4, true);
          s += new TextDecoder().decode(new Uint8Array(self.mem.buffer, p, l)); total += l;
        }
        dv.setUint32(nwritten, total, true);
        self.logBuf += s;
        let k; while ((k = self.logBuf.indexOf('\n')) >= 0) { const line = self.logBuf.slice(0, k); self.logBuf = self.logBuf.slice(k + 1); self.hooks.log && self.hooks.log(line); }
        return 0;
      },
      fd_close: () => 0, fd_seek: () => 0, fd_fdstat_get: () => 0,
    };
    this.inst = new WebAssembly.Instance(this.module, { env, wasi_snapshot_preview1: wasi });
    this.ex = this.inst.exports;
    this.mem = this.ex.memory;
    this.t0 = this.now();                 // 起動からの時間は 0 から
    this.ex._initialize();
    this.ex.web_boot(reason, this.w, this.h, this.imu ? 1 : 0, this.mic ? 1 : 0, (Math.random() * 4294967295) >>> 0);
    for (let i = 0; i < 3; ++i) this.ex.web_rtc_set(i, this.rtc[i]);
    // asyncify の記録場所と、2つ目のコアのスタック
    const BUF = 256 * 1024;
    this.buf = {};
    for (const c of ['main', 'worker']) {
      const p = this.ex.web_alloc(BUF + 8);
      const dv = new DataView(this.mem.buffer);
      dv.setUint32(p, p + 8, true); dv.setUint32(p + 4, p + 8 + BUF, true);
      this.buf[c] = p;
    }
    const WSTACK = 512 * 1024;
    this.sp = { main: this.ex.__stack_pointer.value, worker: this.ex.web_alloc(WSTACK) + WSTACK - 16 };
    this.spBase = { main: this.sp.main, worker: this.sp.worker };
  }
  _saveRtc() { for (let i = 0; i < 3; ++i) this.rtc[i] = this.ex.web_rtc_get(i) >>> 0; }

  _suspend(kind) {
    if (this.rewinding) { this.ex.asyncify_stop_rewind(); this.rewinding = false; return; }
    if (kind === 'switch' && this.ctx === 'main' && !this.ex.nm_has_task()) return;
    this.sp[this.ctx] = this.ex.__stack_pointer.value;
    const dv = new DataView(this.mem.buffer); const p = this.buf[this.ctx];
    dv.setUint32(p, p + 8, true);
    this.ex.asyncify_start_unwind(p);
    this.pending = kind;
  }
  _run(ctx) {                           // ctx を動かす(続きからなら巻き戻して)
    this.ctx = ctx;
    if (this.suspended[ctx]) {
      this.ex.__stack_pointer.value = this.sp[ctx];
      this.ex.asyncify_start_rewind(this.buf[ctx]);
      this.rewinding = true;
    } else {
      this.ex.__stack_pointer.value = this.spBase[ctx];
    }
    if (ctx === 'main') this.ex[this.mainEntry](); else this.ex.nm_task_entry();
    if (this.pending) {
      this.ex.asyncify_stop_unwind();
      const k = this.pending; this.pending = null;
      this.suspended[ctx] = true;
      return k;
    }
    this.suspended[ctx] = false;
    return 'done';
  }
  // 1フレームぶん動かす(setup か loop を1回)。再起動したら true
  frame() {
    if (this.restartPending) return true;
    try {
      let r = this._run('main');
      let guard = 0;
      while (r === 'switch') {
        this._run('worker');
        r = this._run('main');
        if (++guard > 1e7) throw new Error('deadlock');
      }
      if (r === 'done') {
        if (this.mainEntry === 'web_setup') this.setupDone = true;
        this.mainEntry = 'web_loop';
      }
    } catch (e) {
      if (e instanceof RestartSignal) return true;
      throw e;
    }
    return false;
  }
  framebuffer() { return new Uint16Array(this.mem.buffer, this.ex.web_fb(), this.ex.web_w() * this.ex.web_h()); }
  touch(pr, x, y) { this.ex.web_touch(pr ? 1 : 0, x | 0, y | 0); }
  button(id, down) { this.ex.web_btn(id, down ? 1 : 0); }     // 0=A 1=B 2=電源
  accel(x, y, z) { this.ex.web_accel(x, y, z); }
  running() { return this.ex.web_running(); }
  micPush(samples) {                    // Int16Array, 16kHz
    const ring = new Int16Array(this.mem.buffer, this.ex.web_mic_ring(), 1 << 15);
    this.micW = (this.micW || 0);
    for (let i = 0; i < samples.length; ++i) ring[(this.micW + i) & 32767] = samples[i];
    this.micW = (this.micW + samples.length) >>> 0;
    this.ex.web_mic_set_w(this.micW);
  }
}
class RestartSignal extends Error { constructor() { super('restart'); } }
if (typeof module !== 'undefined') module.exports = { NullMenuEmu, RestartSignal };
