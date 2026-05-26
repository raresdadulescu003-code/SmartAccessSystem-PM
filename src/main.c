/*
  Smart Access System 
*/

#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

#define SERVO_CLOSED  2900   
#define SERVO_OPEN    4000    

#define LED_ON()       (PORTC |=  (1 << PC2))
#define LED_OFF()      (PORTC &= ~(1 << PC2))
#define RC_CS_LOW()    (PORTB &= ~(1 << PB2))
#define RC_CS_HIGH()   (PORTB |=  (1 << PB2))

volatile uint8_t exitRequested = 0;
uint8_t masterUID[4];

void int0Init(void) {
    DDRD  &= ~(1 << PD2);    /* PD2 intrare                */
    PORTD |=  (1 << PD2);    /* pull-up intern             */
    EICRA |=  (1 << ISC01);  /* front cazator: ISC01=1     */
    EICRA &= ~(1 << ISC00);  /*                ISC00=0     */
    EIMSK |=  (1 << INT0);   /* activeaza INT0             */
}

ISR(INT0_vect) {
    exitRequested = 1;        /* ISR scurt: doar flag       */
}

/*  LAB 2a: TIMER1 - PWM servo pe OC1A | Registri: TCCR1A, TCCR1B, ICR1, OCR1A  */
void servoInit(void) {
    DDRB |= (1 << PB1);
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13)  | (1 << WGM12) | (1 << CS11);
    ICR1   = 39999;
    OCR1A  = SERVO_CLOSED;
}

void servoWrite(uint16_t ticks) {
    OCR1A = ticks;
}

/* LAB 2b: TIMER2 - PWM buzzer pe OC2B  | Registri: TCCR2A, TCCR2B, OCR2A, OCR2B  */
void buzzerInit(void) {
    DDRD |= (1 << PD3);
}

void buzzerTone(uint16_t freq) {
    uint16_t top = (uint16_t)((F_CPU / (256UL * freq)) - 1);
    if (top > 255) top = 255;
    OCR2A  = (uint8_t)top;
    OCR2B  = (uint8_t)(top / 2);
    TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);
    TCCR2B = (1 << WGM22)  | (1 << CS22)  | (1 << CS21);
}

void buzzerOff(void) {
    TCCR2A = 0;
    TCCR2B = 0;
    PORTD &= ~(1 << PD3);
}

