#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <math.h>

// ================= Wi-Fi =================
const char* WIFI_SSID = "xxxxxxxxxx";
const char* WIFI_PASSWORD = "xxxxxxx";

// ================= ESP32 Pins =================
#define GPS_RX_PIN 16       // NEO-6M TX -> ESP32 GPIO 16
#define GPS_TX_PIN 17       // Optional: ESP32 GPIO 17 -> NEO-6M RX

#define OLED_SDA 21
#define OLED_SCL 22

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C   // Change to 0x3D if needed

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
WebServer server(80);

unsigned long lastGPSByteTime = 0;
unsigned long lastOLEDUpdate = 0;
unsigned long lastWiFiRetry = 0;

// ================= GPS Status =================
bool gpsReceiving() {
  return lastGPSByteTime > 0 &&
         millis() - lastGPSByteTime < 2500;
}

// ================= Boot Animation =================
void runBootAnimation() {
  // Animated radar screen
  for (int frame = 0; frame < 18; frame++) {
    display.clearDisplay();

    display.drawCircle(64, 22, 17, SSD1306_WHITE);
    display.drawCircle(64, 22, 8, SSD1306_WHITE);
    display.drawFastHLine(47, 22, 34, SSD1306_WHITE);
    display.drawFastVLine(64, 5, 34, SSD1306_WHITE);

    float angle = frame * 0.42;
    int x = 64 + (int)(17 * cos(angle));
    int y = 22 + (int)(17 * sin(angle));
    display.drawLine(64, 22, x, y, SSD1306_WHITE);

    if (frame % 4 < 2) {
      display.fillCircle(72, 15, 2, SSD1306_WHITE);
    }

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, 47);
    display.println("GPS TELEMETRY SYSTEM");

    display.setCursor(43, 57);
    display.println("BY ATUL");

    display.display();
    delay(45);
  }

  // Fast progress loader
  for (int progress = 0; progress <= 100; progress += 10) {
    display.clearDisplay();
    display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(17, 10);
    display.println("GPS TELEMETRY SYSTEM");

    display.setTextSize(2);
    display.setCursor(38, 24);
    display.println("ATUL");

    display.setTextSize(1);
    display.setCursor(32, 40);
    display.print("LOADING ");
    display.print(progress);
    display.println("%");

    display.drawRect(14, 51, 100, 9, SSD1306_WHITE);
    display.fillRect(16, 53, (progress * 96) / 100, 5, SSD1306_WHITE);

    display.display();
    delay(60);
  }

  delay(250);
}

// ================= OLED Status =================
void updateOLED() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);

  if (!gpsReceiving()) {
    display.println("ATUL GPS: NO UART");
    display.println("---------------------");
    display.println("Check TX -> GPIO16");
    display.println("Check common GND");
  }
  else if (!gps.location.isValid()) {
    display.println("ATUL GPS: SEARCHING");
    display.println("---------------------");
    display.print("Satellites: ");
    display.println(gps.satellites.value());
    display.println("Move antenna outdoor");
    display.println("Open sky required");
  }
  else {
    display.println("ATUL GPS: FIX OK");
    display.println("---------------------");

    display.print("Lat: ");
    display.println(gps.location.lat(), 5);

    display.print("Lon: ");
    display.println(gps.location.lng(), 5);

    display.print("Sats: ");
    display.print(gps.satellites.value());
    display.print(" Spd:");
    display.println(gps.speed.kmph(), 0);

    if (WiFi.status() == WL_CONNECTED) {
      display.print("IP: ");
      display.println(WiFi.localIP());
    }
  }

  display.display();
}

// ================= GPS JSON API =================
void handleData() {
  bool fixed = gps.location.isValid();

  String json = "{";

  json += "\"receiving\":";
  json += (gpsReceiving() ? "true" : "false");

  json += ",\"fix\":";
  json += (fixed ? "true" : "false");

  json += ",\"satellites\":";
  json += String(gps.satellites.value());

  json += ",\"latitude\":";
  if (fixed) json += String(gps.location.lat(), 6);
  else json += "null";

  json += ",\"longitude\":";
  if (fixed) json += String(gps.location.lng(), 6);
  else json += "null";

  json += ",\"speed\":";
  if (gps.speed.isValid()) json += String(gps.speed.kmph(), 1);
  else json += "0.0";

  json += "}";

  server.send(200, "application/json", json);
}

