#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>
#include "secrets.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SOIL_SENSOR_PIN A0
#define PUMP_PIN 5

#define TOUCH_PIN 13 // Pin pro dotykový senzor

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

bool isDeviceActive = true;
bool isNightMode = false;
bool watering = false;
bool settingMin = false;
bool settingMax = false;

int moisture = 54;
int minMoisture = 20;
int maxMoisture = 50;

volatile unsigned long lastTouchTime = 0;
volatile int clickCount = 0;
const unsigned long doubleClickThreshold = 500; // Časový limit pro dvojité kliknutí

// Funkce pro obsluhu přerušení dotykového senzoru
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
}

void initialize_system() 
{
  // Inicializace displeje
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  {
    while (true)
      ; // Zastaví program
  }

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
  pinMode(TOUCH_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), touchInterrupt, RISING);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("System Initialized");
  display.display();
  delay(2000);
}

String get_current_time() 
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
void enter_sleep_mode() 
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Entering sleep mode...");
  display.display();
  delay(5000);
  esp_sleep_enable_timer_wakeup(10 * 60 * 1000000);  // Uspat na 10 minut
  esp_deep_sleep_start();
}

// Funkce pro čtení hodnoty vlhkosti půdy
int read_soil_moisture() 
{
  int moisture = analogRead(SOIL_SENSOR_PIN);
  return moisture;  // Zde je nutné případně kalibrovat
}

// Funkce pro aktivaci čerpadla
void activate_pump() 
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Pump activated!");
  display.display();
  digitalWrite(PUMP_PIN, HIGH);
  delay(5000);  // Čerpadlo běží 5 sekund
  digitalWrite(PUMP_PIN, LOW);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Pump deactivated.");
  display.display();
}

// Funkce pro přepnutí režimu zařízení
void toggle_device_mode() 
{
    isDeviceActive = !isDeviceActive;
    display.clearDisplay();
    display.setTextSize(2);

    String message = isDeviceActive ? "ACTIVE" : "INACTIVE";
    
    uint16_t textWidth = message.length() * 12; // Přibližná šířka písma 6x2
    uint16_t textHeight = 16; // Výška písma při velikosti 2

    // Výpočet pozice pro vycentrování textu
    int16_t x = (SCREEN_WIDTH - textWidth) / 2;
    int16_t y = (SCREEN_HEIGHT - textHeight) / 2;

    display.setCursor(x, y);
    display.print(message);
    display.display();
    display.setTextSize(1);

    delay(2000);
}

// Funkce pro kontrolu nočního režimu
void check_night_mode() 
{
  String currentTime = get_current_time();
  if (currentTime >= "22:00:00" || currentTime <= "08:00:00") 
  {
    isNightMode = true;
  } 
  else 
  {
    isNightMode = false;
  }
}

// Vykreslení měsíce (indikátor nočního režimu)
void drawMoon() 
{
  if (isNightMode == true) 
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
  if (isDeviceActive == false) 
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
void update_display() 
{
  display.clearDisplay();
  display.setCursor(0, 0);

  display.println("Connected to:");
  display.println(ssid);

  drawSign();
  drawMoon();

  displaySignalStrength();

  // TODO - zobrazit vlhkost půdy
  // TODO - zobrazit minimální a maximální vlhkost půdy

  display.println("Time: " + get_current_time());
  display.println("Min: " + String(minMoisture) + "%");
  display.println("Max: " + String(maxMoisture) + "%");
  
  int xPos = SCREEN_WIDTH - (String(moisture).length() + 1) * 12;
  int yPos = SCREEN_HEIGHT - 16;
  display.setCursor(xPos, yPos);
  display.setTextSize(2);
  display.println("54%");
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
    
    delay(200);
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

    // TMP - simulace nastaveni hodnoty
      maxMoisture += 1;
      if (maxMoisture > 100) 
      {
          maxMoisture = 0;
      }
    // TMP END

    delay(200);
  }
}

// Funkce pro zpracování dvojitého kliknutí
void process_touch_clicks() 
{
  if (clickCount == 2) 
  {
    toggle_device_mode();
    clickCount = 0;
  }
}

void setup() 
{
  initialize_system();
}

void loop() 
{
  process_touch_clicks();

  if (settingMin) 
    setMinimumMoisture();
  if (settingMax) 
    setMaximumMoisture();

  if (isDeviceActive) 
  {
    int moisture = read_soil_moisture();

    if (moisture < 300) 
    {
      watering = true;
    }

    if (watering) 
    {
      // activate_pump();
    }

    check_night_mode();
    update_display();
  }
  else 
  {
    update_display();
  }

  delay(1000);
}
