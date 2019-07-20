/*
	Name:       AVRtest.ino
	Created:	5-7-2019 23:01:40
	Author:     DESKTOP-UUF0FDC\gebruiker

	diverse testen met Attiny85
	om dekoder te laten werken moet attiny op 16Mhz internal clock, branden met arduino IDE? VS schijnt dat niet te doen...
*/

#include <EEPROM.h>
//Declaraties Dekoder attiny85 begin ********
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
//end dekoder declarations**********************************

byte func; //0=wisselaandrijving, 1= continue, 2= multi postie
byte COM_reg;
byte MEM_reg; //register in eeprom
volatile unsigned int count = 0;
byte shiftbyte;
byte shiftled;//bit0~2
byte switchcount;
byte switchstatus;
unsigned long Ctime;
unsigned long Stime;

byte stepcount; //ff int

byte stepfase;
byte speed; //ff int

byte teller;
byte slowcount;
byte counter[6]; //diverse tellers
byte prgmode;
byte prgfase; //fase van programmaverloop
byte ledmode;
int Currentposition; //ook negatieve getallen mogelijk????
int Targetposition;



void setup()
{
	DDRB |= (1 << 3); //portB3 as output
	DDRB |= (1 << 1);
	DDRB |= (1 << 2);
	DDRB |= (1 << 5); //port5 as output


	DDRB &= ~(1 << 4); //port b4 input
	DDRB &= ~(1 << 0); //port b0 input
	PORTB |= (1 << 4); //pull-up  to pb4 (pin3)   	 


	//DeKoder Attiny85 part, interrupt on PIN5 (PB0)
	PCMSK |= (1 << 0); //pin change mask register bit 0 = PB0
	//DDRD &= ~(1 << 2);//bitClear(DDRD, 2); //pin2 input
	DEK_Tperiode = micros();
	MCUCR |= (1 << 0);//EICRA – External Interrupt Control Register A bit0 > 1 en bit1 > 0 (any change)
	//EICRA &= ~(1 << 1);	//bitClear(EICRA, 1);
	//EIMSK |= (1 << INT0);//bitSet(EIMSK, INT0);//EIMSK – External Interrupt Mask Register bit0 INT0 > 1
	GIMSK |= (1 << 5); //PCIE: Pin Change Interrupt Enable

	//start
	MEM_read();
	switchstatus = 7;
	COM_reg |= (1 << 2);
}

void MEM_read() {
	MEM_reg = EEPROM.read(0);

	speed = EEPROM.read(1);
	if (speed > 60) {
		speed = 10;
		EEPROM.update(1, speed);
	}
	/*

	EEPROM.get(2, Targetposition); //int 4 bytes
	if (Targetposition > 650) {
		Targetposition = 400;
		EEPROM.put(2, Targetposition);
	}
*/
	Targetposition = EEPROM.read(2);
	if (Targetposition > 35) {
		Targetposition = 20;
		EEPROM.update(2, Targetposition);
	}
	Targetposition = Targetposition * 10;
}
void MEM_change() {
	byte temp;
	EEPROM.update(0, MEM_reg);
	temp = Targetposition / 10;
	EEPROM.update(2, temp);

	//restore parameters
	MEM_read();
}


void switchinit() {
	//check status positie schakelaar

}

void leddir() {
	if (prgmode == 0 & ledmode == 0) {
		shiftled &= ~(3 << 0);
		if (bitRead(COM_reg, 0) == false) {
			shiftbyte |= (1 << 3);
			shiftled |= (1 << 0);
		}
		else {
			shiftbyte &= ~(1 << 3);
			shiftled |= (1 << 1);
		}
	}
}
void debug(byte type) {

	switch (type) {
	case 0: //count 255 led 3
		teller++;
		if (teller == 0) shiftbyte ^= (1 << 3);
		break;
	case 1: //toggle led 4
		shiftbyte ^= (1 << 3);
		break;
	}

}


