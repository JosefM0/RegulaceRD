#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>

const char* ssid     = "maruska";
const char* password = "maruska1";
WiFiServer server(80);
// Variable to store the HTTP request
String header;


float T = 200; //perioda vyorkovani v ms
float Kp = 12000; //zesileni regulatoru
float Ti = 0; //integtracni konst
float Td = 0; //derivacni konst

int VentJede = 0; //ventil v pohybu
//unsigned int krok = 0; //cislo vzorku k
float NamTep = 250;
//float MinulaTep = 200;
float pozadovana = 28;
float Poloha = 0; //bufer pohybu ventilu
float u = 0; //Akcni velicina aktualni
//float MinulaU = 0; //Akcni velicina posledni -1kT
float e = 0; //Reg odchylka aktualni
//float MinulaE = 0; //Reg odchylka posledni -1kT
//float Minula2E = 0; //Reg odchylka predposledni -2kT
float SumE = 0; //suma minulych odchzlek
unsigned long CasTep = 0;
unsigned long CasVentil = 0;
unsigned long currentTime = 0;
unsigned long previousTime = 0;
const long timeoutTime = 2000;

int CerpPin = 5; //pin D1
int VentPlus  = 16; //pin D0
int VentMinus  = 0;  //pin D3
const int pinCidlaDS = 4;

OneWire oneWireDS(pinCidlaDS);
DallasTemperature senzoryDS(&oneWireDS);

void setup() {
  Serial.begin(9600);
  senzoryDS.begin();
  delay(1000);
  //nastavení výstupů: HIGH = Off
  pinMode(CerpPin, OUTPUT);
  digitalWrite(CerpPin, HIGH);
  pinMode(VentPlus, OUTPUT);
  digitalWrite(VentPlus, HIGH);
  pinMode(VentMinus, OUTPUT);
  digitalWrite(VentMinus, LOW);
  Serial.print("Pripojuji se k siti ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi pripojeno.");
  Serial.println("IP addresa: ");
  Serial.println(WiFi.localIP());
  server.begin();
  Serial.println("");
  reference();
  Serial.println("Inicializace pumpy:");
  CasTep = millis();
  while (millis() <= (CasTep + 5000)) {
    Serial.print("Cas do konce reference je: ");
    Serial.print((5000 - (millis() - CasTep)) / 1000);
    Serial.println(" s");
    NamTep = ZmerTeplotu();
    pumpa();   // pokud neni prekrocena MAX teplota, zapni cerpadlo, jinak vypni
    delay(1000);
  }
  Serial.println("");
  CasTep = millis();
  Serial.println("Konec inicializace");
}

