#include <Wire.h>
#include <DHT.h>

#include "MAX30105.h"
#include "heartRate.h"

#include <U8g2lib.h>

// =====================================================
// PIN CONFIGURATION
// =====================================================

#define SDA_PIN 21
#define SCL_PIN 22

#define DHT_PIN 4
#define DHT_TYPE DHT22

#define MQ135_PIN 34
#define MQ2_PIN   35

// =====================================================
// OLED
// JMD1.3A / SH1106 / 128x64
// =====================================================

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
U8G2_R0,
U8X8_PIN_NONE
);

// =====================================================
// DHT22
// =====================================================

DHT dht(DHT_PIN, DHT_TYPE);

// =====================================================
// MAX30102
// =====================================================

MAX30105 max30102;

// =====================================================
// HEART RATE
// =====================================================

const byte RATE_SIZE = 4;

byte rates[RATE_SIZE];

byte rateSpot = 0;

long lastBeat = 0;

float bpm = 0;

int averageBPM = 0;

long irValue = 0;

// =====================================================
// SENSOR VALUES
// =====================================================

float temperature = NAN;
float humidity = NAN;

int mq135Value = 0;
int mq2Value = 0;

// =====================================================
// TIMERS
// =====================================================

unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2000;

unsigned long lastSerialPrint = 0;
const unsigned long SERIAL_INTERVAL = 1000;

// =====================================================
// PERSONAL BASELINE
// =====================================================

bool baselineComplete = false;

unsigned long baselineStartTime = 0;

const unsigned long BASELINE_DURATION = 60000;

// Baseline values

float baselineHR = 0;

float baselineTemperature = 0;

float baselineHumidity = 0;

float baselineMQ135 = 0;

float baselineMQ2 = 0;

// Baseline sums

float hrSum = 0;

float temperatureSum = 0;

float humiditySum = 0;

float mq135Sum = 0;

float mq2Sum = 0;

// Valid heart-rate samples

int validHRSamples = 0;

// Number of baseline samples

int baselineSamples = 0;

// Baseline sampling timer

unsigned long lastBaselineSample = 0;

const unsigned long BASELINE_SAMPLE_INTERVAL = 1000;

// =====================================================
// EXPOSURE TRACKING
// =====================================================

bool exposureActive = false;

unsigned long exposureStartTime = 0;

unsigned long totalExposureTime = 0;

float exposureMinutes = 0;

// =====================================================
// RISK SYSTEM
// =====================================================

int environmentalScore = 0;

int physiologicalScore = 0;

int exposureScore = 0;

int personalRiskScore = 0;

String riskLevel = "LOW";

// =====================================================
// ALERT SYSTEM
// =====================================================

bool alertActive = false;

String alertMessage = "NORMAL";

unsigned long alertConditionStart = 0;

unsigned long lastAlertTime = 0;

// Condition must remain elevated
// for 10 seconds before alert

const unsigned long ALERT_CONFIRM_TIME = 10000;

// Prevent repeated serial alerts

const unsigned long ALERT_COOLDOWN = 60000;

// =====================================================
// SCREEN MANAGEMENT
// =====================================================

unsigned long lastScreenChange = 0;

const unsigned long SCREEN_INTERVAL = 4000;

int currentScreen = 0;

// =====================================================
// FUNCTION DECLARATIONS
// =====================================================

void readDHT();

void readHeartRate();

void collectBaseline();

void calculateRisk();

void updateExposure();

void updateAlert();

void updateOLED();

void printSerialData();

// =====================================================
// SETUP
// =====================================================

