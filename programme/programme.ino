
#include <SD.h>
#include <Ethernet.h>
#include <Wire.h>
#include <SPI.h>
#include "rgb_lcd.h"
#include <avr/pgmspace.h>

/* Parametres de configuration */
#define MAC_ADDRESS	        {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}
#define IP_ADDRESS	        { 192, 168, 0, 100 }
#define SERVER_PORT	        80
#define SERIAL_BAUD	        9600
#define SD_CARD_PIN	        10
#define CS_PIN              4

#define web_buffer          64

#define GET                 10
#define PUT                 11

/* Buffer permettant de charger les chaines de characteres stockées en flash */
char buffer_progmem[64];

/* Stockage des chaine de charactere en flash */
const char APP_JSON[]                       PROGMEM = "application/json";
const char LIST_I2C[]                       PROGMEM = "i2c/list.txt";
const char REG_PARSE_JSON_PWM[]             PROGMEM = "{\"id\":%i,\"type\":\"PWM\",\"value\":%i}";
const char REG_PARSE_JSON_TOR[]             PROGMEM = "{\"id\":%i,\"type\":\"TOR\",\"value\":%i}";
const char REG_SERIALIZE_I2C_CHAR[]         PROGMEM = "{\"name\":\"%s\",\"value\":\"%s\"}";
const char REG_SERIALIZE_I2C_INT[]          PROGMEM = "{\"%s\":%i}";
const char * const _APP_JSON                PROGMEM = APP_JSON;
const char * const _LIST_I2C                PROGMEM = LIST_I2C;
const char * const _REG_PARSE_JSON_PWM      PROGMEM = REG_PARSE_JSON_PWM;
const char * const _REG_PARSE_JSON_TOR      PROGMEM = REG_PARSE_JSON_TOR;
const char * const _REG_SERIALIZE_I2C_CHAR  PROGMEM = REG_SERIALIZE_I2C_CHAR;
const char * const _REG_SERIALIZE_I2C_INT   PROGMEM = REG_SERIALIZE_I2C_INT;

/* Variables globales */
uint8_t   type_request;
char      reqParameter1_str[8];
char      reqParameter2_str[8];
char      reqParameter3_str[8];
char      reqParameter4_str[17];
int       valPwm[10];

/* Configuration du port du serveur */
EthernetServer server(SERVER_PORT);

/* Déclaration de l'écran i2c */
rgb_lcd screen;

void setup()
{
  /* Variable locale */
  int i;
  
  /* Configuration de l'écran LCD avec 2 lignes et 16 colonnes */
  screen.begin(16, 2);
  screen.setRGB(0, 255, 0);
  
  /* Initialisation du port serie */
  Serial.begin(SERIAL_BAUD);

  /* Initialisation de la carte SD */
  if (!SD.begin(CS_PIN))
  {
    Serial.println(F("Carte SD non presente"));
  }

  /* Declaration des pins digitales en sorties */
  for (i = 2; i <= 13; i++)
  {
    pinMode(i, OUTPUT);
  }
  
  /* Initialisation du serveur web */
  byte mac[] = MAC_ADDRESS; 
  byte ip[] = IP_ADDRESS;
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print(F("IP address : "));
  Serial.println(Ethernet.localIP());

  /* Restauration des valeurs des sorties digitales */
  restoreDigitalState();
}

