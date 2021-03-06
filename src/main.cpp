#include <Arduino.h>

/* ************************* Rega Automatica Pedrorv *************************
   Criado por: Pedro Henrique Ravanelli Valias
   Rev.: 01
   Data: 15.06.2020

   Guia de conexão:
   LCD RS: pino 12
   LCD Enable: pino 11
   LCD VSS: GND
   LCD VCC: VCC (5V)

   Sensor de umidade do solo: Pino A0
   Módulo Relé: Pino ??
   Válvula Solenóide: Pino ??
   Sensor Temperatura DHT11: Pino ??
   Sensor indicador de nivel máximo de água: Pino ??
   LED RGB indicador de ações da jardineira: Pino ?? 
   LED indicador de nível máximo de água: Pino ??


   Este código utiliza a biblioteca LiquidCrystal

   Library originally added 18 Apr 2008
   by David A. Mellis
   library modified 5 Jul 2009
   by Limor Fried (http://www.ladyada.net)

  Guia pinagem útil NodeMCU
  #define D0 16
  #define D1 5 SCL
  #define D2 4 SDA
  #define D3 0
  #define D4 2
  #define D5 14
  #define D6 12
  #define D7 13
  #define D8 15
  
   
 ****************************************************************************** */

// Inclui a biblioteca do LCD:
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <FirebaseESP8266.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


// Define a conexão entre o Arduino e o Display LCD utilizando a biblioteca I2C
LiquidCrystal_I2C lcd(0x27,16,2); // Endereco, colunas, linhas

// Define Constantes do Firebase
#define FIREBASE_HOST "teste-ndmc-db.firebaseio.com/"                 // URL DATABASE
#define FIREBASE_AUTH "6hTAnWY6vnHrF4vZB68CA2osuDyejpwdrzcuzJX4"      // SECRET KEY

// Define Constantes da conexao WiFi
#define WIFI_SSID "AP 303"                                                
#define WIFI_PASSWORD "c662d8983e" 

// Constantes sensor Temeperatura/Umidade do ar
#define DHTPIN 0 // Pino D3
#define DHTTYPE DHT11 
DHT dht(DHTPIN,DHTTYPE);
int contadorAltaTemperatura = 0;
int contadorBaixaTemperatura = 0;
boolean autoAjusteLoop = true;

// Define os pinos dos sensores dos sistema
const int sensorNivelMaximo = 16;           // PINO D0
const int pinoSensorUmidade = A0;          // PINO A0
const int pinoValvula = 2;                 // PINO D4

// Define os pinos da iluminacao
const int PINO_LED_R = 12;      // Vermelho   PINO D6
const int PINO_LED_G = 14;      // Verde      PINO D5
const int PINO_LED_B = 13;      // Azul       PINO D7
// const int ledNivelAgua = 0;    //            PINO D3

// Variaveis constantes do programa
int limiarSeco = 0;             // Porcentagem determinada para umidade do solo aceitavel 
int tempoRega = 0;              // Tempo de acao da rega em segundos
int tempoLoop = 0;              // Tempo que vai levar a cada acao de regagem em horas
int tempoLoopControle = 0;
const int delayLoop = 5;
const int quatroHoras = 4;//2880;   // 4*3600/delayLoop = 2880 = 4h

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
int obterDadosDHT(int i);
void iniciarWifi();
void info_serial();
void executa_loop();
void envia_dados_firebase();
int verifica_alteracao_parametros(int t);
void iniciarArduinoOTA ();
void gravarLog();

//Define FirebaseESP8266 data object
FirebaseData firebaseData;
FirebaseJson json;

bool conexao_status = false;

int id = 0001;

// void printResult(FirebaseData &data);
// String myString;

// Declara uma variavel para o caminho raiz dos dados no banco de dados
String jd_x = "/Jardineira_x";

