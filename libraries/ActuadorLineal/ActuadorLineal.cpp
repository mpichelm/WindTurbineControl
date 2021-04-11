/*
 Name:		ActuadorLineal.cpp
 Created:	18/02/2018 10:31:13
 Author:	PC
 Editor:	http://www.visualmicro.com
*/

// NOTA: Los reles tienen logica invertida. Es decir, si no damos señal, están comuntados al "normally open",
// lo cual es una putada porque consumen más. Asi que nos adaptaremos para utilizar esta lógica y que por defecto
// estén apagados cuando ningun actuador está actuando.

#include "Arduino.h"
#include "ActuadorLineal.h"

ActuadorLineal::ActuadorLineal() {}

ActuadorLineal::ActuadorLineal(int pinReleRetraerActuador, int pinReleExtenderActuador, int pinSensorHall, float longitudMaximaActuadorMilimetros, float longitudActuadorUtilizableMilimetros, int numeroMaximoVueltas) {

	// Asignamos la funcion "interrupt" que queremos que salte cuando el engranaje del actuador da una "vuelta"
	attachInterrupt(digitalPinToInterrupt(pinSensorHall), LeerSensorHallEngranaje, RISING);

	// guardamos los pines en las variables internas
	this->pinReleExtenderActuador = pinReleExtenderActuador;
	this->pinReleRetraerActuador = pinReleRetraerActuador;
	this->pinSensorHall = pinSensorHall;

	// guardamos los datos del actuador en la variable interna
	this->longitudMaximaActuador = longitudMaximaActuadorMilimetros;
	this->longitudActuadorUtilizable = longitudActuadorUtilizableMilimetros;

	// Declaramos los pines involucrados
	pinMode(pinReleRetraerActuador, OUTPUT); // pin que activa el relé que extiende el actuador
	pinMode(pinReleExtenderActuador, OUTPUT); // pin que activa el relé que retrae el actuador
	pinMode(pinSensorHall, INPUT); // pin que recibe la señal del sensor hall para contar vueltas

	// Realizamos la calibración para saber cuantas vueltas de engranaje del actuador son
	// necesarias para obtener la máxima extensión del mismo
	// Primero extraemos completamente el actuador
	RetraerActuador();
	delay(MILISEGUNDOS_CALIBRACION_ACTUADOR); // mantenemos el el pin de extension durante 15 segundos, tiempo suficiente para que el actuador se expanda completamente
	DetenerActuador();

	// resetamos la variable del número de vueltas del engranaje
	numeroVueltasTotalesEngranaje = numeroMaximoVueltas; 
	numeroVueltasEngranajeActual = 0;

	// Calculamos cuanta extensión corresponde con cada vuelta de engranaje
	relacionExtensionPorVueltaEngranaje = longitudMaximaActuadorMilimetros / numeroVueltasTotalesEngranaje;	

	// flag de calibración
	flagCalibrando = false;	
}

void ActuadorLineal::Operar() {

	//Serial.println("---------");
	//Serial.print("Objetivo: ");
	//Serial.println(numeroVueltasObjetivo);

	//Serial.print("Actual: ");
	//Serial.println(numeroVueltasEngranajeActual);

	if (!flagCalibrando) { // solo hacemos esto si no estamos calibrando
		// La parte más importante de la operación es controlar que la extensión del actuador coincide con la pedida por el usuario
		// calculamos el número de vueltas de engranaje que correponde con la extensión que buscamos
		numeroVueltasObjetivo = alargamientoSolicitadoMilimetros / relacionExtensionPorVueltaEngranaje;

		// Comparamos el número de vueltas del engranaje actual con la objetivo para ver si tenemos que activar el relé de extensión, de retracción, o ninguno
		if (numeroVueltasObjetivo > numeroVueltasEngranajeActual &&
			abs(numeroVueltasObjetivo - numeroVueltasEngranajeActual) > NUMERO_VUELTAS_ERROR_ADMITIDO) { // en este caso necesitamos expandir el actuador
			ExtenderActuador();
		}
		else if (numeroVueltasObjetivo < numeroVueltasEngranajeActual &&
			abs(numeroVueltasObjetivo - numeroVueltasEngranajeActual) > NUMERO_VUELTAS_ERROR_ADMITIDO) { // en este caso necesitamos retraer
			RetraerActuador();
		}
		else { // si no estamos en ninguno de los casos anteriores, significa que las vueltas objetivo coninciden con las que tenemos
			DetenerActuador();
		}
	}
	else { // si estamos calibrando, controlamos la calibración
		if (millis() - tiempoInicioCalibracion > MILISEGUNDOS_CALIBRACION_ACTUADOR) { // esperamos 15 segundos para desactivar la calibración
			flagCalibrando = false; // ya podemos desactivar el flag
			numeroVueltasEngranajeActual = 0; // el actuador se habrá retraido completamente, asi que resetamos las vueltas del engranaje
		}
	}
}