void loop() {
  
  EthernetClient client = server.available();
  
  if (client)
  {
    /* Connection d'un client */
    Serial.println(F("Client connecte"));

    /* Gestion du client web et analyse de la requete pour traitement */
    if (listen_web_client(client) == 1)
    { 
      char fileRepository[16];

      /* Requete de type GET */
      if (type_request == GET)
      {
        /* Demande de la ressource analog */
        if (strstr(reqParameter1_str, "analog") != 0)
        {
          strcpy_P(buffer_progmem, (char*)pgm_read_word(&(    _APP_JSON   )));
          send_special_header(client, buffer_progmem);
          
          readAndSerializeAnalog(client);
        }
        /* Demande de la ressource digital */
        else if (strstr(reqParameter1_str, "digital") != 0)
        {
          strcpy_P(buffer_progmem, (char*)pgm_read_word(&(    _APP_JSON   )));
          send_special_header(client, buffer_progmem);
          
          readPwm();
          serializePwm(client);
        }
        /* Demande de la ressource i2c */
        else if (strstr(reqParameter1_str, "i2c") != 0)
        {
          /* Demande de la liste des périphériques i2c */
          if (strlen(reqParameter2_str) == 0)
          {
            strcpy_P(buffer_progmem, (char*)pgm_read_word(&(    _APP_JSON   )));
            send_special_header(client, buffer_progmem);
            
            strcpy_P(buffer_progmem, (char*)pgm_read_word(&(    _LIST_I2C   )));
            send_data(client, buffer_progmem);
          }
          /* Demande de l'état d'un périphérique i2c */
          else
          {
            strcpy_P(buffer_progmem, (char*)pgm_read_word(&(    _APP_JSON   )));
            send_special_header(client, buffer_progmem);
            
            sprintf(fileRepository, "%s/%s/%s.txt", reqParameter1_str, reqParameter2_str, reqParameter3_str);
            
            readStrFromSdCard(fileRepository, reqParameter4_str);
            
            if (strstr(reqParameter3_str, "line") != 0)
            {
              serializeDeviceInfos(client, reqParameter3_str, reqParameter4_str);
            }
          }
        }
        else
        {
          /* Renvoie erreur 404 pour une ressource invalide */
          send_header_404(client);
        }
      }
      /* Requete de type PUT */
      else if (type_request == PUT)
      {      
        /* Demande de modification de l'état d'une sortie digitale */
        if (strstr(reqParameter1_str, "digital") != 0)
        {
          send_header(client);

          sprintf(fileRepository, "D%s.txt", reqParameter2_str);
          
          updateFile(fileRepository, reqParameter3_str);

          uint8_t pin = atoi(reqParameter2_str);
          uint8_t value = atoi(reqParameter3_str);

          /* Sortie de type PWM */
          if (pin == 3 || pin == 5 || pin == 6 || pin == 9)
          {
            analogWrite(pin, value);
          }
          /* Sortie de type TOR */
          else if (pin == 2 || pin == 7 || pin == 8)
          {
            if (value > 0)    digitalWrite(pin, HIGH);
            else              digitalWrite(pin, LOW);
          }
        }
        /* Demande de la modification de l'état d'un périphérique i2c */
        else if (strstr(reqParameter1_str, "i2c") != 0)
        {
          send_header(client);

          sprintf(fileRepository, "%s/%s/%s.txt", reqParameter1_str, reqParameter2_str, reqParameter3_str);

          updateFile(fileRepository, reqParameter4_str);

          /* Périphérique de type écran LCD */
          if (strstr(reqParameter3_str, "line") != 0)
          {        
            Serial.println(F("Mise a jour ligne LCD"));
            if (strstr(reqParameter3_str, "0") != 0)
            {
              screen.setCursor(0, 0);
            }
            else if (strstr(reqParameter3_str, "1") != 0)
            {
              screen.setCursor(0, 1);
            }

            uint8_t i;
            uint8_t lnStr = strlen(reqParameter4_str);

            /* Remplace les & par des espaces */
            for (i = 0; i < lnStr; i++)
            {
              if (reqParameter4_str[i] == '&')    reqParameter4_str[i] = ' ';
            }

            /* Ajoute des espaces pour effacer l'écran */
            for (i = lnStr; i < 16; i++)
            {
              reqParameter4_str[i] = ' ';
            }
            reqParameter4_str[16] = '\0';

            /* Mise à jour de l'affichage sur l'écran */
            screen.print(reqParameter4_str);
          }
        }
        else
        {
          /* Renvoie erreur 404 pour une ressource invalide */
          send_header_404(client);
        }
      }
      
      /* Cloture la la liaison avec le client */
      client.stop();
    }
  }
}

