/*
 * CS120B Final Project Code.cpp
 *
 * Created: 3/9/2020 7:42:00 PM
 * Author : Richard Manzano
 */ 



#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdlib.h>
#include <util/delay.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "io.c"
#include "io.h"
#include "scheduler.h"
#include "timer.h"
#include "bit.h"



#define DS_PORT    PORTB
#define DS_PIN     0
#define ST_CP_PORT PORTB
#define ST_CP_PIN  1
#define SH_CP_PORT PORTB
#define SH_CP_PIN  2

#define DS_low()  DS_PORT&=~_BV(DS_PIN)
#define DS_high() DS_PORT|=_BV(DS_PIN)
#define ST_CP_low()  ST_CP_PORT&=~_BV(ST_CP_PIN)
#define ST_CP_high() ST_CP_PORT|=_BV(ST_CP_PIN)
#define SH_CP_low()  SH_CP_PORT&=~_BV(SH_CP_PIN)
#define SH_CP_high() SH_CP_PORT|=_BV(SH_CP_PIN)

//Define functions
//===============================================
void output_led_state(unsigned int __led_state);
//===============================================


void output_led_state(unsigned int __led_state)
{
	SH_CP_low();
	ST_CP_low();
	for (int i=0;i<16;i++)
	{
		if ((_BV(i) & __led_state) == _BV(i))  //bit_is_set doesn’t work on unsigned int so we do this instead
		DS_high();
		else
		DS_low();
		
		
		SH_CP_high();
		SH_CP_low();
	}
	ST_CP_high();
}

//EEPROM Functions
//===============================================
void saveScore(unsigned char score) {
	eeprom_write_byte(0, score);
}

unsigned char loadScore() {
	return eeprom_read_byte(0);
}

void saveScrollStack(unsigned int config){
	eeprom_write_byte(1, config);
}

unsigned char loadScrollStack() {
	return eeprom_read_byte(1);
}
//End EEPROM Functions
//===============================================


