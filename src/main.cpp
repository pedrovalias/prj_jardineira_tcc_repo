//#include <Arduino.h>

/* ******************** Rega Automática Pedrorv ********************
   Criado por: Pedro Henrique Ravanelli Valias
   Rev.: 01
   Data: 15.06.2020

   Guia de conexão:
   LCD RS: pino 12
   LCD Enable: pino 11
   LCD D4: pino 5
   LCD D5: pino 4
   LCD D6: pino 3
   LCD D7: pino 2
   LCD R/W: GND
   LCD VSS: GND
   LCD VCC: VCC (5V)
   Potenciômetro de 10K terminal 1: GND
   Potenciômetro de 10K terminal 2: V0 do LCD (Contraste)
   Potenciômetro de 10K terminal 3: VCC (5V)
   Sensor de umidade do solo A0: Pino A0
   Módulo Relé (Válvula): Pino 10
   Sensor indicador de ninvel de agua: Pino A5
   Sensor indicador de nivel máximo de água: Pino 7
   LED indicador de nível máximo de água: Pino 13

   Este código utiliza a biblioteca LiquidCrystal

   Library originally added 18 Apr 2008
   by David A. Mellis
   library modified 5 Jul 2009
   by Limor Fried (http://www.ladyada.net)

  //PLACA nodeMCU
  #define D0 16
  #define D1 5 SCL
  #define D2 4 SDA
  #define D3 0
  #define D4 2
  #define D5 14
  #define D6 12
  #define D7 13
  #define D8 15
  
   
 ***************************************************************************** */

// Inclui a biblioteca do LCD:
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include "FirebaseESP8266.h"
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Define a conexão entre o Arduino e o Display LCD utilizando a biblioteca I2C
LiquidCrystal_I2C lcd(0x27,16,2); // Endereco, colunas, linhas

// Define Constantes do Firebase
#define FIREBASE_HOST "teste-ndmc-db.firebaseio.com/"                 // URL DATABASE
#define FIREBASE_AUTH "6hTAnWY6vnHrF4vZB68CA2osuDyejpwdrzcuzJX4"      // SECRET KEY

// Define Constantes da conexao WiFi
#define WIFI_SSID "AP 303"                                                
#define WIFI_PASSWORD "c662d8983e" 

// Constantes sensor Temeperatura/Umidade do ar
#define DHTPIN 14
#define DHTTYPE DHT11 
DHT dht(DHTPIN,DHTTYPE);

// Define os pinos dos sensores dos sistema
const int sensorNivelMaximo = 2;           // PINO D2
const int pinoSensorUmidade = A0;          // PINO A0
const int pinoValvula = 0;                 // PINO D1

// Define os pinos da iluminacao
const int PINO_LED_R = 13;      // Vermelho   PINO D7
const int PINO_LED_G = 12;      // Verde      PINO D6
const int PINO_LED_B = 16;      // Azul       PINO D5
const int ledNivelAgua = 15;    //            PINO D8

// Variaveis constantes do programa
int limiarSeco = 0;             // Porcentagem determinada para umidade do solo aceitavel 
int tempoRega = 0;              // Tempo de acao da rega em segundos
int tempoLoop = 0;              // Tempo que vai levar a cada acao de regagem em horas

// Inicializa todos os sensores zerados
int nivelMaximo = 0;            // inicializador deteccao liquido
int umidadeSolo = 0;            // inicializador sebsor umidade do solo

// Inicializa contador de execucoes do programa
int contador = 0;

// Inicializa uma variavel para limpreza do LCD
bool limpaLCD = true;

// Declaracao das funcoes do programa
void escreveLCD(bool limpa, int coluna, int linha, String texto);
int verificaNivel();
void ativarLed(char corLed[10]);
void realiza_rega(int tipoRega);
void regar();
void obterDadosDHT();
void iniciarWifi();

//Define FirebaseESP8266 data object
FirebaseData firebaseData;
FirebaseJson json;

