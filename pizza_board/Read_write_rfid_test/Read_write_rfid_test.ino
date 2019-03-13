/*  
 *  RFID 13.56 MHz / NFC Module
 *  
 *  Copyright (C) Libelium Comunicaciones Distribuidas S.L. 
 *  http://www.libelium.com 
 *  
 *  This program is free software: you can redistribute it and/or modify 
 *  it under the terms of the GNU General Public License as published by 
 *  the Free Software Foundation, either version 3 of the License, or 
 *  (at your option) any later version. 
 *  
 *  This program is distributed in the hope that it will be useful, 
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License 
 *  along with this program.  If not, see http://www.gnu.org/licenses/. 
 *  
 *  Version:           1.0
 *  Design:            David Gascón 
 *  Implementation:    Luis Miguel Martí
 */

uint8_t dataRX[35];//Receive buffer.
uint8_t dataTX[35];//Transmit buffer.
//stores the status of the executed command:
short state;
//auxiliar buffer:
unsigned char aux[16];
//stores the UID (unique identifier) of a card:
unsigned char _CardID[4];
//stores the key or password:
unsigned char keyOld[]= {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};// Old key access
uint8_t meassures[16];
byte counter;
boolean stored = true;
//only edit config if strong knowledge about the RFID/NFC module
unsigned char config[] = {
  0xFF, 0x07, 0x80, 0x69};
uint8_t key_address = 7;
uint8_t write_address = 4;

void setup()
{
  // start serial port 115200 bps:
  Serial.begin(115200);
  Serial.print("RFID/NFC @ 13.56 MHz module started");
  delay(1000);
  //! It is needed to launch a simple command to sycnchronize
  getFirmware();
  configureSAM();
  delay(100); 
  Serial.println("Welcome");
  Serial.println("Ready to save data");
  Serial.flush();
  counter = 0;
  pinMode(10,OUTPUT);
  pinMode(11,OUTPUT);
  pinMode(12,OUTPUT);
}

void loop()
{
  //take 8 lecture from sensor and write them into the tag card
  while(counter<8){

    Serial.print("\n");
    Serial.println("Ready to write...");
    /////////////////////////////////////////////////////////////
    //Get the UID Identifier

    init(_CardID, aux);
    /////////////////////////////////////////////////////////////
    //Auntenticate a block with his keyAccess
    state = authenticate(_CardID, write_address, keyOld);
    if ( state == 0) {
      Serial.print("\n");
      Serial.print( "The UID : ");
      print(_CardID , 4);
      Serial.print("\n");
      Serial.println("Authentication block OK");
    } 
    else {
      Serial.println("Authentication failed");
    }
    /////////////////////////////////////////////////////////////
    //Write blockData in a address, after authentication.
    if ( state == 0 ) {
      //saves sensor meassurement into its corresponding bytes
      meassures[counter] = analogRead(A0)>>2;
      //saves time when meassurement was taken
      meassures[counter+8] = millis()/1000;
      state = writeData(write_address, meassures);
      Serial.print("\n");
      if ( state == 0) {
        Serial.println("Write block OK");
        counter++;
        Serial.print("\n");
        Serial.print("Data written : ");
        Serial.print(meassures[counter],DEC);
        Serial.print(" ");
        Serial.print(meassures[counter+7],DEC);
        Serial.print("\n");
      } 
      else {
        Serial.println("Write failed, no data stored");
      }


      delay(1000);
    }
  }
  //wait for card to be waved again and read the whole data stored before
  while (stored){
    Serial.print("\n");
    Serial.println("Ready to read...");
    /////////////////////////////////////////////////////////////
    //Get the UID Identifier
    init(_CardID, aux);
    Serial.print("\n");
    Serial.print( "The UID : ");
    print(_CardID , 4);
    /////////////////////////////////////////////////////////////
    //Auntenticate a block with his keyAccess
    state = authenticate(_CardID, write_address, keyOld);
    Serial.print("\n");

    if ( state == 0) {
      Serial.println("Authentication block OK");
    } 
    else {
      Serial.println("Authentication failed");
    }
    /////////////////////////////////////////////////////////////
    //Read from address after authentication
    state = readData(write_address, aux);
    Serial.print("\n");

    if (state == 0) {
      Serial.println("Read block OK");
      stored = false;
    } 
    else {
      Serial.println("Read failed");
    }

    Serial.print("\n");
    Serial.print("Data read: ");
    for (byte i = 0; i<8; i++){

      Serial.print(aux[i], DEC);
      Serial.print(" ");
      Serial.print(aux[i+8], DEC);
      Serial.print("  ");
    }
    Serial.print("\n");
    delay(1000);
  }
}