void usartInit(void) {
    UBRR0H = 0;
    UBRR0L = 103;
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void usartTx(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

void usartPrint(const char* s) {
    while (*s) usartTx(*s++);
}

void usartHex(uint8_t b) {
    const char* h = "0123456789ABCDEF";
    usartTx(h[b >> 4]);
    usartTx(h[b & 0x0F]);
}

void eepromWrite(uint16_t addr, uint8_t data) {
    while (EECR & (1 << EEPE));
    uint8_t sreg = SREG;
    cli();
    EEAR = addr;
    EEDR = data;
    EECR |= (1 << EEMPE);
    EECR |= (1 << EEPE);
    SREG  = sreg;
}

uint8_t eepromRead(uint16_t addr) {
    while (EECR & (1 << EEPE));
    EEAR = addr;
    EECR |= (1 << EERE);
    return EEDR;
}

void loadMaster(void) {
    if (eepromRead(0) == 0xA5) {
        uint8_t i;
        for (i = 0; i < 4; i++)
            masterUID[i] = eepromRead(1 + i);
    } else {
        uint8_t i;
        uint8_t def[4] = {0xDE, 0xAD, 0xBE, 0xEF};
        for (i = 0; i < 4; i++) {
            masterUID[i] = def[i];
            eepromWrite(1 + i, def[i]);
        }
        eepromWrite(0, 0xA5);
    }
}

void saveMaster(uint8_t* uid) {
    uint8_t i;
    for (i = 0; i < 4; i++) {
        masterUID[i] = uid[i];
        eepromWrite(1 + i, uid[i]);
    }
    eepromWrite(0, 0xA5);
}

void spiInit(void) {
    DDRB |=  (1 << PB2) | (1 << PB3) | (1 << PB5);
    DDRB &= ~(1 << PB4);
    PORTB |= (1 << PB2);
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
}

uint8_t spiTransfer(uint8_t d) {
    SPDR = d;
    while (!(SPSR & (1 << SPIF)));
    return SPDR;
}

void rcWrite(uint8_t reg, uint8_t val) {
    RC_CS_LOW();
    spiTransfer((reg << 1) & 0x7E);
    spiTransfer(val);
    RC_CS_HIGH();
}

uint8_t rcRead(uint8_t reg) {
    RC_CS_LOW();
    spiTransfer(((reg << 1) & 0x7E) | 0x80);
    uint8_t v = spiTransfer(0x00);
    RC_CS_HIGH();
    return v;
}

void rcSetBit(uint8_t reg, uint8_t m) { rcWrite(reg, rcRead(reg) | m);  }
void rcClrBit(uint8_t reg, uint8_t m) { rcWrite(reg, rcRead(reg) & ~m); }

void rc522Init(void) {
    DDRD |= (1 << PD4);
    PORTD &= ~(1 << PD4); _delay_ms(2);
    PORTD |=  (1 << PD4); _delay_ms(50);
    rcWrite(0x01, 0x0F);   _delay_ms(50);  
    rcWrite(0x2A, 0x80);                  
    rcWrite(0x2B, 0xA9);                  
    rcWrite(0x2C, 0x03);                  
    rcWrite(0x2D, 0xE8);                  
    rcWrite(0x15, 0x40);                  
    rcWrite(0x11, 0x3D);                 
    rcSetBit(0x14, 0x03);                 
}

uint8_t rcTransceive(uint8_t* tx, uint8_t txLen,
                     uint8_t* rx, uint8_t rxMax,
                     uint8_t lastBits) {
    uint8_t i, n;
    uint16_t guard;
    uint8_t irq;

    rcWrite(0x01, 0x00);
    rcWrite(0x04, 0x7F);
    rcWrite(0x0A, 0x80);
    for (i = 0; i < txLen; i++) rcWrite(0x09, tx[i]);
    rcWrite(0x0D, lastBits);
    rcWrite(0x01, 0x0C);
    rcSetBit(0x0D, 0x80);

    guard = 2000;
    do {
        irq = rcRead(0x04);
        guard--;
    } while (guard && !(irq & 0x30) && !(irq & 0x01));

    rcClrBit(0x0D, 0x80);
    if (!guard) return 0;
    if (rcRead(0x06) & 0x13) return 0;

    n = rcRead(0x0A);
    if (n > rxMax) n = rxMax;
    for (i = 0; i < n; i++) rx[i] = rcRead(0x09);
    return n;
}

uint8_t rcReadCard(uint8_t* uid4) {
    uint8_t atqa[2];
    uint8_t reqa  = 0x26;
    uint8_t uid5[5];
    uint8_t cl[2] = {0x93, 0x20};
    uint8_t i;

    if (rcTransceive(&reqa, 1, atqa, 2, 0x07) != 2) return 0;
    if (rcTransceive(cl, 2, uid5, 5, 0x00) != 5)    return 0;
    if ((uid5[0] ^ uid5[1] ^ uid5[2] ^ uid5[3]) != uid5[4]) return 0;

    for (i = 0; i < 4; i++) uid4[i] = uid5[i];
    return 1;
}

void lcdPulseE(void) {
    PORTD |=  (1 << PD6); _delay_us(1);
    PORTD &= ~(1 << PD6); _delay_us(50);
}

void lcdNibble(uint8_t n) {
    if (n & 0x01) PORTD |= (1<<PD7); else PORTD &= ~(1<<PD7);
    if (n & 0x02) PORTB |= (1<<PB0); else PORTB &= ~(1<<PB0);
    if (n & 0x04) PORTC |= (1<<PC0); else PORTC &= ~(1<<PC0);
    if (n & 0x08) PORTC |= (1<<PC1); else PORTC &= ~(1<<PC1);
    lcdPulseE();
}

void lcdByte(uint8_t b, uint8_t isData) {
    if (isData) PORTD |=  (1 << PD5);
    else        PORTD &= ~(1 << PD5);
    lcdNibble(b >> 4);
    lcdNibble(b & 0x0F);
    _delay_us(50);
}

#define lcdCmd(c)  lcdByte(c, 0)
#define lcdChar(c) lcdByte(c, 1)

void lcdInit(void) {
    DDRD |= (1<<PD5) | (1<<PD6) | (1<<PD7);
    DDRB |= (1<<PB0);
    DDRC |= (1<<PC0) | (1<<PC1);
    PORTD &= ~(1 << PD5);

    _delay_ms(50);
    lcdNibble(0x03); _delay_ms(5);
    lcdNibble(0x03); _delay_us(150);
    lcdNibble(0x03); _delay_us(150);
    lcdNibble(0x02); _delay_us(150);

    lcdCmd(0x28); _delay_us(50);   
    lcdCmd(0x0C); _delay_us(50);   
    lcdCmd(0x01); _delay_ms(2);    
    lcdCmd(0x06); _delay_us(50);   
}

void lcdClear(void) {
    lcdCmd(0x01);
    _delay_ms(2);
}

void lcdSetCursor(uint8_t col, uint8_t row) {
    uint8_t addr = col + (row == 0 ? 0x00 : 0x40);
    lcdCmd(0x80 | addr);
    _delay_us(50);
}

void lcdPrint(const char* s) {
    while (*s) { lcdChar((uint8_t)*s++); _delay_us(50); }
}

void lockSystem(void) {
    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("Sistem Blocat");
    LED_OFF();
    servoWrite(SERVO_CLOSED);
}

void grantAccess(void) {
    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("Acces Aprobat");
    LED_ON();
    buzzerTone(1500); _delay_ms(120);
    buzzerTone(2200); _delay_ms(150);
    buzzerOff();
    servoWrite(SERVO_OPEN);
    _delay_ms(3000);
    servoWrite(SERVO_CLOSED);
    lockSystem();
}

void denyAccess(void) {
    uint8_t i;
    lcdClear();
    lcdSetCursor(0, 0);
    lcdPrint("Card Invalid!");
    buzzerTone(250);
    for (i = 0; i < 5; i++) {
        LED_ON();  _delay_ms(150);
        LED_OFF(); _delay_ms(150);
    }
    buzzerOff();
    lockSystem();
}

uint8_t isMaster(uint8_t* uid) {
    uint8_t i;
    for (i = 0; i < 4; i++)
        if (uid[i] != masterUID[i]) return 0;
    return 1;
}


int main(void) {
    uint8_t uid[4];

    /* LED iesire */
    DDRC |= (1 << PC2);

    /* Init periferice */
    lcdInit();
    usartInit();
    int0Init();
    servoInit();
    buzzerInit();
    spiInit();
    rc522Init();
    loadMaster();
    sei();

    usartPrint("VersionReg=0x");
    usartHex(rcRead(0x37));
    usartPrint("\r\n");

    lockSystem();

    while (1) {
        if (exitRequested) {
	        exitRequested = 0;
	        EIMSK &= ~(1 << INT0);
	        _delay_ms(50);

	        /* Numara cat timp e tinut butonul */
	        uint16_t held = 0;
	        while (!(PIND & (1 << PD2)) && held < 30) {
		        _delay_ms(100);
		        held++;
	        }

	        if (held >= 30) {
		        /* Apasare lunga: inrolare */
		        uint8_t euid[4];
		        lcdClear();
		        lcdSetCursor(0, 0);
		        lcdPrint("Mod inrolare...");
		        usartPrint("Apropie cardul..\r\n");
		        while (!rcReadCard(euid)) { }
				saveMaster(euid);
				buzzerTone(1800); _delay_ms(300); buzzerOff();
				usartPrint("Card salvat in EEPROM!\r\n");
				lockSystem();
		     } 
			 else {
		     /* Apasare scurta: deschide */
		     grantAccess();
			 }

	        while (!(PIND & (1 << PD2)));
	        _delay_ms(100);
	        EIFR  = (1 << INTF0);
	        exitRequested = 0;
	        EIMSK |= (1 << INT0);
	        continue;
        }

        /* Card RFID */
        if (rcReadCard(uid)) {
            uint8_t i;
            usartPrint("UID:");
            for (i = 0; i < 4; i++) {
                usartTx(' ');
                usartHex(uid[i]);
            }
            usartPrint("\r\n");

            if (isMaster(uid)) grantAccess();
            else               denyAccess();

            _delay_ms(1000);
        }
    }

    return 0;
}
