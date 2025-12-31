// import libarays for touchscreen
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
// import libarys for WiFi and HTTP/HTTPS calls
#include <Arduino.h>

#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
// import libarys for spotify calls
#include <SpotifyArduino.h>
#include <SpotifyArduinoCert.h>
// impport librarys for image displaying
#include <FS.h>
#include <TJpg_Decoder.h>
#include <secret.h>
// Spotify Authorizing Information
char scope[] = "user-read-playback-state%20user-modify-playback-state";
const char *SpotifyMarket = "US";
const char *refreshToken = NULL;
// Global Variables 
String lastAlbumArtURL;         // Image Variables
SpotifyImage albumImage;
char *songName;                 // Song and Current playing Variables
char *previousSong;
char *artistName;
char *volumePercent;
bool shuffleState;
char *repeatState;
String albumArtURL;
char volumeBuffer[4]; 
char songPercentageBuffer[4];
const char *deviceID;
int x, y, z;            // touch screen varibles
int volume;

// Wait Time Variables
unsigned long delayBetweenReq = 10000;      // Time between requests ms (One minute)
unsigned long reqDueTime;                   // Time when req due
unsigned long delayBetweenRFT = 3600000;
unsigned long refreshTokenTime;

// Finding Devices Methods
struct SimpleDevice {
  char name[SPOTIFY_DEVICE_ID_CHAR_LENGTH];
  char id[SPOTIFY_DEVICE_ID_CHAR_LENGTH];
};
const int maxDevices = 6;

SimpleDevice deviceList[maxDevices];
int numberOfDevices = -1;

void printDevice(SpotifyDevice device) {
  Serial.print("Device ID: ");
  Serial.println(device.id);


  Serial.print("Device Name: ");
  Serial.println(device.name);

  Serial.print("Device Type: ");
  Serial.println(device.type);

  Serial.print("Is Active: ");
  // check if device is active
  if(device.isActive){
    Serial.println("Yes");
  }
  else {
    Serial.println("No");
  }
  //check if resticed from API
  Serial.println("Restricted:");
  if (device.isRestricted) {
    Serial.println("Yes, Device is restriced from API.");
  }
  else {
    Serial.println("Naw.");
  }
  //Check Volume
  Serial.println("Volume percent: ");
  Serial.println(device.volumePercent);
  Serial.println("---------------------------------------");
  if (device.isActive) {
    deviceID = device.id;
    Serial.println(deviceID);
  }
}
bool getDeviceCallback(SpotifyDevice device, int index, int numOfDevices){
  if (index == 0){
    if(numOfDevices < numberOfDevices) {      // if number of device is less than six, add to number of devices
      numberOfDevices = numOfDevices; // -1 = however many devices
    }
    else {
      numberOfDevices = maxDevices;
    }
  }
  if (index < maxDevices) {
    printDevice(device);
    Serial.println("strncpy");
    strncpy(deviceList[index].name, device.name, sizeof(deviceList[index].name)); //DO NOT use deviceList[index].name = device.name, it won't work as you expect!
    deviceList[index].name[sizeof(deviceList[index].name) - 1] = '\0';            //ensures its null terminated

    strncpy(deviceList[index].id, device.id, sizeof(deviceList[index].id));
    deviceList[index].id[sizeof(deviceList[index].id) - 1] = '\0';
    if (index == maxDevices - 1) {
      return false; // stop searching for any more devices
    }
    else {
      return true;
    }

  }
  // FORCE QUIT SEARCH
  return false;
}

// Web Creation/Access Methods 

WebServer server(80);                         // enable web server at port 80

WiFiClientSecure client;                      // create client
SpotifyArduino spotify(client, clientID, clientSecret);

String authLink = "https://accounts.spotify.com/authorize?";

// HTML code for ESP32 webpage at localIP.
const char *webPageHTML = 
  R"(<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
  </head>
  <body>
    <div>
     <a href="https://accounts.spotify.com/authorize?client_id=%s&response_type=code&redirect_uri=%s&scope=%s">spotify Auth</a>
    </div>
  </body>
</html>
)";


