/*
	Name:       AVRtest.ino
	Created:	5-7-2019 23:01:40
	Author:     DESKTOP-UUF0FDC\gebruiker
*/





volatile unsigned int count=0;
byte shiftbyte;

unsigned long Ctime;


void setup()
{
	DDRB |= (1 << 3); //portB3 as output
	DDRB |= (1 << 1);
	DDRB |= (1 << 2);
	//pinMode(0, OUTPUT);
	TCCR1 |= B00000110; //interrupt ~1ms
	//TCCR1 |= (1 << CS10);
	//TCCR1 |= (1 << CS12);
	TIMSK |= (1 << 2);
}

ISR(TIMER1_OVF_vect) {



//		dataout();


	SHIFT();
}

void dataout() {
	shiftbyte = shiftbyte << 1;
	if (shiftbyte == 0) shiftbyte = 1;
}

void SHIFT() {
	/*
	aansluitingen SIPO SN74HC595
	SER ser data  attiny pin 6, PB1 > pin 14 SER
	SRCLK (shift command) attiny pin 7, PB2 > pin 11
	RCLK (latch command) attiny pin 2, PB3 > pin 12

	bit 0-4
	bit 5-7 via 3 schakelaars terugleiden naar portB4 deze als input, na shift port b4 lezen overeenkomend met welke uitgang
	van de shiftregister dan laag is. Twee schakelaars drukknoppen dus....


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
	

	if (millis() - Ctime > 1000) {
		Ctime = millis();
		dataout();
	}



}
