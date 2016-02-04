
/* Fonction reception des donnees du client web */
int listen_web_client
(
  EthernetClient client     /* Référence sur le client */
)
{
  /* Variable locale */
  char buffer[web_buffer];
  char request_extracted[32];

  if (client)
  {
    while (client.connected() && client.available())
    {
      search_request(client, buffer, web_buffer);

      /* Identification d'une requete de type GET */
      if (strstr(buffer, "GET ") != 0)
      {
        Serial.println(F("Methode get : "));
        type_request = GET;
        extract_request(buffer, request_extracted);
      }
      /* Identification d'une requete de type PUT */
      else if (strstr(buffer, "PUT ") != 0)
      {
        Serial.println(F("Methode put : "));
        type_request = PUT;
        extract_request(buffer, request_extracted);
      }
    }
    
    return 1;
  }
  else
  {
    delay(10);
    
    client.stop();

    return 0;
  }
}

/* Fonction d'extraction des parametres de la requete */
void extract_request
(
  char * requestToLook,       /* Requete brute */
  char * request_extracted    /* Requete extraite */
)
{
  /* Variables locales */
  char * ptr_debut = NULL;
  char * ptr_fin = NULL;
  int pos_debut, pos_fin;

  ptr_debut = strchr(requestToLook, ' ') + 1;
  ptr_fin = strchr(ptr_debut + 1, ' ');

  pos_debut = ptr_debut - requestToLook + 1;
  pos_fin = ptr_fin - requestToLook + 1;

  strncpy(request_extracted, ptr_debut, pos_fin - pos_debut);
  request_extracted[pos_fin - pos_debut] = '\0';

  /* Suppression du / de debut */
  request_extracted = request_extracted + 1;

  /* Affichage dans la console */
  Serial.print("\'"); Serial.print(request_extracted); Serial.println("\'");

  /* Récupération du parametre 1 */
  ptr_debut = strtok(request_extracted,"/");
  if (strlen(ptr_debut) < sizeof(reqParameter1_str))
  {
    strcpy(reqParameter1_str, ptr_debut);
  }

  /* Récupération du parametre 2 */
  ptr_debut = strtok(NULL,"/");
  if (strlen(ptr_debut) < sizeof(reqParameter2_str))
  {
    strcpy(reqParameter2_str, ptr_debut);
  }

  /* Récupération du parametre 3 */
  ptr_debut = strtok(NULL,"/");
  if (strlen(ptr_debut) < sizeof(reqParameter3_str))
  {
    strcpy(reqParameter3_str, ptr_debut);
  }

  /* Récupération du parametre 4 */
  ptr_debut = strtok(NULL,"/");
  if (strlen(ptr_debut) < sizeof(reqParameter4_str))
  {
    strcpy(reqParameter4_str, ptr_debut);
  }
}

/* Fonction permettant de rechercher une requete */
int search_request(EthernetClient client, char * buffer, int buffer_size)
{
  /* Variables locales */
  int line_size = 0;
  char c;

  do
  {
    buffer[line_size++] = client.read();
    
  } while ((client.available()) && (c != '\n') && (line_size <= buffer_size));
  
  return line_size;
}

/* Fonction envoi de l'application au client web */
void send_data
(
  EthernetClient client,    /* Référence sur le client */
  char *fichier             /* Fichier à transmettre au client */
)
{
  /* Variables locales */
  File fichier_source = SD.open(fichier, FILE_READ);
  int size_buffer;
  uint8_t buffer[web_buffer];

  if (fichier_source)
  {
    while (fichier_source.available())
    {
      size_buffer = 0;
      int16_t c;

      while (fichier_source.available() && size_buffer < web_buffer - 1)
      {
        c = fichier_source.read();
        buffer[size_buffer] = (char)c;
        size_buffer++;
      }
      buffer[size_buffer] = '\0';
      client.write(buffer, size_buffer);
    }

    fichier_source.close();
  }
}

/* Fonction qui permet d'envoyer une entete HTTP classique */
void send_header
(
  EthernetClient client     /* Référence sur le client */
)
{
  client.println(F("HTTP/1.1 200 OK"));
  send_access_control(client);
  client.println(F("Content-Type: text/html"));
  client.println();
}

/* Fonction qui envoi une entete du type : erreur 404 */
void send_header_404
(
  EthernetClient client     /* Référence sur le client */
)
{
  client.println(F("HTTP/1.1 404 Not Found"));
  send_access_control(client);
  client.println(F("Content-Type: text/html"));
  client.println();
  client.println(F("404 Error"));
}

/* Foncion qui envoi une entete speciale en fonction du parametre transmis */
void send_special_header
(
  EthernetClient client,          /* Référence sur le client */
  const char * content_type       /* Type de contenu */
)
{
  client.println(F("HTTP/1.1 200 OK"));
  send_access_control(client);
  client.print(F("Content-Type: "));
  client.println(content_type);
  client.println();
}

/* Permet de transmettre les Access-Control au client web */
void send_access_control
(
  EthernetClient client     /* Référence sur le client */
)
{
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println(F("Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS"));
  client.println(F("Access-Control-Allow-Headers: X-Requested-With, Content-Type"));
}

/* Fonction permettant de serialiser les valeurs des entrées analogiques
 * au format JSON et de les envoyer au client web
 */