void ADC_init() {
	ADMUX=(1<<REFS0);
	ADCSRA=(1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
}


unsigned short ReadADC(unsigned char ch){
	ADC_init();
	//Select ADC Channel ch 0-7
	ch=ch&0b00000111;
	ADMUX|=ch;
	//Start conversion
	ADCSRA|=(1<<ADSC);
	//Waiting for conversion
	while(!(ADCSRA & (1<<ADIF)));
	ADCSRA|=(1<<ADIF);
	return(ADC);
}



int isLeft() {
	if(ReadADC(1) < 300) {
		return 1;
	}
	return 0;
}

int isRight() {
	if(ReadADC(1) > 800) {
		return 1;
	}
	return 0;
}

int isDown() {
	if(ReadADC(0) > 800) {
		return 1;
	}
	return 0;
}

int isUp() {
	if(ReadADC(0) < 300 ) {
		return 1;
	}
	return 0;
}

int currPlayerStackLED = 0b0011000011000111;
int scrollStackLED = 0b1100000011000011;

int shiftleft;
int shiftright;

int shiftleft_player;
int shiftright_player;

int currStreak;
int highestStreak;

int stackLvl = 1;

char LCD_Menu[32] = "This Streak:     Best Streak: ";



int isStacked(){
	//currplayer =      0b0011000011000111
	//scrolling stack = 0b1100000011000011
	
	//currplayer =      0b0000110011001111
	//scrolling stack = 0b0011000011000111
	
	//currplayer =      0b0000001111011111
	//scrolling stack = 0b0000110011001111
	int playerStackTemp = (currPlayerStackLED & 0x00FF);
	int scrollingStackTemp = (scrollStackLED & 0x00FF);
	playerStackTemp = playerStackTemp & scrollingStackTemp;
	
	if (playerStackTemp == scrollingStackTemp){
		return 1;
	}
	else {
		return 0;
	}
}
enum stackScroller{ Init, Left, Right, Stack } state;
int jscnt = 0; //for the delay in the joystick

void stack_scroller_tick(){ 
	switch(state){
		case Init:
			++jscnt;
			if(isLeft() && jscnt > 20){ 
				state = Left;
				break;
			}
			else if(isRight() && jscnt > 20){
				state = Right;
				break;
			}
			else if (((~PINA & 0x04) == 0x04) && (isStacked() == 1)){
				stackLvl++;
				currStreak++;
				highestStreak++;
				LCD_DisplayString(14, currStreak);
				state = Stack;
				break;
			}
			else if (((~PINA & 0x04) == 0x04) && (isStacked() == 0)){
				currStreak = 0;
				stackLvl = 1;
				state = Init;
				break;
			}
			else{
				state = Init;
			}
			break;
			
		case Left:
			jscnt++;
			if(currPlayerStackLED == 0b0011000011111000 || currPlayerStackLED == 0b0000110011111100 || currPlayerStackLED == 0b0000001111111110){
				state = Init;
				break;
			}
			else if(jscnt > 20){
				//0b00110000 11000111
				shiftleft_player = currPlayerStackLED & 0x00FF;
				shiftleft_player = shiftleft_player >> 1;
				shiftleft_player = shiftleft_player + 128;
				shiftleft_player = shiftleft_player & 0x00FF;
				if (stackLvl == 1) {
					currPlayerStackLED = shiftleft_player + 12288;
				}
				else if (stackLvl == 2){
					currPlayerStackLED = shiftleft_player + 3072;
				}
				else if (stackLvl == 3){
					currPlayerStackLED = shiftleft_player + 768;
				}
				output_led_state(currPlayerStackLED);
				jscnt = 0;
				state = Init;
				
			}
			break;
		
		case Right:
			jscnt++;
			if(currPlayerStackLED == 0b0011000000011111 || currPlayerStackLED == 0b0000110000111111 || currPlayerStackLED == 0b0000001101111111 ){
				state = Init;
				break;
			}
			else if(jscnt > 20){
				//0b00110000 11000111
				shiftright_player = currPlayerStackLED & 0x00FF;
				shiftright_player = shiftright_player << 1;
				shiftright_player = shiftright_player + 1;
				shiftright_player = shiftright_player & 0x00FF;
				if (stackLvl == 1) {
					currPlayerStackLED = shiftright_player + 12288;
				}
				else if (stackLvl == 2){
					currPlayerStackLED = shiftright_player + 3072;
				}
				else if (stackLvl == 3){
					currPlayerStackLED = shiftright_player + 768;
				}
				output_led_state(currPlayerStackLED);
				jscnt = 0;
				state = Init;
			}
			break;
		
		case Stack:
		
			++jscnt;
			//currplayer =      0b0011000011000111
			//scrolling stack = 0b1100000011000011
			int playerStackTemp_stacking1 = currPlayerStackLED & 0x00FF;
			
			//int scrollingStackTemp_stacking1 = scrollStackLED & 0x00FF;
			//int playerStackTemp_stacking2 = currPlayerStackLED & 0xFF00;
			//int scrollingStackTemp_stacking2 = scrollStackLED & 0xFF00;
			
			if(stackLvl == 1 && jscnt > 20){
				jscnt = 0;
				output_led_state(currPlayerStackLED);
				state = Init;
			}
			if(stackLvl == 2 && jscnt > 20){
				scrollStackLED = currPlayerStackLED | scrollStackLED;
				currPlayerStackLED = playerStackTemp_stacking1 + 3072;
				jscnt = 0;
				output_led_state(currPlayerStackLED);
				state = Init;
			}
			else if(stackLvl == 3 && jscnt > 20){
				scrollStackLED = currPlayerStackLED | scrollStackLED;
				currPlayerStackLED = playerStackTemp_stacking1 + 768;
				jscnt = 0;
				output_led_state(currPlayerStackLED);
				state = Init;
			}
			else if(stackLvl == 4 && jscnt > 20){
				scrollStackLED = currPlayerStackLED | scrollStackLED;
				currPlayerStackLED = playerStackTemp_stacking1 + 768;
				jscnt = 0;
				output_led_state(currPlayerStackLED);
				state = Init;
			}
			else {
				jscnt = 0;
				output_led_state(currPlayerStackLED);
				state = Init;
			}
			
			break;
		
		default:
			break;
	}
	
	switch(state){
		case Init:
			output_led_state(currPlayerStackLED);
			break;
		case Right:
		
			break;
		case Left:
		
			break;
	
		case Stack:
			output_led_state(currPlayerStackLED);
			break;
		default:
			break;
	}
}








enum scrollingStacker{Init2, LeftScroll, RightScroll}state2;

int escnt = 0;
int LR_flag = 0;

void scrolling_stack_tick(){
	switch(state2){
		case Init2:
		
			if (LR_flag == 0){
				state2 = RightScroll;
				break;
			}
			else if (LR_flag == 1){
				state2 = LeftScroll;
				break;
			}
			else {
				state2 = Init2;
				break;
			}
			
			break;
			
		case RightScroll:

			// || 0b0011000000011111  || 0b0000110000111111 
			if(scrollStackLED == 0b1100000000001111){
				state2 = Init2;
				LR_flag = 1;
				break;
			}
			
			escnt++;
			if(escnt > 20){
					//LEDs = 0b1100000011000011
					shiftleft = scrollStackLED & 0x00FF;
					shiftleft = shiftleft << 1;
					shiftleft = shiftleft + 1;
					shiftleft = shiftleft & 0x00FF;
					if (stackLvl == 1){
						scrollStackLED = shiftleft + 49152;
					}
					else if (stackLvl == 2){
						scrollStackLED = shiftleft + 12288;
					}
					else if(stackLvl == 3){
						scrollStackLED = shiftleft + 3072;
					}
					escnt = 0;	
				
				
		
			}
			break;
			
		case LeftScroll:
	
			//|| 0b0011000011111000 || 0b0000110011111100
			if(scrollStackLED == 0b1100000011110000){
				state2 = Init2;
				LR_flag = 0;
				break;
			}
			
			escnt++;
			if(escnt > 20){
				//0b11000000 00001111
				shiftright = scrollStackLED & 0x00FF;
				shiftright = shiftright >> 1;
				shiftright = shiftright + 128;
				shiftright = shiftright & 0x00FF;
				if (stackLvl == 1){
					scrollStackLED = shiftright + 49152;
				}
				else if (stackLvl == 2){
					scrollStackLED = shiftright + 12288;
				}
				else if(stackLvl == 3){
					scrollStackLED = shiftright + 3072;
				}
				escnt = 0;
				
			}
			break;
		
		default:
			break;	
	}
	switch(state2){
		case Init2:
			output_led_state(scrollStackLED);
			break;
		case LeftScroll:
			output_led_state(scrollStackLED);
			break;
		case RightScroll:
			output_led_state(scrollStackLED);
			break;
		default:
			break;
	}
}

void reset() {
	while(!((~PINA & 0x08) == 0x08)){}
	int currPlayerStackLED = 0b0010000011000111;
	int scrollStackLED = 0b1100000011000011;
}





int main (void)
{
	DDRA = 0x00; PORTA = 0xFF;
	DDRB = 0x07; PORTB = 0xF8; //Output for the LED matrix
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xFF; PORTD = 0x00; 
	
	ADC_init();
	 
	LCD_init();
	LCD_DisplayString(1, LCD_Menu);
	
	state = Init;
	state2 = Init2;
	
	unsigned long int Stack_Scroller_calc = 11;
	unsigned long int Scrolling_Stacker_calc = 10;
	
	//Calculating GCD
	unsigned long int GCD = 5;

	//Recalculate GCD periods for scheduler
	unsigned long int Stack_Scroller_period = Stack_Scroller_calc/GCD;
	unsigned long int Scrolling_Stacker_period = Scrolling_Stacker_calc/GCD;
	
	
	static task task1, task2;
	task *tasks[] = { &task1, &task2 };
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);
	
	// Task 1
	task1.state = -1;//Task initial state.
	task1.period = Stack_Scroller_period;//Task Period.
	task1.elapsedTime = Stack_Scroller_period;//Task current elapsed time.
	task1.TickFct = &stack_scroller_tick;//Function pointer for the tick.
	

	// Task 2
	task2.state = -1;//Task initial state.
	task2.period = Scrolling_Stacker_period;//Task Period.
	task2.elapsedTime = Scrolling_Stacker_period;//Task current elapsed time.
	task2.TickFct = &scrolling_stack_tick;//Function pointer for the tick.
	
	
	
	TimerSet(GCD);
	TimerOn();
	

	while(1)
	{
		
		for (unsigned short i = 0; i < numTasks; i++ ) {
			if ( tasks[i]->elapsedTime == tasks[i]->period ) {
				// Setting next state for task
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				// Reset the elapsed time for next tick.
				tasks[i]->elapsedTime = 0;
			}
			tasks[i]->elapsedTime += 1;
		}
		while(!TimerFlag);
		TimerFlag = 0;
		
	}

	return 0;
}
