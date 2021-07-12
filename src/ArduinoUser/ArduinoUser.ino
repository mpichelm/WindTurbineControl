/*
 Name:		ProyectoArduino.ino
 Created:	26/11/2017 12:03:07
 Author:	PC
*/

// CODIGO PARA EL ARDUINO USUARIO (el otro ser� el ARDUINO CONTROL)

// NOTA1: Este arduino enviar� al arduino de control del aerogenerador la petici�n de frenado y
//        ser� el arduino de control el que se encarge de llevar el control de las peticiones de frenado
//		  por viento, RPM o petici�n usuario, guardando los tiempos necesarios entre frenados (si se recive petici�n de 
//	      frenado por viento, no desactivar el freno durante 1 min, por ejemplo)
// NOTA2: Para conectar la pantalla LCD, SDA al analogico 5 de arduino. SCL al analogico 4 de arduino. Resetear arduino despues de conectar
// NOTA3: Formato display:
//	      Current status:
//        Temp: xx.x�C HR: xx% 
//	      Wind:xxm/s Max:xxm/s 
//        RPM: xxx   Max: xxx 
// NOTA4: Formato de mensajes
// ID de mensajes enviados y dato asociado (Arduino CONTROL)
// ________ID_______|_______Variable_______|________tipo_________
//		   100		|       Temperatura    |       double
//		   101		|       Humedad        |       double
//		   102		|		Vel. viento	   | 	   double									  
//		   103		|		RPM			   |       double
//		   104		|		Estado freno   |       int (0 = desactivado, 1 = activado, 2 = en movimiento)
//		   105		|% deflexion palas auto|       int 


// ID de mensajes enviados y dato asociado (Arduino USUARIO / Android)
// ________ID_______|_______Variable_______|________tipo_________
//		   200		|       Max RPM	       |       double
//		   201		|       Max viento     |       double
//		   202		|		Interr freno   | 	   int (0 = desactivado, 1 = activado). 						  
//		   203		|   Modo cambio paso   |       int (0 = manual, 1 = autom�tico).
//		   204		|   % deflexion palas  |       double
//		   205		|   id maestro actual  |       int (0 = arduino usuario, 1 = andorid)



#include <Metro.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

#include <stdlib.h>

#define LONGITUD_BUFFER 200

// Definicion de pines
const int pinPotMaxRPM = 0; // Entrada anal�gica 0 para el potenciometro que controla las RPM a las que se activa el freno.
const int pinPotMaxViento = 1; // Entrada anal�gica 1 para el potenciometro que controla las velocidad de viento m�xima a las que se activa el freno.
const int pinInterruptorFreno = 24; // Entrada digital 2 para el interruptor de freno.
const int pinLedFrenadoGreen = 3; // Salida digital 4 para el LED que indica si el freno est� activado.
const int pinLedFrenadoBlue = 4; // Salida digital 5 para el LED que indica si el freno est� activado.
const int pinLedFrenadoRed = 2; // Salida digital 6 para el LED que indica si el freno est� activado.
const int pinInterruptorModoPasoVariable = 26; // Pin del interruptor que permite seleccionar el modo de paso variable (manual o auto)
const int pinModoHC12 = 22; // Pin para seleccionar el modo del HC12. (LOW = comandos AT, HIGH = modo transparente (usamos este))
const int pinSlider = 2; // Entrada anal�gica 3 para el potenciometro lieal para el cambio de paso

//Definicion de constantes
const float maxRPM = 300; // revoluciones m�ximas configurables con el potenci�metro.
const float maxViento = 30; // velocidad m�xima de viento [m/s] configurable a la que salta el freno.

// Datos recibidos aerogenerador
float temperaturaActualCelsius = 0; // temperatura ambiente en el aerogenerador
float humedadActual = 0; // humedad ambiente en el aerogenerador
float velocidadVientoMetroSegundo = 0; // velocidad de viento en el aerogenerador
float rpmAerogenerador = 0; // rpm de rotaci�n de las palas 
int estadoFreno = 0; // estado actual del freno (0 = desactivado, 1 = activado, 2 = en desplazamiento)

