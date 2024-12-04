#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include "secrets.h"
#include <PubSubClient.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define PUMP_PIN 15
#define PUMP_PIN2 16

#define TOUCH_PIN 13
#define SOIL_SENSOR_PIN 33

Preferences preferences;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

bool deviceActive = true;
bool nightMode = false;
bool watering = false;
bool settingMin = false;
bool settingMax = false;
bool timedWakeup = false;
bool touchWakeup = false;

int moisture;
int minMoisture;
int maxMoisture;

unsigned long lastButtonPress = 0;
volatile unsigned long lastTouchTime = 0;
volatile unsigned long lastWakeup = 0;
volatile int clickCount = 0;
const unsigned long doubleClickThreshold = 400; // Casovy limit pro dvojite kliknuti

// Definice proměnných pro vlhkost
const float alpha = 0.01; // Koeficient pro EMA (0 < alpha < 1)
float previousEMA = 0;    // Předchozí hodnota EMA

WiFiClient espClient;
PubSubClient client(espClient);

// Konfigurace MQTT serveru
const char *mqtt_client_id = "esp32_smartWatering";

const char *mqtt_topic_moisture = "smartWatering/moisture";
const char *mqtt_topic_min_moisture = "smartWatering/min";
const char *mqtt_topic_max_moisture = "smartWatering/max";
const char *mqtt_topic_night_mode = "smartWatering/night_mode";
const char *mqtt_topic_device_active = "smartWatering/device_active";

void reconnect() 
{
  // 5 pokusu o připojeni
  for (int i = 0; i < 5; i++) {
    Serial.print("Connecting to MQTT...");
    // Pripojeni k brokeru
    if (client.connect(mqtt_client_id, mqttAccount, mqttPassword)) {
      Serial.println("connected");
      
      // Prihlaseni k odberu zprav
      client.subscribe(mqtt_topic_min_moisture);
      client.subscribe(mqtt_topic_max_moisture);
      client.subscribe(mqtt_topic_night_mode);
      client.subscribe(mqtt_topic_device_active);
    } 
    else 
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(2000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Prijeti minimalni vlhkosti
  if (String(topic) == mqtt_topic_min_moisture) 
  {
    minMoisture = message.toInt();
    preferences.putInt("minMoisture", minMoisture);
  }
  
  // Prijeti maximalni vlhkosti
  if (String(topic) == mqtt_topic_max_moisture) 
  {
    maxMoisture = message.toInt();
    preferences.putInt("maxMoisture", maxMoisture);
  }

  // Prijeti stavu zarizeni
  if (String(topic) == mqtt_topic_device_active) 
  {
    deviceActive = message.toInt();
    preferences.putBool("deviceActive", deviceActive);
  }

  // Prijeti nocniho rezimu
  if (String(topic) == mqtt_topic_night_mode) 
  {
    nightMode = message.toInt();
  }
}

unsigned long singleClickTimeout = 0;
void IRAM_ATTR touchInterrupt() 
{
  unsigned long currentTime = millis();
  if (currentTime - lastTouchTime < doubleClickThreshold) 
  {
    clickCount++;
  } 
  else 
  {
    clickCount = 1;
  }
  lastTouchTime = currentTime;
  singleClickTimeout = lastTouchTime + doubleClickThreshold;
}

void sendMoisture() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); 

  // Publikovani vlhkosti
  String moistureStr = String(moisture);
  client.publish(mqtt_topic_moisture, moistureStr.c_str());
}

void sendMinMoisture() 
{
  if (!client.connected())
    reconnect();

  client.loop();

  String minMoistureStr = String(minMoisture);
  client.publish(mqtt_topic_min_moisture, minMoistureStr.c_str());
}

void sendMaxMoisture() {
  if (!client.connected())
    reconnect();

  client.loop();

  String maxMoistureStr = String(maxMoisture);
  client.publish(mqtt_topic_max_moisture, maxMoistureStr.c_str());
}

void sendNightMode() {
  if (!client.connected())
    reconnect();

  client.loop();

  String nightModeStr = String(nightMode);
  client.publish(mqtt_topic_night_mode, nightModeStr.c_str());
}

void sendDeviceActive() {
  if (!client.connected())
    reconnect();

  client.loop();

  String deviceActiveStr = String(deviceActive);
  client.publish(mqtt_topic_device_active, deviceActiveStr.c_str());
}