//deKoder Attiny85 begin ********************************************************
ISR(PCINT0_vect) { //syntax voor een ISR
				   //isr van PIN2
	//DEK_Reg fase van bit ontvangst
	//bit 0= bitpart ok (1) of failed(0)
	//bit1= truepart 
	//bit2=falsepart
	//bit3= received bit true =true false =false
	//bit4=restart, begin, failed as true



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
	} //close for loop
} //close void
void DEK_BitRX() { //new version
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
	int decoder;
	int channel = 1;
	int adres;
	boolean port = false;
	boolean onoff = false;
	int cv;
	int value;

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
void COM_exe(boolean type, int decoder, int channel, boolean port, boolean onoff, int cv, int value) {

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

void APP_exe(boolean type, int adres, int decoder, int channel, boolean port, boolean onoff, int cv, int value) {
	if (adres < 5) {


		if (port == true) {
			shiftbyte |= (1 << 3);
			//PORTB |= (1 << 1);
		}
		else {
			shiftbyte &= ~(1 << 3);
			//PORTB &= ~(1 << 1);
		}

	}
}

void steps() {
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
	if (prgmode == 0)if (Currentposition == Targetposition) stopstep();
}
void step4() {
	//volgorde spoelen kan wisselen FULL stepper
	/*
	voor snellere bewegingen eens ijken of onder een bepaalde snelheid naar een 4 steps kan worden omgeschakeld.
	Nu is 2ms het snelst voor koppel
		*/

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
	if (prgmode == 0)if (Currentposition == Targetposition) stopstep();
}


void stopstep() {
	shiftbyte &= ~(15 << 4);
	COM_reg &= ~(1 << 1);
	COM_reg &= ~(1 << 4);
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
	/*
	aansluitingen SIPO SN74HC595
	SER ser data  attiny pin 6, PB1 > pin 14 SER
	SRCLK (shift command) attiny pin 7, PB2 > pin 11
	RCLK (latch command) attiny pin 2, PB3 > pin 12

	bit 3-7
	bit 0-2 via 3 schakelaars terugleiden naar portB4 deze als input, na shift port b4 lezen overeenkomend met welke uitgang
	van de shiftregister dan laag is. 3 schakelaars drukknoppen dus....
	*/
	Shift1();
	PINB |= (1 << 3);
	PINB |= (1 << 3);
}

void switches() {
	boolean status;
	status = bitRead(PINB, 4);
	if (status != bitRead(switchstatus, switchcount)) {
		if (status == false) { //switch pushed
			switchstatus &= ~(1 << switchcount); //klopt eigenlijk niet 
			switch (switchcount) {
			case 0: //primaire switch op pcb
				COM_reg |= (1 << 5);
				counter[5] = 0;

				switch (prgmode) {
				case 0:
					COM_reg ^= (1 << 0);
					COM_reg |= (1 << 1); //start stepper
					break;
				case 1: //change startup direction, dit kan straks voor alle prgmodes??
					COM_reg |= (1 << 3);
					break;
				}
				break;

			case 1: //secundaire switch alleen extern aan te sluiten
				COM_reg |= (1 << 0); //direction counter clockwise left
				if (bitRead(COM_reg, 4) == true) COM_reg |= (1 << 1); //start stepper  only if position switch is set
				break;
			case 2: //positie schakelaar, meerdere functies mogelijk, voorlopig alleen de init stop	
				//shiftbyte &= ~(15 << 4);	
				shiftbyte = 0;
				COM_reg &= ~(1 << 1);
				Currentposition = 0;
				COM_reg |= (1 << 4); //stepper in 0 position
				break;
			}
		}
		else { //switch released
			switchstatus |= (1 << switchcount); //klopt niet eigenlijk, switchcount is decimal, switchstatus binair tot 3 kan dit

			switch (switchcount) {
			case 0: //inS
				if (prgmode > 0) {
					//debug(1);
					COM_reg &= ~(1 << 5);
					counter[5] = 0;
					COM_reg |= (1 << 6); //flag for ending program mode
				}
				switch (prgmode) {
				case 1: //direction in start
					if (bitRead(COM_reg, 3) == true) {
						MEM_reg ^= (1 << 0);
						shiftled ^= (1 << 1);
					}
					else {
						if (bitRead(MEM_reg, 0) == true)shiftled |= (1 << 1);
					}
					break;

				case 2: //uitslag in mode 1 (WA)
					switch (prgfase) {
					case 0: //start
						//stepper naar begin stand
						if (bitRead(COM_reg, 4) == false) {
							COM_reg &= ~(1 << 0); //richting
							COM_reg |= (1 << 1);
						}
						else {
							prgstart();
							prgfase++;
						}
						break;
					case 1:
						//stop stepper, aantal steps opslaan?
						stopstep();
						Targetposition = Currentposition;
						prgEnd();
						break;
					}


					break;
				case 3:
					break;
				}
				break;
			case 1: //exS
				break;
			case 2: //position switch
				break;
			}
		}
	}
	else { //dus geen verandering en status is false dus knop ingedrukt gehouden
		if (status == false & switchcount == 0) {
			if (counter[5] > 40 + (prgmode * 2)) { //40 =periode van vasthouden tussen programma fases, duurt langer in hogere fases
				shiftled &= ~(3 << 0); //kill leds
				if (bitRead(COM_reg, 6) == true) { //stop program mode
					prgEnd();
				}
				else {
					prgmode++;
					ledmode = 1;
					counter[5] = 0;
					if (prgmode > 8)COM_reg &= ~(1 < 6); //next cycle stops program mode
					//clearcounters();
				}
			}
		}
	}
	leddir();
}
void prgEnd() {

	ledmode = 20;
	counter[2] = 0;
	counter[5] = 0;
	COM_reg &= ~(1 << 6);
	MEM_change(); //store changes in EEPROM

	switchstatus = 0x0;
	//switchstatus |=(1 << 2); //reset position switch
	Currentposition = 0;
	COM_reg &= ~(1 << 0);
	COM_reg |= (1 << 1);
	prgmode = 0;
	prgfase = 0;

}
void prgstart() { //start position meting
	COM_reg |= (1 << 0);
	COM_reg |= (1 << 1);
	speed = 40;
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
	//read switches, set leds
	counter[0] ++;
	if (counter[0] > 80) {
		counter[0] = 0;
		switchset();
		SHIFT();
		switches();
		ledset();
		SHIFT();
		//initial start
		if (bitRead(COM_reg, 2) == true) {
			counter[4]++;
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
		blink();
		if (bitRead(COM_reg, 5) == true) {
			//debug(1);
			counter[5]++;
		}
	}

	stepcount++;
	if (stepcount > speed & bitRead(COM_reg, 1) == true) { //speed 2 = minimum
		stepcount = 0;
		step4();
		SHIFT();
	}
}

void blink() {
	switch (ledmode) {
	case 0:
		break;

	case 1: //program fase 1
		counter[2]++;
		if (counter[2] == 3)shiftled |= (1 << 0);

		if (counter[2] == 4) {
			shiftled &= ~(1 << 0);
			counter[3]++;

			if (counter[3] >= prgmode) {
				counter[2] = 10;
				counter[3] = 0;
			}
			else {
				counter[2] = 0;
			}
		}
		if (counter[2] > 20)counter[2] = 0;
		break;

	case 20:
		counter[2]++;
		if (counter[2] == 2)shiftled |= (3 << 0);
		if (counter[2] == 20) {
			shiftled &= ~(7 << 0);
			counter[2] == 0;
			ledmode = 0;
		}
		break;
	}
}
void loop() {
	//SHIFT();
	slowcount++;
	DEK_DCCh();
	if (slowcount == 0)slowevents();
}
