/*
 Name:		DutChie.ino
 Created:	9/5/2019 9:42:29 PM
 Author:	Rob Antonisse

 Attiny85 sketch for puzzel of the ruder of The Flying Dutchman room

 Attiny85 bootloader burn internal clock 8Mhz
 33

*/

//declarations
byte shiftbyte;
byte counter;
byte switchstatus;
byte encoderstatus;
byte testswitch;
byte COM_reg;
unsigned int slowtimer;
byte resetcount;

byte gameTurn[6] = { 3,1,4,3,4,2 };
byte gameFase;
byte turncount;
unsigned int candleTimer;
unsigned int CandleDelay;
byte candle;

/*
3 turns right, 1 turn left
4 turns right, 3 turns left
4 turns right, 2 turns left
*/

void setup() {
	delay(200);
	//setup ports 
	DDRB |= (1 << 3);
	DDRB |= (1 << 1);
	DDRB |= (1 << 2);
	DDRB |= (1 << 5);

	DDRB &= ~(1 << 4);
	DDRB &= ~(1 << 0); //pin5/PB0 as input TEST button
	PORTB |= (1 << 0); //internal pullup to pin 5 PB0
	PORTB |= (1 << 4);//pull-up to pb4   	 

	//shiftbyte = B00000001;
	resetGame();
}

void resetGame() {
	shiftbyte = B10000000;
	gameFase = 0;
	turncount = 0;
	COM_reg |= (1 << 3); //requested direction true (right)
}

void shift() {
	Shift1();
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

void slowevents() {
	byte status;
	byte sw;
	//Timer to delay lighting of candle after correct code, needed to hide exact sensor position
	candleTimer++;
	if (candleTimer == 0) candleTimer++;
	if (candleTimer == CandleDelay) {
		CandleDelay = 0;
		shiftbyte |= (1 << candle);
	}
	//reset game procedure to kill all kind of startup issues
	if (bitRead(COM_reg, 7) == false) {
		resetcount++;
		if (resetcount > 200) {
			COM_reg |= (1 << 7);
			resetGame();
		}
	}
	//check switch status TEST switch
	status = bitRead(PINB, 0);

	if (status != testswitch) {
		if (status == false) {
			testswitch = false;
			COM_reg ^= (1 << 0);
			test();
		}
		else {
			testswitch = true;
		}
	}
	//reedcontact lezen om en om 
	shiftbyte |= (B00000011 << 0);
	COM_reg ^= (1 << 1);
	if (bitRead(COM_reg, 1) == true) {
		sw = 0;
		shiftbyte &= ~(1 << 0);
	}
	else {
		shiftbyte &= ~(1 << 1);
		sw = 1;
	}
	shift();
	//test reeds
	status = bitRead(PINB, 4);
	if (status != bitRead(switchstatus, sw)) {
		if (status == false) {
			switchstatus &= ~(1 << sw);
			if (bitRead(COM_reg, 0) == true)shiftbyte |= (1 << sw + 4);
		}
		else {
			switchstatus |= (1 << sw);
			if (bitRead(COM_reg, 0) == true)shiftbyte &= ~(1 << sw + 4);
		}
		if (bitRead(COM_reg, 0) == false) encoder(sw, status);
	}
	shift();
}
void encoder(byte sw, boolean onoff) {
	if (onoff == false) { //een reed gesloten
		switch (encoderstatus) {
		case B11: //both open
			switch (switchstatus) {
			case B01:
				action(1);
				break;
			case B10:
				action(2);
				break;
			}
			break;

		case B01:
			action(3);
			break;
		case B10:
			action(4);
			break;
		}
	}
	else { //een reed open
		if (switchstatus == 3) { //both open
			//shiftbyte |= (1 << 6);
			switch (encoderstatus) {
			case B01:
				action(5);
				break;
			case B10:
				action(6);
				break;
			}
		}
	}
	encoderstatus = switchstatus;
}
void action(byte event) {
	/*
	1 R aangegaan
	2 L aangegaan
	3 R was aan, nu beide aan
	4 L was aan nu beide aan
	5 R gaat als laatste uit, rotatie L
	6 L gaat als laatste uit, rotatie R
	*/
	switch (event) {
	case 1:
		COM_reg |= (1 << 2);
		break;
	case 2:
		COM_reg &= ~(1 << 2);
		break;
	case 5:
		if (bitRead(COM_reg, 2) == false) {
			game(false);
		}
		break;
	case 6:
		if (bitRead(COM_reg, 2) == true) {
			game(true);
		}
		break;
	}
}
void game(boolean dir) {
	if (dir == bitRead(COM_reg, 3)) {
		turncount++;
		if (turncount == gameTurn[gameFase]) {
			COM_reg ^= (1 << 3); //toggle requested direction
			
			turncount = 0;
			gameFase++;
			if (bitRead(COM_reg, 3) == true)candles();
			if (gameFase > 5) gameSolved();
		}
	}
	else { //wrong direction
		resetGame();
	}
}
void candles() {
	switch (gameFase) {
	case 2:
		candle = 4;
		break;
	case 4:
		candle = 5;
		break;
	case 6:
		candle = 6;
		break;
	}
	CandleDelay = random(200, 1000);
	candleTimer = 100;
}
void gameSolved() {
	shiftbyte &= ~(1 << 7);
}
void test() {
	if (bitRead(COM_reg, 0) == true) {
		//start testmode
		//shiftbyte |= (B01110000 << 0);
		shiftbyte = B01110000;
	}
	else {
		//end testmode
		//shiftbyte &= ~(B01110000 << 0);
		resetGame();
	}
}
// the loop function runs over and over again until power down or reset
void loop() {
	slowtimer++;
	if (slowtimer == 1000) {
		slowevents();
		slowtimer = 0;
	}
}
