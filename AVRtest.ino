/*
	Name:       AVRtest.ino/ singel stepper driver
	Created:	summer 2019
	Version:    V1.01
	Author:     Rob Antonisse


	Atmel attiny85 program for driving a unipolaid steppermotor like 28byj-48
	Two working modes: as switch engine or slow motor with high torque.
	Many progammable parameters see www.wisselmotor.nl for manual

	Notes:
	Clockspeed Internal 16Mhz. To set by bootloader burn with arduino IDE
	I Used Arduino as ISP programmer
	I used Microsoft Visual studio 2017 with visualmicro as plugin (https://www.visualmicro.com/)
	PCB available at https://sites.google.com/site/wisselmotor/ or https://www.pcbway.com

*/

#include <EEPROM.h>

//Declarations Dekoder attiny85
volatile unsigned long DEK_Tperiode; //laatst gemeten tijd 
volatile unsigned int DEK_duur; //gemeten duur van periode tussen twee interupts
boolean DEK_Monitor = false; //shows DCC commands as bytes
byte DEK_Reg; //register voor de decoder 
byte DEK_Status = 0;
byte DEK_byteRX[6]; //max length commandoos for this decoder 6 bytes (5x data 1x error check)
byte DEK_countPA = 0; //counter for preample
byte DEK_BufReg[12]; //registerbyte for 12 command buffers
//bit7= free (false) bit0= CV(true)
byte DEK_Buf0[12];
byte DEK_Buf1[12];
byte DEK_Buf2[12];
byte DEK_Buf3[12];
byte DEK_Buf4[12];
byte DEK_Buf5[12];
//end dekoder declarations

unsigned int DCCadres;
byte COM_reg; //common flag register
byte MEM_reg; //EEPROM memorie register
byte shiftbyte;//databyte to shiftregister
byte shiftled;//databyte status leds
byte switchcount; //counter
byte switchstatus; //register for status switches
boolean status;//state of input port B4
byte stepcount; //ff int
byte stepfase;//fase in stepper cycle
byte speed;//speed of stepper movement
byte counter[6]; //general purpose counters
byte prgmode;//mode of program
byte prgfase; //fase in programcycle
byte ledmode;//blinking effects mode
int Currentposition; //current position of stepper
int Targetposition; //target position for stepper