// void printResult(FirebaseData &data);
// String myString;

// Declara uma variavel para o caminho raiz dos dados no banco de dados
String path = "/Dados";

void setup() {

  Serial.begin(115200);   // Inicializa o console serial 
  lcd.init();             // Inicializa o visor LCD
  lcd.backlight();        // Inicializa a iluminacao do LCD
  dht.begin();            // Inicializa o sensor DHT (Temperatura e umidade relativa do ambiente)

  limiarSeco = 75;        // Porcentagem determinada para umidade do solo aceitavel 
  tempoRega = 9;          // Tempo de acao da rega em segundos
  tempoLoop = 12;         // Tempo em horas de intervalo entre as acoes de rega

  // Define o pino como saída e inicializa a valvula de agua como desligada
  pinMode(pinoValvula, OUTPUT);
  digitalWrite(pinoValvula, HIGH);

  // Define os LEDS informativos como saída e o detector de água como entrada - funciona como um botão
  pinMode(PINO_LED_R, OUTPUT); // LED vermelho
  pinMode(PINO_LED_G, OUTPUT); // LED verde
  pinMode(PINO_LED_B, OUTPUT); // LED azul
  pinMode(ledNivelAgua, OUTPUT); 
  pinMode(sensorNivelMaximo, INPUT);

  Serial.println("#### PARÂMETROS DO SISTEMA ####");
  Serial.print("Tempo loop:  "); 
  Serial.println(tempoLoop,0);
  Serial.print("Tempo rega: ");
  Serial.println(tempoRega);
  Serial.print("Limiar Seco: ");
  Serial.println(limiarSeco);
  Serial.println("-------------------------------");

  delay(500);

  iniciarWifi();
}

void loop() {
  // Lê o estado do pino destinado ao detector de agua
  //  ######### detecAgua = digitalRead(sensorNivelMaximo);
  
  Serial.println("# SISTEMA INICIADO");

  escreveLCD(limpaLCD,0,0,"Rega Automatica ");

  delay(2000);

  // Se for a primeira execucao do programa (contador = 0), realiza a rega
  if(contador < 1){
      realiza_rega(contador);
  }

  escreveLCD(limpaLCD,0,0,"Rega Automatica ");

  // Mede a umidade do solo a cada 3 segundos. Faz isso durante tempo estipulado (9 horas)
  for(int i=0; i < tempoLoop; i++) { //tempoloop
    escreveLCD(!limpaLCD,0,1,"Umidade: "); 
    
    // Faz a leitura do sensor de umidade do solo
    umidadeSolo = analogRead(pinoSensorUmidade);
    
    // Converte a variação do sensor de 0 a 1023 para 0 a 100
    umidadeSolo = map(umidadeSolo, 1023, 0, 0, 100);

    // Envia leitura de umidade ao Firebase em tempo real
    Firebase.setInt(firebaseData, path + "/umidade_solo",umidadeSolo);

    // Exibe a mensagem no Display LCD:
    lcd.print(umidadeSolo);
    lcd.print(" %    ");

    obterDadosDHT();

    // Exibe tempo espera no console para acompanhamento
    Serial.print(i);
    Serial.print(". ");

    if(Firebase.getBool(firebaseData, "/Acionamentos/Rega")){
      if(firebaseData.boolData() == true) {
        Serial.println("# Rega acionada manualmente #");
        Serial.println(firebaseData.boolData());
        regar();
        Firebase.setBool(firebaseData, "/Acionamentos/Rega",false);
      }
    }
    else {
      Serial.println(firebaseData.errorReason());
    }

    // Espera 3 segundos
    delay(3000);
  }

  realiza_rega(2);
  
  contador ++;
  Serial.print("Execuções do programa: ");
  Serial.println(contador);
  Serial.println("_______________________________");
  Serial.println(" ");
}