void setup()
{
Serial.begin(115200);

delay(1000);

Serial.println();
Serial.println("========================================");
Serial.println("       AI HEALTH MONITOR");
Serial.println("========================================");

// ===================================================
// I2C
// ===================================================

Wire.begin(SDA_PIN, SCL_PIN);

Serial.println("I2C initialized");

Serial.println("SDA = GPIO 21");

Serial.println("SCL = GPIO 22");

// ===================================================
// OLED
// ===================================================

Serial.println();

Serial.println("Initializing SH1106 OLED...");

display.begin();

Serial.println("SH1106 OLED initialized");

// ===================================================
// STARTUP SCREEN
// ===================================================

display.clearBuffer();

display.setFont(
u8g2_font_6x10_tf
);

display.drawStr(
0,
10,
"AI HEALTH MONITOR"
);

display.drawStr(
0,
25,
"Starting system..."
);

display.drawStr(
0,
40,
"Please wait..."
);

display.sendBuffer();

delay(2000);

// ===================================================
// DHT22
// ===================================================

Serial.println();

Serial.println("Initializing DHT22...");

dht.begin();

Serial.println("DHT22 initialized");

// ===================================================
// MAX30102
// ===================================================

Serial.println();

Serial.println("Initializing MAX30102...");

if (!max30102.begin(
Wire,
I2C_SPEED_FAST))
{
Serial.println("MAX30102 NOT FOUND!");

Serial.println();

Serial.println("Check:");

Serial.println("VIN  -> 5V");

Serial.println("GND  -> GND");

Serial.println("SDA  -> GPIO 21");

Serial.println("SCL  -> GPIO 22");


while (1)
{
  display.clearBuffer();

  display.setFont(
    u8g2_font_6x10_tf
  );

  display.drawStr(
    0,
    20,
    "MAX30102 ERROR"
  );

  display.drawStr(
    0,
    35,
    "Check wiring"
  );

  display.sendBuffer();

  delay(1000);
}

}

Serial.println("MAX30102 detected!");

// ===================================================
// MAX30102 CONFIGURATION
// ===================================================

max30102.setup();

max30102.setPulseAmplitudeRed(0x0A);

max30102.setPulseAmplitudeGreen(0);

Serial.println("MAX30102 configured");

// ===================================================
// ADC
// ===================================================

analogReadResolution(12);

Serial.println("ADC = 12 bit");

// ===================================================
// HEART RATE ARRAY
// ===================================================

for (byte i = 0; i < RATE_SIZE; i++)
{
rates[i] = 0;
}

// ===================================================
// START BASELINE
// ===================================================

baselineStartTime = millis();

baselineComplete = false;

baselineSamples = 0;

validHRSamples = 0;

hrSum = 0;

temperatureSum = 0;

humiditySum = 0;

mq135Sum = 0;

mq2Sum = 0;

Serial.println();

Serial.println("========================================");

Serial.println("PERSONAL BASELINE STARTED");

Serial.println("Duration: 60 seconds");

Serial.println("Please remain still");

Serial.println("Keep finger on MAX30102");

Serial.println("========================================");

// ===================================================
// BASELINE OLED
// ===================================================

display.clearBuffer();

display.setFont(
u8g2_font_6x10_tf
);

display.drawStr(
0,
10,
"AI HEALTH MONITOR"
);

display.drawStr(
0,
25,
"CALIBRATING..."
);

display.drawStr(
0,
40,
"Keep finger steady"
);

display.drawStr(
0,
55,
"Baseline: 60 sec"
);

display.sendBuffer();

delay(1000);
}

// =====================================================
// READ DHT22
// =====================================================

void readDHT()
{
temperature = dht.readTemperature();

humidity = dht.readHumidity();

if (isnan(temperature))
{
Serial.println(
"DHT22 temperature error"
);
}

if (isnan(humidity))
{
Serial.println(
"DHT22 humidity error"
);
}
}

// =====================================================
// READ HEART RATE
// =====================================================

