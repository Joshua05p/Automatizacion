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
#include <util/delay.h>
#define MAX_DUTY 1000
#define MIN_DUTY 0
#define FACTOR 250
#define LED_PIN PD7
#define NUM_LEDS 8

/****************************************/
// Variables
volatile uint8_t start1 = 0;
volatile uint8_t start2 = 0;
volatile uint8_t start3 = 0;

volatile uint16_t duty1 = 0;
volatile uint16_t duty2 = 0;
volatile uint16_t duty3 = 0;

volatile uint16_t contador_ms = 0;


/****************************************/
// CONFIGURACIONES

void salidas(void){
	// PWM salidas
	DDRB |= (1 << DDB1) | (1 << DDB2); // OC1A, OC1B
	DDRD |= (1 << DDD3);              // OC2B

	// Botones (PD2, PD4, PD5, PD6)
	DDRD &= ~((1<<DDD2)|(1<<DDD4)|(1<<DDD5)|(1<<DDD6));
	PORTD |= (1<<PORTD2)|(1<<PORTD4)|(1<<PORTD5)|(1<<PORTD6);
	
	//Indicador Valvulas
	DDRB |= (1<<DDB0) | (1<<DDB3) | (1<<DDB4);
	
	DDRD |= (1<<DDD7);

	// Interrupciones
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1<<PCINT18)|(1<<PCINT20)|(1<<PCINT21)|(1<<PCINT22);

	sei();
}

void enviar_bit_1(){
	PORTD |= (1<<LED_PIN);
	_delay_us(0.8);
	PORTD &= ~(1<<LED_PIN);
	_delay_us(0.45);
}

void enviar_bit_0(){
	PORTD |= (1<<LED_PIN);
	_delay_us(0.4);
	PORTD &= ~(1<<LED_PIN);
	_delay_us(0.85);
}

void enviar_byte(uint8_t byte){
	for(uint8_t i=0;i<8;i++){
		if(byte & (1<<(7-i))){
			enviar_bit_1();
			}else{
			enviar_bit_0();
		}
	}
}

void enviar_color(uint8_t r, uint8_t g, uint8_t b){
	// WS2812 usa GRB
	enviar_byte(g);
	enviar_byte(r);
	enviar_byte(b);
}
void mostrar_barra(uint16_t duty){
	uint8_t leds_encendidos = (duty * NUM_LEDS) / MAX_DUTY;

	cli(); // timing crítico

	for(uint8_t i=0; i<NUM_LEDS; i++){
		if(i < leds_encendidos){
			enviar_color(0, 50, 0); // verde
			}else{
			enviar_color(0, 0, 0); // apagado
		}
	}

	sei();

	_delay_us(50); // reset latch
}

// ?? TIMER1 ? PWM 1 y 2 (1kHz)
void confi_timer1(void){
	TCCR1A = (1<<COM1A1)|(1<<COM1B1)|(1<<WGM11);
	TCCR1B = (1<<WGM13)|(1<<WGM12)|(1<<CS11); // prescaler 8

	ICR1 = 1000;

	OCR1A = duty1;
	OCR1B = duty2;
}

// ?? TIMER2 ? PWM 3 (1kHz aprox)
void confi_timer2(void){
	TCCR2A = (1<<COM2B1)|(1<<WGM21)|(1<<WGM20);
	TCCR2B = (1<<CS21); // prescaler 8

	OCR2A = 249; // ~1kHz
	OCR2B = duty3/4; // ajustar escala (0-255)
}

// ?? TIMER0 ? tiempo
void confi_timer0(void){
	TCCR0A = (1<<WGM01);
	TCCR0B = (1<<CS01)|(1<<CS00);

	OCR0A = 124; // 1ms
	TIMSK0 |= (1<<OCIE0A);
}

// USART (igual)
void USART_init(unsigned int ubrr){
	UBRR0H = (ubrr >> 8);
	UBRR0L = ubrr;

	UCSR0B = (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
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


/****************************************/
// MAIN

int main(void)
{
	salidas();
	confi_timer1();
	confi_timer2();
	confi_timer0();

	
	USART_init(51);

	sei();

	while (1)
	{
	}
}

/****************************************/
// INTERRUPCIONES
// BOTONES

ISR(PCINT2_vect)
{
	// PWM1
	if (!(PIND & (1<<PIND2))){
		start1 ^= 1;

		if (start1){
			PORTB |= (1<<PORTB0); // 🔥 LED1 ON
		}
	}

	// PWM2
	if (!(PIND & (1<<PIND4))){
		start2 ^= 1;

		if (start2){
			PORTB |= (1<<PORTB3); // 🔥 LED2 ON
		}
	}

	// PWM3
	if (!(PIND & (1<<PIND5))){
		start3 ^= 1;

		if (start3){
			PORTB |= (1<<PORTB4); // 🔥 LED3 ON
		}
	}

	// RESET
	if (!(PIND & (1<<PIND6))){
		start1 = start2 = start3 = 0;
		duty1 = duty2 = duty3 = 0;

		OCR1A = 0;
		OCR1B = 0;
		OCR2B = 0;

		// 🔥 APAGAR LEDs
		PORTB &= ~((1<<PORTB0)|(1<<PORTB3)|(1<<PORTB4));
	}
	mostrar_barra(duty1);
}
/****************************************/
// CURVA SUAVE

ISR(TIMER0_COMPA_vect)
{
	contador_ms++;

	if (contador_ms >= 250)
	{
		contador_ms = 0;

		// PWM1
		if (start1){
			duty1 += (MAX_DUTY - duty1)/FACTOR;
			if ((MAX_DUTY - duty1) < 2) duty1 = MAX_DUTY;
			OCR1A = duty1;
		}

		// PWM2
		if (start2){
			duty2 += (MAX_DUTY - duty2)/FACTOR;
			if ((MAX_DUTY - duty2) < 2) duty2 = MAX_DUTY;
			OCR1B = duty2;
		}

		// PWM3
		if (start3){
			duty3 += (MAX_DUTY - duty3)/FACTOR;
			if ((MAX_DUTY - duty3) < 2) duty3 = MAX_DUTY;
			OCR2B = duty3/4; // escala a 8 bits
		}
			USART_sendString("Duty: ");
			USART_sendNumber(duty1);
			USART_sendString("\r\n");
	}
}