/* Funcao para escrecver no LCD
   * Limpa o LCD  
   * Posiciona o cursor do LCD na coluna 0 linha 1
   * (Obs: linha 1 é a segunda linha, a contagem começa em 0
   * Exibe a mensagem no Display LCD
  */
void escreveLCD(bool limpa, int coluna, int linha, String texto) {
  if(limpa == true){
    lcd.clear();
  }
  lcd.setCursor(coluna, linha);
  lcd.print(texto);
}

void realiza_rega(int tipoRega){
  
  if(tipoRega == 0){
    Serial.println("Primeira Rega - inicialização");
    regar();
  } else {
    
    /* Verifica condição de humidade do solo
     * Se solo estiver com humidade abaixo do valor estipulado, segue com a rega
    */
    if(umidadeSolo < limiarSeco) {
      regar();
    } else {
      // Desliga a valvula de agua (logica invertida - depende de como estiver ligado o modulo rele)
      digitalWrite(pinoValvula, HIGH);
      Firebase.setBool(firebaseData, path + "/valvula_status",digitalRead(pinoValvula));
      escreveLCD(!limpaLCD,0,1,"Solo Encharcado ");
      Serial.println("Solo encharcado");
      ativarLed("vermelho");
    }
  }
  return;
}

// Inicializa acao de rega da jardineira   
void regar(){

  /* Se verificacao do sensor nivel maximo nao detectar contato com a agua
   * (sensor == LOW)
  */
  if(!verificaNivel()){
    
    Serial.println("Iniciando Rega");

    // Variavel de controle para saida do laco de repeticao
    int controle = 1;

    // Enquanto estiver na condição de controle, funciona regagem
    while(controle != 0) {
      escreveLCD(!limpaLCD,0,1,"    Regando     ");

      // Ativa LED indicativo de processo de rega em andamento
      ativarLed("azul");
      
      // Liga a valvula de agua
      digitalWrite(pinoValvula, LOW);
      Firebase.setBool(firebaseData, path + "/valvula_status",digitalRead(pinoValvula));

      Serial.print("Regando | estado válvula: ");
      Serial.println(pinoValvula);
      
      // Espera o tempo de rega estipulado na declaracao de constantes
      Serial.print("Tempo funcionamento válvula: ");
      Serial.println(tempoRega);
      for(int j=0; j<tempoRega*1000 ; j+=1000){
        Serial.println("for: j= ");
        Serial.println(j);
  
        /* Valida nivel de agua e atualiza condição de controle
          * Se vaso ficou cheio, finaliza a regagem
        */
        if(verificaNivel()){
          j = tempoRega * 1000;
          Serial.print("Tempo rega de controle: ");
          Serial.println(j);
          // Serial.println(s);
          controle = 0;
        } else {
          /* Vai atualizando a mensagem do LCD conforme andamento da regagem
            * Divide o tempo determinado em 3 partes, exibindo um ponto de controle a cada parte atingida 
            */
          escreveLCD(!limpaLCD,0,1,"    Regando.    ");
          Serial.println("Regando.");
          
          if(j >= (tempoRega*1000)/3 && j < (tempoRega*1000)/3*2){
            escreveLCD(!limpaLCD,0,1,"    Regando..   ");
            Serial.println("Regando..");
          }
          if(j >= (tempoRega*1000/3)*2){
            escreveLCD(!limpaLCD,0,1,"    Regando...  ");
            Serial.println("Regando...");
          }
          
          delay(1000);  
        }
      }

      // digitalWrite(pinoValvula, LOW);
      lcd.clear();
      controle = 0;
    }
    // Desliga a valvula de agua e ativa LED informativo de conclusão de rega
    digitalWrite(pinoValvula, HIGH);
    Firebase.setBool(firebaseData, path + "/valvula_status",digitalRead(pinoValvula));
    escreveLCD(limpaLCD,0,1,"REGA FINALIZADA!");
    Serial.println("Processo de rega finalizado");
    ativarLed("verde");
    delay(3000);
  }
}

