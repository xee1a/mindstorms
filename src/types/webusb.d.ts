// Minimal WebUSB typings (not yet part of TypeScript's DOM lib).

interface USBEndpoint {
  endpointNumber: number;
  direction: 'in' | 'out';
  type: 'bulk' | 'interrupt' | 'isochronous';
  packetSize: number;
}

interface USBAlternateInterface {
  endpoints: USBEndpoint[];
}

interface USBInterface {
  interfaceNumber: number;
  alternate: USBAlternateInterface;
}

interface USBConfiguration {
  configurationValue: number;
  interfaces: USBInterface[];
}

interface USBInTransferResult {
  data?: DataView;
  status: 'ok' | 'stall' | 'babble';
}

interface USBOutTransferResult {
  bytesWritten: number;
  status: 'ok' | 'stall';
}

interface USBDevice {
  readonly vendorId: number;
  readonly productId: number;
  readonly productName?: string;
  readonly serialNumber?: string;
  readonly opened: boolean;
  readonly configuration: USBConfiguration | null;
  open(): Promise<void>;
  close(): Promise<void>;
  reset(): Promise<void>;
  selectConfiguration(value: number): Promise<void>;
  claimInterface(n: number): Promise<void>;
  releaseInterface(n: number): Promise<void>;
  clearHalt(direction: 'in' | 'out', endpoint: number): Promise<void>;
  transferIn(endpoint: number, length: number): Promise<USBInTransferResult>;
  transferOut(endpoint: number, data: BufferSource): Promise<USBOutTransferResult>;
}

interface USBConnectionEvent extends Event {
  readonly device: USBDevice;
}

interface USB extends EventTarget {
  getDevices(): Promise<USBDevice[]>;
  requestDevice(options: { filters: { vendorId?: number; productId?: number }[] }): Promise<USBDevice>;
  addEventListener(type: 'connect' | 'disconnect', listener: (ev: USBConnectionEvent) => void): void;
  removeEventListener(type: 'connect' | 'disconnect', listener: (ev: USBConnectionEvent) => void): void;
}

interface Navigator {
  readonly usb?: USB;
}
