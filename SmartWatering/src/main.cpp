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

#define TOUCH_PIN 13 // Pin pro dotykový senzor
#define SOIL_SENSOR_PIN 33

Preferences preferences;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

bool deviceActive = true;
bool nightMode = false;
bool watering = false;
bool settingMin = false;
bool settingMax = false;
bool timedWakeup = false;

int moisture;
int minMoisture;
int maxMoisture;

#define CLK 26
#define DT 27
#define SW 14

int counter = 0;                    // Counter for encoder position
int currentStateCLK;                // Current state of CLK
int lastStateCLK;                   // Last state of CLK
String currentDir = "";             // Current direction of rotation
unsigned long lastButtonPress = 0;  // Time of last button press

volatile unsigned long lastTouchTime = 0;
volatile int clickCount = 0;
const unsigned long doubleClickThreshold = 500; // Časový limit pro dvojité kliknutí


WiFiClient espClient;
PubSubClient client(espClient);

// konfigurace MQTT serveru
const char *mqtt_server = "ha-home-prerov.duckdns.org";
const char *mqtt_client_id = "esp32_soil_moisture";
const char *mqtt_topic_moisture = "soil_moisture/reading";
const char *mqtt_topic_min_moisture = "soil_moisture/min";
const char *mqtt_topic_max_moisture = "soil_moisture/max";

void reconnect() {
  // Vynuceni pripojeni k MQTT brokeru
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    // Připojení k brokeru
    if (client.connect(mqtt_client_id, mqttAccount, mqttPassword)) {
      Serial.println("connected");
      // Odběr pro nastavení minimální a maximální vlhkosti
      client.subscribe(mqtt_topic_min_moisture);
      client.subscribe(mqtt_topic_max_moisture);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Přijetí minimální vlhkosti
  if (String(topic) == mqtt_topic_min_moisture) {
    minMoisture = message.toInt();
    preferences.putInt("minMoisture", minMoisture);
    Serial.print("Min moisture set to: ");
    Serial.println(minMoisture);
  }
  
  // Přijetí maximální vlhkosti
  if (String(topic) == mqtt_topic_max_moisture) {
    maxMoisture = message.toInt();
    preferences.putInt("maxMoisture", maxMoisture);
    Serial.print("Max moisture set to: ");
    Serial.println(maxMoisture);
  }
}


// Funkce pro obsluhu přerušení dotykového senzoru
void IRAM_ATTR setting() 
{
  if (settingMin)
  {
    settingMin = false;
    settingMax = true;
  }
  else if (settingMax)
  {
    settingMax = false;
  }
  else
  {
    settingMin = true;
  }

}
void IRAM_ATTR touchInterrupt() 
{
  // if (settingMin)
  // {
  //   settingMin = false;
  //   settingMax = true;
  // }
  // else if (settingMax)
  // {
  //   settingMax = false;
  // }
  // else
  // {
  //   settingMin = true;
  // }

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
}

void sendMoisture() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Zpracování zpráv

  // Publikování vlhkosti
  String moistureStr = String(moisture);
  client.publish(mqtt_topic_moisture, moistureStr.c_str());
}


void initializeSystem() 
{
  // Inicializace displeje
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  {
    while (true)
      ; // Zastaví program
  }

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Otevření preferences
  preferences.begin("moistureSett", false);

  minMoisture = preferences.getInt("minMoisture", 20);
  maxMoisture = preferences.getInt("maxMoisture", 60);
  deviceActive = preferences.getBool("deviceActive", false);

// CHECK
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) 
  {
    timedWakeup = true;
  }
  else
  {
    timedWakeup = false;
  }
// CHECK

  // Připojení k Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
  }

  // Nastavení NTP serveru
  configTime(3600, 3600, "pool.ntp.org");

  // Inicializace pinů
  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(PUMP_PIN2, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);

  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  pinMode(SW, INPUT_PULLUP);

  lastStateCLK = digitalRead(CLK);


  attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), touchInterrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(SW), setting, RISING);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
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

  if (deviceActive || nightMode) 
    esp_sleep_enable_timer_wakeup(60UL * 60 * 1000000);  // Uspat na 60 minut
  else
    esp_sleep_enable_timer_wakeup(10 * 60 * 1000000);  // Uspat na 10 minut

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13,1);
  esp_deep_sleep_start();
}