void readHeartRate()
{
irValue = max30102.getIR();

// ===================================================
// FINGER DETECTED
// ===================================================

if (irValue > 5000)
{
if (checkForBeat(irValue))
{
long currentTime = millis();

  long delta =
    currentTime - lastBeat;


  lastBeat = currentTime;


  if (delta > 0)
  {
    bpm =
      60.0 /
      (delta / 1000.0);


    // =================================================
    // VALID BPM
    // =================================================

    if (bpm > 20 && bpm < 255)
    {
      rates[rateSpot] =
        (byte)bpm;


      rateSpot++;

      rateSpot %= RATE_SIZE;


      // =================================================
      // AVERAGE BPM
      // =================================================

      averageBPM = 0;

      byte count = 0;


      for (
        byte i = 0;
        i < RATE_SIZE;
        i++
      )
      {
        if (rates[i] > 0)
        {
          averageBPM +=
            rates[i];

          count++;
        }
      }


      if (count > 0)
      {
        averageBPM /=
          count;
      }
    }
  }
}

}

// ===================================================
// NO FINGER
// ===================================================

else
{
bpm = 0;

averageBPM = 0;


for (
  byte i = 0;
  i < RATE_SIZE;
  i++
)
{
  rates[i] = 0;
}


rateSpot = 0;

}
}

// =====================================================
// COLLECT PERSONAL BASELINE
// =====================================================

void collectBaseline()
{
unsigned long elapsed =
millis() - baselineStartTime;

// ===================================================
// SAMPLE ONCE PER SECOND
// ===================================================

if (
millis() - lastBaselineSample
< BASELINE_SAMPLE_INTERVAL
)
{
return;
}

lastBaselineSample = millis();

// ===================================================
// HEART RATE
// ===================================================

if (averageBPM > 0)
{
hrSum += averageBPM;

validHRSamples++;

}

// ===================================================
// TEMPERATURE
// ===================================================

if (!isnan(temperature))
{
temperatureSum += temperature;
}

// ===================================================
// HUMIDITY
// ===================================================

if (!isnan(humidity))
{
humiditySum += humidity;
}

// ===================================================
// MQ135
// ===================================================

mq135Sum += mq135Value;

// ===================================================
// MQ2
// ===================================================

mq2Sum += mq2Value;

baselineSamples++;

// ===================================================
// REMAINING TIME
// ===================================================

int secondsRemaining =
60 - (elapsed / 1000);

if (secondsRemaining < 0)
{
secondsRemaining = 0;
}

// ===================================================
// BASELINE OLED
// ===================================================

display.clearBuffer();

display.setFont(
u8g2_font_6x10_tf
);

display.drawStr(
0,
10,
"AI HEALTH MONITOR"
);

display.drawStr(
0,
25,
"CALIBRATING..."
);

char timeText[25];

sprintf(
timeText,
"Time left: %02d sec",
secondsRemaining
);

display.drawStr(
0,
40,
timeText
);

display.drawStr(
0,
55,
"Keep finger steady"
);

display.sendBuffer();

// ===================================================
// BASELINE COMPLETE
// ===================================================

if (elapsed >= BASELINE_DURATION)
{
// -------------------------------------------------
// ENVIRONMENT BASELINES
// -------------------------------------------------

if (baselineSamples > 0)
{
  baselineTemperature =
    temperatureSum /
    baselineSamples;


  baselineHumidity =
    humiditySum /
    baselineSamples;


  baselineMQ135 =
    mq135Sum /
    baselineSamples;


  baselineMQ2 =
    mq2Sum /
    baselineSamples;
}


// -------------------------------------------------
// HEART RATE BASELINE
// -------------------------------------------------

if (validHRSamples > 0)
{
  baselineHR =
    hrSum /
    validHRSamples;
}
else
{
  baselineHR = 0;
}


baselineComplete = true;


// -------------------------------------------------
// SERIAL OUTPUT
// -------------------------------------------------

Serial.println();

Serial.println(
  "========================================"
);

Serial.println(
  "PERSONAL BASELINE COMPLETE"
);

Serial.println(
  "========================================"
);


Serial.print(
  "Baseline HR: "
);

Serial.println(
  baselineHR,
  1
);


Serial.print(
  "Baseline Temperature: "
);

Serial.println(
  baselineTemperature,
  1
);


Serial.print(
  "Baseline Humidity: "
);

Serial.println(
  baselineHumidity,
  1
);


Serial.print(
  "Baseline MQ135: "
);

Serial.println(
  baselineMQ135,
  1
);


Serial.print(
  "Baseline MQ2: "
);

Serial.println(
  baselineMQ2,
  1
);


Serial.print(
  "Valid HR samples: "
);

Serial.println(
  validHRSamples
);


Serial.println(
  "Personalized monitoring STARTED"
);


// -------------------------------------------------
// RESET EXPOSURE
// -------------------------------------------------

exposureStartTime = millis();

totalExposureTime = 0;

exposureMinutes = 0;

exposureActive = false;

}
}

