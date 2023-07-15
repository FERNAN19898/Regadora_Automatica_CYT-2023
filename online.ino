#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FirebaseESP32.h>
#include <ESP_LM35.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
// Base de datos credenciales
#define FIREBASE_HOST
#define FIREBASE_AUTH



byte Gotita[8] =
{
0b00000,
0b00100,
0b01110,
0b01110,
0b11111,
0b11111,
0b01110,
0b00000
};

byte Termom[8] =
{
0b00100,
0b01010,
0b01010,
0b01010,
0b01010,
0b10011,
0b10001,
0b01110
};

// Pin del sensor de humedad y temperatura
ESP_LM35 temp(34);
// Variables de control
int Humedad = 0;
int Temperatura = 0;
int AguaAhorrada = 0;

int UmbHumedad = 0;
int UmbTemperatura = 0;
int UmbTiempo = 0;

int hora_actual = 0;
int minuto_actual = 0;

int hora_ultimo_riego = 0;
int minuto_ultimo_riego = 0;

bool Regar = false;
String HoraConFormato;

FirebaseData fbdo;
WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Funciones del programa
void PrepararPines(){
  pinMode(35, INPUT); // SENSOR DE TEMP
  pinMode(27, OUTPUT); //LED AZUL
  pinMode(14, OUTPUT); //LED ROJO
  pinMode(13, OUTPUT); //LED VERDE
  pinMode(12, OUTPUT); //LED AMARILLO
}

void FuncionHora(){
  timeClient.update();
  hora_actual = timeClient.getHours();
  minuto_actual = timeClient.getMinutes();
  HoraConFormato = (hora_actual < 10 ? "0" : "") + String(hora_actual) + ":" + (minuto_actual < 10 ? "0" : "") + String(minuto_actual);
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

void FuncionRegar() {
  lcd.clear();

  FerLog("INFO", "Regando...");

  lcd.setCursor(0, 0);
  lcd.print("Regando...");
  lcd.setCursor(0, 1);
  lcd.print("Hora: ");
  lcd.print(HoraConFormato);

  // Preparacion
  Firebase.setBool(fbdo, "/PuedeRegar", false);
  Firebase.setString(fbdo, "/Estado", "Regando");
  Firebase.setInt(fbdo, "/MinUltRiego", minuto_actual);
  Firebase.setInt(fbdo, "/HoraUltRiego", hora_actual);

  // Indicadores:
  digitalWrite(27, HIGH);
  delay(7500);
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
  
  int i = 0.25 * 7.5;
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
}

void setup() {
  lcd.init();                    
  lcd.backlight();
  lcd.createChar(0, Gotita);
  lcd.createChar(1, Termom);
  
  lcd.print("Iniciando. . .");
  PrepararPines();
  Serial.begin(115200);

  delay(250);
  lcd.clear();
  WiFi.begin("SSID", "PASSWORD");
  lcd.print("Conectando a");
  lcd.setCursor(0, 1);
  lcd.print("WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(13, HIGH);
    delay(75);
    digitalWrite(13, LOW);
    delay(75);
  }
  lcd.clear();
  digitalWrite(13, HIGH);

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  timeClient.begin();
  timeClient.setTimeOffset(-18000);
  lcd.print("Exito!");

  digitalWrite(12, HIGH);
  digitalWrite(14, HIGH);
  digitalWrite(27, HIGH);
  delay(250);
  digitalWrite(14, LOW);
  digitalWrite(13, LOW);
  digitalWrite(12, LOW);
  digitalWrite(27, LOW);
  lcd.clear();

  if (WiFi.status() != WL_CONNECTED) {
  digitalWrite(14, HIGH);
  }
}

void ActVal_TO_DB() {
  Humedad = map(analogRead(35), 4095, 0, 0, 100);
  Temperatura = int(temp.tempC());
  Firebase.setInt(fbdo, "/Humedad", Humedad);
  Firebase.setInt(fbdo, "/Temperatura", Temperatura);
}

void ActVal_FROM_DB() {
  UmbHumedad = Firebase.getInt(fbdo, "/UmbHumedad") ? fbdo.intData() : UmbHumedad;
  UmbTemperatura = Firebase.getInt(fbdo, "/UmbTemperatura") ? fbdo.intData() : UmbTemperatura;
  UmbTiempo = Firebase.getInt(fbdo, "/UmbTiempo") ? fbdo.intData() : UmbTiempo;
  hora_ultimo_riego = Firebase.getInt(fbdo, "/HoraUltRiego") ? fbdo.intData() : hora_ultimo_riego;
  minuto_ultimo_riego = Firebase.getInt(fbdo, "/MinUltRiego") ? fbdo.intData() : minuto_ultimo_riego;
}

void Ver_S_o_N() {
  int minutos_desde_riego;

  if (hora_actual > hora_ultimo_riego || (hora_actual == hora_ultimo_riego && minuto_actual >= minuto_ultimo_riego)) {
    minutos_desde_riego = (hora_actual - hora_ultimo_riego) * 60 + (minuto_actual - minuto_ultimo_riego);
  } else {
    minutos_desde_riego = (24 - hora_ultimo_riego + hora_actual) * 60 + (minuto_actual - minuto_ultimo_riego);
  }

  if (minutos_desde_riego >= UmbTiempo && Humedad <= UmbHumedad && Temperatura >= UmbTemperatura) {
    FuncionRegar();
  }
}

void loop() {
  lcd.clear();
  lcd.print("Hora:");
  lcd.print(" ");
  lcd.print(HoraConFormato);
  lcd.setCursor(0, 1);
  lcd.write(1);
  lcd.print(" ");
  lcd.print(Temperatura);
  lcd.print("*C");
  lcd.print("  ");
  lcd.write(0);
  lcd.print(" ");
  lcd.print(Humedad);
  lcd.print("%");
  ActVal_FROM_DB();
  FuncionHora();

  if (Firebase.getBool(fbdo, "/Regar")) { 
    Regar = fbdo.boolData(); 
    if (Regar) { 
      FuncionRegar(); 
    }
  }
  Ver_S_o_N();

  digitalWrite(12, HIGH);
  ActVal_TO_DB();
  delay(250);  
  digitalWrite(12, LOW);
}
