
#include <ESP8266WiFi.h>

#define SSID_PICHEL "MOVISTAR_A90E"
#define PASSWORD_PICHEL "e7dp6UHXQTuPR3Eye3Q3"
#define SSID_DANI "FTE-A458"
#define PASSWORD_DANI "XXsrd7mc"

// NOTA: MAC del modulo wifi utilizado: 84:f3:eb:ed:f9:6d

// Creamos unos punteros a la variable que guardará la red seleccionada.
// Se inicia una por defecto
const char* ssid = SSID_PICHEL;
const char* password = PASSWORD_PICHEL;

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

// Variables auxiliares comunicaciones
#define LONGITUD_BUFFER 200
#define LONGITUD_BUFFER_AUXILIAR 30
bool flagRecibiendoMensaje = false;
char bufferEntrada[LONGITUD_BUFFER + 1]; // buffer para los datos recibidos 
char mensajeRecibido[LONGITUD_BUFFER + 1]; // mensaje recibido copiado del buffer
byte indiceActualEnBuffer = 0; // índice actual en el que estamos escribiendo del buffer
char bufferAuxiliar[LONGITUD_BUFFER_AUXILIAR + 1]; // buffer para comprobacion de mensajes

// buffers auxiliares 
char textoAuxiliar5[6]; // se necesita el +1 para el caracter '\0'
char textoAuxiliar4[5]; // se necesita el +1 para el caracter '\0'
char textoAuxiliar3[4]; // se necesita el +1 para el caracter '\0'
char textoAuxiliar2[3]; // se necesita el +1 para el caracter '\0'
char textoAuxiliar1[2]; // se necesita el +1 para el caracter '\0'

// Datos a enviar al Arduino de control
//float maxRPMActual = 0; // revoluciones máximas permitidas actualmente antes de frenado.
//float maxVientoActual = 0; // velocidad de viento [m/s] máxima permitida actualmente antes de frenado.
//int flagPeticionActivarFrenoArduinoUsuario = 0; // "booleano" que indica si el interrputor de freno está activado. (0 = freno desactivado, 1 = freno activado, -1 = estado no definido) (OJO: el interruptor puede estar desactivado pero el freno activado, porque hay otras causas que lo activan)
//int modoPasoVariable = 0; // 0 = cambio de paso manual, 1 = cambio de paso automatico
//int porcentajeDeflexionPalas = 0; // tanto por ciento de deflexión de las palas entre dos angulos de paso definidos

// Datos recibidos desde el arduino usuario por puerto serie a traves del ESP8266
float temperaturaActualCelsius = 0;
float humedadActual = 0;
float velocidadVientoMetroSegundo = 0;
float rpmAerogenerador = 0;
int estadoFreno = 0;
int porcentajeDeflexionPalas = 0;

float ultimaMuestra = 0;

void setup() {
	Serial.begin(9600);
	delay(10);

	// Obtenemos las redes disponibles y vemos si disponemos de 
	// la contraseña de alguna de ellas
	bool redEncontrada = false;
	while (!redEncontrada) {
		int n = WiFi.scanNetworks();
		Serial.println("scan done");
		if (n == 0) {
			Serial.println("no networks found");
		}
		else {
			Serial.print(n);
			Serial.println(" networks found");
			for (int i = 0; i < n; ++i) {
				// Comprobamos si alguna de las redes es la que esperamos
				if (WiFi.SSID(i).equals(SSID_PICHEL)) {
					ssid = SSID_PICHEL;
					password = PASSWORD_PICHEL;
					redEncontrada = true;
				}
				else if (WiFi.SSID(i).equals(SSID_DANI)) {
					ssid = SSID_DANI;
					password = PASSWORD_DANI;
					redEncontrada = true;
				}
				delay(10);
			}
		}
		delay(2000);
	}

	// Connect to WiFi network
	Serial.println();
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(ssid);

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.println("WiFi connected");
	
	// Start the server
	server.begin();
	Serial.println("Server started");

	// Print the IP address
	Serial.println(WiFi.localIP());

	ultimaMuestra = millis();
}

