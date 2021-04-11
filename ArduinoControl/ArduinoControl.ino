/*
 Name:		ProyectoArduinoCONTROL.ino
 Created:	02/12/2017 10:44:39
 Author:	PC
*/

// CODIGO PARA EL ARDUINO CONTROL (montado sobre el aerognerado)

// NOTA1: Este arduino enviar� al arduino usuario la informaci�n del estado
//		  actual de funcionamiento del mismo.
// NOTA2: Formato de mensajes
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

// NOTA2: Los reles tienen logica invertida. Es decir, si no damos se�al, est�n comuntados al "normally open",
// lo cual es una putada porque consumen m�s. Asi que nos adaptaremos para utilizar esta l�gica y que por defecto
// est�n apagados cuando ningun actuador est� actuando.

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ActuadorLineal.h>
#include <Metro.h>

#define LONGITUD_BUFFER 200
#define LONGITUD_ACTUADOR_UTILIZABLE_MILIMETROS 50.0f

// Datos entrada
const float longitudActuadorPalasMilimetros = 200;

// Definicion de pines (en el arduino mega, los pines que permite interrupts son: 2, 3, 18, 19, 20, 21)
const int pinModoHC12 = 44; // Pin para seleccionar el modo del HC12. (LOW = comandos AT, HIGH = modo transparente (usamos este))
const int pinSensorHallAnemometro = 2; // Pin digital de entrada del sensor hall del anem�metro
const int pinSensorHallRpmRotor = 21; // Pin digital de entrada del sensor hall del rotor
const int pinSensorTemperaturaHumedad = 40; // Pin digital de entrada del sensor de temperatura y humedad 
const int pinReleActivarFreno = 32; // pin digital para activar el freno
const int pinReleDesactivarFreno = 30; // pin digital para desactivar el freno
const int pinReleRetraerActuadorPalas = 31; //pin que controla el rel� que extiende el "servo" de cambio de paso de las palas
const int pinReleExtenderActuadorPalas = 33; //pin que controla el rel� que retrae el "servo" de cambio de paso de las palas
const int pinSensorHallServoPalas = 3; // pin que detecta cuando el engranaje del servo da una nueva "vuelta"

// Freno
const float tiempoDuracionExtensionActuador = 1000; // tiempo en milisegundos que va a durar el movimiento del actuador lineal para liberar freno
const float tiempoDuracionRetraccionActuador = tiempoDuracionExtensionActuador + 500; // el tiempo de retraccion lo pondremos igual al de extension m�s cierto tiempo para asegurar que llega al tope.
const float tiempoMinimoRotorPermaneceParado = 5000; // tiempo minimo que el actuador va a estar parado si se ha activado el freno

// Datos aerogenerador
float temperaturaActualCelsius = 0; // temperatura ambiente en el aerogenerador
float humedadActual = 0; // humedad ambiente en el aerogenerador
float velocidadVientoMetroSegundo = 0; // velocidad de viento en el aerogenerador
float rpmAerogenerador = 0; // rpm de rotaci�n de las palas 
int estadoFreno = 1; // estado actual del freno (0 = desactivado, 1 = activado) (lo iniciamos a "frenado" porque en el setup lo estamos frenando al retraero x segundos)

// Datos recibidos usuario
float maxRPMActual = 100; // revoluciones m�ximas permitidas actualmente antes de frenado.
float maxVientoActual = 20; // velocidad de viento [m/s] m�xima permitida actualmente antes de frenado.
int porcentajeDeflexionPalas = 0; // tanto por ciento de deflexi�n de las palas entre dos angulos de paso definidos
int modoPasoVariable = 1; // 0 = cambio de paso manual, 1 = cambio de paso automatico

// Actuador lineal (modificado como "servo")
ActuadorLineal actuadorPalas; 

// Frecuencia de envio de mensajes mediante el HC12
Metro enviarDatosHC12Timer = Metro(500); // Haremos el envio de datos cada ciertos milisegundos

// Declaramos el sensor de temperatura/humedad
DHT sensorTemperaturaHumedad(pinSensorTemperaturaHumedad, DHT22);
Metro leerDatosTemperaturaHumedad = Metro(10000); // Haremos la lectura de temperatura y humedad cada ciertos milisegundos

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