// Funcao que verifica se o nivel de agua esta no maximo ou nao 
int verificaNivel(){
  nivelMaximo = digitalRead(sensorNivelMaximo);
  Firebase.setInt(firebaseData, path + "/nivel_maximo",nivelMaximo);

  // Se detectado contato com a agua 
  if(nivelMaximo == HIGH){
    escreveLCD(!limpaLCD,0,1,"Vaso esta cheio!");
    Serial.println("Água detectada. Vaso está cheio!");
    
    // Desliga a valvula de agua, caso esteja ligada
    digitalWrite(pinoValvula, HIGH);
    Firebase.setBool(firebaseData, path + "/valvula_status",digitalRead(pinoValvula));

    // Liga LEDS informativos tanto da caixa quanto do vaso da jardineira
    digitalWrite(ledNivelAgua, HIGH);
    Firebase.setBool(firebaseData, path + "/valvula_status",digitalRead(ledNivelAgua));
    ativarLed("vermelho");
    delay(3000);
  } else {
    Serial.println("Vaso incompleto, permitir rega");

    // Desliga LED informativo da caixa
    digitalWrite(ledNivelAgua, LOW);
    Firebase.setBool(firebaseData, path + "/valvula_status",digitalRead(ledNivelAgua));
  }
  Serial.print("Estado sensor nível: ");
  Serial.println(nivelMaximo);

  return nivelMaximo;
}

/* Funcao que aciona os LEDS da jardineira, de acordo com a cor passada por parametro
 * Cada cor tem uma definicao de acao personalizada 
*/
void ativarLed(char corLed[10]){
  if(corLed == "vermelho"){
    // Liga o LED vermelho e desliga o LED azul e verde
    digitalWrite(PINO_LED_R, HIGH); // Liga LED vermelho
    digitalWrite(PINO_LED_G, LOW);  // Desliga LED verde
    digitalWrite(PINO_LED_B, LOW);  // Desliga LED azul
    delay(700);                      

    // Pisca o LED vermelho conforme configuracao desejada
    digitalWrite(PINO_LED_R, LOW);  
    delay(700);
    digitalWrite(PINO_LED_R, HIGH);
    delay(700);
    digitalWrite(PINO_LED_R, LOW);
    delay(700);
    digitalWrite(PINO_LED_R, HIGH);
    delay(2000);
    digitalWrite(PINO_LED_R, LOW);
  }
  if(corLed == "verde"){
    // Liga LED verde e desliga LED vermelho e verde
    digitalWrite(PINO_LED_R, LOW);  // Desliga LED vermelho
    digitalWrite(PINO_LED_G, HIGH); // Liga LED verde
    digitalWrite(PINO_LED_B, LOW);  // Desliga LED azul
    delay(5000);                    
    digitalWrite(PINO_LED_G, LOW);  
  }
  if(corLed == "azul"){
    // Liga LED azul e desliga LED vermelho e verde
    digitalWrite(PINO_LED_R, LOW);  // Desliga LED vermelho
    digitalWrite(PINO_LED_G, LOW);  // Desliga LED verde
    digitalWrite(PINO_LED_B, HIGH); // Liga LED azul
    delay(2000);                    
  }
  if(corLed == "rosa"){
    // Liga LED vermelho e azul para criar rosa
    digitalWrite(PINO_LED_R, HIGH); // Liga LED vermelho
    digitalWrite(PINO_LED_G, LOW);  // Desliga LED verde
    digitalWrite(PINO_LED_B, HIGH); // Liga LED azul
    delay(2000);                    
    digitalWrite(PINO_LED_R, LOW);  // Desliga LED vermelho
    digitalWrite(PINO_LED_B, LOW);  // Desliga LED azul
  } 
  return;
}

// Funcao para desligar LED da jardineira
void desligarLed(){ 
  digitalWrite(PINO_LED_R, LOW); // Liga LED vermelho
  digitalWrite(PINO_LED_G, LOW);  // Desliga LED verde
  digitalWrite(PINO_LED_B, LOW); // Liga LED azul
  return;
}