void initializeSystem() 
{
  // Inicializace displeje
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  {
    while (true)
      ; // Zastavi program
  }

  client.setServer(mqttServer, 1883);
  client.setCallback(callback);

  // Otevreni preferences
  preferences.begin("moistureSett", false);

  minMoisture = preferences.getInt("minMoisture", 20);
  maxMoisture = preferences.getInt("maxMoisture", 60);
  deviceActive = preferences.getBool("deviceActive", false);
  moisture = preferences.getInt("moisture", 0);
  previousEMA = preferences.getFloat("previousEMA", 0.0); // Načtení EMA nebo výchozí 0


// CHECK
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) 
  {
    timedWakeup = true;
    lastWakeup = millis();
  }
  else if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO)
  {
    touchWakeup = true;
    lastWakeup = millis();
  }
  else
  {
    timedWakeup = false;
    touchWakeup = false;
  }
// CHECK

  // Pripojeni k Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
  }

  // Nastaveni NTP serveru
  configTime(3600, 3600, "pool.ntp.org");

  // Inicializace pinu
  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(PUMP_PIN2, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), touchInterrupt, RISING);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  sendMinMoisture();
  sendMaxMoisture();
  sendDeviceActive();
  sendNightMode();
}

String getCurrentTime() 
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) 
  {
    return "N/A"; // Pokud se nepodaří získat čas, vrátíme "N/A"
  }
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo); // Formátování času
  return String(timeString);
}

// Funkce pro vstup do režimu spánku
void enterSleepMode() 
{
  display.clearDisplay();
  display.display();

  if (!deviceActive || nightMode) 
    esp_sleep_enable_timer_wakeup(60UL * 30 * 1000000);  // Uspat na 30 minut
  else
    esp_sleep_enable_timer_wakeup(60UL * 5 * 1000000);  // Uspat na 10 minut

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13,1);
  esp_deep_sleep_start();
}


// Funkce pro čtení hodnoty vlhkosti půdy
void updateMoisture() 
{
  int sensorValue = analogRead(SOIL_SENSOR_PIN);
  
  // Převedení analogové hodnoty na procenta
  float moistureRaw = map(sensorValue, 2647, 976, 0, 100); 

  // Výpočet exponenciálního klouzavého průměru
  previousEMA = alpha * moistureRaw + (1 - alpha) * previousEMA;
  
  // Uložení aktuální EMA jako celé číslo
  int newMoisture = static_cast<int>(round(previousEMA));

  if (newMoisture != moisture) 
  {
    moisture = newMoisture;
    preferences.putInt("moisture", moisture);
    preferences.putFloat("previousEMA", previousEMA);
    sendMoisture();
  }
}

unsigned long lastWateringTime = 0;
unsigned long lastPumpRunTime = 0;
bool pumpRunning = false;

void watering_cycle()
{
  unsigned long currentMillis = millis();

  // Pokud je watering true, spustí se logika zavlažování
  if (watering) 
  {
    // Zkontrolujeme, zda má čerpadlo běžet (běží na 20 sekund)
    if (!pumpRunning && currentMillis - lastWateringTime >= 6000)  // 10 minut čekání
    {
      pumpRunning = true;
      lastPumpRunTime = currentMillis;
      digitalWrite(PUMP_PIN, HIGH);
      digitalWrite(PUMP_PIN2, LOW);
    }

    // Pokud čerpadlo běželo 20 sekund, vypneme ho
    if (pumpRunning && currentMillis - lastPumpRunTime >= 2000)  // 20 sekund běhu čerpadla
    {
      pumpRunning = false;
      digitalWrite(PUMP_PIN, LOW);
      digitalWrite(PUMP_PIN2, LOW);
      lastWateringTime = currentMillis;  // Resetujeme čas čekání
    }

    // Zkontrolujeme vlhkost během čekání a vypneme zavlažování, pokud je vlhkost dostatečná
    if (moisture >= (maxMoisture - 5))  // Pokud je vlhkost blízko maximální hodnotě
    {
      watering = false;
      pumpRunning = false;
    }
  }
}