// Variables de estado
float maxRPMActual; // revoluciones m�ximas permitidas actualmente antes de frenado.
float maxVientoActual; // velocidad de viento [m/s] m�xima permitida actualmente antes de frenado.
int flagInterruptorFrenoActivado; // "booleano" que indica si el interrputor de freno est� activado. (0 = freno desactivado, 1 = freno activado, -1 = estado no definido) (OJO: el interruptor puede estar desactivado pero el freno activado, porque hay otras causas que lo activan)
int modoPasoVariable = 1; // 0 = cambio de paso manual, 1 = cambio de paso automatico
int porcentajeDeflexionPalas = 0; // tanto por ciento de deflexi�n de las palas entre dos angulos de paso definidos
int porcentajeDeflexionPalasRecibidoArduinoControl = 0; // tanto por ciento de deflexi�n de las palas entre dos angulos de paso definidos que hay actualmente en el arduino de control

// Declaramos la pantalla LCD
LiquidCrystal_I2C lcd(0x27, 20, 4); // si no funciona probar 20x4
Metro pantallaLCDTimer = Metro(500); // Haremos el refresco de pantalla cada ciertos milisegundos

// Frecuencia para el envio de mensajes con el HC12
Metro enviarDatosHC12Timer = Metro(500); // Haremos el envio de datos cada ciertos milisegundos

// Timer de comunicacion con el m�dulo wifi ESP8266
Metro comunicacionESP8266Timer = Metro(500); // Haremos el envio de datos cada ciertos milisegundos

// Variables auxiliares
char textoAuxiliar5[6]; // se necesita el +1 para el caracter '\0'
char textoAuxiliar4[5]; // se necesita el +1 para el caracter '\0'
char textoAuxiliar3[4]; // se necesita el +1 para el caracter '\0'
char textoAuxiliar2[3]; // se necesita el +1 para el caracter '\0'
char textoAuxiliar1[2]; // se necesita el +1 para el caracter '\0'

char bufferEntrada[LONGITUD_BUFFER + 1]; // buffer para los datos recibidos 
char mensajeRecibido[LONGITUD_BUFFER + 1]; // mensaje recibido copiado del buffer
bool flagDatosRecibidos;
boolean flagRecibiendoMensaje = false; // flag que se activa cuando se detecta un inicio de mensaje y se pone a false cuando se detecta un final de mensaje. Permanece activo mientras se envia el mensaje
byte indiceActualEnBuffer = 0; // �ndice actual en el que estamos escribiendo del buffer

char bufferEntradaSerial2[LONGITUD_BUFFER + 1]; // buffer para los datos recibidos 
char mensajeRecibidoSerial2[LONGITUD_BUFFER + 1]; // mensaje recibido copiado del buffer
bool flagDatosRecibidosSerial2;
boolean flagRecibiendoMensajeSerial2 = false; // flag que se activa cuando se detecta un inicio de mensaje y se pone a false cuando se detecta un final de mensaje. Permanece activo mientras se envia el mensaje
byte indiceActualEnBufferSerial2 = 0; // �ndice actual en el que estamos escribiendo del buffer

// Variables auxiliares para actualizar solo los datos que introduce el usuario si realmente 
// se detecta un cambio en las lecturas de los potenciometros/interruptores. Esto se necesita
// para cubrir el caso de que se haya hecho un cambio de maestro y los valores de rpmMax o 
// viento max est�n en valores distintos a los que se est�n leyendo en los potenci�metros, pero
// mientras no se toquen, no se va a modificar el valor que haya impuesto el dispositivo anterior
int flagLecturaInterruptorFrenoPasoAnterior = 0;
int lecturaMaxRPMPasoAnterior = -1;
int lecturaMaxVientoPasoAnterior = -1;
int lecturaPorcentajeDeflexionPalasPasoAnterior = -1;
int lecturaModoPasoVariablePasoAnterior = -1;