// void recebeDadosFirebase(int dados){
//   int dadoRecebido = dados;

//   if(Firebase.getInt(firebaseData, "/XXXXXXX/CAMINHO/XXXXXXX")){
//     if(firebaseData.intData() == true) {
//       Serial.println("XXXX TESTE XXXX");
//       Serial.println(firebaseData.intData());
//       // digitalWrite(LED,HIGH);
//     }
//     else if(firebaseData.boolData() == false) {
//       Serial.println("ligth bool = ");
//       Serial.println(firebaseData.boolData());
//       // digitalWrite(LED,LOW);
//     }
//   }
//   else {
//     Serial.println(firebaseData.errorReason());
//   }
// }

void obterDadosDHT(){
  float h=dht.readHumidity(); 
  float t=dht.readTemperature(); 
  Serial.println("Temperatura: "); Serial.print(t);
  Serial.println("| Umidade do Ar: "); Serial.print(h); 
  Firebase.setFloat(firebaseData, "/Dados Ambiente/temperatura",t);
  Firebase.setFloat(firebaseData, "/Dados Ambiente/umidade_relativa",h);
}

void iniciarWifi(){
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando com o Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("CONECTADO COM O IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  //Set the size of WiFi rx/tx buffers in the case where we want to work with large data.
  firebaseData.setBSSLBufferSize(1024, 1024);

  //Set the size of HTTP response buffers in the case where we want to work with large data.
  firebaseData.setResponseSize(1024);

  //Set database read timeout to 1 minute (max 15 minutes)
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  //tiny, small, medium, large and unlimited.
  //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(firebaseData, "tiny");
}
















// Recebe como parâmetro 3 valores correspondentes a hora|minuto|segundo
// void tempo_espera(double hora, double minuto, double segundo){
//   double h = 0, m = 0, s = 0, tempo = 0;
  
//   h = (hora * 3600)* 1000;  // 1 hora é equivalente a 3600 segundos x 1000 milisegundos da função delay
//   m = (minuto * 60) * 1000; // 1 minutos é equivalente a 60 segundos x 1000 milisegundos da função delay
//   s = segundo * 1000;       // 1 segundo x 1000 milisegundos da função delay 

//   tempo = h + m + s;
//   Serial.println("Tempo de espera estipulado: "); 
//   Serial.println(tempo,0);  // Exibe o tempo estipulado, sem casas decimais
//   delay(tempo);
// }

// Funcao para ler o sensor de nivel de agua | DESCOMENTAR SE FOR UTILIZAR
// ################## Ainda dá para ajustar e explorar mais esse componente ##################
//int lerNivelAgua(){
//  nivelAgua = analogRead(nivelAguaPin);
//  return nivelAgua;
//}

  // Leitura do sensor de nível de líquidos #### TIRAR COMENTARIOS SE FOR UTILIZAR  
//  lcd.clear(); 
//  lcd.setCursor(0, 0);
//  int level = lerNivelAgua();
//  lcd.println("Nivel de Agua:  ");
//  
//  // TO DO: necessario ajustar e validar os parametros do sensor de nivel de agua
//  if(level <= 30){
//    lcd.setCursor(0, 1); 
//    lcd.println("VAZIO  |  ");
//    lcd.println(level);
//  }
//  else if(level > 30 && level <= 400){
//    lcd.setCursor(0, 1); 
//    lcd.println("BAIXO  |  ");
//    lcd.println(level);
//  }
//  else if(level > 200 && level <= 538){
//    lcd.setCursor(0, 1); 
//    lcd.println("MEDIO  |  ");
//    lcd.println(level);
//  }
//  else {
//    lcd.setCursor(0, 1); 
//    lcd.println("CHEIO  |  ");
//    lcd.println(level);
//  }
//  Serial.print("sensor = ");
//  Serial.print(level); 
//  Serial.print("\n"); 
//  delay(5000);