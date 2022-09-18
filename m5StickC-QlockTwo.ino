#include <M5StickC.h>
#include <WiFi.h>
#include "time.h"

// WiFi credentials
char const *ssid = "ssid";
char const *password = "password";

char const *ntpServer = "pool.ntp.org";
double vbat = 0.0;
int discharge, charge;

char const *weekdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// define what timezone you are in
int timeZone = 8 * 3600; // 8 for UTC/GMT+08:00, https://blog.csdn.net/Naisu_kun/article/details/115627629

// mode
enum Mode
{
    Qlock = 0,
    Simple,
    ScreenOff,
};

Mode currentMode;

long screenOnTime;
long lastRefreshTime;

RTC_TimeTypeDef RTC_TimeStruct;
RTC_DateTypeDef RTC_DateStruct;

uint8_t bat_3[] =
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
  0x00, 0x6e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0x00, 0xff,
  0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xdb, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xb7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0xdb,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
};

uint8_t bat_2[] =
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
  0x00, 0x6e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0x00, 0xff,
  0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xdb, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xb7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0xdb,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
};

uint8_t bat_1[] =
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
  0x00, 0x6e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0x00, 0xff,
  0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
  0x00, 0xb7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0xdb,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
};

int QLOCK_GREY = M5.Lcd.color565(40, 40, 40);

void buttons_code()
{
    // Button A switch the mode
    if (M5.BtnA.wasPressed())
    {
        switchMode();
    }
    // Button B doing a time resync if pressed for 2 sec
    if (M5.BtnB.pressedFor(2000))
    {
        timeSync();
    }
}

void turnOffScreen()
{
    M5.Lcd.writecommand(ST7735_DISPOFF);
    M5.Axp.ScreenBreath(0); // Brightness: 7 ~ 16
}

void turnOnScreen()
{
    M5.Lcd.writecommand(ST7735_DISPON);
    M5.Axp.ScreenBreath(16);
}

void switchMode()
{
    currentMode = Mode((currentMode + 1) % 3);
    if (currentMode == ScreenOff)
    {
        turnOffScreen();
    }
    else
    {
        turnOnScreen();
        screenOnTime = millis();
        M5.Lcd.fillScreen(BLACK);
    }
}

// Printing time to LCD
void drawSimpleClock()
{
    batteryStatus();
    M5.Lcd.setRotation(1);
    M5.Lcd.setCursor(40, 0, 2);
    M5.Lcd.println("Simple Clock");
    if (millis() >= lastRefreshTime + 1000)
    {
        M5.Lcd.setTextSize(2);
        M5.Rtc.GetTime(&RTC_TimeStruct);
        M5.Rtc.GetData(&RTC_DateStruct);
        M5.Lcd.setCursor(27, 20);
        M5.Lcd.printf("%02d:%02d:%02d", RTC_TimeStruct.Hours, RTC_TimeStruct.Minutes, RTC_TimeStruct.Seconds);
        M5.Lcd.setCursor(27, 55);
        M5.Lcd.setTextSize(1);
        M5.Lcd.printf("%04d-%02d-%02d %s", RTC_DateStruct.Year, RTC_DateStruct.Month, RTC_DateStruct.Date, weekdays[RTC_DateStruct.WeekDay]);
    }
}

void drawQlock()
{
    M5.Lcd.setRotation(0);
    M5.Rtc.GetTime(&RTC_TimeStruct);
    M5.Rtc.GetData(&RTC_DateStruct);
    drawQlockPanel(RTC_TimeStruct.Hours, RTC_TimeStruct.Minutes);
    M5.Lcd.setCursor(6, 130);
    M5.Lcd.setTextSize(1);
    M5.Lcd.printf("%02d-%02d %s", RTC_DateStruct.Month, RTC_DateStruct.Date, weekdays[RTC_DateStruct.WeekDay]);
}