void loop() {
	// Leemos los mensajes que llegan del puerto serie
	leerPuertoSerie();

	if (millis() - ultimaMuestra > 1000) {
		Serial.print("Free heap:");
		Serial.println(ESP.getFreeHeap(), DEC);
		ultimaMuestra = millis();
	}

	// Check if a client has connected
	WiFiClient client = server.available();
	if (!client) {
		return;
	}

	// Wait until the client sends some data
	//Serial.println("new client");

	// Definimos un timeout para los clientes. Chrome abre la conexión de una forma especial
	// y si no se pone este trozo de código, el ESP8266 deja de responder despues de un número
	// random de conexiones. Desde explorer o Andorid parece que no pasaba.
	// Ver más en https://github.com/esp8266/Arduino/issues/3735
	unsigned long timeout = millis();
	while (client.available() == 0) {
		if (millis() - timeout > 200) {
			//Serial.println("\r\n Client Timeout !");
			client.stop();
			return;
		}
	}

	// Read the first line of the request
	String req = client.readStringUntil('\r');
	//Serial.println(req);
	client.flush();

	// Match the request
	int idMensaje;
	double valorMensaje;
	bool flagMensajeValido = parsearMensajeRecibidoWifi(req, &idMensaje, &valorMensaje);
	if (flagMensajeValido) {
		// Si el mensaje es de datos medidos por el arduino de control, simplemente
		// reenviamos el mensaje por puerto serie al arduino usuario. (En caso de que 
		// sea un mensaje de peticion de un dato de usuario, ya se elabora el mensaje de respuesta
		// correspondiente más abajo)
		if (idMensaje >= 200) {
			int indiceInicial = req.indexOf("[");
			int indiceFinal = req.indexOf("]");
			Serial.println(req.substring(indiceInicial, indiceFinal+1));
		}
	}
	else {
		//Serial.println("invalid request");
		client.stop();
		return;
	}

	client.flush();

	// Prepare the response
	String s = elaborarRespuestaCliente(idMensaje);

	// Send the response to the client
	client.print(s);
	delay(1);
	//Serial.println("Client disonnected");

	// The client will actually be disconnected 
	// when the function returns and 'client' object is detroyed
}

// Lectura de mensajes que llegan a través del puerto seriel, en los que se envian al arduino
// usuario los datos que se están pidiendo desde la app
void leerPuertoSerie() {

	char startMarker = '['; // marca de comienzo de un mensaje
	char endMarker = ']'; // marca de fin de un mensaje
	char charRecibido; // Char recibido 

	while (Serial.available() > 0) {
		// Leemos un char     
		charRecibido = Serial.read();

		// Si estamos en medio de la recepción de un mensaje
		if (flagRecibiendoMensaje == true) {
			// Si no estamos en el final de mensaje, almacenamos el char
			// recibido en el buffer, cubriendo el caso de un posible desbordamiento
			if (charRecibido != endMarker) {
				bufferEntrada[indiceActualEnBuffer] = charRecibido;
				indiceActualEnBuffer++;
				if (indiceActualEnBuffer >= LONGITUD_BUFFER) {
					indiceActualEnBuffer = LONGITUD_BUFFER - 1;
				}
			}
			// Si llegamos al final del mensaje, terminamos el string, desactivamos
			// el falg que indica que estamos en medio de la recepción de un mensaje
			// y activamos el de fin de mensaje, que detiene el while
			else {
				bufferEntrada[indiceActualEnBuffer] = '\0'; // terminate the string
				flagRecibiendoMensaje = false;
				indiceActualEnBuffer = 0;

				// copiamos el mensaje del buffer a la variable mensaje
				strcpy(mensajeRecibido, bufferEntrada);

				// parseamos el mensaje recibido para obtener "id" y "valor"
				int idMensaje = -1;
				double valorMensaje = -1;
				if (parsearMensaje(&idMensaje, &valorMensaje)) { // Llamamos a la función que parsea el mensaje, y si devuelve true, es que el mensaje no tenía errores y podemos proceder a guardarlo en la variable que le corresponde
																 // interpretamos el mensaje para guardar el valor en la variable que
																 // corresponde         
					interpretarMensaje(idMensaje, valorMensaje);
				}
			}
		}

		// Si recibimos un identificador de incio de mensaje, ponemos el flag
		// de que estamos recibiendo mensaje a true
		else if (charRecibido == startMarker) {
			flagRecibiendoMensaje = true;
		}
	}
}

// Método que parsea un mensaje completo y extrae el id del mensaje para saber 
// a que dato corresponde, y el valor de dicho dato
bool parsearMensaje(int* idMensaje, double* valorMensaje) {

	char * strtokIndx; // Variable necesaria para la función strtok(), ya que lo utiliza como índice

					   // Leemos el id del mensaje 
	strtokIndx = strtok(mensajeRecibido, ",");
	*idMensaje = atoi(strtokIndx); // convertimos a int y guardamos dicho id 

								   // Hacemos una comprobación del id recibido, ya que se envía por duplicado
	strtokIndx = strtok(NULL, ","); // ahora pasamos NULL a la función para que siga donde lo dejó la última vez que se llamó
	if (*idMensaje != atoi(strtokIndx)) // si el campo de confirmación es distinto del primero, la lectura no es valida y devolvemos false
		return false;

	// Ahora leemos el valor de la variable
	strtokIndx = strtok(NULL, ","); // nuevamente, al pasar null, la función strtok sigue donde lo dejó anteriormente
	*valorMensaje = atof(strtokIndx); // convertimos a double y guardamos el valor

	strtokIndx = strtok(NULL, ",");
	if (*valorMensaje != atof(strtokIndx)) // si el campo de confirmación es distinto del primero, la lectura no es valida y devolvemos false
		return false;

	// Si llegamos a este punto es que la lectura la recepción del dato ha sido satisfactoria
	return true;
}

