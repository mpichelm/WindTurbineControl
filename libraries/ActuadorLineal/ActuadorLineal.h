/*
 Name:		ActuadorLineal.h
 Created:	18/02/2018 10:31:13
 Author:	PC
 Editor:	http://www.visualmicro.com
*/

#ifndef _ActuadorLineal_h
#define _ActuadorLineal_h

#define NUMERO_VUELTAS_ERROR_ADMITIDO 0 // N�mero de vueltas de diferencia que se admiten entre la posicion del actuador solicitada y la actual 
#define MILISEGUNDOS_CALIBRACION_ACTUADOR 15000 // Milisegundos que dura la calibraci�n del actuador

// incluimos las funciones del entorno arduino
#include "arduino.h"

volatile static int numeroVueltasEngranajeActual = 0; // n�mero de vueltas que el engranaje ha dado desde que estaba completamente retraido
volatile static long tiempoUltimaActivacionHallServo = 0; // instante de tiempo en el que fue activado por ultima vez el sensor hall del engranaje del actuador
volatile static int estadoActuadorPalas = 0; // (-1 = retrayendo, 0 = parado, 1 = extendiendo)
volatile static int numeroVueltasTotalesEngranaje; // n�mero m�ximo de vueltas que da el engranaje para que el actuador se expanda completamente

class ActuadorLineal {
	public: // variables internas del programa
		int pinReleRetraerActuador; // NOTA: los rel�s funcionan con l�gica inversa, poner a LOW implica que est� funcionando en "normally open" del rele
		int pinReleExtenderActuador; // NOTA: los rel�s funcionan con l�gica inversa, poner a LOW implica que est� funcionando en "normally open" del rele
		int pinSensorHall;
		float longitudMaximaActuador; // extensi�n m�xima del actuador en milimetros
		float longitudActuadorUtilizable; // extension del actuador que queremos utilizar, en mil�metros. De esta forma, 100% de extensi�n ser� esta longitud
		int numeroVueltasObjetivo; // cuando el usuario pida una determinada extensi�n, se calcular� el n�mero de vueltas objetivo y se activar� el rele que toque hasta conseguirlo 
		float relacionExtensionPorVueltaEngranaje; // extensi�n en mm que sufre el actuador cuando el engranaje gira una vuelta
		bool flagCalibrando; 
		float tiempoInicioCalibracion; // instante de tiempo en el que se inici� la calibraci�n
		float alargamientoSolicitadoMilimetros; // alargamiento solicitado por el usuario, en milimetros

		static void LeerSensorHallEngranaje(); // funci�n "interrupt" que cuenta vueltas del engranaje del actuador

	public: // funciones a la vista del usuario
		ActuadorLineal(); // constructor 
		ActuadorLineal(int pinReleRetraerActuador, int pinReleExtenderActuador, int pinSensorHall, float longitudMaximaActuadorMilimetros, float longitudActuadorUtilizableMilimetros, int numeroMaximoVueltas); // constructor e inicializador de la configuraci�n
		void Operar(); // funci�n que se debe llamar continuamente en el loop de arduino para que se lleve a cabo la operaci�n del actuador de forma continua
		void SetExtensionLongitud(float alargamientoMilimetros);
		void SetExtensionPorcentaje(float porcentajeExtension);
		void Calibrar();
		float GetExtensionActualPorcentaje(); // m�todo que devuelve el % de extensi�n actual del actuador
		void ExtenderActuador();
		void RetraerActuador();
		void DetenerActuador();
		int GetVueltasActuales();
		void SetVueltasActuales(int vueltasActuales);
};

#endif