void handleRoot() {
  char webPage[800];
  sprintf(webPage, webPageHTML, clientID, redirectURL, scope);
  server.send(200, "text/html", webPage);
}
void  handleCallBack() {
  String code = "";
  //const char *refreshToken = NULL;
  for (int i = 0; i < server.args(); i++) {
    Serial.print(server.argName(i));
    if (server.argName(i) == "code") {
      code = server.arg(i);
      Serial.println(code);
      refreshToken = spotify.requestAccessTokens(code.c_str(), redirectURL);
    }
  }
  if (refreshToken != NULL) {
    server.send(200, "text/plain", refreshToken); 
  }
  else {
    server.send(404, "text/plain", "failed to load token");
  }
}

void handleNotFound() {
  String message = "FileNotFound\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMETHOD: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nARGUMENTS: ";
  message += server.args();
  message += "\n";
  for (int i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  Serial.print(message);
  server.send(404, "text/plain", message);
}
// root certificate for https://accounts.spotify.authorize?
const char* rootCert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n" \
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n" \
"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n" \
"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n" \
"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n" \
"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n" \
"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n" \
"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n" \
"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n" \
"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n" \
"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n" \
"1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n" \
"NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n" \
"Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n" \
"8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n" \
"pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n" \
"MrY=\n" \
"-----END CERTIFICATE-----\n";

// Activate the TFT screen
TFT_eSPI tft = TFT_eSPI();

// Activate Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_SIZE 2

int centerX = SCREEN_WIDTH / 2;
int centerY = SCREEN_HEIGHT / 2;
// Display Album Methods
// int16_t is a singed integer and uint16_t is unsigened integer that take up 16 bits. Rather than int that is 32-64 bits.
bool decodeAlbum(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  // stop decoding at half the screen
  if (y >= tft.height()) {
    return 0;     // Dont decode block
  }
  tft.pushImage(x, y, w, h, bitmap);

  return 1;     //Decode block
}
int displayAlbumCover(char *albumCoverURL) {
  uint8_t *imageFile;     // pointer that the libary will save.
  int imageSize;
  bool gotImage = spotify.getImage(albumCoverURL, &imageFile, &imageSize);
  if (gotImage) {
    Serial.println("Album Cover Found.");
    delay(2);
    //x = 0 y =120
    int jpegStatus =TJpgDec.drawJpg(15, 40, imageFile, imageSize);
    free(imageFile);      //FREE THE MEMORY
    return jpegStatus;
  }
  else {
    return -2;
  }
}

// Miscollanous Methods
void drawMessage(int x, int y, char *message) {
  tft.setTextColor(TFT_GREEN);
  tft.drawString(message, x, y);
}
void errorWarning(String location) {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.drawCentreString("Error </3" + location , centerX, centerY, FONT_SIZE);
  delay(9000);
}
void print(String message) {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.drawCentreString(message , centerX, centerY, FONT_SIZE);
  delay(3000);
}
void printMessage(int x, int y, String message) {
  tft.setTextColor(TFT_GREEN);
  tft.drawString(message, x, y);
}
void findDevices() {
  Serial.println("Free Heap: ");
  Serial.println(ESP.getFreeHeap());
  Serial.println("getting Device: ");
  int status = spotify.getDevices(getDeviceCallback);     // Prints out information about each device
  //if (status == 200) {
    //Serial.println("Successfully got devices, transfering play backbetween them");
    //for(int i = 0; i < numberOfDevices; i++) {
      //For loop is an eaxmple of swtiching between devices and having UI to swtich between 
      //spotify.transferPlayback(deviceList[i].id, true);     //true mean to play after transfer
      //delay(6000);
    //}
  //}
}
//------------Setup------------------
void setup() {
  //Start a serial communication with the Serial Monitor at a baud rate of 115200
  Serial.begin(115200);
//-------------TOUCH SCREEN SET UP------------------
  // Start the SPI for the touchscreen and init the touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  // Set the Touchscreen rotation in landscape mode
  touchscreen.setRotation(1);
  // Start the tft display
  tft.init();
  // Set the TFT display rotation in landscape mode
  tft.setRotation(1);
  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_GREEN);
  
  // Album art formating.
  TJpgDec.setJpgScale(2);     //Scale from 1,2,4,8. PERFECT VALUE OF 2
  TJpgDec.setCallback(decodeAlbum);     //Decode Image using decode function.
  TJpgDec.setSwapBytes(true);
  tft.invertDisplay(1);


//--------WIFI CONNECTION----------
  // Display that connection attempt
  tft.drawCentreString("attempting to connect to WiFI:", centerX, 30, FONT_SIZE);
  tft.drawCentreString(ssid, centerX, centerY, FONT_SIZE);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  delay(3000);
  // Attempt to connect to WiFi
  while(WiFi.status() != WL_CONNECTED) {
    tft.drawCentreString(".", centerX, centerY, FONT_SIZE);
    delay(1000);
  }
  IPAddress IP = WiFi.localIP();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.drawCentreString("Connected to:", centerX, 30, FONT_SIZE);
  tft.drawCentreString(ssid , centerX, centerY, FONT_SIZE);
  delay(3000);
  // Certify Client with the root certifcation from spotify.authorication
  client.setCACert(rootCert);
//-----------Start webpage-------------
  server.on("/", handleRoot);
  server.on("/callback/", handleCallBack);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
//------------------START CONNECTING TO API-------------------------
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  printMessage(10, 20, "VOL: ");
  printMessage(60, 20, "%");
  printMessage(170, 20, "REPEAT: ");
  printMessage(255, 20, "SONG: ");
  printMessage(310, 20, "%");
  // PREVIOUS PAUSE / PLAY NEXT
  printMessage(180, 140, "<<");     //PREIVOUS (180, 140)
  //printMessage(180, 180, "<<");
  tft.fillRect(211, 140, 2, 8, TFT_GREEN);      // PAUSE (211, 140)
  tft.fillRect(215, 140, 2, 8, TFT_GREEN);
  printMessage(240, 140, "/");
  tft.fillTriangle(265, 140, 265, 148, 268, 144, TFT_GREEN); // PLAY (265, 148)
  printMessage(290, 140, ">>");     // NEXT (290, 140)
  // VOLUME UP | DOWN
  tft.fillRect(212, 180, 2, 10, TFT_GREEN);      //PLUS SIGN VOLUME
  tft.fillRect(208, 184, 10, 2, TFT_GREEN);      //
  printMessage(240, 180, "|");
  //printMessage(240, 140, "|");
  tft.fillRect(265, 184, 10, 2, TFT_GREEN);      // minus sign VOLUME
  tft.fillRect(15, 205, 290, 5, TFT_GREEN);      // TIME ELAPSED BAR
  tft.fillRect(16, 206, 288, 3, TFT_BLACK);

  printMessage(80, 20, "SHUFFLE: ");
  Serial.println(IP);

//--------------Check if token is Refreshed--------------
}