// =====================================================
// EXPOSURE TRACKING
// =====================================================

void updateExposure()
{
if (!baselineComplete)
{
return;
}

// ===================================================
// ENVIRONMENTAL ELEVATION
// ===================================================

bool environmentElevated = false;

// MQ135 > 20% above baseline

if (
baselineMQ135 > 0 &&
mq135Value >
baselineMQ135 * 1.20
)
{
environmentElevated = true;
}

// MQ2 > 20% above baseline

if (
baselineMQ2 > 0 &&
mq2Value >
baselineMQ2 * 1.20
)
{
environmentElevated = true;
}

// ===================================================
// START / CONTINUE EXPOSURE
// ===================================================

if (environmentElevated)
{
if (!exposureActive)
{
exposureActive = true;

  exposureStartTime =
    millis();
}


totalExposureTime =
  millis() -
  exposureStartTime;

}

// ===================================================
// ENVIRONMENT NORMAL
// ===================================================

else
{
exposureActive = false;

/*
  IMPORTANT:

  We intentionally keep totalExposureTime.

  This means exposure accumulates during
  the current monitoring session.
*/

}

exposureMinutes =
totalExposureTime /
60000.0;
}

// =====================================================
// CALCULATE PERSONALIZED RISK
// =====================================================

void calculateRisk()
{
if (!baselineComplete)
{
return;
}

// ===================================================
// ENVIRONMENTAL SCORE
// MAX = 50
// ===================================================

environmentalScore = 0;

// ---------------------------------------------------
// MQ135 deviation
// ---------------------------------------------------

float mq135Deviation = 0;

if (baselineMQ135 > 0)
{
mq135Deviation =
(
(mq135Value -
baselineMQ135)
/
baselineMQ135
) * 100.0;
}

if (mq135Deviation >= 100)
{
environmentalScore += 25;
}
else if (mq135Deviation >= 50)
{
environmentalScore += 18;
}
else if (mq135Deviation >= 20)
{
environmentalScore += 10;
}

// ---------------------------------------------------
// MQ2 deviation
// ---------------------------------------------------

float mq2Deviation = 0;

if (baselineMQ2 > 0)
{
mq2Deviation =
(
(mq2Value -
baselineMQ2)
/
baselineMQ2
) * 100.0;
}

if (mq2Deviation >= 100)
{
environmentalScore += 25;
}
else if (mq2Deviation >= 50)
{
environmentalScore += 18;
}
else if (mq2Deviation >= 20)
{
environmentalScore += 10;
}

if (environmentalScore > 50)
{
environmentalScore = 50;
}

// ===================================================
// PHYSIOLOGICAL SCORE
// MAX = 30
// ===================================================

physiologicalScore = 0;

if (
baselineHR > 0 &&
averageBPM > 0
)
{
float hrChange =
(
(averageBPM -
baselineHR)
/
baselineHR
) * 100.0;

if (hrChange >= 30)
{
  physiologicalScore = 30;
}
else if (hrChange >= 20)
{
  physiologicalScore = 20;
}
else if (hrChange >= 10)
{
  physiologicalScore = 10;
}

}

// ===================================================
// EXPOSURE SCORE
// MAX = 20
// ===================================================

exposureScore = 0;

if (exposureMinutes >= 60)
{
exposureScore = 20;
}
else if (exposureMinutes >= 30)
{
exposureScore = 15;
}
else if (exposureMinutes >= 15)
{
exposureScore = 10;
}
else if (exposureMinutes >= 5)
{
exposureScore = 5;
}

// ===================================================
// TOTAL
// ===================================================

personalRiskScore =
environmentalScore +
physiologicalScore +
exposureScore;

if (personalRiskScore > 100)
{
personalRiskScore = 100;
}

// ===================================================
// RISK LEVEL
// ===================================================

if (personalRiskScore <= 30)
{
riskLevel = "LOW";
}
else if (personalRiskScore <= 60)
{
riskLevel = "MODERATE";
}
else
{
riskLevel = "HIGH";
}
}

