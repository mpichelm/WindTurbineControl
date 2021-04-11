/*
 Name:		ActuadorLineal.h
 Created:	18/02/2018 10:31:13
 Author:	PC
 Editor:	http://www.visualmicro.com
*/

#ifndef _ActuadorLineal_h
#define _ActuadorLineal_h

#define NUMERO_VUELTAS_ERROR_ADMITIDO 0 // Número de vueltas de diferencia que se admiten entre la posicion del actuador solicitada y la actual 
#define MILISEGUNDOS_CALIBRACION_ACTUADOR 15000 // Milisegundos que dura la calibración del actuador

// incluimos las funciones del entorno arduino
#include "arduino.h"

volatile static int numeroVueltasEngranajeActual = 0; // número de vueltas que el engranaje ha dado desde que estaba completamente retraido
volatile static long tiempoUltimaActivacionHallServo = 0; // instante de tiempo en el que fue activado por ultima vez el sensor hall del engranaje del actuador
volatile static int estadoActuadorPalas = 0; // (-1 = retrayendo, 0 = parado, 1 = extendiendo)
volatile static int numeroVueltasTotalesEngranaje; // número máximo de vueltas que da el engranaje para que el actuador se expanda completamente

class ActuadorLineal {
	public: // variables internas del programa
		int pinReleRetraerActuador; // NOTA: los relés funcionan con lógica inversa, poner a LOW implica que está funcionando en "normally open" del rele
		int pinReleExtenderActuador; // NOTA: los relés funcionan con lógica inversa, poner a LOW implica que está funcionando en "normally open" del rele
		int pinSensorHall;
		float longitudMaximaActuador; // extensión máxima del actuador en milimetros
		float longitudActuadorUtilizable; // extension del actuador que queremos utilizar, en milímetros. De esta forma, 100% de extensión será esta longitud
		int numeroVueltasObjetivo; // cuando el usuario pida una determinada extensión, se calculará el número de vueltas objetivo y se activará el rele que toque hasta conseguirlo 
		float relacionExtensionPorVueltaEngranaje; // extensión en mm que sufre el actuador cuando el engranaje gira una vuelta
		bool flagCalibrando; 
		float tiempoInicioCalibracion; // instante de tiempo en el que se inició la calibración
		float alargamientoSolicitadoMilimetros; // alargamiento solicitado por el usuario, en milimetros

		static void LeerSensorHallEngranaje(); // función "interrupt" que cuenta vueltas del engranaje del actuador

	public: // funciones a la vista del usuario
		ActuadorLineal(); // constructor 
		ActuadorLineal(int pinReleRetraerActuador, int pinReleExtenderActuador, int pinSensorHall, float longitudMaximaActuadorMilimetros, float longitudActuadorUtilizableMilimetros, int numeroMaximoVueltas); // constructor e inicializador de la configuración
		void Operar(); // función que se debe llamar continuamente en el loop de arduino para que se lleve a cabo la operación del actuador de forma continua
		void SetExtensionLongitud(float alargamientoMilimetros);
		void SetExtensionPorcentaje(float porcentajeExtension);
		void Calibrar();
		float GetExtensionActualPorcentaje(); // método que devuelve el % de extensión actual del actuador
		void ExtenderActuador();
		void RetraerActuador();
		void DetenerActuador();
		int GetVueltasActuales();
		void SetVueltasActuales(int vueltasActuales);
};

#endif

