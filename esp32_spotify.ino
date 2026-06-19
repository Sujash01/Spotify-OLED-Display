#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include "base64.h"

unsigned long lastProgressTick = 0;

const char* ssid = "Sujash";
const char* password = "9501644244";

const char* CLIENT_ID = "05d17588aa6a44c7b652fb90c5566366";
const char* CLIENT_SECRET = "a86690ff2e7540b88045397c8af077b8";
const char* REFRESH_TOKEN = "AQC4YNr2Kr1D7Ko--0AFuqPZs09mgkLH8OUHlPWIgM1xH5n629lhj0WHLD1lnq2Q-a4aMtYUBqBRSxZrHhRg9KtLx-KYP98vmezpXyu8E2aQTKesOy74O4lnAER8REd6lyQ";

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);

String accessToken = "";

String song = "";
String artist = "";
bool playing = false;

int progress_ms = 0;
int duration_ms = 1;

unsigned long lastSpotifyPoll = 0;
unsigned long lastTokenRefresh = 0;

String msToTime(int ms)
{
  int total = ms / 1000;

  int mins = total / 60;
  int secs = total % 60;

  char buf[10];

  sprintf(buf,"%d:%02d",mins,secs);

  return String(buf);
}

bool refreshAccessToken()
{
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;

  if(
    !https.begin(
      client,
      "https://accounts.spotify.com/api/token"
    )
  )
    return false;

  String auth =
    String(CLIENT_ID) +
    ":" +
    String(CLIENT_SECRET);

  String encoded =
    base64::encode(auth);

  https.addHeader(
    "Authorization",
    "Basic " + encoded
  );

  https.addHeader(
    "Content-Type",
    "application/x-www-form-urlencoded"
  );

  String body =
    "grant_type=refresh_token&refresh_token=" +
    String(REFRESH_TOKEN);

  int code = https.POST(body);

  if(code != 200)
  {
    https.end();
    return false;
  }

  DynamicJsonDocument doc(4096);

  deserializeJson(
    doc,
    https.getString()
  );

  accessToken =
    doc["access_token"]
      .as<String>();

  https.end();

  return true;
}

void getCurrentPlayback()
{
  if(accessToken == "")
    return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;

  if(
    !https.begin(
      client,
      "https://api.spotify.com/v1/me/player/currently-playing"
    )
  )
    return;

  https.addHeader(
    "Authorization",
    "Bearer " + accessToken
  );

  int code = https.GET();

  if(code == 204)
  {
    song = "";
    artist = "";
    playing = false;

    https.end();
    return;
  }

  if(code != 200)
  {
    https.end();
    return;
  }

  DynamicJsonDocument doc(16384);

  deserializeJson(
    doc,
    https.getString()
  );

  song =
    doc["item"]["name"]
      .as<String>();

  artist =
    doc["item"]["artists"][0]["name"]
      .as<String>();

  progress_ms =
    doc["progress_ms"];

  duration_ms =
    doc["item"]["duration_ms"];

  playing =
    doc["is_playing"];

  https.end();
}

void drawScreen()
{
  u8g2.clearBuffer();

  u8g2.setFont(
    u8g2_font_6x10_tf
  );

  if(song == "")
  {
    u8g2.drawStr(
      10,
      25,
      "No Spotify"
    );

    u8g2.drawStr(
      10,
      45,
      "Playback"
    );

    u8g2.sendBuffer();
    return;
  }

  String header =
    playing
    ? "> Playing"
    : "|| Paused";

  u8g2.drawStr(
    0,
    10,
    header.c_str()
  );

  u8g2.drawStr(
    0,
    25,
    song.c_str()
  );

  u8g2.drawStr(
    0,
    40,
    artist.c_str()
  );

  String timer =
    msToTime(progress_ms)
    + "/" +
    msToTime(duration_ms);

  u8g2.drawStr(
    0,
    52,
    timer.c_str()
  );

  int barWidth =
    map(
      progress_ms,
      0,
      duration_ms,
      0,
      120
    );

  u8g2.drawFrame(
    0,
    56,
    120,
    8
  );

  u8g2.drawBox(
    0,
    56,
    barWidth,
    8
  );

  u8g2.sendBuffer();
}

void setup()
{
  Serial.begin(115200);

  u8g2.begin();

  WiFi.begin(
    ssid,
    password
  );

  while(
    WiFi.status()
    != WL_CONNECTED
  )
  {
    delay(500);
  }

  refreshAccessToken();

  getCurrentPlayback();

  drawScreen();
}

void loop()
{

  if(
    playing &&
    millis() - lastProgressTick >= 1000
)
{
    progress_ms += 1000;

    if(progress_ms > duration_ms)
        progress_ms = duration_ms;

    drawScreen();

    lastProgressTick = millis();
}

  if(
    millis() - lastTokenRefresh
    > 3000000
  )
  {
    refreshAccessToken();

    lastTokenRefresh =
      millis();
  }

  if(
    millis() - lastSpotifyPoll
    > 5000
  )
  {
    getCurrentPlayback();

    drawScreen();

    lastSpotifyPoll =
      millis();
  }
}