#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FirebaseESP32.h>

#include <ESP_LM35.h> 
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// Variables
#define HORA_ULTIMO_RIEGO_ADDRESS 0
#define MINUTO_ULTIMO_RIEGO_ADDRESS 2
#define FIREBASE_HOST
#define FIREBASE_AUTH

byte Gotita[8] = {0b00000, 0b00100, 0b01110, 0b01110, 0b11111, 0b11111, 0b01110, 0b00000};
byte Termom[8] = {0b00100, 0b01010, 0b01010, 0b01010, 0b01010, 0b10011, 0b10001, 0b01110};

// Variables de control
int Humedad = 0;
int Temperatura = 0;
int AguaAhorrada = 0;

int UmbHumedad = 25;
int UmbTemperatura = 10;
int UmbTiempo = 120;

int DurRiego = 5000;

int hora_actual = 0;
int minuto_actual = 0;
int hora_ultimo_riego = 0;
int minuto_ultimo_riego = 0;

bool Regar = false;

String HoraConFormato;
volatile int modo = 1;

// Objetos
LiquidCrystal_I2C lcd(0x27, 16, 2);
ESP_LM35 temp(34);

// Funciones del sistema
void chMode() {
  modo++;
}

void saveVar(int dir, int v) {
  byte ba = highByte(v);
  byte bb = lowByte(v);

  EEPROM.write(dir, ba);
  EEPROM.write(dir + 1, bb);
  EEPROM.commit();
}

int loadVar(int dir) {
  byte ba = EEPROM.read(dir);
  byte bb = EEPROM.read(dir + 1);

  return word(ba, bb);
}

// Funciones de arranque
void pinSetup(){
  pinMode(15, INPUT);// Pulsador1 resta
  pinMode(2, INPUT); // Pulsador2 centro
  pinMode(4, INPUT); // Pulsador3 suma / riega

  pinMode(35, INPUT); // SENSOR DE TEMP
  pinMode(27, OUTPUT); //LED AZUL
  pinMode(14, OUTPUT); //LED ROJO
  pinMode(13, OUTPUT); //LED VERDE
  pinMode(12, OUTPUT); //LED AMARILLO

  attachInterrupt(digitalPinToInterrupt(2), chMode, RISING);
}

// Funciones de hora
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
void timeLoop() {
  hora_ultimo_riego = loadVar(HORA_ULTIMO_RIEGO_ADDRESS);
  minuto_ultimo_riego = loadVar(MINUTO_ULTIMO_RIEGO_ADDRESS);
  hora_actual = timeClient.getHours();
  minuto_actual = timeClient.getMinutes();
  HoraConFormato = (hora_actual < 10 ? "0" : "") + String(hora_actual) + ":" + (minuto_actual < 10 ? "0" : "") + String(minuto_actual);
}

// Funciones de red
FirebaseData fbdo;  
void netSetup() {
  lcd.clear();
  lcd.print("Iniciando. . .");
  WiFi.begin(sopademacaco, unadelicia);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(13, HIGH);
    delay(75);
    digitalWrite(13, LOW);
    delay(75);
  }

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  timeClient.begin();
  timeClient.setTimeOffset(-18000);
  timeClient.update();

  hora_ultimo_riego = loadVar(HORA_ULTIMO_RIEGO_ADDRESS);
  minuto_ultimo_riego = loadVar(MINUTO_ULTIMO_RIEGO_ADDRESS);

  FerLog("INFO", "Iniciando sistema...");
}
void FerLog(const char* tipo, const char* str) {
  String log = HoraConFormato + " -> [" + String(tipo) + "]: " + String(str);
  if (Firebase.getArray(fbdo, "/Registro")) {
    FirebaseJsonArray array = fbdo.jsonArray();
    array.add(log);
    while (array.size() > 10) {
      array.remove(0);
    }
    Firebase.setArray(fbdo, "/Registro", array);
  }
}

void updateJSON(String ruta, String time, int value) {
  String item = time + "-" + value;
  if (Firebase.getArray(fbdo, ruta)) {
    FirebaseJsonArray array = fbdo.jsonArray();
    array.add(item);
    while (array.size() > 6) {
      array.remove(0);
    }
    Firebase.setArray(fbdo, ruta, array);
  }
}