// Funkce pro přepnutí režimu zařízení
void toggleDeviceMode() 
{
  deviceActive = !deviceActive;
  display.clearDisplay();
  display.setTextSize(2);

  String message = deviceActive ? "ACTIVE" : "INACTIVE";
  
  uint16_t textWidth = message.length() * 12; // Přibližná šířka písma 6x2
  uint16_t textHeight = 16; // Výška písma při velikosti 2

  // Výpočet pozice pro vycentrování textu
  int16_t x = (SCREEN_WIDTH - textWidth) / 2;
  int16_t y = (SCREEN_HEIGHT - textHeight) / 2;

  display.setCursor(x, y);
  display.print(message);
  display.display();
  display.setTextSize(1);

  preferences.putBool("deviceActive", deviceActive);

  pumpRunning = false;
  watering = false;
  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(PUMP_PIN2, LOW);

  sendDeviceActive();
  delay(2000);
}

// Funkce pro kontrolu nočního režimu
void checkNightMode() 
{
  bool lastState = nightMode;
  String currentTime = getCurrentTime();
  if (currentTime >= "22:00:00" || currentTime <= "08:00:00") 
  {
    nightMode = true;
  } 
  else 
  {
    nightMode = false;
  }

  if (lastState != nightMode) 
  {
    sendNightMode();
  }
}

// Vykreslení měsíce (indikátor nočního režimu)
void drawMoon() 
{
  if (nightMode == true) 
  {
    display.fillCircle(100, 7, 7, WHITE);
    display.fillCircle(94, 7, 7, BLACK);
  } 
  else
    display.fillCircle(100, 7, 7, BLACK);
}

// Vykreslení zákazové značky (indikátor stavu zařízení)
void drawSign() 
{
  if (deviceActive == false) 
  {
    display.fillCircle(120, 7, 7, WHITE);
    display.fillCircle(120, 7, 5, BLACK);

    display.drawLine(114, 11, 124, 1, WHITE);
    display.drawLine(115, 11, 124, 2, WHITE);
    display.drawLine(115, 11, 125, 2, WHITE);
    display.drawLine(115, 12, 125, 2, WHITE);
    display.drawLine(116, 12, 125, 2, WHITE);
    display.drawLine(116, 12, 125, 3, WHITE);
  } 
  else
    display.fillCircle(120, 7, 7, BLACK);
}

void drawWateringIcon() 
{
  if (watering) 
  {
    int centerX = SCREEN_WIDTH / 2;   // Střed displeje na ose X
    int centerY = SCREEN_HEIGHT - 8; // Dolní část displeje

    // Vymazání prostoru pro ikonu
    display.fillRect(centerX - 14, centerY - 6, 28, 12, SSD1306_BLACK);

    // Kapka vody
    display.fillTriangle(centerX, centerY - 7, centerX - 4, centerY + 1, centerX + 4, centerY + 1, SSD1306_WHITE); // Tělo kapky
    display.fillCircle(centerX, SCREEN_HEIGHT - 6, 4, SSD1306_WHITE); // Horní část kapky
    }
}

// Vykreslení indikátoru síly signálu Wi-Fi
void displaySignalStrength() 
{
  int32_t rssi = WiFi.RSSI();
  int bars = 0;
  if (rssi > -50) 
  {
    bars = 5;
  } 
  else if (rssi > -60) 
  {
    bars = 4;
  } 
  else if (rssi > -70) 
  {
    bars = 3;
  } 
  else if (rssi > -80) 
  {
    bars = 2;
  } 
  else if (rssi > -90) 
  {
    bars = 1;
  } 
  else 
  {
    bars = 0;
  }

  int barWidth = 3;
  int barSpacing = 1;
  int baseX = 71;
  int baseY = 13;

  for (int i = 0; i < 5; i++) 
  {
    if (i < bars) 
    {
      display.fillRect(baseX + (i + 1) * (barWidth + barSpacing), 15 - (i + 1) * 3, barWidth, (i + 1) * 3, SSD1306_WHITE);
    } 
    else 
    {
      display.drawRect(baseX + (i + 1) * (barWidth + barSpacing), 15 - (i + 1) * 3, barWidth, (i + 1) * 3, SSD1306_WHITE);
    }
  }
}

