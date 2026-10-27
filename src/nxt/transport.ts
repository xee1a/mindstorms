export const LEGO_VENDOR_ID = 0x0694;
export const NXT_PRODUCT_ID = 0x0002;

/** A byte-packet link to a brick. Implementations need not be re-entrant; NxtBrick serialises calls. */
export interface Transport {
  readonly kind: 'usb' | 'demo';
  readonly label: string;
  write(packet: Uint8Array): Promise<void>;
  read(timeoutMs: number): Promise<Uint8Array>;
  close(): Promise<void>;
  /** Called once if the link drops (cable pulled, brick switched off). */
  onDisconnect?: () => void;
}

export class TimeoutError extends Error {
  constructor() {
    super('The NXT did not respond in time');
  }
}

export class WebUsbTransport implements Transport {
  readonly kind = 'usb';
  onDisconnect?: () => void;
  private outEp = 1;
  private inEp = 2;
  private pendingIn: Promise<USBInTransferResult> | null = null;
  private readonly handleDisconnect = (ev: USBConnectionEvent) => {
    if (ev.device === this.device) {
      navigator.usb?.removeEventListener('disconnect', this.handleDisconnect);
      this.onDisconnect?.();
    }
  };

  private constructor(private readonly device: USBDevice) {}

  get label() {
    return this.device.productName || 'NXT (USB)';
  }

  static isSupported(): boolean {
    return typeof navigator !== 'undefined' && !!navigator.usb;
  }

  /** Reconnects to a previously permitted brick without user interaction, if one is plugged in. */
  static async findPermitted(): Promise<USBDevice | null> {
    if (!navigator.usb) return null;
    const devices = await navigator.usb.getDevices();
    return devices.find((d) => d.vendorId === LEGO_VENDOR_ID && d.productId === NXT_PRODUCT_ID) ?? null;
  }

  static async request(): Promise<USBDevice> {
    if (!navigator.usb) throw new Error('WebUSB is not available in this environment');
    return navigator.usb.requestDevice({ filters: [{ vendorId: LEGO_VENDOR_ID, productId: NXT_PRODUCT_ID }] });
  }

  static async open(device: USBDevice): Promise<WebUsbTransport> {
    const t = new WebUsbTransport(device);
    if (!device.opened) await device.open();
    if (device.configuration === null) await device.selectConfiguration(1);
    await device.claimInterface(0);
    const iface = device.configuration?.interfaces.find((i) => i.interfaceNumber === 0);
    for (const ep of iface?.alternate.endpoints ?? []) {
      if (ep.type !== 'bulk') continue;
      if (ep.direction === 'out') t.outEp = ep.endpointNumber;
      else t.inEp = ep.endpointNumber;
    }
    navigator.usb?.addEventListener('disconnect', t.handleDisconnect);
    return t;
  }

  async write(packet: Uint8Array): Promise<void> {
    const res = await this.device.transferOut(this.outEp, packet as Uint8Array<ArrayBuffer>);
    if (res.status === 'stall') {
      await this.device.clearHalt('out', this.outEp);
      throw new Error('USB endpoint stalled');
    }
  }

  async read(timeoutMs: number): Promise<Uint8Array> {
    // WebUSB transfers can't be cancelled. If one times out we keep it and hand it to the
    // next read, which discards it when its opcode doesn't match (see NxtBrick.request).
    const pending = this.pendingIn ?? this.device.transferIn(this.inEp, 64);
    this.pendingIn = pending;
    let timer: ReturnType<typeof setTimeout> | undefined;
    const timeout = new Promise<never>((_, reject) => {
      timer = setTimeout(() => reject(new TimeoutError()), timeoutMs);
    });
    try {
      const res = await Promise.race([pending, timeout]);
      this.pendingIn = null;
      if (res.status !== 'ok' || !res.data) {
        if (res.status === 'stall') await this.device.clearHalt('in', this.inEp);
        throw new Error(`USB read failed (${res.status})`);
      }
      return new Uint8Array(res.data.buffer, res.data.byteOffset, res.data.byteLength);
    } finally {
      clearTimeout(timer);
    }
  }

  async close(): Promise<void> {
    navigator.usb?.removeEventListener('disconnect', this.handleDisconnect);
    try {
      await this.device.releaseInterface(0);
    } catch {
      /* already gone */
    }
    try {
      await this.device.close();
    } catch {
      /* already gone */
    }
  }
}
