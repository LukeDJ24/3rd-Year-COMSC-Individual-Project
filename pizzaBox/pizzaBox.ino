#include <SPI.h>
#include <WiFiNINA.h>
#include <SparkFun_UHF_RFID_Reader.h> // Library for controlling the M6E Nano module
#include <Vector.h>                   // storage handling

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


 /* Option: display debug level/information
  * 0 = no debug information
  * 1 = tag info found
  * 2 = 1 + programflow + temperature info
  * 3 = 2 + Nano debug  */
#define PRMDEBUG 0

int buttonPin = 12;

RFID nano;

void setup() {

  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  pinMode(buttonPin, INPUT_PULLUP);

  Array_Init();       // initialize array
  
  //connectWifi();      // initialize WiFi connection
  
}

void loop() {

  int buttonValue = digitalRead(buttonPin);

  if(digitalRead(buttonPin) == LOW){
  Serial.println("button has been pressed!");
//    for (int x = 0; x < 1000 ;x++){
//     Check_EPC();
//    }
//
//   postData();
  } else {
     Serial.println("Waiting for button to be pressed...");
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
  uint8_t myEPC[EPC_ENTRY];     // Most EPCs are 12 bytes

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