// =====================================================
// PERSONALIZED ALERT SYSTEM
// =====================================================

void updateAlert()
{
if (!baselineComplete)
{
return;
}

// ===================================================
// LOW RISK
// ===================================================

if (personalRiskScore <= 30)
{
alertActive = false;

alertMessage = "NORMAL";

alertConditionStart = 0;

return;

}

// ===================================================
// ELEVATED CONDITION START
// ===================================================

if (alertConditionStart == 0)
{
alertConditionStart = millis();
}

unsigned long conditionDuration =
millis() -
alertConditionStart;

// ===================================================
// WAIT 10 SECONDS
// ===================================================

if (
conditionDuration <
ALERT_CONFIRM_TIME
)
{
return;
}

// ===================================================
// MODERATE
// ===================================================

if (
personalRiskScore > 30 &&
personalRiskScore <= 60
)
{
alertActive = true;

alertMessage =
  "EXPOSURE ELEVATED";


if (
  millis() - lastAlertTime
  >= ALERT_COOLDOWN
)
{
  Serial.println();

  Serial.println(
    "******** ALERT ********"
  );

  Serial.println(
    "Environmental exposure elevated."
  );

  Serial.println(
    "Consider moving to cleaner air."
  );

  Serial.println(
    "************************"
  );


  lastAlertTime =
    millis();
}

}

// ===================================================
// HIGH
// ===================================================

else if (personalRiskScore > 60)
{
alertActive = true;

alertMessage =
  "HIGH EXPOSURE";


if (
  millis() - lastAlertTime
  >= ALERT_COOLDOWN
)
{
  Serial.println();

  Serial.println(
    "!!!!!!!! HIGH ALERT !!!!!!!!"
  );

  Serial.println(
    "Personal risk indicator HIGH."
  );

  Serial.println(
    "Move to cleaner air."
  );

  Serial.println(
    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
  );


  lastAlertTime =
    millis();
}

}
}

// =====================================================
// OLED UPDATE
// =====================================================