//=======================================================================================================
//                                            INICIO SETUP
//=======================================================================================================
void setup() {

  Serial.begin(115200);   // Inicializa o console serial 
  lcd.init();             // Inicializa o visor LCD
  lcd.backlight();        // Inicializa a iluminacao do LCD
  dht.begin();            // Inicializa o sensor DHT (Temperatura e umidade relativa do ambiente)

  limiarSeco = 75;        // Porcentagem determinada para umidade do solo aceitavel 
  tempoRega = 9;          // Tempo de acao da rega em segundos
  tempoLoop = 2;         // Tempo em horas de intervalo entre as acoes de rega

  // Define o pino da valvula como saída e a inicializa desligada
  pinMode(pinoValvula, OUTPUT);
  digitalWrite(pinoValvula, HIGH);

  // Define os LEDS informativos como saida e o detector de agua como entrada - funciona como um botao
  pinMode(PINO_LED_R, OUTPUT); // LED vermelho
  pinMode(PINO_LED_G, OUTPUT); // LED verde
  pinMode(PINO_LED_B, OUTPUT); // LED azul
  // pinMode(ledNivelAgua, OUTPUT); 
  pinMode(sensorNivelMaximo, INPUT_PULLUP);

  info_serial();
  
  delay(500);

  iniciarWifi();

  envia_dados_firebase();

//============================================ FIM SETUP ================================================
}


//=======================================================================================================
//                                            INICIO LOOP
//=======================================================================================================
void loop() {
  // Verifica se tem algo a ser atualizado no software
  ArduinoOTA.handle();
  
  Serial.println("# SISTEMA INICIADO");

  escreveLCD(limpaLCD,0,0,"Rega Automatica ");

  delay(2000);

  // Se for a primeira execucao do programa (contador = 0), realiza a rega
  if(contador < 1){
      realiza_rega(contador);
  }

  escreveLCD(limpaLCD,0,0,"Rega Automatica ");

  executa_loop();

  realiza_rega(2);
  
  contador ++;
  Firebase.setInt(firebaseData, jd_x + "/Info/execucoes_programa", contador);
  Serial.print("Execuções do programa: ");
  Serial.println(contador);
  Serial.println("_______________________________");
  Serial.println(" ");

//============================================ FIM LOOP ================================================
}

void envia_dados_firebase() {
  Firebase.setInt(firebaseData, jd_x + "/Identificador/id", id);
  Firebase.setInt(firebaseData, jd_x + "/Acionamentos/limiar_seco", limiarSeco);
  Firebase.setInt(firebaseData, jd_x + "/Acionamentos/tempo_rega", tempoRega);
  Firebase.setInt(firebaseData, jd_x + "/Acionamentos/tempo_loop", tempoLoop);
  Firebase.setBool(firebaseData, jd_x + "/Acionamentos/auto_ajuste_loop", autoAjusteLoop);
}