// Variables para gestionar el maestro
int idMaestroActual = 0; // 0 = Arduino usuario, 1 = Android
unsigned long timeoutAndoridMilisegundos = 30000; // Si pasan estos milisegundos sin recibirse ning�n mensaje wifi, el maestro pasa a ser el arduino usuario
unsigned long instanteUltimoMensajeWifiMilisegundos = (-timeoutAndoridMilisegundos); // a pesar de ser unsigned, esto se puede hacer y lo que hace es que se lleva el valor al punto (-x) antes del overflow

// #########################################################################################################

// the setup function runs once when you press reset or power the board
void setup() {

	// Iniciar lcd
	lcd.begin();
	lcd.backlight(); // iniciar retroiluminacion
	lcd.clear(); // limpiamos la pantalla

	// Texto base del lcd
	lcd.setCursor(0, 0);
	lcd.print("Temp: xx.x C HR: xx%"); // Linea 1
	lcd.setCursor(0, 1);
	lcd.print("Wind:xxm/s Max:xxm/s"); // Linea 2
	lcd.setCursor(0, 2);
	lcd.print("RPM: xxx   Max: xxx"); // Linea 3
	lcd.setCursor(0, 3);
	lcd.print("Paso          xxx% "); // Linea 3

	// Seteamos los pines como entrada o salida
	pinMode(pinInterruptorFreno, INPUT); // lectura del interruptor de frenado
	pinMode(pinLedFrenadoBlue, OUTPUT); // escritura del led de frenado
	pinMode(pinLedFrenadoGreen, OUTPUT); // escritura del led de frenado
	pinMode(pinLedFrenadoRed, OUTPUT); // escritura del led de frenado
	pinMode(pinModoHC12, OUTPUT); // control del modo del HC12
	pinMode(pinInterruptorModoPasoVariable, INPUT); // lectura del interruptor de seleccion de modo de paso variable

	// Iniciamos los valores de los pines
	digitalWrite(pinModoHC12, HIGH);

	// Iniciamos puerto serie
	delay(80); // retardo inicial para HC12
	Serial.begin(9600); // Iniciamos puerto serie del PC a cierto baud rate
	Serial2.begin(9600); // Iniciamos un segundo puerto serie para comunicacion con el m�dulo wifi ESP8266
	Serial1.begin(9600); // Iniciamos puerto serie del HC12 a cierto baud rate

	Serial.println("Fin setup...");

}

// the loop function runs over and over again until power down or reset
void loop() {

	//temperaturaActualCelsius = 25;
	//humedadActual = 75;
	//velocidadVientoMetroSegundo = 12;
	//rpmAerogenerador = 100;
	//estadoFreno = 0;
	//porcentajeDeflexionPalasRecibidoArduinoControl = 65;

	// Gestion de maestro
	gestionCambioMaestro();

	// Efectuamos la lectura del cuadro de mandos
	leerEntradasUsuario();

	// Recibimos datos del arduino CONTROL
	leerDatosHC12();

	// Recibimos datos del Andorid
	leerDatosSerial2();

	// Escribimos por pantalla 
	escribirPantalla();

	// Enviamos datos
	enviarDatosHC12();

	// Enviar datos ESP8266;
	enviarDatosESP8266();

	// Activamos led de freno en caso de que corresponda
	gestionarLedFreno();

}