void updateOLED()
{
if (!baselineComplete)
{
return;
}

// ===================================================
// ALERT SCREEN
// ===================================================

if (alertActive)
{
display.clearBuffer();

display.setFont(
  u8g2_font_6x10_tf
);


// -------------------------------------------------
// HIGH ALERT
// -------------------------------------------------

if (personalRiskScore > 60)
{
  display.drawStr(
    0,
    10,
    "!! HIGH EXPOSURE !!"
  );


  display.drawStr(
    0,
    24,
    "Risk:"
  );


  char riskText[20];


  sprintf(
    riskText,
    "%d HIGH",
    personalRiskScore
  );


  display.drawStr(
    38,
    24,
    riskText
  );


  display.drawStr(
    0,
    39,
    "Exposure elevated"
  );


  display.drawStr(
    0,
    55,
    "MOVE TO CLEANER AIR"
  );
}


// -------------------------------------------------
// MODERATE ALERT
// -------------------------------------------------

else
{
  display.drawStr(
    0,
    10,
    "EXPOSURE ELEVATED"
  );


  display.drawStr(
    0,
    24,
    "Risk:"
  );


  char riskText[20];


  sprintf(
    riskText,
    "%d MODERATE",
    personalRiskScore
  );


  display.drawStr(
    38,
    24,
    riskText
  );


  display.drawStr(
    0,
    39,
    "Reduce exposure"
  );


  display.drawStr(
    0,
    55,
    "MOVE TO CLEANER AIR"
  );
}


display.sendBuffer();


// IMPORTANT:
// Don't allow normal screens
// to overwrite the alert.

return;

}

// ===================================================
// NORMAL SCREEN 0
// ===================================================

if (currentScreen == 0)
{
display.clearBuffer();

display.setFont(
  u8g2_font_6x10_tf
);


display.drawStr(
  0,
  9,
  "AI HEALTH MONITOR"
);


char hrText[22];


if (averageBPM > 0)
{
  sprintf(
    hrText,
    "HR: %d BPM",
    averageBPM
  );
}
else
{
  strcpy(
    hrText,
    "HR: -- BPM"
  );
}


display.drawStr(
  0,
  22,
  hrText
);


char airText[22];


if (environmentalScore >= 30)
{
  strcpy(
    airText,
    "AIR: ELEVATED"
  );
}
else
{
  strcpy(
    airText,
    "AIR: NORMAL"
  );
}


display.drawStr(
  0,
  34,
  airText
);


char exposureText[25];


sprintf(
  exposureText,
  "EXP: %.1f min",
  exposureMinutes
);


display.drawStr(
  0,
  46,
  exposureText
);


char riskText[30];


sprintf(
  riskText,
  "RISK: %d %s",
  personalRiskScore,
  riskLevel.c_str()
);


display.drawStr(
  0,
  59,
  riskText
);


display.sendBuffer();

}

// ===================================================
// NORMAL SCREEN 1
// ENVIRONMENT
// ===================================================

else if (currentScreen == 1)
{
display.clearBuffer();

display.setFont(
  u8g2_font_6x10_tf
);


display.drawStr(
  0,
  9,
  "ENVIRONMENT"
);


char tempText[25];


if (!isnan(temperature))
{
  sprintf(
    tempText,
    "TEMP: %.1f C",
    temperature
  );
}
else
{
  strcpy(
    tempText,
    "TEMP: ERROR"
  );
}


display.drawStr(
  0,
  21,
  tempText
);


char humText[25];


if (!isnan(humidity))
{
  sprintf(
    humText,
    "HUM : %.0f %%",
    humidity
  );
}
else
{
  strcpy(
    humText,
    "HUM : ERROR"
  );
}


display.drawStr(
  0,
  33,
  humText
);


char mq135Text[25];


sprintf(
  mq135Text,
  "MQ135: %d",
  mq135Value
);


display.drawStr(
  0,
  45,
  mq135Text
);


char mq2Text[25];


sprintf(
  mq2Text,
  "MQ2: %d",
  mq2Value
);


display.drawStr(
  0,
  57,
  mq2Text
);


display.sendBuffer();

}

// ===================================================
// NORMAL SCREEN 2
// PERSONAL RESPONSE
// ===================================================

else
{
display.clearBuffer();

display.setFont(
  u8g2_font_6x10_tf
);


display.drawStr(
  0,
  9,
  "PERSONAL RESPONSE"
);


char currentHRText[25];


sprintf(
  currentHRText,
  "CURRENT HR: %d",
  averageBPM
);


display.drawStr(
  0,
  21,
  currentHRText
);


char baselineHRText[25];


sprintf(
  baselineHRText,
  "BASE HR: %.0f",
  baselineHR
);


display.drawStr(
  0,
  33,
  baselineHRText
);


float hrChange = 0;


if (
  baselineHR > 0 &&
  averageBPM > 0
)
{
  hrChange =
    (
      (averageBPM -
       baselineHR)
      /
      baselineHR
    ) * 100.0;
}


char changeText[25];


sprintf(
  changeText,
  "CHANGE: %.1f%%",
  hrChange
);


display.drawStr(
  0,
  45,
  changeText
);


char statusText[25];


if (physiologicalScore >= 20)
{
  strcpy(
    statusText,
    "RESPONSE: ELEVATED"
  );
}
else
{
  strcpy(
    statusText,
    "RESPONSE: STABLE"
  );
}


display.drawStr(
  0,
  57,
  statusText
);


display.sendBuffer();

}
}

// =====================================================
// SERIAL OUTPUT
// =====================================================

