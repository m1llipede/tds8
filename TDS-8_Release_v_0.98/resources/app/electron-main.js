// TDS-8 Electron Main Process
// Wraps the bridge in a native desktop app

const { app, BrowserWindow, Tray, Menu, ipcMain, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const { spawn, exec } = require('child_process');

let mainWindow = null;
let tray = null;
let bridgeProcess = null;
const BRIDGE_PORT = 8088;

// Ensure single-instance behavior: additional launches just focus the existing window
const gotLock = app.requestSingleInstanceLock();
if (!gotLock) {
  app.quit();
} else {
  app.on('second-instance', () => {
    if (mainWindow) {
      if (mainWindow.isMinimized()) mainWindow.restore();
      mainWindow.show();
      mainWindow.focus();
    }
  });
}

// Helper for delays
const wait = (ms) => new Promise(resolve => setTimeout(resolve, ms));

// Kill other instances and clear port 8088
async function cleanStartup() {
  const myPid = process.pid;
  console.log(`[Startup] My PID: ${myPid}. Cleaning up other instances...`);

  return new Promise(async (resolve) => {
    try {
      if (process.platform === 'win32') {
        // Kill other Bridge instances by executable name
        const exeName = path.basename(process.execPath || 'TDS-8 Bridge.exe');
        const candidates = Array.from(new Set([
          exeName,
          'TDS-8 Bridge.exe',   // common packaged name
          'TDS-8-Bridge.exe'    // legacy/alternate name
        ]));

        for (const name of candidates) {
          const killAppCmd = `taskkill /F /IM "${name}" /FI "PID ne ${myPid}"`;
          await new Promise(resolve => exec(killAppCmd, () => {
            console.log(`[Startup] Attempted to kill other Bridge instances named ${name}.`);
            resolve();
          }));
        }
        
        await wait(500);
        
        // Kill any process using port 8088
        await new Promise(resolve => {
          exec(`netstat -aon | findstr :${BRIDGE_PORT}`, (err, stdout) => {
            if (stdout && stdout.trim()) {
              const lines = stdout.split('\n');
              lines.forEach(line => {
                const parts = line.trim().split(/\s+/);
                if (parts.length >= 5) {
                  const pid = parts[4];
                  if (parseInt(pid) !== myPid && parseInt(pid) > 0) {
                    console.log(`[Startup] Killing PID ${pid} on port ${BRIDGE_PORT}`);
                    exec(`taskkill /F /PID ${pid}`, () => {});
                  }
                }
              });
            }
            resolve();
          });
        });
      } else {
        // Kill by process name (macOS/Linux)
        const killCmd = `pkill -f "TDS-8-Bridge" || true`;
        await new Promise(resolve => exec(killCmd, () => {
          console.log('[Startup] Killed other Bridge instances by name.');
          resolve();
        }));
        
        await wait(500);
        
        // Kill any process using port 8088
        await new Promise(resolve => {
          exec(`lsof -ti tcp:${BRIDGE_PORT}`, (err, stdout) => {
            if (stdout && stdout.trim()) {
              stdout.trim().split(/\s+/).forEach(pidStr => {
                const n = parseInt(pidStr, 10);
                if (n && n !== myPid) {
                  console.log(`[Startup] Killing PID ${n} on port ${BRIDGE_PORT}`);
                  exec(`kill -9 ${n}`, () => {});
                }
              });
            }
            resolve();
          });
        });
      }

      await wait(2000); // Wait 2 seconds for OS to release ports/handles
      console.log('[Startup] Cleanup complete.');
      resolve();
    } catch (e) {
      console.error('[Startup] Cleanup error:', e);
      resolve(); // Proceed anyway
    }
  });
}

// Start the bridge server
function startBridge() {
  console.log('Starting TDS-8 bridge server...');
  
  bridgeProcess = spawn('node', [path.join(__dirname, 'index.js')], {
    env: { ...process.env, PORT: BRIDGE_PORT },
    stdio: 'ignore',    // Don't inherit console
    windowsHide: true   // Hide the terminal window on Windows
  });

  bridgeProcess.on('error', (err) => {
    console.error('Bridge process error:', err);
  });

  bridgeProcess.on('exit', (code) => {
    console.log(`Bridge process exited with code ${code}`);
    if (code !== 0 && !app.isQuitting) {
      // Restart if crashed
      setTimeout(startBridge, 2000);
    }
  });
}

// Create the main window
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 1000,
    minWidth: 1200,
    minHeight: 900,
    title: 'TDS-8 Bridge',
    icon: (fs.existsSync(path.join(__dirname, 'assets', 'icon-white.png'))
      ? path.join(__dirname, 'assets', 'icon-white.png')
      : undefined),
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'electron-preload.js')
    },
    backgroundColor: '#f6f7fb',
    titleBarStyle: 'default', // Standard title bar with drag handle
    show: false // Show after ready
  });

  // Wait for bridge to start, then load with retry logic
  let loadAttempts = 0;
  const maxAttempts = 15;
  
  const tryLoadURL = () => {
    loadAttempts++;
    mainWindow.loadURL(`http://localhost:${BRIDGE_PORT}`).catch(err => {
      console.log(`Load attempt ${loadAttempts}/${maxAttempts} failed:`, err.message);
      if (loadAttempts < maxAttempts) {
        console.log('Retrying in 1 second...');
        setTimeout(tryLoadURL, 1000);
      } else {
        console.error('Failed to load after maximum attempts');
      }
    });
  };
  
  // Start first attempt after 5 seconds (giving Node time to fully boot and initialize)
  setTimeout(tryLoadURL, 5000);

  mainWindow.once('ready-to-show', () => {
    mainWindow.show();
  });

  mainWindow.on('closed', () => {
    mainWindow = null;
  });

  // Open external links in browser
  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: 'deny' };
  });

  // Handle minimize to tray
  mainWindow.on('minimize', (event) => {
    if (process.platform === 'darwin') {
      // Mac: minimize normally
      return;
    }
    // Windows/Linux: minimize to tray
    event.preventDefault();
    mainWindow.hide();
  });

  mainWindow.on('close', (event) => {
    // on Windows/Linux, strictly QUIT when the user closes the window.
    // This prevents background zombies and port conflicts.
    if (process.platform !== 'darwin') {
       app.quit();
    }
    // Legacy tray logic (disabled for stability):
    // if (!app.isQuitting && process.platform !== 'darwin') {
    //   event.preventDefault();
    //   mainWindow.hide();
    // }
  });
}