// ================= Web Dashboard =================
void handleHome() {
  String page;
  page.reserve(8500);

  page += F("<!DOCTYPE html><html><head>");
  page += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>ATUL GPS Telemetry</title>");

  page += F("<style>");
  page += F("*{box-sizing:border-box}body{margin:0;padding:18px;min-height:100vh;");
  page += F("font-family:Arial,sans-serif;color:#eefaff;background:#07101c;");
  page += F("background-image:radial-gradient(circle at 10% 10%,#073a4c 0,transparent 30%),");
  page += F("radial-gradient(circle at 90% 90%,#152653 0,transparent 32%)}");
  page += F(".wrap{max-width:900px;margin:auto}.panel{padding:18px;margin-bottom:14px;");
  page += F("border-radius:16px;background:rgba(13,28,47,.92);");
  page += F("border:1px solid rgba(0,243,255,.28);box-shadow:0 0 22px rgba(0,243,255,.10)}");
  page += F(".head{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}");
  page += F("h1{margin:0;color:#00f3ff;font-size:22px;letter-spacing:1px}");
  page += F(".small{font-size:12px;color:#9eb4c6;margin-top:5px}");
  page += F(".watermark{color:#00f3ff;border:1px solid #00f3ff;border-radius:14px;");
  page += F("padding:6px 12px;font-size:12px;font-weight:bold;letter-spacing:1px}");
  page += F(".status{padding:12px;border-radius:12px;text-align:center;font-weight:bold;margin-bottom:14px}");
  page += F(".search{color:#ffc857;border:1px solid #ffc857;background:rgba(255,170,0,.12)}");
  page += F(".fix{color:#55e6a5;border:1px solid #55e6a5;background:rgba(0,255,136,.10)}");
  page += F(".error{color:#ff719b;border:1px solid #ff719b;background:rgba(255,0,85,.10)}");
  page += F(".pulse{display:inline-block;width:9px;height:9px;border-radius:50%;background:currentColor;");
  page += F("margin-right:7px;animation:pulse 1.2s infinite}@keyframes pulse{50%{opacity:.25;transform:scale(.55)}}");
  page += F(".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:12px}");
  page += F(".card{padding:15px;border-radius:13px;background:#101f34;border:1px solid rgba(255,255,255,.08)}");
  page += F(".label{font-size:12px;color:#9eb4c6;letter-spacing:1px}.value{font-size:23px;color:#55e6a5;font-weight:bold;margin-top:7px}");
  page += F(".coord{font-size:17px;color:#00f3ff;margin-top:7px;word-break:break-all}");
  page += F(".meter{height:8px;border-radius:8px;background:#06121e;margin-top:10px;overflow:hidden}");
  page += F(".meter div{height:100%;width:0;transition:width .5s;background:linear-gradient(90deg,#ffaa00,#55e6a5)}");
  page += F("#mapFrame{width:100%;height:300px;border:0;border-radius:13px;margin-top:14px;background:#0a1725}");
  page += F(".buttons{display:flex;gap:12px;flex-wrap:wrap;margin-top:14px}");
  page += F(".button{flex:1;min-width:180px;padding:13px;border-radius:10px;text-align:center;");
  page += F("font-weight:bold;text-decoration:none;cursor:pointer;font-size:14px}");
  page += F(".mapbtn{background:#008ccf;color:white}.copybtn{background:#123c35;color:#55e6a5;border:1px solid #55e6a5}");
  page += F(".disabled{opacity:.45;pointer-events:none}#toast{opacity:0;color:#55e6a5;text-align:center;margin-top:12px;transition:opacity .3s}");
  page += F("#toast.show{opacity:1}</style></head><body>");

  page += F("<div class='wrap'>");
  page += F("<div class='panel head'><div><h1>GPS TELEMETRY SYSTEM</h1>");
  page += F("<div class='small'>ESP32 real-time location monitoring</div></div>");
  page += F("<div class='watermark'>ATUL</div></div>");

  page += F("<div id='status' class='status search'><span class='pulse'></span>LOADING GPS DATA</div>");

  page += F("<div class='grid'>");
  page += F("<div class='card'><div class='label'>SATELLITES</div><div id='sats' class='value'>0</div>");
  page += F("<div class='meter'><div id='satBar'></div></div></div>");
  page += F("<div class='card'><div class='label'>SPEED</div><div id='speed' class='value'>0.0 km/h</div></div>");
  page += F("<div class='card'><div class='label'>LATITUDE</div><div id='lat' class='coord'>--</div></div>");
  page += F("<div class='card'><div class='label'>LONGITUDE</div><div id='lon' class='coord'>--</div></div>");
  page += F("</div>");

  page += F("<div class='panel' style='margin-top:14px'>");
  page += F("<div class='label'>LIVE MAP LOCATION</div>");
  page += F("<div id='location' class='coord'>Waiting for a GPS fix...</div>");
  page += F("<iframe id='mapFrame' src='about:blank'></iframe>");

  page += F("<div class='buttons'>");
  page += F("<a id='mapBtn' class='button mapbtn disabled' target='_blank'>Open in Google Maps</a>");
  page += F("<button class='button copybtn' onclick='copyCoordinates()'>Copy Coordinates</button>");
  page += F("</div><div id='toast'>Coordinates copied.</div></div>");

  page += F("<div class='small' style='text-align:center'>ATUL | ESP32 GPS TELEMETRY SYSTEM</div>");
  page += F("</div>");

  // Normal C++ quoted strings avoid the old Arduino raw-string compilation issue.
  page += F("<script>");
  page += F("let latestLat=null,latestLon=null,lastMapLat=0,lastMapLon=0,mapLoaded=false;");
  page += F("function el(x){return document.getElementById(x);}");
  page += F("function setStatus(t,c){el('status').className='status '+c;");
  page += F("el('status').innerHTML='<span class=\"pulse\"></span>'+t;}");
  page += F("function copyCoordinates(){if(latestLat===null)return;let t=latestLat+', '+latestLon;");
  page += F("if(navigator.clipboard){navigator.clipboard.writeText(t);}else{prompt('Copy coordinates:',t);}");
  page += F("el('toast').className='show';setTimeout(function(){el('toast').className='';},1800);}");
  page += F("function setMap(lat,lon){if(mapLoaded&&Math.abs(lat-lastMapLat)<0.00005&&Math.abs(lon-lastMapLon)<0.00005)return;");
  page += F("let d=.004,w=(lon-d).toFixed(6),e=(lon+d).toFixed(6),s=(lat-d).toFixed(6),n=(lat+d).toFixed(6);");
  page += F("el('mapFrame').src='https://www.openstreetmap.org/export/embed.html?bbox='+w+'%2C'+s+'%2C'+e+'%2C'+n+'&layer=mapnik&marker='+lat+'%2C'+lon;");
  page += F("lastMapLat=lat;lastMapLon=lon;mapLoaded=true;}");
  page += F("function updateGPS(){fetch('/data').then(function(r){return r.json();}).then(function(d){");
  page += F("el('sats').textContent=d.satellites;el('speed').textContent=d.speed+' km/h';");
  page += F("el('satBar').style.width=(Math.min(d.satellites,12)*100/12)+'%';");
  page += F("if(!d.receiving){setStatus('GPS UART: NO DATA','error');return;}");
  page += F("if(!d.fix){setStatus('SEARCHING FOR SATELLITES','search');el('lat').textContent='--';");
  page += F("el('lon').textContent='--';el('location').textContent='Move GPS antenna outdoors with open sky.';return;}");
  page += F("latestLat=Number(d.latitude);latestLon=Number(d.longitude);el('lat').textContent=d.latitude;");
  page += F("el('lon').textContent=d.longitude;el('location').textContent=d.latitude+', '+d.longitude;");
  page += F("el('mapBtn').href='https://www.google.com/maps/search/?api=1&query='+d.latitude+','+d.longitude;");
  page += F("el('mapBtn').className='button mapbtn';setMap(latestLat,latestLon);setStatus('GPS FIX ACQUIRED','fix');");
  page += F("}).catch(function(){setStatus('SERVER CONNECTION ERROR','error');});}");
  page += F("updateGPS();setInterval(updateGPS,1500);");
  page += F("</script></body></html>");

  server.send(200, "text/html", page);
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED not found. Try address 0x3D.");
    while (true);
  }

  runBootAnimation();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Open web page: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection failed");
  }

  server.on("/", handleHome);
  server.on("/data", handleData);
  server.begin();
}

// ================= Main Loop =================
void loop() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
    lastGPSByteTime = millis();
  }

  server.handleClient();

  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastWiFiRetry > 10000) {
    lastWiFiRetry = millis();
    WiFi.reconnect();
  }

  if (millis() - lastOLEDUpdate >= 500) {
    lastOLEDUpdate = millis();
    updateOLED();
  }
}