/* Funcao para escrever no LCD
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

void info_serial() {
  Serial.println("");
  Serial.println("#### PARÂMETROS DO SISTEMA ####");
  Serial.print("Tempo loop:  "); 
  Serial.println(tempoLoop);
  Serial.print("Tempo rega: ");
  Serial.println(tempoRega);
  Serial.print("Limiar Seco: ");
  Serial.println(limiarSeco);
  Serial.println("-------------------------------");
}

void executa_loop() {

  // Hora = 3600s/delay(5s) = 720s
  tempoLoopControle = tempoLoop;//(tempoLoop*3600)/delayLoop;
  Firebase.setInt(firebaseData, jd_x + "/Teste/tempoLoopControle",tempoLoopControle);

  Serial.print("Tempo Loop controle: "); Serial.println(tempoLoopControle);
  
  int t = 0;

  obterDadosDHT(t);
   
// Mede a umidade do solo a cada 3 segundos. Faz isso durante tempo estipulado (9 horas)
  for(int i=0; i < tempoLoopControle ; i++) { // Hora = 3600s/delay(5s) = 720s
    // Verifica se tem algo a ser atualizado no software
    ArduinoOTA.handle();
    // t = i;

    if(WiFi.status() != WL_CONNECTED) {
      Firebase.setBool(firebaseData, jd_x + "/Conexao/conexao_status", conexao_status);
      Serial.println("Reconectando WiFi..");
      iniciarWifi();
    }

    i = obterDadosDHT(i);

    Firebase.setInt(firebaseData, jd_x + "/Teste/i_for",i);

    Firebase.setInt(firebaseData, jd_x + "/Teste/tempoLoopControle_Validacao", tempoLoopControle);
    
    // Se for alterado o tempo de Loop, o loop atual será finalizado (i = tempoLoop)
    i = verifica_alteracao_parametros(i);

    Firebase.setInt(firebaseData, jd_x + "/Teste/i_for",i);

    escreveLCD(limpaLCD,0,0,"Rega Automatica ");
    escreveLCD(!limpaLCD,0,1,"Umidade: "); 
    
    // Faz a leitura do sensor de umidade do solo
    umidadeSolo = analogRead(pinoSensorUmidade);
    
    // Converte a variação do sensor de 0 a 1023 para 0 a 100
    umidadeSolo = map(umidadeSolo, 1023, 0, 0, 100);

    // Envia leitura de umidade ao Firebase em tempo real
    Firebase.setInt(firebaseData, jd_x + "/Sensores/umidade_solo",umidadeSolo);

    // Exibe a mensagem no Display LCD:
    lcd.print(umidadeSolo);
    lcd.print(" %    ");

    // Exibe tempo espera no console para acompanhamento
    Serial.print(i);
    Serial.print(". ");

    // Verifica se houve acionamento manual da rega
    if(Firebase.getBool(firebaseData, jd_x + "/Acionamentos/rega")){
      if(firebaseData.boolData() == true) {
        Serial.println("# Rega acionada manualmente #");
        Serial.println(firebaseData.boolData());
        escreveLCD(!limpaLCD,0,1,"REGA MANUAL - ON");
        regar();
        Firebase.setBool(firebaseData, jd_x + "/Acionamentos/rega",false);
      }
    }
    else {
      Serial.println(firebaseData.errorReason());
    }

    // Espera 5 segundos
    delay(delayLoop*1000);


    // gravarLog();
  }
}

// Funcao para validar se houve alguma alteracao de parametros via aplicativo movel
int verifica_alteracao_parametros(int t){

  if(Firebase.getInt(firebaseData, jd_x + "/Acionamentos/tempo_loop")){
    if(firebaseData.intData() != tempoLoop) {
      Serial.println("TempoLoopDb: "); Serial.println(firebaseData.intData());
      tempoLoop = firebaseData.intData();
      Serial.print("Tempo loop alterado: "); Serial.println(tempoLoop);
      t = tempoLoopControle;
      String txtTempoLoop = String(tempoLoop);
      escreveLCD(limpaLCD, 0,0,"CONFIG. ALTERADA");
      escreveLCD(!limpaLCD,0,1,"Tempo Loop: ");
      escreveLCD(!limpaLCD,0,1,txtTempoLoop);
      escreveLCD(!limpaLCD,0,1,"H ");
      delay(3000);
    }
  }
  
  if (Firebase.getInt(firebaseData, jd_x + "/Acionamentos/tempo_rega")){
    if(firebaseData.intData() != tempoRega) {
      Serial.println("TempoRegaDB: "); Serial.println(firebaseData.intData());
      tempoRega = firebaseData.intData();
      Serial.print("Tempo Rega alterado: "); Serial.println(tempoRega);
      String txtTempoRega = String(tempoRega);
      escreveLCD(limpaLCD, 0,0,"CONFIG. ALTERADA");
      escreveLCD(!limpaLCD,0,1,"Tempo Rega: ");
      escreveLCD(!limpaLCD,0,1,txtTempoRega);
      escreveLCD(!limpaLCD,0,1,"s ");
      delay(3000);
    }
  }
  
  if (Firebase.getInt(firebaseData, jd_x + "/Acionamentos/limiar_seco")){
    if(firebaseData.intData() != limiarSeco) {
      Serial.println("LimiarSecoDB: "); Serial.println(firebaseData.intData());
      limiarSeco = firebaseData.intData();
      Serial.print("Limiar Seco alterado: "); Serial.println(limiarSeco);
      String txtLimiarSeco = String(limiarSeco);
      escreveLCD(limpaLCD, 0,0,"CONFIG. ALTERADA");
      escreveLCD(!limpaLCD,0,1,"Limiar Seco: ");
      escreveLCD(!limpaLCD,0,1, txtLimiarSeco);
      escreveLCD(!limpaLCD,0,1,"%");
      delay(3000);
    }
  }

  if (Firebase.getBool(firebaseData, jd_x + "/Acionamentos/auto_ajuste_loop")){
    if(firebaseData.boolData() != autoAjusteLoop) {
      Serial.println("AutoAjusteDB: "); Serial.println(firebaseData.boolData());
      autoAjusteLoop = firebaseData.boolData();
      Serial.print("Auto ajuste alterado: "); Serial.println(autoAjusteLoop);
      String txtAutoAjuste = String(autoAjusteLoop);
      escreveLCD(limpaLCD, 0,0,"CONFIG. ALTERADA");
      escreveLCD(!limpaLCD,0,1,"Auto Ajuste:");
      escreveLCD(!limpaLCD,0,1, txtAutoAjuste);
      delay(3000);
    }
  }
  else{
    Serial.println(firebaseData.errorReason());
  }
  Firebase.setInt(firebaseData, jd_x + "/Acionamentos/tempo_loop",tempoLoop);
  Firebase.setInt(firebaseData, jd_x + "/Acionamentos/tempo_rega",tempoRega);
  Firebase.setInt(firebaseData, jd_x + "/Acionamentos/limiar_seco",limiarSeco);
  Firebase.setBool(firebaseData, jd_x + "/Acionamentos/auto_ajuste_loop",autoAjusteLoop);

  return t;
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
      escreveLCD(!limpaLCD,0,1,"Solo Encharcado ");
      Serial.println("Solo encharcado");
      ativarLed("vermelho");
    }
  }
  return;
}

// Inicializa acao de rega da jardineira   
void regar(){
  // Como esta regando, reinicializa o contator de leitura da temperatura
  contadorAltaTemperatura = 0;
  contadorBaixaTemperatura = 0;

  /* Se verificacao do sensor nivel maximo nao detectar contato com a agua
   * (sensor == LOW)
  */
  if(!verificaNivel()){
    
    Serial.println("Iniciando Rega");

    // Variavel de controle para saida do laco de repeticao
    int controle = 1;

    // Enquanto estiver na condição de controle, funciona regagem
    while(controle != 0) {
      // Verifica se tem algo a ser atualizado no software
      ArduinoOTA.handle();
      escreveLCD(!limpaLCD,0,1,"    Regando     ");

      // Ativa LED indicativo de processo de rega em andamento
      ativarLed("azul");
      
      // Liga a valvula de agua
      digitalWrite(pinoValvula, LOW);
      Firebase.setBool(firebaseData, jd_x + "/Sensores/valvula_status",digitalRead(pinoValvula));

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
    Firebase.setBool(firebaseData, jd_x + "/Sensores/valvula_status",digitalRead(pinoValvula));
    escreveLCD(limpaLCD,0,1,"REGA FINALIZADA!");
    Serial.println("Processo de rega finalizado");
    ativarLed("verde");
    delay(3000);
  }
}

