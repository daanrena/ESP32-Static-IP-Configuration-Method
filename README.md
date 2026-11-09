# ESP32-Static-IP-Configuration-Method
A step-by-step guide to configuring a static IP address on an ESP32 using Wi-Fi. This method ensures the ESP32 consistently uses a predefined IP address instead of obtaining one dynamically through DHCP, making network management and device communication more reliable.

# ESP32 Static IP Config via Web & NVS

Setting a static IP on an ESP32 is usually done by hardcoding it in the sketch. But if you move to another network or router, you have to plug in the cable and re-flash the board.

This project solves that. You can change the ESP32's static IP directly from a web browser without re-uploading any code. The configuration is saved into the ESP32's flash memory (`Preferences`), so it stays saved even after a reboot or power outage.

## How It Works

1. **Check Stored Settings**: When the ESP32 boots up, it checks internal flash memory (`Preferences`). If you've saved a custom IP before, it uses that. If not, it falls back to the default IP (`192.168.6.150`).
2. **Connect to Wi-Fi**: Connects to the router using the static IP settings.
3. **Open Web Interface**: Open your browser and go to the IP address or `http://esp32.local`.
4. **Change IP via Web Form**: Enter your new IP, Gateway, and Subnet Mask in the form.
5. **Save & Auto-Restart**: When you submit, the ESP32 checks if the IP format is valid, saves it to flash memory, and restarts automatically.
6. **Done**: The ESP32 reboots and connects using the new static IP.

## Extra Features in This Code
- **Power-Loss Recovery**: Remembers relay status (ON/OFF) before power cuts, so it won't reset to OFF when electricity comes back.
- **Hardware Watchdog (8s)**: Automatically restarts the ESP32 if the code ever freezes.
- **Auto Reconnect**: Checks Wi-Fi connection every 10 seconds and reconnects automatically if dropped.