void setDBvars() {
  Firebase.setInt(fbdo, "/Humedad", Humedad);
  Firebase.setInt(fbdo, "/Temperatura", Temperatura);
}

void getDBvars() {
  UmbHumedad = Firebase.getInt(fbdo, "/UmbHumedad") ? fbdo.intData() : UmbHumedad;
  UmbTemperatura = Firebase.getInt(fbdo, "/UmbTemperatura") ? fbdo.intData() : UmbTemperatura;
  UmbTiempo = Firebase.getInt(fbdo, "/UmbTiempo") ? fbdo.intData() : UmbTiempo;
  DurRiego =  Firebase.getInt(fbdo, "/DurRiego") ? fbdo.intData() : DurRiego;
  hora_ultimo_riego = Firebase.getInt(fbdo, "/HoraUltRiego") ? fbdo.intData() : hora_ultimo_riego;
  minuto_ultimo_riego = Firebase.getInt(fbdo, "/MinUltRiego") ? fbdo.intData() : minuto_ultimo_riego;
}

// Funciones 
void exRiego() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Regando...");
  lcd.setCursor(0, 1);
  lcd.print("Hora: ");
  lcd.print(HoraConFormato);

  // Preparacion
  saveVar(HORA_ULTIMO_RIEGO_ADDRESS, hora_actual);
  saveVar(MINUTO_ULTIMO_RIEGO_ADDRESS, minuto_actual);
  EEPROM.commit();
  Firebase.setBool(fbdo, "/PuedeRegar", false);
  Firebase.setString(fbdo, "/Estado", "Regando");
  Firebase.setInt(fbdo, "/MinUltRiego", minuto_actual);
  Firebase.setInt(fbdo, "/HoraUltRiego", hora_actual);
  FerLog("INFO", "Regando...");
     // Indicadores:
  digitalWrite(27, HIGH);
  delay(DurRiego);
  digitalWrite(27, LOW);

  lcd.clear();
  lcd.print("Se termino");
  lcd.setCursor(0, 1);
  lcd.print("de regar!");

  // Final y cambio de variables
  int riegosDados = 0;
  if (Firebase.getInt(fbdo, "/RiegosDados")) {
    riegosDados = fbdo.intData();
  }
  riegosDados++;
  
  int i = 0.25 * (DurRiego / 1000);
  if (Firebase.getInt(fbdo, "/AguaAhorrada")) {
    AguaAhorrada = fbdo.intData() + i;
  }

  Firebase.setInt(fbdo, "/AguaAhorrada", AguaAhorrada);
  Firebase.setInt(fbdo, "/RiegosDados", riegosDados);
  Firebase.setString(fbdo, "/Estado", "Reposo");

  updateJSON("/HumOldTab", HoraConFormato, Humedad);
  updateJSON("/TempOldTab", HoraConFormato, Temperatura);
  updateJSON("/AgAOldTab", HoraConFormato, AguaAhorrada);

  // Finalizar la función
  Firebase.setBool(fbdo, "/Regar", false);
  Firebase.setBool(fbdo, "/PuedeRegar", true);

  lcd.clear();
  modo = 1;
}

void saveUmb() {
  Firebase.setInt(fbdo, "/DurRiego", DurRiego);
  Firebase.setInt(fbdo, "/UmbTiempo", UmbTiempo);
  Firebase.setInt(fbdo, "/UmbTemperatura", UmbTemperatura);
  Firebase.setInt(fbdo, "/UmbHumedad", UmbHumedad);
}
void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  lcd.init();                    
  lcd.backlight();
  lcd.createChar(0, Gotita);
  lcd.createChar(1, Termom);
  
  pinSetup();
  netSetup();

  lcd.print("Exito!");  
  digitalWrite(13, HIGH);
  digitalWrite(12, HIGH);
  digitalWrite(14, HIGH);
  digitalWrite(27, HIGH);
  delay(25);
  digitalWrite(27, LOW);
  delay(500);
  digitalWrite(14, LOW);
  digitalWrite(13, LOW);
  digitalWrite(12, LOW);

  lcd.clear();
}

