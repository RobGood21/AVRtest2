/*
	Name:       AVRtest.ino
	Created:	5-7-2019 23:01:40
	Author:     DESKTOP-UUF0FDC\gebruiker
*/


byte COM_reg;


volatile unsigned int count=0;
byte shiftbyte=B11111000;
byte switchcount;
byte switchstatus;
unsigned long Ctime;
byte stepfase;


void setup()
{
	DDRB |= (1 << 3); //portB3 as output
	DDRB |= (1 << 1);
	DDRB |= (1 << 2);
	DDRB &= ~(1 << 4); //port b4 input
	PORTB |= (1 << 4); //pull-up  to pb4 (pin3)


	//pinMode(0, OUTPUT);
	TCCR1 |= B00000110; //interrupt ~1ms
	//TCCR1 |= (1 << CS10);
	//TCCR1 |= (1 << CS12);
	TIMSK |= (1 << 2);
}

ISR(TIMER1_OVF_vect) {
	boolean status;
		dataout();
	SHIFT();
	//now check switches?
	status = bitRead(PINB, 4);	
	if ( status != bitRead(switchstatus, switchcount)) {

		if (status == false) { //switch pushed
			switchstatus &= ~(1 << switchcount);

			//payload...
			shiftbyte ^=(1 << switchcount+5);
			//if(switchcount==0) COM_reg ^= (1 << 0);
		}
		else { //switch released
			switchstatus |= (1 << switchcount);
		}
	}
	else { //dus geen verandering en status is false dus knop ingedrukt, hier de knopvasthoud programmering realiseren.

	}
}

void STEP() {
	byte temp;
	switch(stepfase) {
	case 0:
		temp = B10010000;
		break;
	case 1:
		temp = B01100000;
		break;
	case 2:
		temp = B01100000;
		break;
	case 3:
		temp = B10010000;
		break;
	}
	stepfase++;
	if (stepfase > 3)stepfase = 0;

	shiftbyte = shiftbyte << 5;
	shiftbyte = shiftbyte >> 5; //clear bits 4~7
	shiftbyte = shiftbyte + temp;
}


void dataout() {
	switchcount++;
	if (switchcount > 2)switchcount = 0;
	byte temp = shiftbyte;

	switch (switchcount) {
	case 0:
		temp = 6; //00000110
		break;
	case 1:
		temp = 5; //000000101
		break;
	case 2:
		temp = 3; //000000011
		break;
	}
	shiftbyte = shiftbyte >> 3;
	shiftbyte = shiftbyte << 3;
	shiftbyte = shiftbyte + temp;
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
	//latch shift register
	PINB |= (1 << 3);
	PINB |= (1 << 3);
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

void loop() {
	/*
	test om verstreken tijd naar werkelijke tijd te bepalen
*/

	

	if (millis() - Ctime > 1) {
		Ctime = millis();
		//dataout();
		//STEP();

	}



}