bool flagPeticionActivarFrenoArduinoUsuario = false; // Petici�n de frenado enviada desde el arduino usuario
float tiempoUltimaLecturaAnemometro = 1e9; // tiempo de la �ltima activaci�n del sensor hall del anem�metro
float tiempoUltimaLecturaRpmRotor = 1e9; // tiempo de la �ltima activaci�n del sensor hall de las revoluciones del rotor
bool flagSensorHallAnemometroActivado = false; // bool que indica si el sensor hall del anem�metro est� en estado alto
bool flagSensorHallRotorActivado = false; // bool que indica si el sensor hall de rpm del rotor est� en estado alto
float tiempoAuxiliarInicioFrenado = 0; // 
float tiempoAuxiliarInicioSoltarFreno = 0; // 
float tiempoManiobraActuador = 0; // cantidad de tiempo que el actuador va a actuar
float tiempoUltimaSolicitudActivacionFreno = 0; // variable que vamos a utilizar para que una vez que se activa el freno, permanezca activado ciert tiempo
bool flagActuadorFrenando = false;
bool flagActuadorSoltandoFreno = false;

// Velocidad media de viento
const int intervaloTemporalMediaMovilAnemometroSegundos = 60; // tiempo durante el cu�l se hace la media movil
const int numeroMuestras = 60; // n�mero de muestras para la media movil
Metro tiempoMediaMovilAnemomentro = Metro(1000); // Cada cierto tiempo guardaremos en un buffer la velocidad de viento actual, de tal forma que vayamos tirando el dato m�s antiguo
float bufferVelocidadViento[numeroMuestras]; // buffer para guardar velocidades de viento
float velocidadVientoMediaMovilActual = 0; // variable que contiene la media de velocidad de viento actual

// Anemometro
unsigned long ultimoTiempoAnemometroMilis = 0;
unsigned long ultimoTiempoTacometroMilis = 0;
float velocidadAngularAnemometroRadPorS = 0;
float velocidadAngularRotorRadPorS = 0;

// the setup function runs once when you press reset or power the board
void setup() {

	// Seteamos los pines como entrada o salida
	pinMode(pinModoHC12, OUTPUT); // control del modo del HC12
	pinMode(pinSensorHallAnemometro, INPUT); // entrada del sensor hall del anem�metro
	pinMode(pinSensorHallRpmRotor, INPUT); // entrada del sensor hall del rotor
	pinMode(pinReleActivarFreno, OUTPUT); // salida para activar el rele de freno
	pinMode(pinReleDesactivarFreno, OUTPUT); // salida para desactivar el rele de freno
	pinMode(pinSensorTemperaturaHumedad, INPUT);

	// Actuador lineal (modificado como "servo")
	actuadorPalas = ActuadorLineal(pinReleRetraerActuadorPalas, pinReleExtenderActuadorPalas, pinSensorHallServoPalas, longitudActuadorPalasMilimetros, LONGITUD_ACTUADOR_UTILIZABLE_MILIMETROS, 104); // Este actuador tiene 118 "vueltas" para extenderse completamente. (Este valor se podria determinar con una calibraci�n en el setup)

	// Iniciamos puerto serie
	delay(80); // retardo inicial para HC12
	Serial.begin(9600); // Iniciamos puerto serie del PC a cierto baud rate
	Serial1.begin(9600); // Iniciamos puerto serie del HC12 a cierto baud rate

	// Iniciamos sensor de temperatura/humedad
	sensorTemperaturaHumedad.begin();

	// iniciamos valores pines
	digitalWrite(pinReleActivarFreno, HIGH);
	digitalWrite(pinReleDesactivarFreno, HIGH);
	digitalWrite(pinModoHC12, HIGH);

	//Retraemos 10 segundos el actuador de freno
	Serial.println("Inicio setup actuador freno...");
	digitalWrite(pinReleActivarFreno, LOW);
	delay(10000);
	digitalWrite(pinReleActivarFreno, HIGH);
	Serial.println("Fin setup actuador freno");

	// Setup del anemometro
	attachInterrupt(digitalPinToInterrupt(pinSensorHallAnemometro), LeerHallAnemometro, RISING);	

	// Setup del tac�metro del rotor
	attachInterrupt(digitalPinToInterrupt(pinSensorHallRpmRotor), LeerHallRpmRotor, RISING);
}

// the loop function runs over and over again until power down or reset
void loop() {

	// Efectuamos la lectura de las variables recibidas por el arduino USUARIO
	leerDatosHC12();

	// Efectuamos las lecturas de temperatura, humedad, viento y rpm
	leerSensorTemperaturaHumedad();

	//velocidadVientoMetroSegundo = 10;
	rpmAerogenerador = 15;
	//mediaMovilViento();

	// Controlamos el frenado
	controlEstadoFreno(); // efectuamos maniobras necesarias
	finalizarManiobraActuadorFreno(); // finalizamos posibles maniobras iniciadas

	// Controlamos el paso
	controlarPasoPalas();

	// Enviamos los datos del estado actual del sistema
	enviarDatos();	
}