void printSerialData()
{
Serial.println();

Serial.println(
"----------------------------------------"
);

Serial.println(
"LIVE MONITORING"
);

// ===================================================
// ENVIRONMENT
// ===================================================

if (!isnan(temperature))
{
Serial.print(
"Temperature: "
);

Serial.print(
  temperature,
  1
);

Serial.println(" C");

}

if (!isnan(humidity))
{
Serial.print(
"Humidity: "
);

Serial.print(
  humidity,
  1
);

Serial.println(" %");

}

Serial.print(
"MQ135: "
);

Serial.println(
mq135Value
);

Serial.print(
"MQ2: "
);

Serial.println(
mq2Value
);

// ===================================================
// MAX30102
// ===================================================

Serial.print(
"MAX30102 IR: "
);

Serial.println(
irValue
);

Serial.print(
"Current HR: "
);

Serial.println(
averageBPM
);

// ===================================================
// BASELINE
// ===================================================

if (baselineComplete)
{
Serial.println();

Serial.println(
  "PERSONAL BASELINE"
);


Serial.print(
  "Baseline HR: "
);

Serial.println(
  baselineHR,
  1
);


Serial.print(
  "Baseline Temperature: "
);

Serial.println(
  baselineTemperature,
  1
);


Serial.print(
  "Baseline Humidity: "
);

Serial.println(
  baselineHumidity,
  1
);


Serial.print(
  "Baseline MQ135: "
);

Serial.println(
  baselineMQ135,
  1
);


Serial.print(
  "Baseline MQ2: "
);

Serial.println(
  baselineMQ2,
  1
);


// =================================================
// HR CHANGE
// =================================================

if (
  baselineHR > 0 &&
  averageBPM > 0
)
{
  float hrChange =
    (
      (averageBPM -
       baselineHR)
      /
      baselineHR
    ) * 100.0;


  Serial.print(
    "HR Change: "
  );

  Serial.print(
    hrChange,
    1
  );

  Serial.println(" %");
}


// =================================================
// RISK
// =================================================

Serial.println();

Serial.println(
  "RISK ANALYSIS"
);


Serial.print(
  "Environmental Score: "
);

Serial.println(
  environmentalScore
);


Serial.print(
  "Physiological Score: "
);

Serial.println(
  physiologicalScore
);


Serial.print(
  "Exposure Score: "
);

Serial.println(
  exposureScore
);


Serial.print(
  "Exposure Time: "
);

Serial.print(
  exposureMinutes,
  1
);

Serial.println(" min");


Serial.print(
  "PERSONAL RISK: "
);

Serial.print(
  personalRiskScore
);

Serial.print(
  " - "
);

Serial.println(
  riskLevel
);


// =================================================
// ALERT
// =================================================

Serial.print(
  "ALERT STATUS: "
);

if (alertActive)
{
  Serial.println(
    alertMessage
  );
}
else
{
  Serial.println(
    "NORMAL"
  );
}

}

Serial.println(
"----------------------------------------"
);
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop()
{
// ===================================================
// DHT22
// ===================================================

if (
millis() - lastDHTRead
>= DHT_INTERVAL
)
{
lastDHTRead = millis();

readDHT();

}

// ===================================================
// MQ135
// ===================================================

mq135Value =
analogRead(MQ135_PIN);

// ===================================================
// MQ2
// ===================================================

mq2Value =
analogRead(MQ2_PIN);

// ===================================================
// MAX30102
// ===================================================

readHeartRate();

// ===================================================
// BASELINE
// ===================================================

if (!baselineComplete)
{
collectBaseline();
}

// ===================================================
// PERSONALIZED MONITORING
// ===================================================

else
{
updateExposure();

calculateRisk();

updateAlert();

}

// ===================================================
// SCREEN ROTATION
// ===================================================

if (
millis() - lastScreenChange
>= SCREEN_INTERVAL
)
{
lastScreenChange = millis();

currentScreen++;


if (currentScreen > 2)
{
  currentScreen = 0;
}

}

// ===================================================
// OLED
// ===================================================

updateOLED();

// ===================================================
// SERIAL
// ===================================================

if (
millis() - lastSerialPrint
>= SERIAL_INTERVAL
)
{
lastSerialPrint = millis();

printSerialData();

}

delay(20);
}