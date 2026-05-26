# Smart Access System (PM)

Sistem inteligent de control acces cu RFID, realizat pe ATmega328P (Xplained Mini), scris integral în C bare-metal, fără biblioteci externe.

## Ce face

- Citește carduri RFID prin **SPI** (modul RC522)
- La card valid: deschide bariera (servo), aprinde LED-ul, sunet de succes, mesaj "Acces Aprobat" pe LCD
- La card invalid: LED clipește, sunet grav, mesaj "Card Invalid!"
- Buton de ieșire din interior (pe întrerupere externă INT0)
- Înrolare carduri direct de pe dispozitiv (apăsare lungă pe buton), cu salvare în **EEPROM**

## Laboratoare acoperite

| Lab | Funcționalitate | Implementare |
|-----|-----------------|--------------|
| 1 | Întreruperi externe | INT0 pe buton (PD2) |
| 2 | Timere & PWM hardware | Timer1 servo, Timer2 buzzer |
| 3 | SPI | Comunicație cu RC522 |
| 4 | USART | Debug în Serial Monitor |
| 5 | EEPROM | Salvare UID card master |

## Structură

- `src/main.c` — codul sursă complet
- `hardware/` — schema electrică și diagrame
- `images/` — poze cu proiectul

## Hardware

- ATmega328P Xplained Mini
- Modul RFID RC522 (alimentat la **3.3V**)
- LCD 1602 paralel (HD44780, mod 4-bit)
- Servomotor SG90
- Buzzer pasiv, LED, buton tactil

## Mediu de dezvoltare

Microchip Studio 7 (avr-gcc), programare prin mEDBG / ISP.