// // Funkce pro čtení hodnoty vlhkosti půdy
// void updateMoisture() 
// {
//   int sensorValue = analogRead(SOIL_SENSOR_PIN);
//   moisture = map(sensorValue, 2647, 976, 0, 100); 
// }

// Definice proměnných pro vlhkost
const float alpha = 0.01; // Koeficient pro EMA (0 < alpha < 1)
float previousEMA = 0;    // Předchozí hodnota EMA
// float moisture = 0;       // Aktuální hodnota vlhkosti

// Funkce pro čtení hodnoty vlhkosti půdy
void updateMoisture() 
{
  int sensorValue = analogRead(SOIL_SENSOR_PIN);
  
  // Převedení analogové hodnoty na procenta
  float moistureRaw = map(sensorValue, 2647, 976, 0, 100); 

  // Výpočet exponenciálního klouzavého průměru
  previousEMA = alpha * moistureRaw + (1 - alpha) * previousEMA;
  
  // Uložení aktuální EMA jako celé číslo
  moisture = static_cast<int>(round(previousEMA));
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
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Pump running...");
      display.display();
      delay(1000);
    }

    // Pokud čerpadlo běželo 20 sekund, vypneme ho
    if (pumpRunning && currentMillis - lastPumpRunTime >= 2000)  // 20 sekund běhu čerpadla
    {
      pumpRunning = false;
      digitalWrite(PUMP_PIN, LOW);
      digitalWrite(PUMP_PIN2, LOW);
      lastWateringTime = currentMillis;  // Resetujeme čas čekání
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Waiting 10 minutes...");
      display.display();
      delay(1000);
    }

    // Zkontrolujeme vlhkost během čekání a vypneme zavlažování, pokud je vlhkost dostatečná
    if (moisture >= (maxMoisture - 5))  // Pokud je vlhkost blízko maximální hodnotě
    {
      watering = false;
      pumpRunning = false;
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Watering complete.");
      display.display();
      delay(1000);
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

  delay(2000);
}

// Funkce pro kontrolu nočního režimu
void checkNightMode() 
{
  String currentTime = getCurrentTime();
  if (currentTime >= "22:00:00" || currentTime <= "08:00:00") 
  {
    nightMode = true;
  } 
  else 
  {
    nightMode = false;
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

  display.display();
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

    // Vykreslení posuvníku
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

    // TMP - simulace nastaveni hodnoty
      minMoisture += 1;
      if (minMoisture > 100) 
      {
          minMoisture = 0;
      }
    // TMP END
    
    preferences.putInt("minMoisture", minMoisture);

    delay(200);
  }
}

void setMaximumMoisture() 
{
  // while (settingMax) 
  // {
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


    // Read the current state of CLK


    preferences.putInt("maxMoisture", maxMoisture);

    // delay(100);
    // }
}

// Funkce pro zpracování dvojitého kliknutí
void processTouchClicks() 
{
  if (clickCount == 2) 
  {
    toggleDeviceMode();
    clickCount = 0;
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
  sendMoisture();  
  
  processTouchClicks();

  if (settingMin) 
    setMinimumMoisture();
  
  if (settingMax) 
  {
    // Read the current state of CLK
    currentStateCLK = digitalRead(CLK);
    
    // If the last and current state of CLK are different, then a pulse occurred
    if (currentStateCLK != lastStateCLK) 
    {
      // Check the state of DT to determine the rotation direction
      if (digitalRead(DT) != currentStateCLK) 
      {
        maxMoisture--;
      } 
      else 
      {
        maxMoisture++;
      }

      // Ensure the moisture value stays between 0 and 100
      if (maxMoisture < 0) maxMoisture = 0;
      if (maxMoisture > 100) maxMoisture = 100;

      preferences.putInt("maxMoisture", maxMoisture);  // Save the new value
          lastStateCLK = currentStateCLK;

    }

    // Remember last CLK state

    delay(30);  // Short delay to debounce
    setMaximumMoisture();  // Update display
  }

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

    checkNightMode();
    updateDisplay();
  }
  else if (!settingMin && !settingMax)
  {
    if (!timedWakeup)
      updateDisplay();
  }

  delay(10);
  // if (watering == false && (millis() - lastTouchTime) > 60000)
  //   enterSleepMode();
}