//**********************************************************************
//!The goal of this command is to detect as many targets (maximum MaxTg)
//  as possible in passive mode.
uint8_t init(uint8_t *CardID , uint8_t *ATQ)   //! Request InListPassive
{
  Serial.flush();

  dataTX[0] = 0x04; // Length
  lengthCheckSum(dataTX); // Length Checksum
  dataTX[2] = 0xD4;
  dataTX[3] = 0x4A; // Code
  dataTX[4] = 0x01; //MaxTarget
  dataTX[5] = 0x00; //BaudRate = 106Kbps
  dataTX[6] = 0x00; // Clear checkSum position
  checkSum(dataTX); 

  sendTX(dataTX , 7 ,23);  

  for (int i = 17; i < (21) ; i++){
    _CardID[i-17] = dataRX[i];
    CardID[i-17] = _CardID[i-17];
  }

  ATQ[0] = dataRX[13];
  ATQ[1] = dataRX[14];

  if ((dataRX[9]== 0xD5) & (dataRX[10] == 0x4B) & (dataRX[11] == 0x01)) {
    return 0;
  } 
  else {
    return 1;
  }
}
//**********************************************************************
//!A block must be authenticated before read and write operations
uint8_t authenticate(uint8_t *CardID, uint8_t blockAddress, uint8_t *keyAccess)
{
  dataTX[0] = 0x0F;
  lengthCheckSum(dataTX);
  dataTX[2] = 0xD4;
  dataTX[3] = 0x40; // inDataEchange
  dataTX[4] = 0x01; //Number of targets
  dataTX[5] = 0x60; // Authentication code
  dataTX[6] = blockAddress;
  for (int i = 0; i < 6 ; i++) {
    dataTX[i + 7] = keyAccess[i];
  }
  dataTX[13] = CardID[0];  
  dataTX[14] = CardID[1];
  dataTX[15] = CardID[2];  
  dataTX[16] = CardID[3];
  dataTX[17] = 0x00;
  checkSum(dataTX);
  sendTX(dataTX , 18 ,14);

  if ((dataRX[9]== 0xD5) & (dataRX[10] == 0x41) & (dataRX[11] == 0x00)) {
    return 0;
  } 
  else {
    return 1;
  }
}
//**********************************************************************
//!Write 16 bytes in address .
uint8_t writeData(uint8_t address, uint8_t *blockData)  //!Writing
{
  Serial.print("                ");
  dataTX[0] = 0x15;
  lengthCheckSum(dataTX); // Length Checksum
  dataTX[2] = 0xD4;
  dataTX[3] = 0x40;  //inDataEchange CODE
  dataTX[4] = 0x01;  //Number of targets
  dataTX[5] = 0xA0; //Write Command
  dataTX[6] = address; //Address    

    for (int i = 0; i < 16; i++) {
    dataTX[i+7] = blockData[i];
  }

  dataTX[23] = 0x00;
  checkSum(dataTX);
  sendTX(dataTX , 24 ,14);      

  if ((dataRX[9]== 0xD5) & (dataRX[10] == 0x41) & (dataRX[11] == 0x00)) {
    return 0;
  } 
  else {
    return 1;
  }
}
//**********************************************************************
//!Read 16 bytes from  address .
uint8_t readData(uint8_t address, uint8_t *readData) //!Reading
{
  Serial.print("                ");   

  dataTX[0] = 0x05;
  lengthCheckSum(dataTX); // Length Checksum
  dataTX[2] = 0xD4; // Code
  dataTX[3] = 0x40; // Code
  dataTX[4] = 0x01; // Number of targets
  dataTX[5] = 0x30; //ReadCode
  dataTX[6] = address;  //Read address
  dataTX[7] = 0x00;
  checkSum(dataTX);
  sendTX(dataTX , 8, 30);
  memset(readData, 0x00, 16);  

  if ((dataRX[9]== 0xD5) & (dataRX[10] == 0x41) & (dataRX[11] == 0x00)) {
    for (int i = 12; i < 28; i++) {
      readData[i-12] = dataRX[i];
    }
    return 0;
  } 
  else {
    return 1;
  }
}
//**********************************************************************
//!The PN532 sends back the version of the embedded firmware.
bool getFirmware(void)  //! It is needed to launch a simple command to sycnchronize
{
  Serial.print("                ");   

  memset(dataTX, 0x00, 35);
  dataTX[0] = 0x02; // Length
  lengthCheckSum(dataTX); // Length Checksum
  dataTX[2] = 0xD4; // CODE
  dataTX[3] = 0x02; //TFI
  checkSum(dataTX); //0x2A; //Checksum   

  sendTX(dataTX , 5 , 17);
  Serial.print("\n");
  Serial.print("Your Firmware version is : ");

  for (int i = 11; i < (15) ; i++){
    Serial.print(dataRX[i], HEX);
    Serial.print(" ");
  }
  Serial.print("\n");
}

