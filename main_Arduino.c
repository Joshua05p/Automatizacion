/*
 * Automatizacion.c
 *
 * Created: 20/03/2026 20:20:54
 * Author : perez
 */ 

/*
 * Automatizacion.c
 */
#define F_CPU 16000000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <util/delay.h>
#define MAX_DUTY 1000
#define MIN_DUTY 0
#define FACTOR 250

#define LED_PIN PD7
#define LED2 PC0
#define LED3 PC1
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

volatile uint8_t actualizar_leds = 0;
volatile uint8_t led_actual = 0;

volatile uint8_t estado_anterior = 0xFF;

volatile uint16_t temperatura = 0;


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
	DDRC |= (1<<LED2)|(1<<LED3);

	// Interrupciones
	PCICR |= (1 << PCIE2);
	PCMSK2 |= (1<<PCINT18)|(1<<PCINT20)|(1<<PCINT21)|(1<<PCINT22);

	sei();
}

void enviar_byte(uint8_t byte){
	for(uint8_t i = 0; i < 8; i++){
		if(byte & 0x80){
			PORTD |= (1<<LED_PIN);
			asm volatile ("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
			PORTD &= ~(1<<LED_PIN);
			asm volatile ("nop\nnop\nnop\nnop\nnop\n");
			}else{
			PORTD |= (1<<LED_PIN);
			asm volatile ("nop\nnop\nnop\nnop\n");
			PORTD &= ~(1<<LED_PIN);
			asm volatile ("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
		}
		byte <<= 1;
	}
}

void enviar_color(uint8_t r, uint8_t g, uint8_t b){
	enviar_byte(g);
	enviar_byte(r);
	enviar_byte(b);
}

void mostrar_porcentaje(uint16_t duty){

	uint16_t valor = duty * NUM_LEDS;

	uint8_t leds_completos = valor / MAX_DUTY;
	uint8_t brillo_parcial = ((valor % MAX_DUTY) * 20) / MAX_DUTY; // 0–10

	cli();

	for(uint8_t i = 0; i < NUM_LEDS; i++){

		if(i < leds_completos){
			enviar_color(0, 10, 0); 
		}
		else if(i == leds_completos){
			enviar_color(0, brillo_parcial, 0); 
		}
		else{
			enviar_color(0, 0, 0);
		}
	}

	sei();

	for(uint16_t i=0;i<800;i++) asm volatile("nop");
}

// ?? TIMER1 ? PWM 1 y 2 (1kHz)
void confi_timer1(void){
	// Fast PWM, TOP = ICR1
	TCCR1A = (1<<COM1A1)|(1<<COM1B1)|(1<<WGM11);
	TCCR1B = (1<<WGM13)|(1<<WGM12)|(1<<CS11); // prescaler 8

	ICR1 = 666; // ?? ~1.5 kHz

	OCR1A = duty1;
	OCR1B = duty2;
}

// ?? TIMER2 ? PWM 3 (1kHz aprox)
void confi_timer2(void){
	// Fast PWM, TOP = OCR2A
	TCCR2A = (1<<COM2B1)|(1<<WGM21)|(1<<WGM20);
	TCCR2B = (1<<CS21) | (1<<CS20); // ?? prescaler 32

	OCR2A = 166; // ?? ~1.5 kHz

	// Escala correcta de 0–1000 ? 0–166
	OCR2B = (duty3 * OCR2A) / MAX_DUTY;
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

	
	USART_init(103);

	sei();

	while (1)
	{
		mostrar_porcentaje(duty1);
		if (actualizar_leds){
			mostrar_porcentaje(duty1);
			actualizar_leds = 0;
		}
	}
}

/****************************************/
// INTERRUPCIONES
// BOTONES

ISR(PCINT2_vect)
{
	uint8_t estado_actual = PIND;

	// detectar flanco de bajada
	uint8_t cambio = estado_anterior & (~estado_actual);

	// PWM1 (PD2)
	if (cambio & (1<<PIND2)){
		start2 ^= 1;
		actualizar_leds = 1;
		if (start2) PORTB |= (1<<PORTB0);
		else        PORTB &= ~(1<<PORTB0);
	}

	// PWM2 (PD4)
	if (cambio & (1<<PIND4)){
		start1 ^= 1;
		actualizar_leds = 1;
		if (start1) PORTB |= (1<<PORTB3);
		else        PORTB &= ~(1<<PORTB3);
	}

	// PWM3 (PD5)
	if (cambio & (1<<PIND5)){
		start3 ^= 1;

		if (start3) PORTB |= (1<<PORTB4);
		else        PORTB &= ~(1<<PORTB4);
	}

	// RESET (PD6)
	if (cambio & (1<<PIND6)){
		start1 = start2 = start3 = 0;
		duty1 = duty2 = duty3 = 0;
		temperatura = 0;
		actualizar_leds = 1;

		OCR1A = 0;
		OCR1B = 0;
		OCR2B = 0;

		PORTB &= ~((1<<PORTB0)|(1<<PORTB3)|(1<<PORTB4));
	}

	estado_anterior = estado_actual;
}
/****************************************/
// CURVA SUAVE

ISR(TIMER0_COMPA_vect)
{
	contador_ms++;

	if (contador_ms >= 20)
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
			temperatura++;
			if(temperatura >= 800){
				temperatura = 800;
			}
			duty2 += (MAX_DUTY - duty2)/FACTOR;
			if ((MAX_DUTY - duty2) < 2) duty2 = MAX_DUTY;
			OCR1B = duty2;
		}

		// PWM3
		if (start3){
			duty3 += (MAX_DUTY - duty3)/FACTOR;
			if ((MAX_DUTY - duty3) < 2) duty3 = MAX_DUTY;
			OCR2B = (duty3 * OCR2A) / MAX_DUTY;
		}
	}
	actualizar_leds = 1;
	USART_sendNumber(temperatura);
	USART_sendString("\n");
}