// M�todo que parsea un mensaje completo y extrae el id del mensaje para saber 
// a que dato corresponde, y el valor de dicho dato
bool parsearMensaje(int* idMensaje, double* valorMensaje) {

	char * strtokIndx; // Variable necesaria para la funci�n strtok(), ya que lo utiliza como �ndice

	// Leemos el id del mensaje 
	strtokIndx = strtok(mensajeRecibido, ",");
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
	case 200: // Max RPM (ID mensaje: 200)
		maxRPMActual = valorMensaje;
		break;
	case 201: // Max viento (ID mensaje: 201)
		maxVientoActual = valorMensaje;
		break;
	case 202: // estado freno manual (ID mensaje: 202)
		flagPeticionActivarFrenoArduinoUsuario = (bool)valorMensaje;
		break;
	case 203: // modo cambio paso (ID mensaje: 203)
		modoPasoVariable = valorMensaje;
		break;
	case 204: // % deflexion palas (ID mensaje: 204)
		porcentajeDeflexionPalas = valorMensaje;
		break;
	}
}

// M�todo que se encarga de leer un mensaje recibido a trav�s del HC12
void leerDatosHC12() {

	char startMarker = '['; // marca de comienzo de un mensaje
	char endMarker = ']'; // marca de fin de un mensaje
	char charRecibido; // Char recibido 

	while (Serial1.available() > 0) {
		// Leemos un char			
		charRecibido = Serial1.read();

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

				//Serial.print("Mensaje recibido: ");
				//Serial.println(mensajeRecibido);
				
				// parseamos el mensaje recibido para obtener "id" y "valor"
				int idMensaje = -1;
				double valorMensaje = -1;
				if (parsearMensaje(&idMensaje, &valorMensaje)) { // Llamamos a la funci�n que parsea el mensaje, y si devuelve true, es que el mensaje no ten�a errores y podemos proceder a guardarlo en la variable que le corresponde
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

// M�todo que envia los datos recogidos por el arduino de control
void enviarDatos() {

	// Solo enviaremos datos cada cierto tiempo
	if (enviarDatosHC12Timer.check()) {

		// Temperatura actual (ID mensaje: 100)
		dtostrf(temperaturaActualCelsius, 5, 1, textoAuxiliar5); // llamamos a la funci�n dtostrf con el valor LONGITUD_TEXTO_AUX para que rellene todos los elementos del char[] y no se quede basura de operaciones anteriores
		enviarMensajeHC12(100, textoAuxiliar5);

		// Humedad actual (ID mensaje: 101)
		dtostrf(humedadActual, 3, 0, textoAuxiliar3);
		enviarMensajeHC12(101, textoAuxiliar3);

		// Velocidad de viento actual (ID mensaje: 102)
		dtostrf(velocidadVientoMetroSegundo, 2, 0, textoAuxiliar2);
		enviarMensajeHC12(102, textoAuxiliar2);

		// Velocidad de giro actual (ID mensaje: 103)
		dtostrf(rpmAerogenerador, 3, 0, textoAuxiliar3);
		enviarMensajeHC12(103, textoAuxiliar3);

		// Estado freno (ID mensaje: 104)
		if (estadoFreno == 1 &&
			flagActuadorFrenando == false &&
			flagActuadorSoltandoFreno == false) {
			dtostrf(1, 1, 0, textoAuxiliar1); // enviamos un 1 indicando que est� frenado 
			enviarMensajeHC12(104, textoAuxiliar1);
		}
		else if (estadoFreno == 0 &&
			flagActuadorFrenando == false &&
			flagActuadorSoltandoFreno == false) {
			dtostrf(0, 1, 0, textoAuxiliar1); // enviamos un 0 indicando que no est� frenado
			enviarMensajeHC12(104, textoAuxiliar1);
		}
		else {
			dtostrf(2, 1, 0, textoAuxiliar1); // enviamos un 2 indicando que el freno se est� moviendo
			enviarMensajeHC12(104, textoAuxiliar1);
		}

		// Porcentaje deflexion palas (ID mensaje: 105)
		dtostrf(porcentajeDeflexionPalas, 3, 0, textoAuxiliar3);
		enviarMensajeHC12(105, textoAuxiliar3);
	}
}

// M�todo que genera y envia un mensaje a partir con un "id" entero dado
// y con un valor dado en un char array a trav�s del HC12
// NOTA: esta funci�n solo vale para un char* que tiene 6 elementos
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

// M�todo que controla si el freno debe ser activado o desactivado
void controlEstadoFreno() {

	if (velocidadVientoMediaMovilActual > maxVientoActual || // si la velocidad de viento supera el limite
		rpmAerogenerador > maxRPMActual || // si la velocidad del rotor supera el limite
		flagPeticionActivarFrenoArduinoUsuario == true) { // si se ha solicitado el frenado
		
		//Serial.print("viento actual: ");
		//Serial.print(velocidadVientoMediaMovilActual);
		//Serial.print("    max viento: ");
		//Serial.println(maxVientoActual);

		//Serial.print(" rpm: ");
		//Serial.print(rpmAerogenerador);
		//Serial.print("    max rpm: ");
		//Serial.println(maxRPMActual);

		//Serial.print(" peticion freno: ");
		//Serial.println(flagPeticionActivarFrenoArduinoUsuario);
		//Serial.println("----------------------------");

		frenar(); // frenamos		
	}
	else {
		if (estadoFreno == 1) {
			liberarFreno(); //  liberamos el freno
		}
	}
}

// M�todo que controla la orden de frenado
void frenar() {

	// tiempo de la ultima solicitud de frenado
	tiempoUltimaSolicitudActivacionFreno = millis();

	// Antes de empezar un nuveo frenado, tenemos que esperar a que se termine de retraer 
	// completamente el actuador para evitar acumular retraccion
	if (estadoFreno == 0 && // solo frenamos si el freno est� desactivado
		flagActuadorSoltandoFreno == false && // si estamos soltando freno, no interrumpimos el desplazamiento
		flagActuadorFrenando == false) // si el actuador ya se est� frenando, no necesitamos entrar
	{
		// activamos el freno
		digitalWrite(pinReleActivarFreno, LOW);

		// Establecemos el tiempo que vamos a esperar hasta desactivar el actuador
		tiempoManiobraActuador = tiempoDuracionRetraccionActuador;

		// Guardamos el tiempo en el que se ha solicitado frenado
		tiempoAuxiliarInicioFrenado = millis();

		// Ponemos al true este flag para indicar que estamos frenando
		flagActuadorFrenando = true;
	}	
}

// M�todo que controla la orden de soltar freno
void liberarFreno() {
	// Solo liberaremos el freno si ha pasado cieto tiempo desde la �ltima activacion
	if (estadoFreno == 1 && // solo frenamos si el freno est� desactivado
		flagActuadorFrenando == false && // si estamos frenando, no interrumpimos la maniobra para no perder la referencia de posici�n del actuador
		flagActuadorSoltandoFreno == false && // si ya estamos soltando freno, no es necesario volver a ordenarlo
		millis()-tiempoUltimaSolicitudActivacionFreno > tiempoMinimoRotorPermaneceParado ) // Solo liberamos el freno si ha pasado un tiempo prudencial desde la ultima vez que se activ�
	{
		// liberamos el freno
		digitalWrite(pinReleDesactivarFreno, LOW);

		//Establecemos el tiempo que vamos a esperar hasta desactivar el actuador
		tiempoManiobraActuador = tiempoDuracionExtensionActuador;

		// El tiempo actual lo seteamos en el inicio de la petici�n de frenado
		tiempoAuxiliarInicioSoltarFreno = millis();

		// ponemos este flag a true para indicar que estamos soltando freno
		flagActuadorSoltandoFreno = true;
	}
}

// M�todo que lee el sensor de humedad y temperatua
void leerSensorTemperaturaHumedad() {

	// Leeremos la temperatura y humedad cada cierto tiempo
	if (leerDatosTemperaturaHumedad.check()) {
		temperaturaActualCelsius = sensorTemperaturaHumedad.readTemperature();
		humedadActual = sensorTemperaturaHumedad.readHumidity();
	}
}

// M�todo que determina si una maniobra iniciada por el 
// actuador para frenar o soltar freno ha sido completada
// y debe realizarse una acci�n para terminarla
void finalizarManiobraActuadorFreno() {
	
	// chequeamos si es momento de detener el movimiento del actuador y dar por
	// finalizada la maniobra de frenado
	if (flagActuadorFrenando && // solo lo chequeamos si el actuador estaba frenando
		millis() - tiempoAuxiliarInicioFrenado > tiempoManiobraActuador) {
			digitalWrite(pinReleActivarFreno, HIGH); // desactivamos el rele del actuador
			flagActuadorFrenando = false; // el actuador ya no est� frenando
			estadoFreno = 1; // el freno est� completamente activo
	}
	
	// chequeamos si es momento de detener el movimiento del actuador y dar por
	// finalizada la maniobra de soltar freno
	if (flagActuadorSoltandoFreno && // solo lo chequeamos si el actuador estaba liberando el freno
		millis() - tiempoAuxiliarInicioSoltarFreno > tiempoManiobraActuador) {
			digitalWrite(pinReleDesactivarFreno, HIGH); // desactivamos el rele del actuador
			flagActuadorSoltandoFreno = false; // el actuador ya no est� soltando freno
			estadoFreno = 0; // el freno est� completamente soltado
	}
}

// M�todo que calcula la media de la velocidad en 
// �ltimo intervalo de tiempo
void mediaMovilViento() {

	// Tomamos una nueva muestra para la media movil solo si corresponde
	if (tiempoMediaMovilAnemomentro.check()) {
		// Corremos la posici�n de todo el buffer una posici�n hacia la derecha
		for (int i = numeroMuestras - 1; i > 0; i--) {
			bufferVelocidadViento[i] = bufferVelocidadViento[i - 1];
		}

		// Almacenamos el valor actual de velocidad de viento en la posici�n [0]
		bufferVelocidadViento[0] = velocidadVientoMetroSegundo;

		// Hacemos la media del buffer para obtener la media movil actual
		float media = 0;
		for (int i = 0; i < numeroMuestras; i++) {
			media += bufferVelocidadViento[i];
		}
		velocidadVientoMediaMovilActual = media / numeroMuestras;	
	}
}

void controlarPasoPalas() {
	if (flagActuadorFrenando || estadoFreno == 1) { // si el actuador est� frenando o frenado, la deflexi�n de las palas debe ponerse a 0%
		actuadorPalas.SetExtensionPorcentaje(0);
	}
	else { // Si est� operando normalmente, vemos el tipo de control de paso
		if (modoPasoVariable == 0) { // Paso variable manual
			actuadorPalas.SetExtensionPorcentaje(porcentajeDeflexionPalas);
		}
		else if (modoPasoVariable == 1) { // Paso variable autom�tico

			// Calculamos el �ngulo del vector velocidad aerodin�mica con el disco del rotor.
			float alpha_wind;
			if (velocidadVientoMetroSegundo < 1) // Si no tenemos viento, tampoco tendremos rpm, y tendr�amos una indeterminaci�n
				alpha_wind = PI / 2;
			else // Calculos para una secci�n a 0.3 de distancia de punta de pala (aprox 33 cm). Convertimos rpm a rad/s 
				alpha_wind = atan(velocidadVientoMetroSegundo * 30 / PI / rpmAerogenerador / 0.33);

			// El �ngulo que tenemos que girar la pala el angulo del viento menos lo que ya tenemos girado
			// por torsi�n y menos el �ngulo que necesitamos para trabajar en la m�xima eficiencia aerodin�mica
			// del perfil
			float beta = alpha_wind - 22 * PI / 180; // el �ngulo de torsi�n en la secci�n 0.3 de la envergadura es 12.75� y el angulo de m�xima eficiencia del perfil es 9.25�, la suma es 22�
			// TODO: relacionar extension actuador con angulo
		}
	}
	
	actuadorPalas.Operar(); // Esta funci�n debe ser llamada siempre en el loop principal para que el actuador haga lo que debe	
}

void LeerHallAnemometro() {

	Serial.println("Interrupt anemometro");

	// Evitamos con este if que la misma vuelta active el iman mas de una vez en una misma pasada
	if (millis() - ultimoTiempoAnemometroMilis > 2) {
		velocidadAngularAnemometroRadPorS = 2 * PI / 3 / (millis() - ultimoTiempoAnemometroMilis) * 1000; // El 1000 es porque los tiempos estan en milisegundoos
		velocidadVientoMetroSegundo = velocidadAngularAnemometroRadPorS; // TODO: multiplicar por el factor del anemometro
		ultimoTiempoAnemometroMilis = millis();
	}
}

void LeerHallRpmRotor() {

	Serial.println("Interrupt tacometro");

	// Evitamos con este if que la misma vuelta active el iman mas de una vez en una misma pasada
	if (millis() - ultimoTiempoTacometroMilis > 2) {
		velocidadAngularRotorRadPorS = 2 * PI / 3 / (millis() - ultimoTiempoTacometroMilis) * 1000; // El 1000 es porque los tiempos estan en milisegundoos
		ultimoTiempoAnemometroMilis = millis();
	}
}