// Método que, dado un id y un valor, interpreta el id y
// guarda el valor en la variable que corresponde
void interpretarMensaje(int idMensaje, double valorMensaje) {

	switch (idMensaje) {
	case 100: // Temperatura actual (ID mensaje: 100)
		temperaturaActualCelsius = valorMensaje;
		break;
	case 101: // Humedad actual (ID mensaje: 101)
		humedadActual = valorMensaje;
		break;
	case 102: // Velocidad de viento actual (ID mensaje: 102)
		velocidadVientoMetroSegundo = valorMensaje;
		break;
	case 103: // Velocidad de giro actual (ID mensaje: 103)
		rpmAerogenerador = valorMensaje;
		break;
	case 104: // % Estado freno (ID mensaje: 104)
		estadoFreno = valorMensaje;
		break;
	case 105: // % Porcentaje deflexion palas (ID mensaje: 105)
		porcentajeDeflexionPalas = valorMensaje;
		break;
	}
}

String elaborarRespuestaCliente(int idMensaje) {

	// inicio del mensaje de respuesta
	String respuesta = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n";

	// Si los mensajes corresponden a datos que está midiendo el arduino de control,
	// simplemente respondemos [ok]
	if (idMensaje >= 200) {
		respuesta += "[ok]";
	}
	else {
		// Si son mensajes de petición de variables, para cada peticion debemos elaborar 
		// un mensaje distinto
		switch (idMensaje) {
		case 100: // Temperatura actual (ID mensaje: 100)
			dtostrf(temperaturaActualCelsius, 5, 1, textoAuxiliar5); // llamamos a la función dtostrf con el valor LONGITUD_TEXTO_AUX para que rellene todos los elementos del char[] y no se quede basura de operaciones anteriores
			respuesta += generarMensaje(100, textoAuxiliar5);
			break;
		case 101: // Humedad actual (ID mensaje: 101)
			dtostrf(humedadActual, 3, 0, textoAuxiliar3);
			respuesta += generarMensaje(101, textoAuxiliar3);
			break;
		case 102: // Velocidad de viento actual (ID mensaje: 102)
			dtostrf(velocidadVientoMetroSegundo, 2, 0, textoAuxiliar2);
			respuesta += generarMensaje(102, textoAuxiliar2);
			break;
		case 103: // Velocidad de giro actual (ID mensaje: 103)
			dtostrf(rpmAerogenerador, 3, 0, textoAuxiliar3);
			respuesta += generarMensaje(103, textoAuxiliar3);
			break;
		case 104: // Estado freno (ID mensaje: 104)
			dtostrf(estadoFreno, 1, 0, textoAuxiliar1); 
			respuesta += generarMensaje(104, textoAuxiliar1);
			break;
		case 105: // Porcentaje deflexion palas (ID mensaje: 105)
			dtostrf(porcentajeDeflexionPalas, 3, 0, textoAuxiliar3);
			respuesta += generarMensaje(105, textoAuxiliar3);
			break;
		}
	}

	// final del mensaje de respuesta
	respuesta += "</html>\n";
	return respuesta;
}

// Función que monta un mensaje String a partir del id y el valor en cadena de caracteres a guardar
String generarMensaje(int idMensaje, char* valor) {
	String mensaje = "[";
	mensaje += idMensaje;
	mensaje += ",";
	mensaje += idMensaje;
	mensaje += ",";
	mensaje += valor;
	mensaje += ",";
	mensaje += valor;
	mensaje += "]";

	return mensaje;
}


// Función que lee el mensaje recibido por wifi y extrae el id y el valor, comprobando además
// si el mensaje es válido
bool parsearMensajeRecibidoWifi(String mensaje, int* idMensaje, double* valorMensaje) {

	mensaje.toCharArray(bufferAuxiliar, LONGITUD_BUFFER_AUXILIAR);
	bufferAuxiliar[LONGITUD_BUFFER_AUXILIAR] = 0;

	char * pch;
	pch = strtok(bufferAuxiliar, "["); // cortamos hasta el primer [
	pch = strtok(NULL, "]"); // nos quedamos con lo que haya desde el primer [ encontrado antes y el primer ] que encontremos ahora

	if (pch != NULL) {
		char * strtokIndx; // Variable necesaria para la función strtok(), ya que lo utiliza como índice

						   // Leemos el id del mensaje 
		strtokIndx = strtok(pch, ",");
		*idMensaje = atoi(strtokIndx); // convertimos a int y guardamos dicho id 

									   // Hacemos una comprobación del id recibido, ya que se envía por duplicado
		strtokIndx = strtok(NULL, ","); // ahora pasamos NULL a la función para que siga donde lo dejó la última vez que se llamó
		if (*idMensaje != atoi(strtokIndx)) // si el campo de confirmación es distinto del primero, la lectura no es valida y devolvemos false
			return false;

		// Ahora leemos el valor de la variable
		strtokIndx = strtok(NULL, ","); // nuevamente, al pasar null, la función strtok sigue donde lo dejó anteriormente
		*valorMensaje = atof(strtokIndx); // convertimos a double y guardamos el valor

		strtokIndx = strtok(NULL, ",");
		if (*valorMensaje != atof(strtokIndx)) // si el campo de confirmación es distinto del primero, la lectura no es valida y devolvemos false
			return false;
	}

	// Si llegamos a este punto es que la lectura la recepción del dato ha sido satisfactoria
	return true;
}