void setup()
{
	delay(200);
	//setup ports 
	DDRB |= (1 << 3);
	DDRB |= (1 << 1);
	DDRB |= (1 << 2);
	DDRB |= (1 << 5);
	DDRB &= ~(1 << 4);
	DDRB &= ~(1 << 0);
	PORTB |= (1 << 4);//pull-up to pb4   	 

	//DeKoder Attiny85 part, interrupt on PIN5 (PB0); setup interupt
	PCMSK |= (1 << 0); //pin change mask register bit 0 = PB0
	//DDRD &= ~(1 << 2);//bitClear(DDRD, 2); //pin2 input
	DEK_Tperiode = micros();
	MCUCR |= (1 << 0);//EICRA – External Interrupt Control Register A bit0 > 1 en bit1 > 0 (any change)
	GIMSK |= (1 << 5); //PCIE: Pin Change Interrupt Enable
	//initialise
	MEM_read();
	switchstatus = 7; //clear status of switches
	COM_reg |= (1 << 2); //start init
}
void MEM_clear() {
	//clears 1e 100bytes of eeprom and restart
	for (byte i = 0; i < 100; i++) {
		EEPROM.update(i, 0xFF);
	}
	delay(50);
	setup();
}
void MEM_read() {
	//setup from memorie
	MEM_reg = EEPROM.read(10);
	speed = EEPROM.read(11);
	if (speed > 60) {
		speed = 5;
		EEPROM.update(11, speed);
		delay(100);
	}
	Targetposition = EEPROM.read(12);
	if (Targetposition == 0xFF) { //alleen na een factory reset
		Targetposition = 50;
		EEPROM.update(12, Targetposition);
		delay(100);
	}
	Targetposition = Targetposition * 10;
	EEPROM.get(15, DCCadres);
	if (DCCadres == 0xFFFF) {
		DCCadres = 1;
		EEPROM.put(15, 1);
		delay(100);
	}
}
void MEM_change() {
	byte temp;
	EEPROM.update(10, MEM_reg);
	delay(100);
	EEPROM.update(11, speed);
	delay(100);
	temp = Targetposition / 10;
	EEPROM.update(12, temp);
	delay(100);
	//restore parameters
	MEM_read();
}
void leddir() {
	//sets state of control leds
	if (prgmode == 0 & ledmode == 0) {
		if (bitRead(COM_reg, 0) == false) {
			shiftbyte |= (1 << 3);
			shiftled &= ~(3 << 0);
		}
		else {
			shiftbyte &= ~(1 << 3);
			shiftled |= (3 << 0);
		}
	}
}
//deKoder for Attiny85 
ISR(PCINT0_vect) {
	cli();
	DEK_duur = (micros() - DEK_Tperiode);
	DEK_Tperiode = micros();
	if (DEK_duur > 50) {
		if (DEK_duur < 62) {
			DEK_Reg |= (1 << 0); //bitSet(DekReg, 0);
			if (bitRead(DEK_Reg, 1) == false) {
				DEK_Reg &= ~(1 << 2); //bitClear(DekReg, 2);
				DEK_Reg |= (1 << 1);
			}
			else { //received full true bit
				DEK_Reg |= (1 << 3);
				DEK_BitRX();
				DEK_Reg &= ~(1 << 2);//bitClear(DekReg, 2);
				DEK_Reg &= ~(1 << 1); //bitClear(DekReg, 1);
			}
		}
		else {
			if (DEK_duur > 106) {

				if (DEK_duur < 124) { //preferred 118 6us extra space in false bit
					DEK_Reg |= (1 << 0); //bitSet(DekReg, 0);
					if (bitRead(DEK_Reg, 2) == false) {
						DEK_Reg &= ~(1 << 1); //bitClear(DekReg, 1);
						DEK_Reg |= (1 << 2);  //bitSet(DekReg, 2);
					}
					else { //received full false bit
						DEK_Reg &= ~(1 << 3); //bitClear(DekReg, 3);
						DEK_BitRX();
						DEK_Reg &= ~(1 << 2);//bitClear(DekReg, 2);
						DEK_Reg &= ~(1 << 1); //bitClear(DekReg, 1);
					}
				}
			}
		}
	}
	sei();
}
void DEK_begin() {//runs when bit is corrupted, or command not correct
	//lesscount++;
	DEK_countPA = 0;
	DEK_Reg = 0;
	DEK_Status = 0;
	for (int i = 0; i < 6; i++) {
		DEK_byteRX[i] = 0; //reset receive array
	}
}
void DEK_BufCom(boolean CV) { //create command in Buffer
	byte i = 0;
	while (i < 12) {

		if (bitRead(DEK_BufReg[i], 7) == false) {
			DEK_BufReg[i] = 0; //clear found buffer


			DEK_Buf0[i] = DEK_byteRX[0];
			DEK_Buf1[i] = DEK_byteRX[1];
			DEK_Buf2[i] = DEK_byteRX[2];

			if (CV == true) {
				DEK_BufReg[i] |= (1 << 0); //set for CV
				DEK_Buf3[i] = DEK_byteRX[3];
				DEK_Buf4[i] = DEK_byteRX[4];
				DEK_Buf5[i] = DEK_byteRX[5];
			}
			else {

				DEK_Buf3[i] = 0;
				DEK_Buf4[i] = 0;
				DEK_Buf5[i] = 0;
			}
			DEK_BufReg[i] |= (1 << 7); //claim buffer
			i = 15;
		}
		i++;
	}
}
void DEK_BitRX() {
	static byte countbit = 0; //counter received bits
	static byte countbyte = 0;
	static byte n = 0;
	DEK_Reg |= (1 << 4);//resets and starts process if not reset in this void
	switch (DEK_Status) {
		//*****************************
	case 0: //Waiting for preample 
		if (bitRead(DEK_Reg, 3) == true) {
			DEK_countPA++;

			if (DEK_countPA > 12) {
				DEK_Status = 1;
				countbit = 0;
				countbyte = 0;
			}
			bitClear(DEK_Reg, 4);
		}
		break;
		//*************************
	case 1: //Waiting for false startbit

		if (bitRead(DEK_Reg, 3) == false) { //startbit receive
			DEK_countPA = 0;
			DEK_Status = 2;
		}
		//if Dekreg bit 3= true no action needed.
		bitClear(DEK_Reg, 4); //correct, so resume process
		break;
		//*************************
	case 2: //receiving data
		if (bitRead(DEK_Reg, 3) == true) DEK_byteRX[countbyte] |= (1 << (7 - countbit));
		countbit++;
		if (countbit == 8) {
			countbit = 0;
			DEK_Status = 3;
			countbyte++;
		}
		bitClear(DEK_Reg, 4); //correct, so resume process
		break;
		//*************************
	case 3: //waiting for separating or end bit
		if (bitRead(DEK_Reg, 3) == false) { //false bit
			DEK_Status = 2; //next byte
			if ((bitRead(DEK_byteRX[0], 6) == false) & (bitRead(DEK_byteRX[0], 7) == true))bitClear(DEK_Reg, 4); //correct, so resume process	
		}
		else { //true bit, end bit, only 3 byte and 6 byte commands handled by this dekoder
			switch (countbyte) {
			case 3: //Basic Accessory Decoder Packet received
				//check error byte
				if (DEK_byteRX[2] = DEK_byteRX[0] ^ DEK_byteRX[1])DEK_BufCom(false);
				break; //6
			case 6: ///Accessory decoder configuration variable Access Instruction received (CV)
				//in case of CV, handle only write command
				if (bitRead(DEK_byteRX[2], 3) == true && (bitRead(DEK_byteRX[2], 2) == true)) {
					//check errorbyte and make command
					if (DEK_byteRX[5] = DEK_byteRX[0] ^ DEK_byteRX[1] ^ DEK_byteRX[2] ^ DEK_byteRX[3] ^ DEK_byteRX[4])DEK_BufCom(true);
				}
				break;
			} //close switch bytecount
		}//close bittype
		break;
		//***************************************
	} //switch dekstatus
	if (bitRead(DEK_Reg, 4) == true)DEK_begin();
}
void DEK_DCCh() { //handles incoming DCC commands, called from loop()
	static byte n = 0; //one buffer each passing
	byte temp;
	unsigned int decoder;
	unsigned int channel = 1;
	unsigned int adres;
	boolean port = false;
	boolean onoff = false;
	unsigned int cv;
	unsigned int value;

	//translate command
	if (bitRead(DEK_BufReg[n], 7) == true) {
		decoder = DEK_Buf0[n] - 128;
		if (bitRead(DEK_Buf1[n], 6) == false)decoder = decoder + 256;
		if (bitRead(DEK_Buf1[n], 5) == false)decoder = decoder + 128;
		if (bitRead(DEK_Buf1[n], 4) == false)decoder = decoder + 64;
		//channel
		if (bitRead(DEK_Buf1[n], 1) == true) channel = channel + 1;
		if (bitRead(DEK_Buf1[n], 2) == true) channel = channel + 2;
		//port
		if (bitRead(DEK_Buf1[n], 0) == true)port = true;
		//onoff
		if (bitRead(DEK_Buf1[n], 3) == true)onoff = true;
		//CV
		if (bitRead(DEK_BufReg[n], 0) == true) {
			cv = DEK_Buf3[n];
			if (bitRead(DEK_Buf2[n], 0) == true)cv = cv + 256;
			if (bitRead(DEK_Buf2[n], 1) == true)cv = cv + 512;
			cv++;
			value = DEK_Buf4[n];
		}
		else {
			cv = 0;
			value = 0;
		}
		COM_exe(bitRead(DEK_BufReg[n], 0), decoder, channel, port, onoff, cv, value);

		//clear buffer
		DEK_BufReg[n] = 0;
		DEK_Buf0[n] = 0;
		DEK_Buf1[n] = 0;
		DEK_Buf2[n] = 0;
		DEK_Buf3[n] = 0;
		DEK_Buf4[n] = 0;
		DEK_Buf5[n] = 0;
	}
	n++;
	if (n > 12)n = 0;
}
void COM_exe(boolean type, unsigned int decoder, unsigned int channel, boolean port, boolean onoff, unsigned int cv, unsigned int value) {

	//type=CV(true) or switch(false)
	//decoder basic adres of decoder 
	//channel assigned one of the 4 channels of the decoder (1-4)
	//Port which port R or L
	//onoff bit3 port on or port off
	//cv cvnumber
	//cv value
	int adres;
	adres = ((decoder - 1) * 4) + channel;
	//Applications 
	//APP_Monitor(type, adres, decoder, channel, port, onoff, cv, value);
	APP_exe(type, adres, decoder, channel, port, onoff, cv, value);
}
//dekoder end**********************************
void APP_exe(boolean type, unsigned int adres, unsigned int decoder, unsigned int channel, boolean port, boolean onoff, unsigned int cv, unsigned int value) {
	//execute of received DCC commands
	if (bitRead(COM_reg, 3) == true) { //waiting for setting DCC adres.
		COM_reg &= ~(1 << 3);
		DCCadres = adres;
		EEPROM.put(15, adres);
		prgEnd();
	}
	else {
		if (adres == DCCadres) {
			if (cv == false) { //switch command
				if (port^bitRead(MEM_reg, 3) == 1) {
					//if (port == true) {
					COM_reg |= (1 << 0);
					if (Currentposition == 0) COM_reg |= (1 << 1); //Start alleen vanuit een nulpositie, richting wisselen terwijl stepper draaid wel.
				}
				else {
					COM_reg &= ~(1 << 0);
					COM_reg |= (1 << 1);
					COM_reg |= (1 << 4);
				}
				switchstatus |= (1 << 2);
			}
			else {//CV commando
				switch (cv) {
				case 8: //factory reset
					if (value == 10) {
						MEM_clear();
					}
					break;
				case 10: //position
					Targetposition = value * 10;
					MEM_change();
					break;
				case 11: //speed
					if (value > 0 & value < 60) {
						speed = value;
						MEM_change();
					}
					break;
				case 12: //Orientatie
					switch (value) {
					case 0:
						MEM_reg &= ~(1 << 3);
						break;
					case 1://default
						MEM_reg |= (1 << 3);
						break;
					}
					MEM_change();
					break;
				}
			}
		}
	}
}
void steps() {
	//STEPPER in HALFSTEP mode
	byte step;
	step = stepfase;
	//false stand is voor de all goods steppers...
	if (bitRead(MEM_reg, 0) == true) step = 7 - step;
	shiftbyte &= ~(15 << 4);
	switch (step) {
	case 0: //0010
		shiftbyte |= (2 << 4);
		break;
	case 1: //0011 (1)
		shiftbyte |= (3 << 4);
		break;
	case 2: //0001
		shiftbyte |= (1 << 4);
		break;
	case 3: //1001 (3)
		shiftbyte |= (9 << 4);
		break;
	case 4: //1000
		shiftbyte |= (8 << 4);
		//shiftbyte &= ~(7 << 4);
		break;
	case 5: //1100 (5)
		shiftbyte |= (12 << 4);
		break;
	case 6: //0100
		shiftbyte |= (4 << 4);
		break;
	case 7: //0110 (7)
		shiftbyte |= (6 << 4);
		break;
	}

	if (bitRead(COM_reg, 0) == true) {
		Currentposition++;
		stepfase++;
		if (stepfase > 7)stepfase = 0;
	}
	else {
		Currentposition--;
		stepfase--;
		if (stepfase > 7)stepfase = 7;
	}
	stopstep();
}
/*
void step4() {
//stepper in FULLstep mode
	byte step;
	step = stepfase;

	//false stand is voor de all goods steppers...
	if (bitRead(MEM_reg, 0) == false) step = 3 - step;
	shiftbyte &= ~(15 << 4);
	switch (step) {
	case 0:
		shiftbyte |= (B1001 << 4);
		break;
		break;
	case 1:
		shiftbyte |= (B0011 << 4);
		break;
	case 2:
		shiftbyte |= (B0110 << 4);
		break;
	case 3:
		shiftbyte |= (B1100 << 4);
		break;
	}

	if (bitRead(COM_reg, 0) == true) {
		Currentposition++;
		stepfase++;
		if (stepfase > 3)stepfase = 0;
	}
	else {
		Currentposition--;
		stepfase--;
		if (stepfase > 3)stepfase = 3;
	}
	stopstep();
}
*/
void speedx() {
	byte add;
	add = 1 + (speed / 5);
	if (bitRead(shiftled, 1) == true) {
		speed = speed + add;
		if (speed > 59)speed = 60;
	}
	else {
		speed = speed - add;
	}

	if (speed == 60 | speed < 3)shiftled ^= (1 << 1); //max speed =2
}
void stopstep() {
	boolean end = false;
	if (Currentposition == Targetposition)end = true;
	switch (prgmode) {
	case 2: //distance
		//do nothing
		break;
	case 3:
		if (end == true & bitRead(MEM_reg, 2) == true) COM_reg ^= (1 << 0); //direction return
		break;

	default: //stop stepper
		if (end == true & bitRead(COM_reg, 4) == true) {
			COM_reg &= ~(1 << 4);
			if (bitRead(MEM_reg, 2) == true) { //stop stepper only in wisselaandrijving mode 
				shiftbyte &= ~(15 << 4);
				COM_reg &= ~(1 << 1);
			}
		}
		break;
	}
}
void ledset() { //sets the leds to be shifted out
	shiftbyte &= ~(7 << 0); //clear bit 0~2
	shiftbyte |= (shiftled << 0);
}
void switchset() { //sets the switch pulses to be shifted
	switchcount++;
	if (switchcount > 2)switchcount = 0;
	shiftbyte &= ~(7 << 0); //clear bit 0~2
	//shiftbyte = 0;
	switch (switchcount) {
	case 0:
		shiftbyte |= (6 << 0);
		break;
	case 1:
		shiftbyte |= (5 << 0);
		break;
	case 2:
		shiftbyte |= (3 << 0);
		break;
	}
}
void SHIFT() {
	Shift1();
	PINB |= (1 << 3);
	PINB |= (1 << 3);
}
void switches() {
	//handles all manual switch events
	byte wait;
	if (status != bitRead(switchstatus, switchcount)) {
		if (status == false) { //switch pushed
			switchstatus &= ~(1 << switchcount); //klopt eigenlijk niet 
			switch (switchcount) {
			case 0: //primaire switch op pcb
				COM_reg |= (1 << 5);
				counter[5] = 0;

				switch (prgmode) {
				case 0:
					if (bitRead(MEM_reg, 1) == true) {
						COM_reg ^= (1 << 0); //toggle direction
					}
					else {
						COM_reg &= ~(1 << 0); //set clockwise
						switchstatus |= (1 << 2);
					}
					if (bitRead(COM_reg, 0) == false)COM_reg |= (1 << 4);
					COM_reg |= (1 << 1); //start stepper
					break;

				case 1: //change startup direction
					break;
				case 3:
					//stop stepper om tijdens snelheids meting de stepper te stoppen als je het programmode wil verlaten
					COM_reg &= ~(1 << 1);
					break;
				}
				break;
			case 1: //secundaire switch alleen extern aan te sluiten

				if (bitRead(COM_reg, 4) == true) {
					COM_reg |= (1 << 1); //start stepper  only if position switch is set
					COM_reg |= (1 << 0); //direction counter clockwise left
				}

				break;

			case 2: //position switch 
				if (prgmode == 3) { //no stop
					COM_reg ^= (1 << 0); //toggle direction
					//COM_reg &=~(1 << 0);
				}
				else {
					shiftbyte &= ~(15 << 4);
					COM_reg &= ~(1 << 1); //stop stepper					
				}

				Currentposition = 0;
				COM_reg |= (1 << 4); //stepper in 0 position
				break;
			}
		}
		else { //switch released
			switchstatus |= (1 << switchcount); //klopt niet eigenlijk, switchcount is decimal, switchstatus binair tot 3 kan dit
			switch (switchcount) {
			case 0: //
				if (prgmode > 0) {
					//debug(1);
					COM_reg &= ~(1 << 5);
					counter[5] = 0;
					COM_reg |= (1 << 6); //flag for ending program mode
				}

				//**********************begin progmode
				switch (prgmode) {
				case 1: //*****direction in start
					prgcom(0);
					break;
				case 2: //set distance
					prg2();
					break;
				case 3: //set Speed
					prg3();
					break;
				case 4: //knob mode
					prgcom(1);
					break;
				case 5: //mode
					prgcom(2);
					break;
				case 6: //DCCCadres
					COM_reg |= (1 << 3);
					break;
				case 7:
					prg7();
					break;
				}
				break;
				//****************** end start prg mode
			case 1: //release switch2 
				if (bitRead(MEM_reg, 1) == true){
				switchstatus |= (1 << 2);
				COM_reg |= (1 << 1); //start stepper  only if position switch is set
				COM_reg &= ~(1 << 0); //direction counter clockwise left
				}
				//)
				//if (COM_reg, 4 == false) {
				//Currentposition = 0;
			//}
				break;
			case 2: //release position switch 
				break;
			}
		}
	}
	else { //dus geen verandering en status is false dus knop ingedrukt gehouden
		if (status == false & switchcount == 0) {
			wait = 50;
			if (bitRead(COM_reg, 6) == false)wait = wait + (10 * prgmode); //increase wait time in higher programs
			if (counter[5] > wait) {
				//stop stepper always
				shiftbyte &= ~(15 << 4);
				COM_reg &= ~(1 << 1);

				if (bitRead(COM_reg, 6) == true) { //stop program mode
					prgEnd();
				}
				else {
					prgfase = 0;
					prgmode++;
					ledmode = 1;
					counter[5] = 0;
					COM_reg |= (1 << 7); //next prgmode flag
					counter[2] = 0;
					counter[3] = 0;
					if (prgmode > 7) prgEnd(); // stops programmode, no more programs.
				}
			}
		}
	}
	leddir();
	status = true;
}
void prg2() {
	switch (prgfase) {
	case 0: //start
		//stepper naar begin stand
		if (bitRead(COM_reg, 4) == false) {
			COM_reg &= ~(1 << 0); //richting
			COM_reg |= (1 << 1);
			//}
			//else {

		}
		prgfase++;
		break;
	case 1:
		COM_reg |= (1 << 0);
		COM_reg |= (1 << 1);
		speed = 50;
		prgfase++;
		break;
	case 2:
		//stop stepper, aantal steps opslaan?
		stopstep();
		Targetposition = Currentposition;
		speed = EEPROM.read(1);
		prgEnd();
		break;
	}
}
void prg3() { //speed
	//beweeg heem em weer coninue
	switch (prgfase) {
	case 0: //begin set speed. Stepper toggles between startposition and targetposition
		if (bitRead(COM_reg, 4) == false) { //set direction
			COM_reg &= ~(1 << 0);
		}
		else {
			COM_reg |= (1 << 0);
		}

		COM_reg |= (1 << 1); //start stepper
		shiftled &= ~(1 << 1); //kill green led
		prgfase++;
		break;
	case 1: //
		COM_reg |= (1 << 1); //start stepper, is stopt when button pressed.
		speedx();
		break;

	}
}
void prg4() {
	//knob mode

	switch (prgfase) {
	case 0:
		prgfase++;
		break;
	case 1:
		MEM_reg ^= (1 << 1);
		break;
	}
	shiftled &= ~(1 << 1);
	if (bitRead(MEM_reg, 1) == true)shiftled |= (1 << 1);
}
void prg7() { //factory reset
	switch (prgfase) {
	case 0:
		shiftled &= ~(1 << 1); //kill green led
		break;
	case 1:
		shiftled |= (1 << 1); //lite green led		
		break;
	case 2:
		ledmode = 5; //flash green led
		break;
	case 3:
		//clear memorie
		MEM_clear();
		prgEnd();
		break;
	}
	prgfase++;
}
void prgcom(byte mem) {
	switch (prgfase) {
	case 0:
		prgfase++;
		break;
	case 1:
		MEM_reg ^= (1 << mem);
		break;
	}
	shiftled &= ~(1 << 1);
	if (bitRead(MEM_reg, mem) == true)shiftled |= (1 << 1);
}
void prgEnd() {
	//ends, closes program mode
	ledmode = 20;
	counter[2] = 0;
	counter[3] = 0;
	counter[5] = 0;
	COM_reg &= ~(1 << 6);
	//switchstatus = 0x0;
	switchstatus |= (1 << 2); //reset position switch
	Currentposition = 0;
	COM_reg &= ~(1 << 0);
	COM_reg |= (1 << 1);
	prgmode = 0;
	prgfase = 0;
	MEM_change(); //store changes in EEPROM
}
void setTarget() {
	Targetposition = 600;
}
void Shift1() {
	//shift 1e byte
	for (byte i = 0; i < 8; i++) {
		PORTB &= ~(1 << 1); //clear ser
		if (bitRead(shiftbyte, i) == true) PORTB |= (1 << 1);
		PINB |= (1 << 2);
		PINB |= (1 << 2);
	}
}
void slowevents() {
	//called 1x in 255 loop cycles
	counter[0] ++;
	if (counter[0] > 65) {
		counter[0] = 0;
		switches();
	}
	if (counter[0] == 60) {
		switchset();
		SHIFT();
		ledset();
		status = bitRead(PINB, 4);
		SHIFT();
		//initial start
		if (bitRead(COM_reg, 2) == true) {
			if (counter[4] == 20) {
				counter[4] = 0;
				COM_reg &= ~(1 << 0);
				//starten
				COM_reg |= (1 << 1);
				COM_reg &= ~(1 << 2);
				switchstatus = 0xFF;
				leddir();
			}
		}
		else {
			if (counter[4] > 1) {
				counter[4] = 0;
				blink();
				if (bitRead(COM_reg, 5) == true) {
					//debug(1);
					counter[5]++;
				}
			}
		}
		counter[4]++;
	}
	stepcount++;
	if (stepcount > speed & bitRead(COM_reg, 1) == true) { //speed 2 = minimum
		stepcount = 0;
		steps(); //Steps=half stepper, step4=full stepper
		SHIFT();
	}
}
void blink() {
	//drives all kind of effects on control leds
	if (bitRead(COM_reg, 7) == true) {
		//overgang signaal
		counter[2]++;
		if (counter[2] == 5) {
			shiftled |= (1 << 1); //groene led
			shiftbyte |= (1 << 3); //rode led
		}
		else if (counter[2] > 10) {
			shiftled &= ~(1 << 1);
			shiftbyte &= ~(1 << 3);
			counter[2] = 0;
			//counter[3] = 0;
			COM_reg &= ~(1 << 7);
		}
	}
	else {
		//rode led op pin3 van de shiftregister
		switch (ledmode) {
		case 0:
			break;
		case 1: //program fase 1
			counter[2]++;
			if (counter[2] == 3)shiftbyte |= (1 << 3);
			if (counter[2] == 4) {
				shiftbyte &= ~(1 << 3);
				counter[3]++;
				if (counter[3] >= prgmode) {
					counter[2] = 20;
					counter[3] = 0;
				}
				else {
					counter[2] = 0;
				}
			}
			if (counter[2] > 30)counter[2] = 0;
			break;
		case 5:
			shiftled ^= (1 << 1);
			break;
		case 20:
			counter[2]++;
			if (counter[2] == 2) {
				shiftled |= (1 << 1); //groene led
				shiftbyte |= (1 << 3); //rode led
			}

			if (counter[2] == 20) {
				shiftled &= ~(1 << 1);
				shiftbyte &= ~(1 << 3);
				counter[2] = 0;
				//counter[3] = 0;
				ledmode = 0;
			}
			break;
		}

	}
}
void loop() {
	DEK_DCCh();
	GPIOR2++; //use general purpose register as slow events counter
	if (GPIOR2 == 0) slowevents();
}