void leerEntradasUsuario() {
	// La lectura de entradas de ususario se efectuar� en cada interaci�n del loop principal,
	// pero solo se actualizan las variable si se detecta un cambio en el valor, para cubrir el caso
	// de que los valores est�n modificados porque los ha modificado otro dispositivo
	
	// Si el arduino usuario no es el maestro y est� en fase de emitir, no se leen datos
	if (idMaestroActual == 0) {
		// Lectura del interruptor de frenado
		if (flagLecturaInterruptorFrenoPasoAnterior != digitalRead(pinInterruptorFreno)) {
			flagInterruptorFrenoActivado = digitalRead(pinInterruptorFreno);
			flagLecturaInterruptorFrenoPasoAnterior = flagInterruptorFrenoActivado;
		}

		// Lectura del potenci�metro de RPM m�ximas.
		int lecturaPotenciometro = analogRead(pinPotMaxRPM);
		lecturaPotenciometro = map(lecturaPotenciometro, 0, 1023, 0, maxRPM);
		if (lecturaMaxRPMPasoAnterior != lecturaPotenciometro) {
			maxRPMActual = lecturaPotenciometro;
			lecturaMaxRPMPasoAnterior = lecturaPotenciometro;
		}

		// Lectura del potenci�metro de velocidad de viento m�xima
		lecturaPotenciometro = analogRead(pinPotMaxViento);
		lecturaPotenciometro = map(lecturaPotenciometro, 0, 1023, 0, maxViento);
		if (lecturaMaxVientoPasoAnterior != lecturaPotenciometro) {
			maxVientoActual = lecturaPotenciometro;
			lecturaMaxVientoPasoAnterior = lecturaPotenciometro;
		}

		// Lectura del potenci�metro de cambio de paso
		lecturaPotenciometro = analogRead(pinSlider);
		lecturaPotenciometro = map(lecturaPotenciometro, 0, 1023, 100, 0);
		if (lecturaPorcentajeDeflexionPalasPasoAnterior != lecturaPotenciometro) {
			porcentajeDeflexionPalas = lecturaPotenciometro;
			lecturaPorcentajeDeflexionPalasPasoAnterior = lecturaPotenciometro;
		}

		// Lectura del interruptor de modo de paso variable
		if (lecturaModoPasoVariablePasoAnterior != digitalRead(pinInterruptorModoPasoVariable)) {
			modoPasoVariable = digitalRead(pinInterruptorModoPasoVariable);
			lecturaModoPasoVariablePasoAnterior = modoPasoVariable;
		}
	}
}

void escribirPantalla() {
	// actualizamos la pantalla solo si ha pasado cierto tiempo
	if (pantallaLCDTimer.check()) {

		// TODO: en modo paso automatico, en la pantalla mostrar el porcentaje actual, no el del potenciometro

		// Actualizamos la temperatura
		dtostrf(temperaturaActualCelsius, 5, 1, textoAuxiliar5); // pasamos la temperatura a char[]. Queremos que el string tenga 5 elementos incluyendo signo y coma decimal, queremos 1 valor decimal y queremos que lo guarde en la variable textoAuxiliar5.
		lcd.setCursor(5, 0);
		lcd.print(textoAuxiliar5[0]); // signo
		lcd.print(textoAuxiliar5[1]); // decenas
		lcd.print(textoAuxiliar5[2]); // unidades
		lcd.print(textoAuxiliar5[3]); // punto decimal
		lcd.print(textoAuxiliar5[4]); // decimas

		// Actualizamos humedad
		dtostrf(humedadActual, 3, 0, textoAuxiliar5); // pasamos la humedad a char[]. Queremos que el string tenga 3 elementos incluyendo signo y coma decimal, queremos 0 valores decimales y queremos que lo guarde en la variable textoAuxiliar5.
		lcd.setCursor(16, 0);
		lcd.print(textoAuxiliar5[0]); // centenas
		lcd.print(textoAuxiliar5[1]); // decenas
		lcd.print(textoAuxiliar5[2]); // unidades

		// Actualizamos velocidad de viento
		dtostrf(velocidadVientoMetroSegundo, 2, 0, textoAuxiliar5); // pasamos la velocidad de viento a char[]. Queremos que el string tenga 2 elementos incluyendo signo y coma decimal, queremos 0 valor decimal y queremos que lo guarde en la variable textoAuxiliar5.
		lcd.setCursor(5, 1);
		lcd.print(textoAuxiliar5[0]); // decenas
		lcd.print(textoAuxiliar5[1]); // unidades

		// Actualizamos velocidad de viento m�xima
		dtostrf(maxVientoActual, 2, 0, textoAuxiliar5); // pasamos la velocidad de viento a char[]. Queremos que el string tenga 2 elementos incluyendo signo y coma decimal, queremos 0 valor decimal y queremos que lo guarde en la variable textoAuxiliar5.
		lcd.setCursor(15, 1);
		lcd.print(textoAuxiliar5[0]); // decenas
		lcd.print(textoAuxiliar5[1]); // unidades

		// Actualizamos la velocidad de giro
		dtostrf(rpmAerogenerador, 3, 0, textoAuxiliar5); // pasamos la velocidad de rotacion a char[]. Queremos que el string tenga 3 elementos incluyendo signo y coma decimal, queremos 0 valor decimal y queremos que lo guarde en la variable textoAuxiliar5.
		lcd.setCursor(5, 2);
		lcd.print(textoAuxiliar5[0]); // centenas
		lcd.print(textoAuxiliar5[1]); // decenas
		lcd.print(textoAuxiliar5[2]); // unidades

		// Actualizamos la velocidad de giro maxima
		dtostrf(maxRPMActual, 3, 0, textoAuxiliar5); // pasamos la velocidad de rotacion a char[]. Queremos que el string tenga 3 elementos incluyendo signo y coma decimal, queremos 0 valor decimal y queremos que lo guarde en la variable textoAuxiliar5.
		lcd.setCursor(16, 2);
		lcd.print(textoAuxiliar5[0]); // centenas
		lcd.print(textoAuxiliar5[1]); // decenas
		lcd.print(textoAuxiliar5[2]); // unidades

		// Actualizamos modo de paso variable
		if (modoPasoVariable == 0) {
			lcd.setCursor(5, 3);
			lcd.print("manual"); 	

			dtostrf(porcentajeDeflexionPalas, 3, 0, textoAuxiliar5); // pasamos el % de deflexion de las palas a char[]. Queremos que el string tenga 3 elementos incluyendo signo y coma decimal, queremos 0 valor decimal y queremos que lo guarde en la variable textoAuxiliar5.
			lcd.setCursor(14, 3);
			lcd.print(textoAuxiliar5[0]); // centenas
			lcd.print(textoAuxiliar5[1]); // decenas
			lcd.print(textoAuxiliar5[2]); // unidades

		}
		else {
			lcd.setCursor(5, 3);
			lcd.print("auto. ");

			dtostrf(porcentajeDeflexionPalasRecibidoArduinoControl, 3, 0, textoAuxiliar5); // pasamos el % de deflexion de las palas a char[]. Queremos que el string tenga 3 elementos incluyendo signo y coma decimal, queremos 0 valor decimal y queremos que lo guarde en la variable textoAuxiliar5.
			lcd.setCursor(14, 3);
			lcd.print(textoAuxiliar5[0]); // centenas
			lcd.print(textoAuxiliar5[1]); // decenas
			lcd.print(textoAuxiliar5[2]); // unidades
		}

	}
}

