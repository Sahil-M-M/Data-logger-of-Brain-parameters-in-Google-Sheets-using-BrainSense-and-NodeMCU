#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
 
#define ON_Board_LED 2                                  // Defining an On Board LED, used for indicators when the process of connecting to a wifi router
 
const char* ssid = "Lenovo K8";                         // Wi-Fi SSID.
const char* password = "1234567890";                    // Wi-Fi password.

#define BAUDRATE 57600                                 // Baudrate
#define DEBUGOUTPUT 1

//----------------------------------- checksum variables----------------------------------------------------------------------
byte generatedChecksum = 0;
byte checksum = 0; 
int payloadLength = 0;
byte payloadData[64] = {0};
byte poorQuality = 0;
byte attention = 0;
byte meditation = 0;

//----------------------------------- System variables----------------------------------------------------------------------
long lastReceivedPacket = 0;
boolean bigPacket = false;
boolean Send = false;
 
//----------------------------------------Host & httpsPort----------------------------------------------------------------------
const char* host = "script.google.com";
const int httpsPort = 443;
//--------------------------------------------------------------------------------------------------------------
 
WiFiClientSecure client;                                   // Create a WiFiClientSecure object.
 
String GAS_ID = "AKfycbxakVMe4TWL0J0jo6wI50ilRaTdNgxB27q7fLSLOj7k91UMiSULm_OH4tIbaZJVN6r_";             // spreadsheet script ID
 
void setup() {
  
  Serial.begin(BAUDRATE);                     // Configure serial port
  delay(500);

  WiFi.begin(ssid, password);                 // Connect to Wi-Fi
  Serial.println("");
    
  pinMode(ON_Board_LED,OUTPUT);              // Set On Board LED port as output
  digitalWrite(ON_Board_LED, HIGH);          // Turn off Led On Board
 
  //----------------------------------------Wait for connection--------------------------------------------------------------------------------
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    //----------------------------------------Make the On Board Flashing LED on the process of connecting to the wifi router.----------------------------------------
    digitalWrite(ON_Board_LED, LOW);
    delay(250);
    digitalWrite(ON_Board_LED, HIGH);
    delay(250);
    //------------------------------------------------------------------------------------------------------------------------
  }
  //------------------------------------------------------------------------------------------------------------------------
  #if DEBUGOUTPUT
  Serial.println("");
  Serial.print("Successfully connected to : ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  #endif
 
  client.setInsecure();

}
 
void loop() {
  collect_data();                                           // Call the collect_data Subroutine

  //Wait for a bit to keep serial data from saturating
  delay(1);
  if(Send){
    sendData(poorQuality, attention, meditation);           // Call the sendData Subroutine
    Send=false;
  }

}

// Subroutine for collect data from BrainSense
void collect_data(){

  // Look for sync bytes
  if(ReadOneByte() == 170) 
  {
    if(ReadOneByte() == 170) 
    {
        payloadLength = ReadOneByte();                  // Read payload length 
      
        if(payloadLength > 169)                         // Payload length can not be greater than 169
        return;
        generatedChecksum = 0;                          // Reset generated checksum to 0
        for(int i = 0; i < payloadLength; i++) 
        {  
        payloadData[i] = ReadOneByte();                // Read payload into memory
        generatedChecksum += payloadData[i];           // Calculate checksun
        }   
        
        checksum = ReadOneByte();                      // Read checksum byte from data stream      
        generatedChecksum = 255 - generatedChecksum;   // Take one's compliment of generated checksum

        if(checksum == generatedChecksum)              // Check read checksum & generated checksum
        {    
          // Reset poorQuality, attention, meditation
          poorQuality = 200;
          attention = 0;
          meditation = 0;

          for(int i = 0; i < payloadLength; i++)            // Parse the payload
          {                                          
            switch (payloadData[i]) 
            {
            case 2:
              i++;            
              poorQuality = payloadData[i];
              Send=true;
              bigPacket = true;            
              break;
            case 4:
              i++;
              attention = payloadData[i];                        
              break;
            case 5:
              i++;
              meditation = payloadData[i];
              break;
            case 0x80:
              i = i + 3;
              break;
            case 0x83:
              i = i + 25;      
              break;
            default:
              break;
            } // switch
         } // for loop

        #if DEBUGOUTPUT

          if(bigPacket) 
          {
            if(poorQuality == 0)
              digitalWrite(ON_Board_LED, HIGH);
            else
              digitalWrite(ON_Board_LED, LOW);
            
            Serial.print("PoorQuality: ");
            Serial.print(poorQuality, DEC);
            Serial.print(" Attention: ");
            Serial.print(attention, DEC);
            Serial.print(" Meditation: ");
            Serial.print(meditation, DEC);
            Serial.print(" Time since last packet: ");
            Serial.print(millis() - lastReceivedPacket, DEC);
            lastReceivedPacket = millis();
            Serial.print("\n");
          }
        #endif   

        bigPacket = false;        
      }
      else {
        Serial.print("Failure to match checksum");// Checksum Error
      }  // end if else for checksum
    } // end if read 0xAA byte
  } // end if read 0xAA byte
}

// Subroutine for reading 1 byte
byte ReadOneByte() 
{
  int ByteRead;
  while(!Serial.available());
  ByteRead = Serial.read();

  #if DEBUGOUTPUT  
    Serial.print((char)ByteRead);   // echo the same byte out the USB serial (for debug purposes)
  #endif

  return ByteRead;
}


// Subroutine for sending data to Google Sheets
void sendData(byte poorQuality, byte attention, byte meditation) {
  Serial.println("==================================================");
  Serial.print("connecting to ");
  Serial.println(host);
  
  //----------------------------------------Connect to Google host------------------------------------------------------------------------------
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }
  //-------------------------------------------------------------------------------------------------------------------------------------------------------------
 
  //----------------------------------------Processing data and sending data------------------------------------------------------------------------------
  String string_poorQuality =  String(poorQuality);
  String string_attention =  String(attention);
  String string_meditation =  String(meditation);
  
  String url = "/macros/s/" + GAS_ID + "/exec?poorQuality=" + string_poorQuality + "&attention=" + string_attention + "&meditation=" + string_meditation;
  Serial.print("requesting URL: ");
  Serial.println(url);
 
  //------------------------------------------------------------------------------Send data------------------------------------------------------------------------------
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +
         "User-Agent: BuildFailureDetectorESP8266\r\n" +
         "Connection: close\r\n\r\n");
 
  Serial.println("request sent");
  //----------------------------------------------------------------------------------------------------------------------
 
  //----------------------------------------Checking whether the data was sent successfully or not------------------------------------------------------------------------------
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed");
  }
  Serial.print("reply was : ");
  Serial.println(line);
  Serial.println("closing connection");
  Serial.println("=========================================================================");
  Serial.println();
  //----------------------------------------------------------------------------------------------------------------------
} 