void songDuration(CurrentlyPlaying currentlyPlaying) {
  long progress = currentlyPlaying.progressMs; // duration passed in the song
  long duration = currentlyPlaying.durationMs; // Length of Song
  
  //Serial.print("Elapsed time of song (ms): ");
  //Serial.print(progress);
  //Serial.print(" of ");
  Serial.println(duration);
  //Serial.println();
  
  float percentage = ((float)progress / (float)duration) * 100;
  float percentageBarStatus = percentage / 100;
  int clampedPercentage = (int)percentage;
  sprintf(songPercentageBuffer, "%d", clampedPercentage);      // turn int to char* by allocating a buffer with enough space for numbers 0-100
  //progress bar changer
  if (songName != previousSong) {
    tft.fillRect(16, 206, 288, 3, TFT_BLACK);
    previousSong = songName;
  }
  else {
    float barProgress = (percentageBarStatus) *  int16_t(289);
    Serial.print(barProgress);
    tft.fillRect(16, 206, barProgress, 3, TFT_GREEN);
    

  }
  
  //percent changeri
  tft.fillRect(285, 20, 20, 10, TFT_BLACK);
  drawMessage(285, 20, songPercentageBuffer);
}
void printCurrentPlaying(CurrentlyPlaying currentlyPlaying) {
  // clear display with new song.
  //tft.fillRect(0, 120, 128, 130, TFT_BLACK);
  tft.fillRect(175, 60, 150, 40, TFT_BLACK);
  Serial.println("-------------Currently Playing--------------");
  Serial.println("Is playing: ");
  if (currentlyPlaying.isPlaying) {
    Serial.print("Yup");
  }
  else {
    Serial.print("Naw");
  }
  Serial.print("Track: ");
  Serial.println(currentlyPlaying.trackName);
  songName = const_cast<char *>(currentlyPlaying.trackName);
  //drawMessage(0,120, songName);
  drawMessage(175,60, songName);
  Serial.print("Track URI: ");
  Serial.println(currentlyPlaying.trackUri);
  Serial.println();

  Serial.println("Artists: ");
  for (int i = 0; i < currentlyPlaying.numArtists; i++)
  {
      Serial.print("Name: ");
      Serial.println(currentlyPlaying.artists[i].artistName);
      artistName = const_cast<char *>(currentlyPlaying.artists[0].artistName);
      //drawMessage(0,130, artistName);
      drawMessage(175, 80, artistName);
      Serial.print("Artist URI: ");
      Serial.println(currentlyPlaying.artists[i].artistUri);
      Serial.println();
  }

  Serial.print("Album: ");
  Serial.println(currentlyPlaying.albumName);
  Serial.print("Album URI: ");
  Serial.println(currentlyPlaying.albumUri);
  Serial.println();

  if (currentlyPlaying.contextUri != NULL)
  {
      Serial.print("Context URI: ");
      Serial.println(currentlyPlaying.contextUri);
      Serial.println();
  }
  // Time Duration of song
  long progress = currentlyPlaying.progressMs; // duration passed in the song
  long duration = currentlyPlaying.durationMs; // Length of Song
  
  Serial.print("Elapsed time of song (ms): ");
  Serial.print(progress);
  Serial.print(" of ");
  Serial.println(duration);
  Serial.println();

  float percentage = ((float)progress / (float)duration) * 100;
  int clampedPercentage = (int)percentage;
  Serial.print(clampedPercentage);
  sprintf(songPercentageBuffer, "%d", clampedPercentage);      // turn int to char* by allocating a buffer with enough space for numbers 0-100
  tft.fillRect(285, 20, 20, 10, TFT_BLACK);
  drawMessage(285, 20, songPercentageBuffer);
  Serial.print("<");
  for (int j = 0; j < 50; j++)
  {
      if (clampedPercentage >= (j * 2))
      {
          Serial.print("=");
      }
      else
      {
          Serial.print("-");
      }
  }
  Serial.println(">");
  Serial.println();

  // will be in order of widest to narrowest
  // currentlyPlaying.numImages is the number of images that
  // are stored
  for (int i = 0; i < currentlyPlaying.numImages; i++)
  {
      albumImage = currentlyPlaying.albumImages[1];       ///PERFECT SIZE IS 1
      albumArtURL = currentlyPlaying.albumImages[1].url;
      Serial.println("------------------------");
      Serial.print("Album Image: ");
      Serial.println(currentlyPlaying.albumImages[i].url);
      Serial.print("Dimensions: ");
      Serial.print(currentlyPlaying.albumImages[i].width);
      Serial.print(" x ");
      Serial.print(currentlyPlaying.albumImages[i].height);
      Serial.println();
  }
  Serial.println("------------------------");
}
void findCurrentlyPlaying() {
  Serial.print("Currently Playing: ");
  int status = spotify.getCurrentlyPlaying(printCurrentPlaying, SpotifyMarket);
  if (status == 200) {
    Serial.println("Successfully found currently playing.");
    //String newAlbum = String(albumImage.url);
    String newAlbum = albumArtURL;
    Serial.println(newAlbum);
    Serial.println(lastAlbumArtURL);
    if (newAlbum != lastAlbumArtURL){
      Serial.println("Updating Album Art...");
      char *newImageURL = const_cast<char *>(albumArtURL.c_str());
      Serial.println(newImageURL);
      int displayNewImage = displayAlbumCover(newImageURL);
      if (displayNewImage == 0) {
        lastAlbumArtURL = newAlbum;
        //lastAlbumArtURL = albumArtURL;
      }
      else {
        Serial.println("Failed to display image: ");
        Serial.print(displayNewImage);
      }
    }
  }
  else if (status == 204) {
    Serial.println("No song is currently playing.");
  }
  else {
    Serial.print("Error: ");
    Serial.println(status);
  }
}
void getPlayerDetails(PlayerDetails playerDetails) {
  Serial.println("---------player details---------");
  Serial.println("Device Name:");
  Serial.println(playerDetails.device.name);
  Serial.println("VOL: ");
  Serial.println(playerDetails.device.volumePercent);
  volume = playerDetails.device.volumePercent;
  sprintf(volumeBuffer, "%d", playerDetails.device.volumePercent);      // turn int to char* by allocating a buffer with enough space for numbers 0-100
  tft.fillRect(35, 20, 20, 10, TFT_BLACK);      // DISPLAY VOLUME DETAILS
  drawMessage(35, 20, volumeBuffer);
  Serial.print("Progress (Ms): ");
  Serial.println(playerDetails.progressMs);
  Serial.print("Shuffle State: ");
    if (playerDetails.shuffleState)
    {
        shuffleState = playerDetails.shuffleState;
        tft.fillRect(130, 20, 30, 10, TFT_BLACK);
        printMessage(130, 20, "TRUE");      // SHUFFLE state (130, 20)
        Serial.println("On");
    }
    else
    {

        tft.fillRect(130, 20, 30, 10, TFT_BLACK);
        printMessage(130, 20, "FALSE");
        Serial.println("Off");
    }

    Serial.print("Repeat State: ");
    switch (playerDetails.repeateState)
    {
    case repeat_track:
        repeatState = 0;
        tft.fillRect(215, 20, 30, 10, TFT_BLACK);
        printMessage(215, 20, "TRACK");
        Serial.println("track");
        break;
    case repeat_context:
        repeatState = 0;
        tft.fillRect(215, 20, 30, 10, TFT_BLACK);
        printMessage(215, 20, "ALBUM");
        Serial.println("context");
        break;
    case repeat_off:
        repeatState = 0;
        tft.fillRect(215, 20, 30, 10, TFT_BLACK);
        printMessage(215, 20, "OFF");
        Serial.println("off");
        break;
    }

    Serial.println("------------------------");

}
void findPlayerDetails() {
  Serial.println("Getting player Details:");
  // Market can be excluded if you want e.g. spotify.getPlayerDetails()
  int status = spotify.getPlayerDetails(getPlayerDetails, SpotifyMarket);
  if (status == 200) {
    Serial.println("Successfully got player details");
  }
  else if (status == 204)  {
   Serial.println("No active player?");
  }
  else  {
    Serial.print("Error: ");
    Serial.println(status);
  }
}
void touchScreenButtons() {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point touchPoint = touchscreen.getPoint();
    x = map(touchPoint.x, 200, 3700 , 1, SCREEN_WIDTH);
    y = map(touchPoint.y, 240, 3800 , 1, SCREEN_HEIGHT);
    z = touchPoint.z;
    if ( x > 200 && x < 220) {
      if (y > 175 && y < 200) {
        if(volume < 100) {
          volume = volume + 10;
        }
        spotify.setVolume(volume);      // UP VOLUME
        //tft.fillRect(20,35,40,40,TFT_RED);
      }
    }
    if ( x > 260 && x < 285) {
      if (y > 175 && y < 200) {
        if (volume > 0) {
          volume = volume - 10;
        }
        spotify.setVolume(volume);      // UP VOLUME
        //tft.fillRect(20,35,40,40,TFT_RED);
      }
    }
    

    if ( x > 175 && x < 195) {
      if (y > 140 && y < 160) {
        spotify.previousTrack();      // perivous button
        //tft.fillRect(20,35,40,40,TFT_RED);
      }
    }
    if ( x > 205 && x < 230 ) {
      if (y > 140 && y < 160) {
        spotify.pause();
        //tft.fillRect(20,35,40,40,TFT_GREEN);
      }
    }
    if (x > 260 && x < 280 ) {
      if (y > 140 && y < 160) {
        spotify.play();
        //tft.fillRect(20,35,40,40,TFT_PINK);
      }
    }
    if (x > 291 && x < 320) {
      if (y > 140 && y < 160) {
        spotify.nextTrack();
        //tft.fillRect(20,35,40,40,TFT_BROWN);

      }
    }

    if (x > 130 && x < 160) {
      if( y > 20 && y < 30) {
        //tft.fillRect(20,35,40,40,TFT_BLUE);
        spotify.toggleShuffle(!(shuffleState));
      } 
    }
  }
}

void loop() {
  if (refreshToken == NULL) {
    server.handleClient();
  }
  
  
  if (millis() > reqDueTime && refreshToken != NULL) {
    //findDevices();
    findPlayerDetails();
    findCurrentlyPlaying();

    reqDueTime = millis() + delayBetweenReq;
  }
  if (refreshToken != NULL && deviceID != NULL) {
    int status = spotify.getCurrentlyPlaying(songDuration, SpotifyMarket);

    delay(2000);
  }
  if (refreshToken != NULL && deviceID == NULL) {
    findDevices();
    Serial.println(deviceID);
  }
  if (refreshToken != NULL && deviceID != NULL) {
    touchScreenButtons(); 
  }
}