// M�todo que env�a al Arduino de CONTROL los datos del usuario mediante radiofrecuencia
void enviarDatosHC12() {

	// Solo enviaremos datos cada cierto tiempo 
	if (enviarDatosHC12Timer.check()){

			// Velocidad de giro m�xima (ID mensaje: 200)
			dtostrf(maxRPMActual, 3, 0, textoAuxiliar3); // llamamos a la funci�n dtostrf con el valor LONGITUD_TEXTO_AUX para que rellene todos los elementos del char[] y no se quede basura de operaciones anteriores
			enviarMensajeHC12(200, textoAuxiliar3);

			// Velocidad de viento m�xima (ID mensaje: 201)
			dtostrf(maxVientoActual, 2, 0, textoAuxiliar2);
			enviarMensajeHC12(201, textoAuxiliar2);

			// Estado interruptor freno (ID mensaje: 202)
			dtostrf(flagInterruptorFrenoActivado, 1, 0, textoAuxiliar1);
			enviarMensajeHC12(202, textoAuxiliar1);

			// Modo cambio paso (ID mensaje: 203)
			dtostrf(modoPasoVariable, 1, 0, textoAuxiliar1);
			enviarMensajeHC12(203, textoAuxiliar1);

			// Deflexion palas % (ID mensaje: 204)
			dtostrf(porcentajeDeflexionPalas, 3, 0, textoAuxiliar3);
			enviarMensajeHC12(204, textoAuxiliar3);
	}
}

