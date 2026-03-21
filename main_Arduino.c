/*
 * Automatizacion.c
 *
 * Created: 20/03/2026 20:20:54
 * Author : perez
 */ 

/*
 * Automatizacion.c
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

/****************************************/
// Variables

volatile uint8_t iniciar_pwm = 0;
volatile uint16_t duty = 1000;  // valor inicial (?s aprox)
volatile uint16_t contador_ms = 0;

/****************************************/
// CONFIGURACIONES

void salidas(void) {
	// OC1A (PB1) como salida PWM
	DDRB |= (1 << DDB1);

	// Botón PD2
	DDRD &= ~(1 << DDD2);
	PORTD |= (1 << PORTD2);

	// Interrupción por cambio
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1 << PCINT18);

	sei();
}

// ?? TIMER1 ? PWM 50ms
void confi_timer1(void){
	// Fast PWM, TOP = ICR1
	TCCR1A = (1 << COM1A1) | (1 << WGM11);
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS12); // prescaler 256

	// Calculo:
	// 16MHz / 256 = 62500 Hz
	// 50ms ? 0.05 * 62500 = 3125
	ICR1 = 3125;

	OCR1A = duty; // duty inicial
}

void USART_init(unsigned int ubrr){
	UBRR0H = (ubrr >> 8);
	UBRR0L = ubrr;

	UCSR0B = (1 << TXEN0); // solo transmisión
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // 8 bits
}

void USART_sendChar(char c){
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = c;
}

void USART_sendString(char *str){
	while (*str){
		USART_sendChar(*str++);
	}
}

void USART_sendNumber(uint16_t num){
	char buffer[10];
	itoa(num, buffer, 10);
	USART_sendString(buffer);
}

// ?? TIMER0 ? base de tiempo (1ms)
void confi_timer0(void){
	// CTC
	TCCR0A = (1 << WGM01);
	TCCR0B = (1 << CS01) | (1 << CS00); // prescaler 64

	// 16MHz / 64 = 250kHz
	// 1ms ? 250 cuentas
	OCR0A = 249;

	TIMSK0 |= (1 << OCIE0A);
}

/****************************************/
// MAIN

int main(void)
{
	salidas();
	confi_timer1();
	confi_timer0();

	
	USART_init(103);

	sei();

	while (1)
	{
	}
}

/****************************************/
// INTERRUPCIONES

// BOTÓN
ISR(PCINT2_vect)
{
	if (!(PIND & (1 << PIND2)))
	{
		iniciar_pwm = 1;
		duty = 500;
		OCR1A = duty;

		USART_sendString("Inicio PWM\r\n");
	}
}

// TIMER0 cada 1 ms
ISR(TIMER0_COMPA_vect)
{
	if (iniciar_pwm)
	{
		contador_ms++;

		if (contador_ms >= 1000) // 1 segundo
		{
			contador_ms = 0;

			duty += 100;

			if (duty >= 2500)
			{
				duty = 2500;
				iniciar_pwm = 0;
			}

			OCR1A = duty;

			
			USART_sendString("Duty: ");
			USART_sendNumber(duty);
			USART_sendString("\r\n");
		}
	}
}
