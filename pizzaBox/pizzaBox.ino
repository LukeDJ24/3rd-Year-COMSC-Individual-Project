#include <SPI.h>
#include <WiFiNINA.h>
#include <SparkFun_UHF_RFID_Reader.h> // Library for controlling the M6E Nano module
#include <Vector.h>                   // storage handling

// include the LCD library code:
#include "Wire.h"
#include "Adafruit_LiquidCrystal.h"

// Connect via i2c, default address #0 (A0-A2 not jumpered)
Adafruit_LiquidCrystal lcd(0);

#include "arduino_secrets.h" // header file containing WiFi SSID and password. Used to maintain privacy of WiFi credentials
char ssid[] = SECRET_SSID;        // your network SSID (name)
char password[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;
WiFiClient client;

// EDIT: 'Server' address to match your domain
char server[] = "192.168.1.81"; // This could also be 192.168.1.18/~me if you are running a server on your computer on a local network.

// This is the data that will be passed into your POST and matches your mysql column
String yourarduinodata = "";
String yourdatacolumn = "yourdata=";
String yourdata;

// ==== supporting routines
void serialTrigger(String message);

#define EPC_COUNT 50        // Max number of unique EPC's
#define EPC_ENTRY 12        // max bytes in EPC
 /* Option: size of HTML buffer */
#define HTML_BUFFER 400

String header = "";

typedef struct Epcrecv {
    uint8_t epc[EPC_ENTRY];         // save EPC
    uint8_t cnt;                    // how often detected
} Epcrecv;

Epcrecv epc_space[EPC_COUNT];       // allocate the space
Vector <Epcrecv> epcs;              // create vector

/////////////////////////////////////////////////
//            RFID shield Definitions          //
/////////////////////////////////////////////////

#include <SoftwareSerial.h> //Used for transmitting to the device

SoftwareSerial softSerial(2, 3); //RX, TX

  /* Option: define the serial speed to communicate with the shield */
#define RFID_SERIAL_SPEED 38400

 /* MUST: Define the region in the world you are in
  * It defines the frequency to use that is available in your part of the world
  *
  * Valid options are :
  * REGION_INDIA, REGION_JAPAN, REGION_CHINA, REGION_EUROPE, REGION_KOREA,
  * REGION_AUSTRALIA, REGION_NEWZEALAND, REGION_NORTHAMERICA
  */
#define RFID_Region REGION_EUROPE

  /* Option: define the Readpower between 0 and 27dBm. if only using the
   * USB power, do not go above 5dbm (=500) */
#define RFID_POWER 500

 /* Option: display debug level/information
  * 0 = no debug information
  * 1 = tag info found
  * 2 = 1 + programflow + temperature info
  * 3 = 2 + Nano debug  */
#define PRMDEBUG 1

int buttonPin = 12;

RFID nano;

#define SERIAL 115200

void setup() {
  // set up the LCD's number of rows and columns: 
  lcd.begin(16, 2);
  lcd.setBacklight(HIGH);

  // Print a message to the LCD.
  lcd.print("Please order...");
  
  Serial.begin(SERIAL);

  while (!Serial); //Wait for the serial port to come online
  
  //Set up the RFID sheild
  if (setupNano(38400) == false) //Configure nano to run at 38400bps
  {
    Serial.println(F("Module failed to respond. Please check wiring."));
    while (1); //Freeze!
  }

  while (!Serial); //Wait for the serial port to come online

  if (setupNano(38400) == false) //Configure nano to run at 38400bps
  {
    Serial.println(F("Module failed to respond. Please check wiring."));
    while (1); //Freeze!
  }
  
  pinMode(buttonPin, INPUT_PULLUP);

  Array_Init();       // initialize array
  
  connectWifi();      // initialize WiFi connection
  
}

void loop() {

  lcd.setCursor(0, 0);

  int buttonValue = digitalRead(buttonPin);

  if(digitalRead(buttonPin) == LOW){
  Serial.println("button has been pressed!");
  lcd.print("Button pressed!");
  //for (int x = 0; x < 1000 ;x++){
    Check_EPC();
  //}

   postData();
  } else {
     Serial.println("Waiting for button to be pressed...");
     lcd.print("Please order...");
  }
  delay(1000);
}

void connectWifi() {
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < "1.0.0") {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(ssid, password);

    // wait 10 seconds for connection:
    delay(10000);
  }

  // you're connected now, so print out the data:
  Serial.print("You're connected to the network");
  printWifiStatus();
}