// M�todo que env�a al m�dulo ESP8266 los datos recibidos desde el arduino de control. El arduino usuario actua como puente
void enviarDatosESP8266() {
	// Solo enviaremos datos cada cierto tiempo 
	if (comunicacionESP8266Timer.check()) {

		//Serial.print("id maestro actual: ");
		//Serial.println(idMaestroActual);
		//Serial.print("Estado interruptor freno: ");
		//Serial.println(flagInterruptorFrenoActivado);

		// Temperatura actual (ID mensaje: 100)
		dtostrf(temperaturaActualCelsius, 5, 1, textoAuxiliar5); // llamamos a la funci�n dtostrf con el valor LONGITUD_TEXTO_AUX para que rellene todos los elementos del char[] y no se quede basura de operaciones anteriores
		enviarMensajeESP8266(100, textoAuxiliar5);

		// Humedad actual (ID mensaje: 101)
		dtostrf(humedadActual, 3, 0, textoAuxiliar3);
		enviarMensajeESP8266(101, textoAuxiliar3);

		// Velocidad de viento actual (ID mensaje: 102)
		dtostrf(velocidadVientoMetroSegundo, 2, 0, textoAuxiliar2);
		enviarMensajeESP8266(102, textoAuxiliar2);

		// Velocidad de giro actual (ID mensaje: 103)
		dtostrf(rpmAerogenerador, 3, 0, textoAuxiliar3);
		enviarMensajeESP8266(103, textoAuxiliar3);

		// Estado freno (ID mensaje: 104)
		dtostrf(estadoFreno, 1, 0, textoAuxiliar1); 
		enviarMensajeESP8266(104, textoAuxiliar1);
		
		// Porcentaje deflexion palas (ID mensaje: 105)
		dtostrf(porcentajeDeflexionPalasRecibidoArduinoControl, 3, 0, textoAuxiliar3);
		enviarMensajeESP8266(105, textoAuxiliar3);
	}

}

// M�todo que gestiona el color que debe tener el led de frenado
// en cada momento
void gestionarLedFreno() {

	// Chequeamos si debemos activar el led de freno
	if (estadoFreno == 1) {
		ColorLedFreno(255, 0, 0);
	}
	else if(estadoFreno == 0) {
		ColorLedFreno(0, 255, 0);
	}
	else if (estadoFreno == 2) {
		//ColorLedFreno(255, 127, 39);
		ColorLedFreno(0, 0, 255);

	}
}

// Funci�n establece un color para el led de frenado
void ColorLedFreno(int R, int G, int B)
{
	// Ponemos 225-valor porque usamos diodos de anodo comun
	analogWrite(pinLedFrenadoRed, 255-R);   // Red    - Rojo
	analogWrite(pinLedFrenadoGreen, 255-G);   // Green - Verde
	analogWrite(pinLedFrenadoBlue, 255-B);   // Blue - Azul
}


