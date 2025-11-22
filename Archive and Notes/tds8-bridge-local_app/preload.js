const { contextBridge } = require('electron');

contextBridge.exposeInMainWorld('tds8', {
  platform: process.platform,
});