// Función que establece el alargamiento del actuador en una determinada distancia en milímetros tomando como referencia cuando está totalmente retraído
// Viene a ser un método setter
void ActuadorLineal::SetExtensionLongitud(float alargamientoMilimetros) {
	alargamientoMilimetros = min(alargamientoMilimetros, longitudActuadorUtilizable);
	alargamientoSolicitadoMilimetros = alargamientoMilimetros;
}

// Función que establece el alargamiento del actuador en un cierto % de la extensión utilizable que hemos elegido para el actuador.
void ActuadorLineal::SetExtensionPorcentaje(float porcentajeExtension) {
	alargamientoSolicitadoMilimetros = porcentajeExtension / 100 * longitudActuadorUtilizable;
}

// Método que calcula el % de extensión del actuador respecto a la longitud utilizable elegida
float ActuadorLineal::GetExtensionActualPorcentaje() {
	return numeroVueltasEngranajeActual * relacionExtensionPorVueltaEngranaje / longitudActuadorUtilizable * 100;
}

void ActuadorLineal::ExtenderActuador()
{
	DetenerActuador();
	estadoActuadorPalas = 1;
	digitalWrite(pinReleRetraerActuador, HIGH);
	digitalWrite(pinReleExtenderActuador, LOW);
}

void ActuadorLineal::RetraerActuador()
{
	DetenerActuador();
	estadoActuadorPalas = -1;
	digitalWrite(pinReleExtenderActuador, HIGH);
	digitalWrite(pinReleRetraerActuador, LOW);
}

void ActuadorLineal::DetenerActuador()
{
	estadoActuadorPalas = 0;
	digitalWrite(pinReleExtenderActuador, HIGH);
	digitalWrite(pinReleRetraerActuador, HIGH);
}

int ActuadorLineal::GetVueltasActuales()
{
	return numeroVueltasEngranajeActual;
}

void ActuadorLineal::SetVueltasActuales(int vueltasActuales)
{
	numeroVueltasEngranajeActual = vueltasActuales;
}

// Método que permite calibrar el actuador. No es igual que la calibración inicial, en la que se determina el número de vueltas de engranaje 
// necesarias para extender completamente el actuador. En este caso solo se retrae el actuador hasta asegurar que el 0% de extensión sucede
// cuando está completamente retraído y no ha habido una deriva
void ActuadorLineal::Calibrar() {

	if (!flagCalibrando) { // si no estamos calibrando, podemos activarla
		flagCalibrando = true; // empezamos a calibrar
		tiempoInicioCalibracion = millis(); // anotamos el tiempo en el que empezamos, porque la calibración dura un tiempo concreto
		RetraerActuador(); // retraemos el actuador.
	}
}

void ActuadorLineal::LeerSensorHallEngranaje() {
	// Evitamos con este if que la misma vuelta active el iman mas de una vez en una misma pasada
	if (millis() - tiempoUltimaActivacionHallServo > 10) {
		if (estadoActuadorPalas == 1) {
			numeroVueltasEngranajeActual = numeroVueltasEngranajeActual + 1;
			Serial.print("+++ : ");
			Serial.println(numeroVueltasEngranajeActual);

		}
		else if (estadoActuadorPalas == -1) {
			numeroVueltasEngranajeActual = numeroVueltasEngranajeActual -1;
			Serial.print("--- : ");
			Serial.println(numeroVueltasEngranajeActual);
		}
		//numeroVueltasEngranajeActual = max(0, min(numeroVueltasEngranajeActual, numeroVueltasTotalesEngranaje)); // Saturamos entre las vueltas máximas y minimas
		numeroVueltasEngranajeActual = max(0, numeroVueltasEngranajeActual); // Saturamos entre las vueltas máximas y minimas

		tiempoUltimaActivacionHallServo = millis();


	}
}