void readAndSerializeAnalog
(
  EthernetClient client     /* Référence sur le client */
)
{
  /* Variable locale */
  int index = 0;
  char bufferAnalog[32];

  /* Debut du tableau JSON */
  client.write("[", 1);

  for (index = 0; index < 4; index++)
  {
    sprintf ( bufferAnalog,
              "{\"id\":%i,\"value\":%i}",
              index,
              analogRead(index)
            );
    client.write(bufferAnalog, strlen(bufferAnalog));
    
    if (index < 3)
    {
      /* Ajout d'une virgule pour le prochain element du tableau JSON */
      client.write(",", 1);
    }
    else
    {
      /* Dernier element du tableau JSON */
      client.write("]", 1);
    }
  }

}

/* Fonction permettant de lire un entier dans un fichier texte stocké en carte SD */
int readIntFromSdCard
(
  char * fichier
)
{
  /* Variables locales */
  int index = 0;
  char val[4];
  
  File fileFromSd = SD.open(fichier, FILE_READ);
  
  if (fileFromSd)
  {
    while (fileFromSd.available() && index < 4)
    {
      val[index] = fileFromSd.read();
      index++;
    }
    val[index] = '\0';
    
    fileFromSd.close();

    return atoi(val);
  }
  
  return -1;
}

/* Permet de lire l'état des entrées digitales et de les stocker dans
 * un tableau en variable partagée
 */
void readPwm
(
  /* Ne prend pas de paramètres */
)
{
  /* Variable locale */
  char fileName[10];
  int index = 0;
  int i;

  for (i = 2; i <= 9; i++)
  {
    sprintf(fileName, "D%i.txt", index);
    valPwm[index] = readIntFromSdCard(fileName);
    index++;
  }
}

/* Permet de sérialiser les valeurs de sorties digitales au format JSON
 * et de transmettre au client web
 */
void serializePwm
(
  EthernetClient client     /* Référence sur le client */
)
{
  /* Variables locales */
  int index;
  char bufferPwm[64];

  /* Debut du tableau JSON */
  client.write("[", 1);

  for (index = 2; index <= 9; index++)
  {
    /* Sorties de type PWM */
    if (index == 3 || index == 5 || index == 6 || index == 9)
    {
      strcpy_P(buffer_progmem, (char*)pgm_read_word(&(    _REG_PARSE_JSON_PWM   )));
    }
    /* Sorties de type tout ou rien */
    else if (index == 2 || index == 7 || index == 8)
    {
      strcpy_P(buffer_progmem, (char*)pgm_read_word(&(    _REG_PARSE_JSON_TOR   )));
    }

    if (index == 3 || index == 5 || index == 6 || index == 9 || index == 2 || index == 7 || index == 8)
    {
      sprintf ( bufferPwm,
                  buffer_progmem,
                  index,
                  valPwm[index]
                );
      
      client.write(bufferPwm, strlen(bufferPwm));
      
      if (index < 9)
      {
        /* Ajout d'une virgule pour le prochain element du tableau JSON */
        client.write(",", 1);
      }
      else
      {
        /* Dernier element du tableau JSON */
        client.write("]", 1);
      }
    }
  }
}

/* Mise à jour du contenu d'un fichier texte stocké en carte SD */
void updateFile
(
  char * fileRepository,
  char * valueToWrite
)
{
  if (SD.exists(fileRepository))
  {
    SD.remove(fileRepository);
  }

  File fileFromSd = SD.open(fileRepository, FILE_WRITE);
  
  if (fileFromSd)
  {
    fileFromSd.print(valueToWrite);
    fileFromSd.close();
  }
}

/* Lecture d'une chaine de charactères stocké dans un fichier en carte SD */
int readStrFromSdCard
(
  char * fichier,
  char * data
)
{
  /* Variables locales */
  int index = 0;
  
  File fileFromSd = SD.open(fichier, FILE_READ);
  
  if (fileFromSd)
  {
    while (fileFromSd.available() && index < 16)
    {
      data[index] = fileFromSd.read();
      index++;
    }
    data[index] = '\0';
    
    fileFromSd.close();
  }
}

/* Permet de serialiser les informations d'un périphérique i2c au format json */
void serializeDeviceInfos
(
  EthernetClient client,     /* Référence sur le client */
  char * key,
  char * value
)
{
  /* Variables locales */
  char bufferDevice[42];

  strcpy_P(buffer_progmem, (char*)pgm_read_word(&(    _REG_SERIALIZE_I2C_CHAR   )));
  
  sprintf ( bufferDevice,
            buffer_progmem,
            key,
            value
          );
   client.write(bufferDevice, strlen(bufferDevice));
}

/* Permet de serialiser les informations d'un périphérique i2c au format json */
void serializeDeviceInfos
(
  EthernetClient client,     /* Référence sur le client */
  char * key,
  int value
)
{
   /* Variables locales */
  char bufferDevice[16];

  strcpy_P(buffer_progmem, (char*)pgm_read_word(&(    _REG_SERIALIZE_I2C_INT    )));
  
  sprintf ( bufferDevice,
            buffer_progmem,
            key,
            value
          );
   client.write(bufferDevice, strlen(bufferDevice));
}

/* Fonction permettant de restaurer l'état des sorties digitales */
void restoreDigitalState
(
  /* Ne prend pas de parametres */
)
{
  int pin;
  
  readPwm();

  for (pin = 2; pin <= 9; pin++)
  {
    if (pin == 3 || pin == 5 || pin == 6 || pin == 9)
    {
      analogWrite(pin, valPwm[pin]);
    }
    else if (pin == 2 || pin == 7 || pin == 8)
    {
      if (valPwm[pin] > 0)    digitalWrite(pin, HIGH);
      else                    digitalWrite(pin, LOW);
    }
  }
}

