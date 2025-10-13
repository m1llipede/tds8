\# TDS-8 Bridge (USB Serial + Local Dashboard)



A tiny desktop app that:

\- Talks to your \*\*ESP32-C3\*\* over \*\*USB serial\*\*

\- Serves a local dashboard at \*\*http://localhost:8088\*\*

\- Sends the same commands you used over Wi-Fi (now via serial)

\- Checks a GitHub \*\*manifest.json\*\* and lets you \*\*update firmware\*\*

&nbsp; - Over Wi-Fi (device downloads URL itself), or

&nbsp; - Via USB (calls `esptool.py` if installed)



\## 1) Install \& run



```bash

\# in the td8-bridge folder

npm install

npm start