void checkRiego() {
  int minutos_desde_riego;

  if (hora_actual > hora_ultimo_riego || (hora_actual == hora_ultimo_riego && minuto_actual >= minuto_ultimo_riego)) {
    minutos_desde_riego = (hora_actual - hora_ultimo_riego) * 60 + (minuto_actual - minuto_ultimo_riego);
  } else {
    minutos_desde_riego = (24 - hora_ultimo_riego + hora_actual) * 60 + (minuto_actual - minuto_ultimo_riego);
  }

  if (minutos_desde_riego >= UmbTiempo && Humedad <= UmbHumedad && Temperatura >= UmbTemperatura) {
    exRiego();
  }
}

float tempsec = 5.0;
void loop() {
  timeLoop();

  int P1status = digitalRead(15);
  int P2status = digitalRead(2);
  int P3status = digitalRead(4);
  
  Humedad = 100 - constrain((100.0 - ((float)(analogRead(35) - 4095) / (1200 - 4095) * 100.0)), 0, 100);
  Temperatura = int(temp.tempC());
  
  switch (modo) {
    case 2:
      lcd.setCursor(0, 0);
      lcd.print("Ajust Umbrales:  ");
      lcd.setCursor(0, 1);
      lcd.print("Hum: ");
      lcd.print(UmbHumedad);
      lcd.print(" %        ");
      if (P1status == HIGH) {
        UmbHumedad = UmbHumedad - 1;
        delay(50);
      } else if (P3status == HIGH) {
        UmbHumedad = UmbHumedad + 1;
        delay(50);
      }
      break;
      
    case 3:
      lcd.setCursor(0, 0);
      lcd.print("Ajust Umbrales:  ");
      lcd.setCursor(0, 1);
      lcd.print("Temp: ");
      lcd.print(UmbTemperatura);
      lcd.print(" *C        ");
      if (P1status == HIGH) {
        UmbTemperatura = UmbTemperatura - 1;
        delay(50);
      } else if (P3status == HIGH) {
        UmbTemperatura = UmbTemperatura + 1;
        delay(50);
      }
      break;
      
    case 4:
      lcd.setCursor(0, 0);
      lcd.print("Ajust Umbrales:  ");
      lcd.setCursor(0, 1);
      lcd.print("Tiemp: ");
      lcd.print(float(UmbTiempo) / 60);
      lcd.print(" Hrs        ");
      if (P1status == HIGH) {
        UmbTiempo = UmbTiempo - 30;
        delay(100);
      } else if (P3status == HIGH) {
        UmbTiempo = UmbTiempo + 30;
        delay(100);
      }
      break;
      
    case 5:
      lcd.setCursor(0, 0);
      lcd.print("Ajustar Riego   ");
      lcd.setCursor(0, 1);
      lcd.print("Dur: ");
      lcd.print(tempsec);
      lcd.print(" Sec.      ");
      if (P1status == HIGH) {
        tempsec = tempsec - 0.5;
        delay(50);
        DurRiego = tempsec * 1000;
      } else if (P3status == HIGH) {
        tempsec = tempsec + 0.5;
        delay(50);
        DurRiego = tempsec * 1000;
      }
      break;
    case 6:
      saveUmb();
      delay(200);
      modo = 1;
      break;
    case 1:
      getDBvars();
      digitalWrite(12, HIGH);
      if (P3status == HIGH) { exRiego(); };
      if (Firebase.getBool(fbdo, "/Regar")) { 
        Regar = fbdo.boolData(); 
        if (Regar) { exRiego(); }
      }

      checkRiego();
      setDBvars();

      lcd.setCursor(0, 0);
      lcd.print("Hora: ");
      lcd.print(HoraConFormato + "      ");
      
      lcd.setCursor(0, 1);
      lcd.write(1);
      lcd.print(" " + String(Temperatura) + "*C  ");
      lcd.write(0);
      lcd.print(" " + String(Humedad) + "%      ");
  
      digitalWrite(12, LOW);
      break;
  }

  if (modo >= 7) {
    modo = 1;
  }
}
