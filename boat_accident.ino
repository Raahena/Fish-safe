#include <WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <MPU6050.h>

const char *ssid = "AP";
IPAddress local_IP(192,168,71,22);
IPAddress gateway(192,168,4,9);
IPAddress subnet(255,255,255,0);

#define DHTPIN 5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define RAIN_SENSOR_PIN 36
#define WATER_LEVEL_PIN 34
#define BUZZER_PIN 25
int16_t gx, gy, gz;
int16_t ax, ay, az;
MPU6050 mpu;

WiFiServer server(80);
const int WATER_LEVEL_THRESHOLD = 2000;  // Threshold for high water level
const float ROLL_THRESHOLD = 30.0;
unsigned long waterHighStartTime = 0;
bool waterAlertActive = false;

void setup() {
  Serial.begin(115200);
  dht.begin();
  Wire.begin();
  mpu.initialize();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid);
  server.begin();
}

String interpretWaterLevel(int level) {
  if (level < 500) return "No water";
  else if (level < 1000) return "Little water (not a threat)";
  else if (level < WATER_LEVEL_THRESHOLD) return "More water";
  else return "High water level";
}

String interpretRainfall(int rainfall) {
  // Check for the special case of no rainfall indicated by 4095
  if (rainfall == 4095) return "No rainfall";
  else if (rainfall > 3000) return "No rainfall";  // Adjust threshold as necessary
  else if (rainfall > 2000) return "Drizzling";
  else if (rainfall > 1000) return "Moderate rain";
  else return "Heavy rain";
}

String interpretBoatStatus(float roll) {
  if (abs(roll) < 10) return "Boat is upright";
  else if (abs(roll) < ROLL_THRESHOLD) return "Boat is about to sink";
  else return "Boat is not upright";
}

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int rainfall = analogRead(RAIN_SENSOR_PIN);
  int waterLevel = analogRead(WATER_LEVEL_PIN);
  mpu.getRotation(&gx, &gy, &gz);
  mpu.getAcceleration(&ax, &ay, &az);
  float roll = atan2(ay, az) * 180.0 / PI;

  String waterLevelStatus = interpretWaterLevel(waterLevel);
  String rainfallStatus = interpretRainfall(rainfall);
  String boatStatus = interpretBoatStatus(roll);

  Serial.println("----------------------------");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Rainfall: ");
  Serial.println(rainfallStatus);

  Serial.print("Water Level: ");
  Serial.println(waterLevelStatus);

  Serial.print("Boat Status: ");
  Serial.println(boatStatus);

  // Water level alert logic with a 2-minute timer
  if (waterLevel >= WATER_LEVEL_THRESHOLD) {
    if (!waterAlertActive) {
      waterHighStartTime = millis();
      waterAlertActive = true;
    } else if (millis() - waterHighStartTime >= 120000) {  // 2 minutes
      Serial.println("Warning: High water level sustained for 2 minutes, activating buzzer.");
      digitalWrite(BUZZER_PIN, HIGH);
    }
  } else {
    waterAlertActive = false;
    digitalWrite(BUZZER_PIN, LOW);
  }

  // Rainfall alert logic
  if (rainfall < 1000) { // Heavy rain
    Serial.println("Warning: Heavy rain detected, activating buzzer.");
    digitalWrite(BUZZER_PIN, HIGH);
  } else if (rainfall < 2000) { // Moderate rain
    Serial.println("Moderate rain detected.");
    digitalWrite(BUZZER_PIN, LOW);
  } else if (rainfall < 3000) { // Drizzling
    Serial.println("Drizzling detected.");
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    digitalWrite(BUZZER_PIN, LOW); // No rainfall
  }

  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<html>");
    client.println("<head>");
    client.println("<style>");
    client.println("body { font-family: Arial, sans-serif; background-color: #f0f8ff; color: #333; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; margin: 0;font-size: 1.5em; }");
    client.println("header { width: 100%; padding: 20px; background-color: #003366; color: #fff; text-align: center; font-size: 2.5em; font-weight: bold; }");
    client.println(".content { display: flex; flex-direction: column; align-items: center; justify-content: center; flex-grow: 1; text-align: center; }");
    client.println("p { font-size: 2em; margin: 10px 0; }");
    client.println(".data { font-weight: bold; color: #003366; }");
    client.println(".refresh { margin-top: 20px; padding: 10px 20px; font-size: 1.2em; background-color: #003366; color: #fff; border: none; border-radius: 5px; cursor: pointer; }");
    client.println(".refresh:hover { background-color: #002244; }");
    client.println("</style>");
    client.println("</head>");
    client.println("<body>");
    client.println("<header>Environment Monitoring</header>");
    client.println("<div class='content'>");
    client.println("<p>Temperature: <span class='data'>" + String(temperature) + " °C</span></p>");
    client.println("<p>Humidity: <span class='data'>" + String(humidity) + " %</span></p>");
    client.println("<p>Rainfall: <span class='data'>" + rainfallStatus + "</span></p>");
    client.println("<p>Water Level: <span class='data'>" + waterLevelStatus + "</span></p>");
    client.println("<p>Boat Status: <span class='data'>" + boatStatus + "</span></p>");
    client.println("<button class='refresh' onclick='location.reload();'>Refresh</button>");
    client.println("</div>");
    client.println("</body>");
    client.println("</html>");

    client.stop();
  }

  delay(10000);
}