// Syncing time from NTP Server
void timeSync()
{
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextSize(1);
    Serial.println("Syncing Time");
    Serial.printf("Connecting to %s ", ssid);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(20, 15);
    M5.Lcd.println("connecting WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" CONNECTED");
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(20, 15);
    M5.Lcd.println("Connected");
    // Set ntp time to local
    configTime(timeZone, 0, ntpServer);

    // Get local time
    struct tm timeInfo;
    if (getLocalTime(&timeInfo))
    {
        // Set RTC time
        RTC_TimeTypeDef TimeStruct;
        TimeStruct.Hours = timeInfo.tm_hour;
        TimeStruct.Minutes = timeInfo.tm_min;
        TimeStruct.Seconds = timeInfo.tm_sec;
        M5.Rtc.SetTime(&TimeStruct);

        RTC_DateTypeDef DateStruct;
        DateStruct.WeekDay = timeInfo.tm_wday;
        DateStruct.Month = timeInfo.tm_mon + 1;
        DateStruct.Date = timeInfo.tm_mday;
        DateStruct.Year = timeInfo.tm_year + 1900;
        M5.Rtc.SetData(&DateStruct);
        Serial.println("Time now matching NTP");
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(20, 15);
        M5.Lcd.println("S Y N C");
        delay(500);
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setCursor(40, 0, 2);
        M5.Lcd.println("Simple Clock");
    }
}

const char *clockPanel = "ITLISASAMPM"
                         "ACQUARTERDC"
                         "TWENTYFIVEX"
                         "HALFSTENFTO"
                         "PASTERUNINE"
                         "ONESIXTHREE"
                         "FOURFIVETWO"
                         "EIGHTELEVEN"
                         "SEVENTWELVE"
                         "TENSEOCLOCK";

const int R = 10;
const int C = 11;
const int INT_SIZE = 32;

typedef struct
{
    char *word;
    uint32_t encoding[4];
} Word;

// 110 characters in total, 4 int32_t needed
Word itis = {(char *)"IT IS", {0x1B, 0, 0, 0}};
Word am = {(char *)"AM", {0x180, 0, 0, 0}};
Word pm = {(char *)"PM", {0x600, 0, 0, 0}};
Word quarter = {(char *)"A QUARTER", {0xFE800, 0, 0, 0}};
Word twenty = {(char *)"TWENTY", {0xFC00000, 0, 0, 0}};
Word mFive = {(char *)"FIVE", {0xF0000000, 0, 0, 0}};
Word half = {(char *)"HALF", {0, 0x1E, 0, 0}};
Word mTen = {(char *)"TEN", {0, 0x1C0, 0, 0}};
Word to = {(char *)"TO", {0, 0xC00, 0, 0}};
Word past = {(char *)"PAST", {0, 0xF000, 0, 0}};
Word nine = {(char *)"NINE", {0, 0x780000, 0, 0}};
Word one = {(char *)"ONE", {0, 0x3800000, 0, 0}};
Word six = {(char *)"SIX", {0, 0x1C000000, 0, 0}};
Word three = {(char *)"THREE", {0, 0xE0000000, 0x3, 0}};
Word four = {(char *)"SIX", {0, 0, 0x3C, 0}};
Word hFive = {(char *)"FIVE", {0, 0, 0x3C0, 0}};
Word two = {(char *)"TWO", {0, 0, 0x1C00, 0}};
Word eight = {(char *)"EIGHT", {0, 0, 0x3E000, 0}};
Word eleven = {(char *)"ELEVEN", {0, 0, 0xFC0000, 0}};
Word seven = {(char *)"SEVEN", {0, 0, 0x1F000000, 0}};
Word twelve = {(char *)"TWELVE", {0, 0, 0xE0000000, 0x7}};
Word hTen = {(char *)"TEN", {0, 0, 0, 0x38}};
Word oclock = {(char *)"O'CLOCK", {0, 0, 0, 0x3F00}};

Word hourWords[] = {twelve, one, two, three, four, hFive, six, seven, eight, nine, hTen, eleven};

int lastUpdateTime = 0;

void drawQlockMinuteDot(int dm) // 0 ~ 4
{
    for (int i = 0; i < 4; i++)
    {
        int xc = 14 + i * 16;
        M5.Lcd.drawLine(xc - 1, 120, xc + 1, 120, i < dm ? WHITE : QLOCK_GREY);
    }
}

void drawQlockPanel(int h, int m)
{
    Word words[10] = {itis};
    int p = 1;

    words[p++] = h < 12 ? am : pm;

    int approxM = m / 5 * 5;

    if (approxM == 0)
    {
        // do nothing, o'clock
    }
    else if (approxM == 5 || approxM == 55)
    {
        words[p++] = mFive;
    }
    else if (approxM == 10 || approxM == 50)
    {
        words[p++] = mTen;
    }
    else if (approxM == 15 || approxM == 45)
    {
        words[p++] = quarter;
    }
    else if (approxM == 20 || approxM == 40)
    {
        words[p++] = twenty;
    }
    else if (approxM == 25 || approxM == 35)
    {
        words[p++] = twenty;
        words[p++] = mFive;
    }
    else // (approxM == 30)
    {
        words[p++] = half;
    }

    if (approxM > 0)
    {
        if (approxM < 30)
        {
            words[p++] = past;
        }
        else
        {
            words[p++] = to;
            h += 1;
        }
        words[p++] = hourWords[h % 12];
    }
    else
    {
        words[p++] = hourWords[h % 12];
        words[p++] = oclock;
    }

    int len = p;
    p = 0;
    uint32_t mask[4] = {0, 0, 0, 0};
    while (p <= len)
    {
        for (int i = 0; i < 4; i++)
        {
            mask[i] |= words[p].encoding[i];
        }
        p++;
    }
    
    int intSize = sizeof(*mask);
    for (int i = 0, y = 20; i < R; i++, y += 9)
    {
        for (int j = 0, x = 0; j < C; j++, x += 7)
        {
            int pos = i * C + j;
            int p = pos / INT_SIZE;
            int b = pos % INT_SIZE;
            int color = mask[p] & (1 << b) ? WHITE : QLOCK_GREY;
            M5.Lcd.drawChar(x, y, clockPanel[pos], color, BLACK, 1);
            if (pos == 104)
            {
                M5.Lcd.drawLine(x + 6, y - 1, x + 5, y, color); // o'clock
            }
        }
    }

    drawQlockMinuteDot(m - approxM);
}

void batterySaver()
{
    if (millis() > screenOnTime + 10 * 1000 && currentMode != ScreenOff)
    {
        turnOffScreen();
        currentMode = ScreenOff;
    }
}

void batteryStatus()
{
    vbat = M5.Axp.GetVbatData() * 1.1 / 1000;
    discharge = M5.Axp.GetIdischargeData() / 2;
    if (vbat >= 4)
    {
        M5.Lcd.pushImage(145, 1, 14, 8, bat_3);
    }
    else if (vbat >= 3.7)
    {
        M5.Lcd.pushImage(145, 1, 14, 8, bat_2);
    }
    else if (vbat < 3.7)
    {
        M5.Lcd.pushImage(145, 1, 14, 8, bat_1);
    }
    else
    {
    }
    // M5.Lcd.setTextColor(TFT_YELLOW);
    // M5.Lcd.setCursor(140, 12);
    // M5.Lcd.setTextSize(1);
    // M5.Lcd.println(discharge);
}

void setup()
{
    M5.begin();

    M5.Lcd.fillScreen(BLACK);

    // timeSync(); //uncomment if you want to have a timesync everytime you turn device on (if no WIFI is avail mostly bad)

    screenOnTime = millis();
    lastRefreshTime = millis();
}

void loop()
{
    M5.update();
    buttons_code();

    if (currentMode == Simple)
    {
        drawSimpleClock();
    }
    if (currentMode == Qlock)
    {
        drawQlock();
    }

    batterySaver();

    // TODO: low battery flashlight, 
    // battery: https://github.com/m5stack/m5-docs/blob/master/docs/zh_CN/api/axp192_m5stickc.md
}
