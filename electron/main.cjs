const { app, BrowserWindow, session, nativeTheme } = require('electron');
const path = require('node:path');

const LEGO_VENDOR_ID = 0x0694;
const NXT_PRODUCT_ID = 0x0002;

const isLego = (device) => device && device.vendorId === LEGO_VENDOR_ID;

function setUpUsbPermissions(ses) {
  // navigator.usb.requestDevice() has no picker in Electron, so choose the NXT ourselves.
  ses.on('select-usb-device', (event, details, callback) => {
    event.preventDefault();
    const nxt = details.deviceList.find(
      (d) => d.vendorId === LEGO_VENDOR_ID && d.productId === NXT_PRODUCT_ID,
    );
    callback(nxt ? nxt.deviceId : '');
  });

  ses.setPermissionCheckHandler((_wc, permission) =>
    ['usb', 'clipboard-sanitized-write'].includes(permission),
  );

  // Lets navigator.usb.getDevices() return the brick on later launches without asking again.
  ses.setDevicePermissionHandler((details) => details.deviceType === 'usb' && isLego(details.device));
  ses.setUSBProtectedClassesHandler((details) => details.protectedClasses);
}

function createWindow() {
  nativeTheme.themeSource = 'dark';
  const win = new BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 1024,
    minHeight: 680,
    backgroundColor: '#0d1117',
    title: 'NXT Studio',
    titleBarStyle: 'hidden',
    titleBarOverlay: { color: '#00000000', symbolColor: '#c9d1d9', height: 44 },
    show: false,
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      // Keep gamepad polling and motor commands running at full rate.
      backgroundThrottling: false,
    },
  });
  win.once('ready-to-show', () => win.show());

  const devUrl = process.env.VITE_DEV_SERVER_URL;
  if (devUrl) win.loadURL(devUrl);
  else win.loadFile(path.join(__dirname, '..', 'dist', 'index.html'));
}

app.whenReady().then(() => {
  setUpUsbPermissions(session.defaultSession);
  createWindow();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