void printWifiStatus() {
  // Print the SSID of the network you're attached to
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // Print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// This method makes a HTTP connection to the server and POSTs data
void postData() {
  // Combine yourdatacolumn header (yourdata=) with the data recorded from your arduino
  // (yourarduinodata) and package them into the String yourdata which is what will be
  // sent in your POST request
  vector2string();
  yourdata = yourdatacolumn + yourarduinodata;

  // If there's a successful connection, send the HTTP POST request
  if (client.connect(server, 80)) {
    Serial.println("connecting...");

    // EDIT: The POST 'URL' to the location of your insert_mysql.php on your web-host
    client.println("POST /rfiddata.php HTTP/1.1");

    // EDIT: 'Host' to match your domain
    client.println("Host: 192.168.1.81");
    client.println("User-Agent: Arduino/1.0");
    client.println("Connection: close");
    client.println("Content-Type: application/x-www-form-urlencoded;");
    client.print("Content-Length: ");
    client.println(yourdata.length());
    client.println();
    client.println(yourdata); 
  } 
  else {
    // If you couldn't make a connection:
    Serial.println("Connection failed");
    Serial.println("Disconnecting.");
    client.stop();
  }
}

////////////////////////////////////////////////////////////
//                   ARRAY routines                       //
////////////////////////////////////////////////////////////

/* initialize the array to capture detected EPC */
void Array_Init()
{
  epcs.setStorage(epc_space);
}

/* count number of entries with unique EPC received */
int Array_Cnt()
{
  return((int) epcs.size());
}

/* Empty the EPC array */
void Array_Clr()
{
  epcs.clear();
}

/* Remove a single entry from the array */
void Array_Rmv_Entry(int i)
{
  epcs.remove(i);
}

/* add entry to array (if not there already) */
void Array_Add(uint8_t *msg, byte mlength)
{
    bool found = false;
    byte j;
    size_t i;

    for (i = 0; i < epcs.size(); i++)
    {
        found = true;

        // check each entry for a match
        for (j = 0 ; j < mlength, j < EPC_ENTRY ; j++)
        {
          if (epcs[i].epc[j] != msg[j])
          {
            found = false;  // indicate not found
            break;
          }
        }

        if (found)
        {
          if (PRMDEBUG > 1)
          {
            Serial.print(F("FOUND EPC in array entry "));
            Serial.println(i);
          }
          epcs[i].cnt++;
          return;
        }
    }

  // debug info
  if (PRMDEBUG > 1) Serial.println(F("Not found adding"));

  if ( i >= epcs.max_size())
  {
    Serial.println(F("Can not add more"));
    return;
  }

  // add new entry in array
  Epcrecv newepc;
  for (j = 0 ; j < EPC_ENTRY ; j++) newepc.epc[j] = msg[j];
  newepc.cnt = 0;
  epcs.push_back(newepc);     // append to end
}

void Check_EPC()
{
  Serial.print("checking EPC");
  uint8_t myEPC[EPC_ENTRY];     // Most EPCs are 12 bytes

  //Serial.print("In the check epc fucktion");

  if (nano.check() == true)     // Check to see if any new data has come in from module
  {
    byte responseType = nano.parseResponse(); //Break response into tag ID, RSSI, frequency, and timestamp

    if (responseType == RESPONSE_IS_KEEPALIVE)
    {
      if (PRMDEBUG > 1) Serial.println(F("Scanning"));
    }
    else if (responseType == RESPONSE_IS_TAGFOUND)
    {
      // extract EPC
      for (byte x = 0 ; x < EPC_ENTRY ; x++)   myEPC[x] = nano.msg[31 + x];

      // add to array (if not already there)
      Array_Add(myEPC, EPC_ENTRY);

      // display for debug information
      if (PRMDEBUG > 0)
      {
        int rssi = nano.getTagRSSI();            // Get the RSSI for this tag read
        long freq = nano.getTagFreq();           // Get the frequency this tag was detected at
        long timeStamp = nano.getTagTimestamp(); // Get the time this was read, (ms) since last keep-alive message

        Serial.print(F(" rssi["));
        Serial.print(rssi);
        Serial.print(F("] freq["));

        Serial.print(freq);
        Serial.print(F("] time["));

        Serial.print(timeStamp);
        Serial.print(F("] epc["));

        // Print EPC bytes,
        for (byte x = 0 ; x < EPC_ENTRY ; x++)
        {
          if (myEPC[x] < 0x10) Serial.print(F("0")); // Pretty print adding zero
          Serial.print(myEPC[x], HEX);
          Serial.print(F(" "));
        }
        Serial.println("]");
      }
    }
    else                                             // Unknown response
    {
      if (PRMDEBUG > 1) nano.printMessageArray();    // Print the response message. Look up errors in tmr__status_8h.html
    }
  }
}

//Gracefully handles a reader that is already configured and already reading continuously
//Because Stream does not have a .begin() we have to do this outside the library
boolean setupNano(long baudRate)
{
  nano.begin(softSerial); //Tell the library to communicate over software serial port

  //Test to see if we are already connected to a module
  //This would be the case if the Arduino has been reprogrammed and the module has stayed powered
  softSerial.begin(baudRate); //For this test, assume module is already at our desired baud rate
  while (softSerial.isListening() == false); //Wait for port to open

  //About 200ms from power on the module will send its firmware version at 115200. We need to ignore this.
  while (softSerial.available()) softSerial.read();

  nano.getVersion();

  if (nano.msg[0] == ERROR_WRONG_OPCODE_RESPONSE)
  {
    //This happens if the baud rate is correct but the module is doing a ccontinuous read
    nano.stopReading();

    Serial.println(F("Module continuously reading. Asking it to stop..."));

    delay(1500);
  }
  else
  {
    //The module did not respond so assume it's just been powered on and communicating at 115200bps
    softSerial.begin(115200); //Start software serial at 115200

    nano.setBaud(baudRate); //Tell the module to go to the chosen baud rate. Ignore the response msg

    softSerial.begin(baudRate); //Start the software serial port, this time at user's chosen baud rate

    delay(250);
  }

  //Test the connection
  nano.getVersion();
  if (nano.msg[0] != ALL_GOOD) return (false); //Something is not right

  //The M6E has these settings no matter what
  nano.setTagProtocol(); //Set protocol to GEN2

  nano.setAntennaPort(); //Set TX/RX antenna ports to 1

  return (true); //We are ready to rock
}

void vector2string() {

  int j, esize, i = 0;
  bool sent_comma = false;
  bool detect_cnt = false;

  // use to store data to be send
  char HtmlBuf[HTML_BUFFER];

  header = "";
  HtmlBuf[0] = 0x0;
  
  header += HtmlBuf;
  esize = Array_Cnt();  // get count

  // loop through the complete list
  while ( i < esize ) {

    // add comma in between EPC's
    if (sent_comma) header += ",";
    else sent_comma = true;

    // add epc
    HtmlBuf[0] = 0x0;
    for (j = 0 ; j < EPC_ENTRY; j++)
      sprintf(HtmlBuf, "%s %02x",HtmlBuf, epcs[i].epc[j]);

    // add detection count ?
    if (detect_cnt) sprintf(HtmlBuf, "%s:%02d",HtmlBuf, epcs[i].cnt);

    header += HtmlBuf;

    // if epc_clr was called before, it will clear entry after adding EP
    //if (enable_clr){
      Array_Rmv_Entry(i);
      esize--;
      i=0;
    //}
    //else
      i++;    // next EPC
  //}
  }

  yourarduinodata = header;
  
}

///////////////////////////////////////////////////////////////////////////////
///                           Supporting routines                            //
///////////////////////////////////////////////////////////////////////////////

/* serialTrigger prints a message, then waits for something
 * to come in from the serial port. */
void serialTrigger(String message)
{
  Serial.println();
  Serial.println(message);
  Serial.println();
  while (!Serial.available());
  while (Serial.available())Serial.read();
}
