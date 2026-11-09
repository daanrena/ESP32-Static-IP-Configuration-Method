#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <esp_task_wdt.h>  // Hardware Watchdog Timer

#define WDT_TIMEOUT 8  // Watchdog reset jika macet > 8 detik

Preferences preferences;

const char* ssid = YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

const int relayPin = 25;
const int relayPin1 = 27;

WebServer server(80);

IPAddress local_ip(192, 168, 6, 150);    
IPAddress gateway(192, 168, 6, 1);       
IPAddress subnet(255, 255, 255, 0);  

bool shouldRestart = false;
unsigned long restartTimer = 0;

// Variabel untuk pengecekan Wi-Fi berkala
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 10000; // Cek setiap 10 detik

// Fungsi helper untuk mengubah status relay & simpan ke flash
void setRelayState(bool state) {
  // state: true = ON (LOW), false = OFF (HIGH)
  int pinVal = state ? LOW : HIGH;
  digitalWrite(relayPin, pinVal);
  digitalWrite(relayPin1, pinVal);

  preferences.begin("state", false);
  preferences.putBool("relay", state);
  preferences.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // 1. Ambil status terakhir relay sebelum listrik padam
  preferences.begin("state", true);
  bool lastRelayState = preferences.getBool("relay", false); // Default OFF
  preferences.end();

  // Terapkan status terakhir
  digitalWrite(relayPin, lastRelayState ? LOW : HIGH);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin1, lastRelayState ? LOW : HIGH);
  pinMode(relayPin1, OUTPUT);

  // 2. Baca Konfigurasi IP
  preferences.begin("settings", true);
  String storedIP = preferences.getString("local_ip", "");
  String storedGW = preferences.getString("gateway", "");
  String storedSN = preferences.getString("subnet", "");
  preferences.end();

  IPAddress tempIP, tempGW, tempSN;
  if (tempIP.fromString(storedIP) && tempGW.fromString(storedGW) && tempSN.fromString(storedSN)) {
    local_ip = tempIP;
    gateway = tempGW;
    subnet = tempSN;
  }

  // 3. Setup Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true); // Fitur bawaan ESP32
  WiFi.config(local_ip, gateway, subnet);
  WiFi.begin(ssid, password); 

  Serial.print("Menghubungkan ke WiFi");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Terhubung! IP: " + WiFi.localIP().toString());
  }

  // 4. mDNS
  MDNS.begin("esp32");

  // 5. Routing Web Server
  server.on("/", HTTP_GET, []() {
    preferences.begin("state", true);
    bool isRelayOn = preferences.getBool("relay", false);
    preferences.end();

    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;} button{padding:12px 24px;margin:8px;font-size:16px;border-radius:5px;cursor:pointer;}</style></head><body>";
    html += "<h1>Kontrol Lampu 24/6</h1>";
    html += "<p>Status Saat Ini: <b>" + String(isRelayOn ? "<span style='color:green;'>MENYALA (ON)</span>" : "<span style='color:red;'>MATI (OFF)</span>") + "</b></p>";
    html += "<a href=\"/relay/on\"><button style=\"background:#4CAF50;color:white;border:none;\">Nyalakan</button></a>";
    html += "<a href=\"/relay/off\"><button style=\"background:#f44336;color:white;border:none;\">Matikan</button></a><br><br>";
    html += "<hr><h3>Pengaturan Jaringan</h3>";
    html += "<form action=\"/set_ip\" method=\"GET\">";
    html += "IP: <input type=\"text\" name=\"ip\" value=\"" + local_ip.toString() + "\"><br><br>";
    html += "GW: <input type=\"text\" name=\"gateway\" value=\"" + gateway.toString() + "\"><br><br>";
    html += "Subnet: <input type=\"text\" name=\"subnet\" value=\"" + subnet.toString() + "\"><br><br>";
    html += "<input type=\"submit\" value=\"Simpan & Restart\">";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/relay/on", HTTP_GET, []() {
    setRelayState(true);
    server.send(200, "text/html", "<h3>Lampu ON</h3><meta http-equiv=\"refresh\" content=\"1;url=/\">");
  });

  server.on("/relay/off", HTTP_GET, []() {
    setRelayState(false);
    server.send(200, "text/html", "<h3>Lampu OFF</h3><meta http-equiv=\"refresh\" content=\"1;url=/\">");
  });

  server.on("/set_ip", HTTP_GET, []() {
    if (!server.hasArg("ip") || !server.hasArg("gateway") || !server.hasArg("subnet")) {
      server.send(400, "text/html", "<h3>Form tidak lengkap</h3>");
      return;
    }
    String ip_str = server.arg("ip");
    String gw_str = server.arg("gateway");
    String sn_str = server.arg("subnet");

    IPAddress nIP, nGW, nSN;
    if (!nIP.fromString(ip_str) || !nGW.fromString(gw_str) || !nSN.fromString(sn_str)) {
      server.send(400, "text/html", "<h3>Format IP Salah</h3>");
      return;
    }

    preferences.begin("settings", false);
    preferences.putString("local_ip", ip_str);
    preferences.putString("gateway", gw_str);
    preferences.putString("subnet", sn_str);
    preferences.end();

    String html = "<h3>IP Berhasil Diganti ke " + ip_str + ". Restarting...</h3>";
    html += "<meta http-equiv=\"refresh\" content=\"5;url=http://" + ip_str + "/\">";
    server.send(200, "text/html", html);

    shouldRestart = true;
    restartTimer = millis();
  });

  server.begin();

  // 6. Aktifkan Hardware Watchdog Timer
  esp_task_wdt_init(WDT_TIMEOUT, true); // true = restart sistem saat timeout
  esp_task_wdt_add(NULL);               // Daftarkan loop task ke watchdog
  Serial.println("Sistem & Watchdog Siap.");
}

void loop() {
  // Reset timer watchdog (tanda bahwa sistem berjalan normal)
  esp_task_wdt_reset();

  server.handleClient();

  // Auto-reconnect Wi-Fi jika terputus di tengah jalan
  if (millis() - lastWiFiCheck >= wifiCheckInterval) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi terputus! Mencoba menyambung kembali...");
      WiFi.reconnect();
    }
  }

  // Handle restart tertunda
  if (shouldRestart && (millis() - restartTimer >= 1500)) {
    ESP.restart();
  }
}