//**********************************************************************
//!Print data stored in vectors .
void print(uint8_t * _data, uint8_t length)
{
  for (int i = 0; i < length ; i++){
    Serial.print(_data[i], HEX);
    Serial.print(" ");
  }
  Serial.print("\n");
}
//**********************************************************************
//!This command is used to set internal parameters of the PN532,
bool configureSAM(void)//! Configure the SAM
{
  Serial.print("               ");

  dataTX[0] = 0x05; //Length
  lengthCheckSum(dataTX); // Length Checksum
  dataTX[2] = 0xD4;
  dataTX[3] = 0x14;
  dataTX[4] = 0x01; //Normal mode
  dataTX[5] = 0x14; // TimeOUT
  dataTX[6] = 0x00; // IRQ
  dataTX[7] = 0x00; // Clean checkSum position
  checkSum(dataTX);

  sendTX(dataTX , 8, 13);
}
//**********************************************************************
//!Send data stored in dataTX
void sendTX(uint8_t *dataTX, uint8_t length, uint8_t outLength)
{
  Serial.print(char(0x00));
  Serial.print(char(0x00));
  Serial.print(char(0xFF)); 

  for (int i = 0; i < length; i++) {
    Serial.print(char(dataTX[i]));
  }

  Serial.print(char(0x00));
  getACK();
  waitResponse();    // 1C - receive response
  getData(outLength);
}
//**********************************************************************
//!Wait for ACK response and stores it in the dataRX buffer
void getACK(void)
{
  delay(5);
  waitResponse();
  for (int i = 0; i < 5 ; i++) {
    dataRX[i] = Serial.read();
  }
}
//**********************************************************************
//!Wait the response of the module
void waitResponse(void)
{
  int val = 0xFF;
  int cont = 0x00;
  while(val != 0x00) { //Wait for 0x00 response
    val = Serial.read();
    delay(5);
    cont ++;
  }
}
//**********************************************************************
//!Get data from the module
void getData(uint8_t outLength)
{
  for (int i=5; i < outLength; i++) {
    dataRX[i] = Serial.read(); // read data from the module.
  }
}
//**********************************************************************
//!Calculates the checksum and stores it in dataTX buffer
void checkSum(uint8_t *dataTX)
{
  for (int i = 0; i < dataTX[0] ; i++) {
    dataTX[dataTX[0] + 2] += dataTX[i + 2];
  }
  byte(dataTX[dataTX[0] + 2]= - dataTX[dataTX[0] + 2]);
}
//**********************************************************************
//!Calculates the length checksum and sotres it in the buffer.
uint8_t lengthCheckSum(uint8_t *dataTX)
{
  dataTX[1] = byte(0x100 - dataTX[0]);
}
//**********************************************************************
//Prints via serial data read from the TAG in block "read_adress"
void printData()
{
  Serial.print("\n");
  Serial.println("Ready to read...");
  /////////////////////////////////////////////////////////////
  //Get the UID Identifier
  init(_CardID, aux);
  Serial.print("\n");
  Serial.print( "The UID : ");
  print(_CardID , 4);
  /////////////////////////////////////////////////////////////
  //Auntenticate a block with his keyAccess
  state = authenticate(_CardID, write_address, keyOld);
  Serial.print("\n");

  if ( state == 0) {
    Serial.println("Authentication block OK");
  } 
  else {
    Serial.println("Authentication failed");
  }
  /////////////////////////////////////////////////////////////
  //Read from address after authentication
  state = readData(write_address, aux);
  Serial.print("\n");

  if (state == 0) {
    Serial.println("Read block OK");
  } 
  else {
    Serial.println("Read failed");
  }

  Serial.print("\n");
  Serial.print("Data read: ");
  print(aux , 16);
  Serial.print("\n");
}


        