// M�todo que se encarga de leer un mensaje recibido a trav�s del HC12
void leerDatosHC12() {

		char startMarker = '['; // marca de comienzo de un mensaje
		char endMarker = ']'; // marca de fin de un mensaje
		char charRecibido; // Char recibido 
		
		while (Serial1.available() > 0) {
			// Leemos un char			
			charRecibido = Serial1.read();
			//Serial.print(charRecibido);

			// Si estamos en medio de la recepci�n de un mensaje
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
				// el falg que indica que estamos en medio de la recepci�n de un mensaje
				// y activamos el de fin de mensaje, que detiene el while
				else {
					bufferEntrada[indiceActualEnBuffer] = '\0'; // terminate the string
					flagRecibiendoMensaje = false;
					indiceActualEnBuffer = 0;
					flagDatosRecibidos = true;

					// copiamos el mensaje del buffer a la variable mensaje
					strcpy(mensajeRecibido, bufferEntrada); 

					Serial.print("Mensaje recibido: ");
					Serial.println(mensajeRecibido);

					// parseamos el mensaje recibido para obtener "id" y "valor"
					int idMensaje = -1;
					double valorMensaje = -1;
					if (parsearMensaje(&idMensaje, &valorMensaje, mensajeRecibido)) { // Llamamos a la funci�n que parsea el mensaje, y si devuelve true, es que el mensaje no ten�a errores y podemos proceder a guardarlo en la variable que le corresponde
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

// M�todo que se encarga de leer un mensaje recibido a trav�s del puerto Serial2 procedente del ESP8266
void leerDatosSerial2() {

	char startMarker = '['; // marca de comienzo de un mensaje
	char endMarker = ']'; // marca de fin de un mensaje
	char charRecibido; // Char recibido 

	while (Serial2.available() > 0) {
		// Leemos un char			
		charRecibido = Serial2.read();
		Serial.print(charRecibido);

		// Si estamos en medio de la recepci�n de un mensaje
		if (flagRecibiendoMensajeSerial2 == true) {
			// Si no estamos en el final de mensaje, almacenamos el char
			// recibido en el buffer, cubriendo el caso de un posible desbordamiento
			if (charRecibido != endMarker) {
				bufferEntradaSerial2[indiceActualEnBufferSerial2] = charRecibido;
				indiceActualEnBufferSerial2++;
				if (indiceActualEnBufferSerial2 >= LONGITUD_BUFFER) {
					indiceActualEnBufferSerial2 = LONGITUD_BUFFER - 1;
				}
			}
			// Si llegamos al final del mensaje, terminamos el string, desactivamos
			// el falg que indica que estamos en medio de la recepci�n de un mensaje
			// y activamos el de fin de mensaje, que detiene el while
			else {
				bufferEntradaSerial2[indiceActualEnBufferSerial2] = '\0'; // terminate the string
				flagRecibiendoMensajeSerial2 = false;
				indiceActualEnBufferSerial2 = 0;
				flagDatosRecibidosSerial2 = true;

				// copiamos el mensaje del buffer a la variable mensaje
				strcpy(mensajeRecibidoSerial2, bufferEntradaSerial2);

				//Serial.println(mensajeRecibidoSerial2);

				// Solo nos interesan los mensajes �que recibimos del m�dulo wifi si el maestro actual es el Andorid
				if (idMaestroActual == 1) {
					// parseamos el mensaje recibido para obtener "id" y "valor"
					int idMensaje = -1;
					double valorMensaje = -1;

					if (parsearMensaje(&idMensaje, &valorMensaje, mensajeRecibidoSerial2)) { // Llamamos a la funci�n que parsea el mensaje, y si devuelve true, es que el mensaje no ten�a errores y podemos proceder a guardarlo en la variable que le corresponde
																	 // interpretamos el mensaje para guardar el valor en la variable que
																	 // corresponde
						interpretarMensaje(idMensaje, valorMensaje);
					}
				}

				// Actualizamos el instante de tiempo en el que hemos recibido el �ltimo mensaje de wifi.
				instanteUltimoMensajeWifiMilisegundos = millis();
			}
		}

		// Si recibimos un identificador de incio de mensaje, ponemos el flag
		// de que estamos recibiendo mensaje a true
		else if (charRecibido == startMarker) {
			flagRecibiendoMensajeSerial2 = true;
		}
	}
}

// M�todo que parsea un mensaje completo y extrae el id del mensaje para saber 
// a que dato corresponde, y el valor de dicho dato
bool parsearMensaje(int* idMensaje, double* valorMensaje, char* mensaje) {
	
	char * strtokIndx; // Variable necesaria para la funci�n strtok(), ya que lo utiliza como �ndice

	// Leemos el id del mensaje 
	strtokIndx = strtok(mensaje, ","); 
	*idMensaje = atoi(strtokIndx); // convertimos a int y guardamos dicho id 

	// Hacemos una comprobaci�n del id recibido, ya que se env�a por duplicado
	strtokIndx = strtok(NULL, ","); // ahora pasamos NULL a la funci�n para que siga donde lo dej� la �ltima vez que se llam�
	if (*idMensaje != atoi(strtokIndx)) // si el campo de confirmaci�n es distinto del primero, la lectura no es valida y devolvemos false
		return false;


	// Ahora leemos el valor de la variable
	strtokIndx = strtok(NULL, ","); // nuevamente, al pasar null, la funci�n strtok sigue donde lo dej� anteriormente
	*valorMensaje = atof(strtokIndx); // convertimos a double y guardamos el valor


	strtokIndx = strtok(NULL, ","); 
	if (*valorMensaje != atof(strtokIndx)) // si el campo de confirmaci�n es distinto del primero, la lectura no es valida y devolvemos false
		return false;

	// Si llegamos a este punto es que la lectura la recepci�n del dato ha sido satisfactoria
	return true;
}

// M�todo que, dado un id y un valor, interpreta el id y
// guarda el valor en la variable que corresponde
void interpretarMensaje(int idMensaje, double valorMensaje) {
	
	switch (idMensaje) {	
	case 100: // Temperatura
		temperaturaActualCelsius = valorMensaje;
		break;
	case 101: // % humedad relativa
		humedadActual = valorMensaje;
		break;
	case 102: // velocidad viento
		velocidadVientoMetroSegundo = valorMensaje;
		break;
	case 103: // revoluciones rotor
		rpmAerogenerador = valorMensaje;
		break;
	case 104: // estado freno
		estadoFreno = valorMensaje;
		break;
	case 105: // % deflexion palas recibido desde arduino control
		porcentajeDeflexionPalasRecibidoArduinoControl = valorMensaje;
		break;
	case 200: // Max RPM (ID mensaje: 200)
		maxRPMActual = valorMensaje;
		break;
	case 201: // Max viento (ID mensaje: 201)
		maxVientoActual = valorMensaje;
		break;
	case 202: // estado freno manual (ID mensaje: 202)
		flagInterruptorFrenoActivado = valorMensaje;
		break;
	case 203: // modo cambio paso (ID mensaje: 203)
		modoPasoVariable = valorMensaje;
		break;
	case 204: // % deflexion palas (ID mensaje: 204)
		porcentajeDeflexionPalas = valorMensaje;
		break;
	}
}

// M�todo que genera y envia un mensaje a partir con un "id" entero dado
// y con un valor dado en un char array a trav�s del HC12
void enviarMensajeHC12(int idMensaje, char* valor) {

	Serial1.print("[");
	Serial1.print(idMensaje);
	Serial1.print(",");
	Serial1.print(idMensaje);
	Serial1.print(",");
	Serial1.print(valor);
	Serial1.print(",");
	Serial1.print(valor);
	Serial1.println("]"); // aqu� ponemos println, no print
}

// M�todo que genera y envia un mensaje a partir con un "id" entero dado
// y con un valor dado en un char array a trav�s del ESP8266
void enviarMensajeESP8266(int idMensaje, char* valor) {

	Serial2.print("[");
	Serial2.print(idMensaje);
	Serial2.print(",");
	Serial2.print(idMensaje);
	Serial2.print(",");
	Serial2.print(valor);
	Serial2.print(",");
	Serial2.print(valor);
	Serial2.println("]"); // aqu� ponemos println, no print
}

// M�todo que gestiona en cada momento quien controla al aerogenerador
void gestionCambioMaestro() {

	// Si trancurre m�s de un cierto tiempo sin que se reciba un mensaje por wifi, se procede a devolver 
	// el estado de maestro al arduino usuario
	if (millis() - instanteUltimoMensajeWifiMilisegundos > timeoutAndoridMilisegundos) {
		idMaestroActual = 0; 
	}
	else {
		idMaestroActual = 1;			
	}
}

// note: the _in array should have increasing values
int multiMap(int val, int* _in, int* _out, uint8_t size)
{
	// take care the value is within range
	// val = constrain(val, _in[0], _in[size-1]);
	if (val <= _in[0]) return _out[0];
	if (val >= _in[size - 1]) return _out[size - 1];

	// search right interval
	uint8_t pos = 1;  // _in[0] allready tested
	while (val > _in[pos]) pos++;

	// this will handle all exact "points" in the _in array
	if (val == _in[pos]) return _out[pos];

	// interpolate in the right segment for the rest
	return (val - _in[pos - 1]) * (_out[pos] - _out[pos - 1]) / (_in[pos] - _in[pos - 1]) + _out[pos - 1];
}