// Create system tray
function createTray() {
  // Use a simple icon (you can replace with custom icon)
  const iconPath = path.join(__dirname, 'assets', 'icon-white.png');
  if (!fs.existsSync(iconPath)) {
    console.warn('[Tray] Icon not found, skipping tray creation.');
    return;
  }

  tray = new Tray(iconPath);

  const contextMenu = Menu.buildFromTemplate([
    {
      label: 'Show TDS-8',
      click: () => {
        if (mainWindow) {
          mainWindow.show();
          mainWindow.focus();
        } else {
          createWindow();
        }
      }
    },
    { type: 'separator' },
    {
      label: 'Open in Browser',
      click: () => {
        shell.openExternal(`http://localhost:${BRIDGE_PORT}`);
      }
    },
    { type: 'separator' },
    {
      label: 'Quit TDS-8',
      click: () => {
        app.isQuitting = true;
        app.quit();
      }
    }
  ]);

  tray.setToolTip('TDS-8 Bridge');
  tray.setContextMenu(contextMenu);

  // Double-click to show window
  tray.on('double-click', () => {
    if (mainWindow) {
      mainWindow.show();
      mainWindow.focus();
    } else {
      createWindow();
    }
  });
}

// App lifecycle
app.on('ready', async () => {
  await cleanStartup();
  startBridge();
  createWindow();
  createTray();
});

app.on('window-all-closed', () => {
  // Keep app running on Mac
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (mainWindow === null) {
    createWindow();
  }
});

app.on('before-quit', () => {
  app.isQuitting = true;
});

app.on('will-quit', () => {
  // Stop bridge process immediately
  if (bridgeProcess) {
    console.log('Force-killing bridge process...');
    bridgeProcess.kill('SIGKILL'); // Mercilessly kill the backend
  }
});

// IPC handlers
ipcMain.handle('get-version', () => {
  return app.getVersion();
});

ipcMain.handle('minimize-to-tray', () => {
  if (mainWindow) {
    mainWindow.hide();
  }
});