// Funcao que verifica se o nivel de agua esta no maximo ou nao 
int verificaNivel(){
  nivelMaximo = digitalRead(sensorNivelMaximo);
  Firebase.setBool(firebaseData, jd_x + "/Sensores/nivel_maximo",nivelMaximo);

  // Se detectado contato com a agua 
  if(nivelMaximo == HIGH){
    escreveLCD(!limpaLCD,0,1,"Vaso esta cheio!");
    Serial.println("Água detectada. Vaso está cheio!");
    
    // Desliga a valvula de agua, caso esteja ligada
    digitalWrite(pinoValvula, HIGH);
    Firebase.setBool(firebaseData, jd_x + "/Sensores/valvula_status",digitalRead(pinoValvula));

    // Liga LEDS informativos tanto da caixa quanto do vaso da jardineira
    // digitalWrite(ledNivelAgua, HIGH);
    ativarLed("vermelho");
    delay(3000);
  } else {
    Serial.println("Vaso incompleto, permitir rega");

    // Desliga LED informativo da caixa
    // digitalWrite(ledNivelAgua, LOW);
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

int obterDadosDHT(int i){

  float h=dht.readHumidity(); 
  float t=dht.readTemperature(); 

  // Valida se existe algum erro de leitura dos valores do DHT 
  // Se houver, seta a umidade e temperatura com valor zero
  if (isnan(h) || isnan(t)) { 
    h=0; t=0; 
  }

  // Serial.print("# Temperatura: "); Serial.print(t);
  // Serial.print("| Umidade do Ar: "); Serial.println(h);

  /* 
     Realiza duas verificacoes: 
     Se auto ajuste estiver selecionado, sera alterado o comportamento de rega da jardineira
     Para temperatura registrada acima de 30 graus por um periodo de 4h, tempo rega alterado para acontecer a cada 5h
     Para temperatura abaixo de 28 pelo periodo de 4h, tempo rega alterado para a cada 9h
  */
  if(t > 30 && autoAjusteLoop) {
    contadorAltaTemperatura++;
    Firebase.setInt(firebaseData, jd_x + "/Teste/contadorAltaTemperatura",contadorAltaTemperatura);
  
    if(contadorAltaTemperatura >= quatroHoras && tempoLoop >= 7) {
      i = tempoLoopControle;
      tempoLoop = 5;
      Firebase.setInt(firebaseData, jd_x + "/Teste/alterou_loop_automatico",tempoLoop);
      Firebase.setInt(firebaseData, jd_x + "/Acionamentos/tempo_loop",tempoLoop);
      Serial.println("Auto ajuste acionado. TempoLoop: "); Serial.println(tempoLoop);  
    }
  }
  if(t < 28 && autoAjusteLoop) {
    contadorBaixaTemperatura++;
    Firebase.setInt(firebaseData, jd_x + "/Teste/contadorBaixaTemperatura",contadorBaixaTemperatura);
  
    if(contadorBaixaTemperatura >= quatroHoras && tempoLoop < 7) {
      i = tempoLoopControle;
      tempoLoop = 9;
      Firebase.setInt(firebaseData, jd_x + "/Teste/alterou_loop_automatico",tempoLoop);
      Firebase.setInt(firebaseData, jd_x + "/Acionamentos/tempo_loop",tempoLoop);
      Serial.println("Auto ajuste acionado. TempoLoop: "); Serial.println(tempoLoop);  
    }
  }
  
  // Envia a temperatura e a umidade relativa do ar ao Firebase 
  Firebase.setInt(firebaseData, jd_x + "/Dados Ambiente/temperatura",t);
  Firebase.setInt(firebaseData, jd_x + "/Dados Ambiente/umidade_relativa",h);

  Firebase.setInt(firebaseData, jd_x + "/Teste/i_obter",i);
  
  return i;
}

void iniciarWifi(){
  // TODO : SETAR UM IP FIXO PARA O NODEMCU
  IPAddress ip;

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

  conexao_status = WiFi.status();
  ip = WiFi.localIP();
  Firebase.setBool(firebaseData, jd_x + "/Conexao/conexao_status", conexao_status);
  Firebase.setString(firebaseData, jd_x + "/Conexao/ip",ip.toString());
  iniciarArduinoOTA();
}

void iniciarArduinoOTA () {
  // Porta padrao do ESP8266 para OTA eh 8266 - Voce pode mudar ser quiser, mas deixe indicado!
  // ArduinoOTA.setPort(8266);
 
  // O Hostname padrao eh esp8266-[ChipID], mas voce pode mudar com essa funcao
  ArduinoOTA.setHostname("jardineira_smart");
 
  // Nenhuma senha eh pedida, mas voce pode dar mais seguranca pedindo uma senha pra gravar
  // NÃO FUNCIONA
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    escreveLCD(limpaLCD,0,0,"  UPLOADING...  ");
    Serial.println("Inicio...");
  }); 
  ArduinoOTA.onEnd([]() {
    Serial.println("nFim!");
    escreveLCD(limpaLCD,0,0,"     UPLOAD    ");
    escreveLCD(!limpaLCD,0,1,"   FINALIZADO   ");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progresso: %u%%r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Erro [%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Autenticacao Falhou");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Falha no Inicio");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Falha na Conexao");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Falha na Recepcao");
    else if (error == OTA_END_ERROR) Serial.println("Falha no Fim");
  });
  ArduinoOTA.begin();
  Serial.println("Pronto");
  Serial.print("Endereco IP: ");
  Serial.println(WiFi.localIP());
}

void gravarLog() {
  int n = contador;
  // Firebase.pushInt(firebaseData,jd_x + "/Logs", n);
  String txtN = String(n);
  String teste = "teste";
  Firebase.pushString(firebaseData,jd_x + "/Logs", teste + n + "texto: " + txtN);
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