void loop() {

  if (millis() >= (CasTep + T)) {
    CasTep = millis();
    //    krok = krok + 1;
    //    MinulaTep = NamTep;
    NamTep = ZmerTeplotu();
    pumpa();

    //    Minula2E = MinulaE;
    //    MinulaE = e;
    e = pozadovana - NamTep;
    //    MinulaU = u;
    u = Kp * e; //P regulator
    //    u = Kp * (e + ((T / Ti) * (SumE + e))); //PI regulator
    u = constrain(u, 0, 120000);  //saturace - anti wind up

        //rozjed ventil nahoru
    if (((u - Poloha) >= 1000) && (VentJede != 1)) {
      if (VentJede == -1) {
        if ((Poloha != 0) && (u != 0)){
        Poloha = Poloha - (millis() - CasVentil); //aktualizuj polohu ventilu pred prepnutim smeru
        }
        digitalWrite(VentMinus, HIGH);
        delay(20);
      }
      digitalWrite(VentPlus, LOW);
      CasVentil = millis();
      VentJede = 1;
    }
    //rozjed ventil dolu
    if (((u - Poloha) <= -1000) && (VentJede != -1)) {
      if (VentJede == +1) {
        if ((Poloha != 120000) && (u != 120000)){
        Poloha = Poloha + (millis() - CasVentil); //aktualizuj polohu ventilu pred prepnutim smeru
        }
        digitalWrite(VentPlus, HIGH);
        delay(5);
      }
      digitalWrite(VentMinus, LOW);
      CasVentil = millis();
      VentJede = -1;
    }
  }

  //aktualizace polohy, zastaveni ventilu:
  if (VentJede == 1) { //ventil jede nahoru
    if (Poloha != 120000) {
      Poloha = Poloha + (millis() - CasVentil);
      CasVentil = millis();
    }
    if ((u - Poloha) <= 20) {
      if (u == 120000) { //pokud je na dorazu, ponecha rele zapnute a vyuzije jako referenci
        delay(20);
        Poloha = 120000;
      }
      else {
        digitalWrite(VentPlus, HIGH);
        digitalWrite(VentMinus, HIGH);
        VentJede = 0;
      }
    }
  }
  if (VentJede == -1) { //ventil jede dolu
    if (Poloha != 0 ) {
      Poloha = Poloha - (millis() - CasVentil);
      CasVentil = millis();
    }
    if ((u - Poloha) >= -20) {
      if (u == 0) { //pokud je na dorazu, ponecha rele zapnute a vyuzije jako referenci
        delay(20);
        Poloha = 0;
      }
      else {
        digitalWrite(VentPlus, HIGH);
        digitalWrite(VentMinus, HIGH);
        VentJede = 0;
      }

    }
  }

  WiFiClient client = server.available();   // Listen for incoming clients
  if (client) {                             // If a new client connects,
    //    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        //        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            //              // turns the GPIOs on and off
            if (header.indexOf("GET /pozadovana/-") >= 0) {
              //              Serial.println("/pozadovana/-");
              pozadovana = pozadovana - 1;
            } else if (header.indexOf("GET /pozadovana/+") >= 0) {
              pozadovana = pozadovana + 1;
            }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>ESP8266 Web Server</h1>");

            client.print("<p>Pozadovana teplota: ");
            client.print(pozadovana);
            client.println(" °C</p>");
            client.println("<p><a href=\"/pozadovana/-\"><button class=\"button\">-</button></a>");
            client.println("   <a href=\"/pozadovana/+\"><button class=\"button\">+</button></a></p>");
            client.print("<p>Namerena teplota: ");
            client.print(NamTep);
            client.println(" °C</p>");
            client.print("<p>Ventil jede: ");
            client.print(VentJede);
            client.println(" ");
            client.print("<p>Poloha ventilu: ");
            client.print(Poloha);
            client.println(" ms</p>");
            client.print("<p>Akcni velicina: ");
            client.print(u);
            client.println(" ms</p>");
            client.print("<p>Regulacni odchylka: ");
            client.print(e);
            client.println(" °C</p>");
            client.print("<p>Systemovy cas: ");
            client.print(millis());
            client.println(" ms</p>");

            client.print("<p>Integracni konstanta: ");
            client.print(Ti);
            client.println(" </p>");

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    //    Serial.println("Client disconnected.");
    //    Serial.println("");
  }
}

float ZmerTeplotu() {
  senzoryDS.requestTemperatures();
  Serial.print(senzoryDS.getTempCByIndex(0));
  Serial.println("!");
  return senzoryDS.getTempCByIndex(0);
  //  return 20;
}

// pokud neni prekrocena MAX teplota, zapni cerpadlo, jinak vypni a zavirej ventil:
void pumpa() {
  if ((NamTep >= 0) && (NamTep <= 40)) {
    digitalWrite(CerpPin, LOW);
  }
  else {
    digitalWrite(CerpPin, HIGH);
    digitalWrite(VentPlus, HIGH);
    delay(10);
    digitalWrite(VentMinus, LOW);
    Serial.println("Teplota mimo meze!!!");
    Serial.println(NamTep);
    delay(10000000);
    ESP.reset();
  }
}

void reference() {
  int iTmp0 = 120;//doba reference(s)
  Serial.println("Reference ventilu:");
  
  Serial.print("DEUG RetmillisFce: "); //DEBUG
  Serial.println(millis()); //DEBUG
  
  digitalWrite(VentPlus, HIGH);
  delay(10);
  digitalWrite(VentMinus, LOW);
  CasTep = millis();
  while (millis() <= (CasTep + (1000*iTmp0))) {
    Serial.print("Cas do konce reference je: ");
    Serial.print(iTmp0 - ((millis() - CasTep) / 1000));
    Serial.println(" s");
    delay(1000);
  }
  Poloha = 0;
  digitalWrite(VentMinus, HIGH);
}
