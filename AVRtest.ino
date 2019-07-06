/*
    Name:       AVRtest.ino
    Created:	5-7-2019 23:01:40
    Author:     DESKTOP-UUF0FDC\gebruiker
*/

byte count;


void setup()
{
	DDRB |= (1 << 3);
	//pinMode(0, OUTPUT);
	TCCR1 |= (1 << 15);
	//TCCR1 |= (1 << CS11);
	//TCCR1 |= (1 << CS12);
	TIMSK |= (1 << 2);
}

ISR(TIMER1_OVF_vect) {
	count++;
	if(count==0x0) PINB |= (1 << 3);
}
void loop()
{

	//PINB |= (1 << 3);
	//delay(500);


	/*
	
	digitalWrite(0, HIGH);
	delay(500);
	digitalWrite(0, LOW);
	delay(100);
*/

}