// Aktualizace displeje
void updateDisplay() 
{
  display.clearDisplay();
  display.setCursor(0, 0);

  display.println("Connected to:");
  display.println(ssid);

  drawSign();
  drawMoon();
  drawWateringIcon();

  displaySignalStrength();

  display.println("Time: " + getCurrentTime());
  display.println("Min: " + String(minMoisture) + "%");
  display.println("Max: " + String(maxMoisture) + "%");
  
  int xPos = SCREEN_WIDTH - (String(moisture).length() + 1) * 12;
  int yPos = SCREEN_HEIGHT - 16;
  display.setCursor(xPos, yPos);
  display.setTextSize(2);
  display.print(String(moisture) + "%");
  display.setTextSize(1);

  // Zobrazení stavu MQTT
  display.setCursor(0, SCREEN_HEIGHT - 8);
  display.print("MQTT");
  if (!client.connected()) // Pokud není připojeno k MQTT
  {
    display.drawLine(0, SCREEN_HEIGHT - 5, 4 * 6 - 2, SCREEN_HEIGHT - 5, SSD1306_WHITE);
    display.drawLine(0, SCREEN_HEIGHT - 4, 4 * 6 - 2, SCREEN_HEIGHT - 4, SSD1306_WHITE);
  }

  display.display();
}

// Funkce pro zpracovani poctu kliknuti
void processTouchClicks() 
{
  if (clickCount == 1 && millis() > singleClickTimeout) 
  {
    if (settingMin)
    {
      settingMin = false;
      sendMinMoisture();
      settingMax = true;
    }
    else if (settingMax)
    {
      settingMax = false;
      sendMaxMoisture();
    }
    else
    {
      settingMin = true;
    }

    clickCount = 0;
  }
  else if (clickCount == 2) 
  {
    toggleDeviceMode();
    clickCount = 0;
  }
}

void setMinimumMoisture() 
{
  while (settingMin) 
  {
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Set Min Moisture");

    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("Min: ");
    display.print(minMoisture);
    display.print("%");

    // Vykresleni posuvníku
    display.setTextSize(1);
    display.setCursor(0, 50);
    int sliderPosition = map(minMoisture, 0, 100, 0, SCREEN_WIDTH);

    display.drawRect(0, 50, SCREEN_WIDTH, 5, SSD1306_WHITE);
    display.fillRect(0, 50, sliderPosition, 5, SSD1306_WHITE);

    // Indikator aktualni hodnoty
    int indicatorWidth = 6;
    int indicatorHeight = 13;
    int indicatorX = sliderPosition - (indicatorWidth / 2);
    display.fillRect(indicatorX, 46, indicatorWidth, indicatorHeight, SSD1306_WHITE);

    display.display();

    minMoisture += 1;
    if (minMoisture > 100) 
    {
        minMoisture = 0;
    }

    preferences.putInt("minMoisture", minMoisture);

    processTouchClicks();

    delay(250);
  }
}

void setMaximumMoisture() 
{
  while (settingMax) 
  {
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Set Max Moisture");

    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("Max: ");
    display.print(maxMoisture);
    display.print("%");

    // Vykreslení posuvníku
    display.setTextSize(1);
    display.setCursor(0, 50);
    int sliderPosition = map(maxMoisture, 0, 100, 0, SCREEN_WIDTH);

    display.drawRect(0, 50, SCREEN_WIDTH, 5, SSD1306_WHITE);
    display.fillRect(0, 50, sliderPosition, 5, SSD1306_WHITE);

    // Indikator aktualni hodnoty
    int indicatorWidth = 6;
    int indicatorHeight = 13;
    int indicatorX = sliderPosition - (indicatorWidth / 2);
    display.fillRect(indicatorX, 46, indicatorWidth, indicatorHeight, SSD1306_WHITE);

    display.display();

    maxMoisture += 1;
    if (maxMoisture > 100) 
    {
        maxMoisture = 0;
    }

    preferences.putInt("maxMoisture", maxMoisture);

    processTouchClicks();

    delay(250);
  }
}

void setup() 
{
  initializeSystem();
}

void loop() 
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();  // Zpracování zpráv

  updateMoisture();
  
  processTouchClicks();

  if (settingMin) 
    setMinimumMoisture();
  
  if (settingMax) 
    setMaximumMoisture();

  if (deviceActive) 
  {
    if (moisture < minMoisture) 
    {
      watering = true;
    }

    if (watering) 
    {
      watering_cycle();
    }
    if (!timedWakeup) // Automaticke probuzeni - nevykresit displej
      updateDisplay();
  }
  else if (!settingMin && !settingMax)
  {
    if (!timedWakeup)
      updateDisplay();
  }
  
  checkNightMode();

  delay(10);

  // if (watering == false && (millis() - lastTouchTime) > 1000 * 180) // 3 minuty
  //   enterSleepMode();
  // else if (watering == false && timedWakeup && (millis() - lastWakeup) > 1000 * 30) // 30 sekund na aktualizaci vlhkosti
  //   enterSleepMode();
}
