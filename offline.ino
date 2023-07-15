#include <ESP_LM35.h> 
#include <LiquidCrystal_I2C.h>

#include <EEPROM.h>
#define HORA_ULTIMO_RIEGO_ADDRESS 0
#define MINUTO_ULTIMO_RIEGO_ADDRESS 2

void GuardarVar(int direccion, int valor) {
  byte byte_alto = highByte(valor);
  byte byte_bajo = lowByte(valor);

  EEPROM.write(direccion, byte_alto);
  EEPROM.write(direccion + 1, byte_bajo);
  EEPROM.commit();
}

// Función para cargar una variable de dos bytes desde la EEPROM
int CargarVar(int direccion) {
  byte byte_alto = EEPROM.read(direccion);
  byte byte_bajo = EEPROM.read(direccion + 1);

  int valor = word(byte_alto, byte_bajo);
  return valor;
}

LiquidCrystal_I2C lcd(0x27, 16, 2);
ESP_LM35 temp(34);

byte Gotita[8] = {0b00000, 0b00100, 0b01110, 0b01110, 0b11111, 0b11111, 0b01110, 0b00000};
byte Termom[8] = {0b00100, 0b01010, 0b01010, 0b01010, 0b01010, 0b10011, 0b10001, 0b01110};

// Variables de control
int Humedad = 0;
int Temperatura = 0;

int UmbHumedad = 25;
int UmbTemperatura = 10;
int UmbTiempo = 120;

int DurRiego = 5000;

int hora_actual = 0;
int minuto_actual = 0;

int hora_ultimo_riego = 0;
int minuto_ultimo_riego = 0;

bool Regar = false;
bool canRegar = true; 
bool canPuls = true;

String HoraConFormato;

volatile int modo = 1;
void cambiarmodo() {
  if (canPuls) {
    modo++;
  }
  canPuls = false;
}

void PrepararPines(){
  pinMode(15, INPUT); // Pulsador1 resta
  pinMode(2, INPUT); // Pulsador2 centro
  pinMode(4, INPUT); // Pulsador3 suma / riega

  pinMode(35, INPUT); // SENSOR DE TEMP
  pinMode(27, OUTPUT); //LED AZUL
  pinMode(14, OUTPUT); //LED ROJO
  pinMode(13, OUTPUT); //LED VERDE
  pinMode(12, OUTPUT); //LED AMARILLO

  attachInterrupt(digitalPinToInterrupt(2), cambiarmodo, RISING);
}

#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void HoraSetup() {
  lcd.clear();
  lcd.print("Obteniendo");
  lcd.setCursor(0, 1);
  lcd.print("hora. . .");
  WiFi.begin("HUAWEI-2.4G", "Fer198_ASD");
  delay(7000);
  timeClient.begin();
  timeClient.setTimeOffset(-18000);
  timeClient.update();
  WiFi.disconnect(true);
  lcd.clear();
}

void HoraLoop() {
  hora_ultimo_riego = CargarVar(HORA_ULTIMO_RIEGO_ADDRESS);
  minuto_ultimo_riego = CargarVar(MINUTO_ULTIMO_RIEGO_ADDRESS);
  hora_actual = timeClient.getHours();
  minuto_actual = timeClient.getMinutes();
  HoraConFormato = (hora_actual < 10 ? "0" : "") + String(hora_actual) + ":" + (minuto_actual < 10 ? "0" : "") + String(minuto_actual);
}

void FuncionRegar() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Regando...");
  lcd.setCursor(0, 1);
  lcd.print("Hora: ");
  lcd.print(HoraConFormato);

  // Preparacion
  GuardarVar(HORA_ULTIMO_RIEGO_ADDRESS, hora_actual);
  GuardarVar(MINUTO_ULTIMO_RIEGO_ADDRESS, minuto_actual);
  EEPROM.commit();
  canRegar = false;

  // Indicadores:
  digitalWrite(27, HIGH);
  
  delay(DurRiego);
  
  digitalWrite(27, LOW);

  lcd.clear();
  lcd.print("Se termino");
  lcd.setCursor(0, 1);
  lcd.print("de regar!");

  Regar = false;
  canRegar = true;
  delay(3000);

  lcd.clear(); 
}

void setup() {  
  Serial.begin(115200);
  EEPROM.begin(512);
  lcd.init();                    
  lcd.backlight();
  
  lcd.createChar(0, Gotita);
  lcd.createChar(1, Termom);
  
  PrepararPines();
  HoraSetup();
  hora_ultimo_riego = CargarVar(HORA_ULTIMO_RIEGO_ADDRESS);
  minuto_ultimo_riego = CargarVar(MINUTO_ULTIMO_RIEGO_ADDRESS);

  delay(500);
  lcd.clear();
  lcd.print("Iniciando. . .");
  PrepararPines();
  delay(500);

  int i = 0;
  while (i < 25) {
    digitalWrite(13, HIGH);
    delay(120);
    digitalWrite(13, LOW);
    delay(120);
    i++;
  }
  lcd.clear();
  lcd.print("Exito!");
  
  digitalWrite(13, HIGH);
  digitalWrite(12, HIGH);
  digitalWrite(14, HIGH);
  digitalWrite(27, HIGH);
  delay(250);
  digitalWrite(14, LOW);
  digitalWrite(13, LOW);
  digitalWrite(12, LOW);
  digitalWrite(27, LOW);

  lcd.clear();
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

float tempsec = 5.0;
void loop() {
  HoraLoop();
  int P1status = digitalRead(15);
  int P2status = digitalRead(2);
  int P3status = digitalRead(4);
  
  Humedad = map(analogRead(35), 4095, 0, 0, 100);
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
      
    case 1:
      if (P3status == HIGH) { FuncionRegar(); };
      digitalWrite(12, HIGH);
      Ver_S_o_N();

      lcd.setCursor(0, 0);
      lcd.print("Hora: ");
      lcd.print(HoraConFormato + "      ");
      
      lcd.setCursor(0, 1);
      lcd.write(1);
      lcd.print(" " + String(Temperatura) + "*C  ");
      lcd.write(0);
      lcd.print(" " + String(Humedad) + "%      ");
      
      delay(250);
      digitalWrite(12, LOW);
      break;
  }

  if (modo >= 6 && canPuls) {
    modo = 1;
  }

  int tempC = analogRead(34); 
   
  // Calculamos la temperatura con la fórmula
  tempC = (1.1 * tempC * 100.0)/1024.0;
  Serial.println(tempC);
  Serial.println(String(hora_ultimo_riego) + " : " + String(minuto_ultimo_riego));
  delay(400);
  canPuls = true;
}
