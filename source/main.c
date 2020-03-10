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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "io.c"
#include "io.h"
#include "scheduler.h"
#include "timer.h"
#include "bit.h"
#include "lcd_8bit_task.h"


//--------------EEPROM Functionality for final project--------------------
void saveScore(unsigned char score) {
	eeprom_write_byte(0, score);
}

unsigned char loadScore() {
	return eeprom_read_byte(0);
}
//--------------End EEPROM Code-------------------------------------------


//--------------Start PWM Functionality-----------------------------------

void set_PWM(double frequency) {
	// 0.954 hz is lowest frequency possible with this function,
	// based on settings in PWM_on()
	// Passing in 0 as the frequency will stop the speaker from generating sound
	
	static double current_frequency; // Keeps track of the currently set frequency
	// Will only update the registers when the frequency changes, otherwise allows
	// music to play uninterrupted.
	if (frequency != current_frequency) {
		if (!frequency) { TCCR3B &= 0x08; } //stops timer/counter
		else { TCCR3B |= 0x03; } // resumes/continues timer/counter
		
		// prevents OCR3A from overflowing, using prescaler 64
		// 0.954 is smallest frequency that will not result in overflow
		if (frequency < 0.954) { OCR3A = 0xFFFF; }
		
		// prevents OCR0A from underflowing, using prescaler 64					// 31250 is largest frequency that will not result in underflow
		else if (frequency > 31250) { OCR3A = 0x0000; }
		
		// set OCR3A based on desired frequency
		else { OCR3A = (short)(8000000 / (128 * frequency)) - 1; }

		TCNT0 = 0; // resets counter
		current_frequency = frequency; // Updates the current frequency
	}
}

void PWM_on() {
	TCCR3A = (1 << COM0A0);
	// COM3A0: Toggle PB3 on compare match between counter and OCR0A
	TCCR3B = (1 << WGM02) | (1 << CS01) | (1 << CS00);
	// WGM02: When counter (TCNT0) matches OCR0A, reset counter
	// CS01 & CS30: Set a prescaler of 64
	set_PWM(0);
}

void PWM_off() {
	TCCR0A = 0x00;
	TCCR3B = 0x00;
}

//---------------------End PWM Functionality------------------------------



//--------------Joystick Reference----------------------------------------
void ADC_init(){
	ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE);
}


unsigned short ReadADC(unsigned char ch){

	ch = ch & 0x07;

	ADMUX = (ADMUX & 0xF8) | ch;

	ADCSRA |= (1<<ADSC);

	while(!(ADCSRA & (1<<ADIF)));
	//        ADCSRA |= (1<<ADIF);	
	return(ADC);

}

unsigned char _left = 0;
unsigned char _right = 0;
unsigned char _up = 0;
unsigned char _down = 0;
unsigned char led = 0x00;

//#define x ReadADC(0)

//#define y ReadADC(1)

unsigned short x;
unsigned short y;

void js(){

	x = ReadADC(0);
	y = ReadADC(1);

	if(x > 900){
		led = 0x08;
		_left = 1; //left
	}
	else if(x < 300){
		led = 0x04; //right
		_right = 1;
	}
	else if(y < 300){
		led = 0x02; //up
		_up = 1;
	}
	else if(y > 900 ){
		led = 0x01; //down
		_down = 1;
	}
	else{
		led = 0x00;
		_left = 0;
		_right = 0;
		_down = 0;
		_up = 0;
	}
}

//--------------End Joystick Reference----------------------



//--------------Start Global shared variables---------------

unsigned char playFlag = 0;
unsigned char gameOverFlag = 0;
unsigned char score = 0;
unsigned char lives = 0;
unsigned char key = ' '; // key pressed
unsigned char terrainShift = 0; // how much the terrain should be moved by


//--------------End Global shared variables



enum collision_detection_states {COLLISION_INIT, COLLISION_PLAYING};

int collision_tick(int state) {
	switch(state) {
		case COLLISION_INIT:
			if (playFlag == 1) {
				state = COLLISION_PLAYING;
			}
			else {
				state = state;
			}
			break;
		case COLLISION_PLAYING:
			if (playFlag == 0) {
				state = COLLISION_INIT;
			}
			else {
				state = state;
			}
			break;
	}
	
	switch(state) {
		case COLLISION_INIT:
			break;
		
		case COLLISION_PLAYING:
		// check if player shot an enemy, inc score, remove projectile, remove enemy
			for (unsigned char i = 0; i < 5; i++) {
				if (player_one_projectile_location[i] != 255) {
					for (unsigned char j = 0; j < 3; j++) {
						if (enemy_location[j] != 255) {
							if (player_one_projectile_location[i] == enemy_location[j]) {
								set_PWM(PROJECTILEHITENEMYSOUND);
								score++;
								player_one_projectile_location[i] = 255;
								enemy_location[j] = 255;
							}
						
						}
					}
				}
			}
		
			// terrain collision detection
			static unsigned char terrainCollisionCounter = 0;
			for (unsigned char i = 0; i < 16; i++) {
				unsigned char terrain_location = i + 16;
				unsigned char terrain_sprite = terrain[(i + terrainShift) % 32];
			
				if ((terrain_sprite != ' ') && (terrain_location == player_one_location)) {
					terrainCollisionCounter++;
					// make sure we only count lives lost once
					if (terrainCollisionCounter < 2) {
						if (lives > 1) {
							set_PWM(PLAYERHITTERRAINSOUND);
							lives--;
							} else {
							gameOverFlag = 1;
						}
						} else {
						terrainCollisionCounter = 0;
					}
				}
			
			}
		
			// enemy collision detection
			static unsigned char enemyCollisionCounter = 0;
			for (unsigned char i = 0; i < 3; i++) {
				if (player_one_location == enemy_location[i]) {
					// make sure we only count lives lost once
					if (enemyCollisionCounter < 2) {
						if (lives > 1) {
							set_PWM(PLAYERHITENEMYSOUND);
							lives--;
							} else {
							gameOverFlag = 1;
						}
						} else {
						enemyCollisionCounter = 0;
					}
				}
			}
			break;
	}
	return state;
}



//Start LCD EEPROM function 

enum lcd_display_tick_states {DISPLAY_START_MESSAGE, DISPLAY_STREAKOVER_MESSAGE, DISPLAY_STREAK};
int lcd_display_tick(int state) {
	static unsigned char tempScore = 0;
	static unsigned char hundreds = 0;
	static unsigned char tens = 0;
	static unsigned char ones = 0;
	
	static const unsigned char startMessage[32] = {
		'P', 'r', 'e', 's', 's', ' ', '1', ' ', 't', 'o', ' ', 'S', 't', 'a', 'r', 't',
		'P', 'r', 'e', 's', 's', ' ', '2', ' ', 't', 'o', ' ', 'R', 'e', 's', 'e', 't'
	}; // 16 x 2 characters
	
	static unsigned char scoreMessage[32] = {
		'S', 't', 'r', 'e', 'a', 'k', ':', '', '', '', '', '', ' ', ' ', ' ', '',
		'P', 'r', 'e', 's', 's', ' ', '2', ' ', 't', 'o', ' ', 'R', 'e', 's', 'e', 't'
	}; // 16 x 2 characters
	
	static unsigned char screenBuffer[32] = {
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
		' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '
	}; // 16 x 2 characters
	
	switch(state) {
		case DISPLAY_START_MESSAGE:
			if (playFlag == 1) {
				state = DISPLAY_STREAK;
			}
			else {
				state = state;
			}
			break;
		
		case DISPLAY_STREAKOVER_MESSAGE:
			if ((gameOverFlag == 0) && (playFlag == 0)) {
				state = DISPLAY_START_MESSAGE;
			}
			if (playFlag == 1) {
				state = DISPLAY_STREAK;
			}
			else {
				state = state;
			}
			break;
		
		case DISPLAY_STREAK:
			if (gameOverFlag == 1) {
				LCD_ClearScreen();
				state = DISPLAY_STREAKOVER_MESSAGE;
			}
			else if (playFlag == 0) {
				state = DISPLAY_START_MESSAGE;
			}
			else {
				state = state;
			}
			break;
	}
	
	switch(state) {
		case DISPLAY_START_MESSAGE:
		
			for (unsigned char i = 0; i < 32; i++) {
				LCD_Cursor(i + 1);
				LCD_WriteData(startMessage[i]);
				LCD_Cursor(0);
			}
			
			break;
		
		case DISPLAY_STREAKOVER_MESSAGE:
		
			for (unsigned char i = 0; i < 32; i++) {
				LCD_Cursor(i + 1);
				LCD_WriteData(scoreMessage[i]);
				LCD_Cursor(0);
			}
			set_PWM();
			break;
		
		case DISPLAY_STREAK:
		
		
			for (unsigned char i = 0; i < 32; i++) {
				LCD_Cursor(i + 1);
			
				
			
				LCD_WriteData(screenBuffer[i]);
				LCD_Cursor(0);
			}
		
			displayLives(lives);
		
			break;
	}
	
	return state;
}




int main() {
	// Set Data Direction Registers
	DDRA = 0xFF; PORTA = 0x00; //LED matrix output
	DDRB = 0x00; PORTB = 0xFF; //Joystick, reset, and start input 
	DDRC = 0xFF; PORTC = 0x00; //LCD display output
	DDRD = 0xFF; PORTD = 0x00; //LCD display output

	// Period for the tasks
	/*
	unsigned long int key_tick_calc = 200;
	unsigned long int display_tick_calc = 100;
	unsigned long int game_start_tick_calc = 100;
	unsigned long int player_one_tick_calc = 200;
	unsigned long int player_one_projectile_tick_calc = 200;
	unsigned long int terrain_tick_calc = 400;
	unsigned long int enemy_tick_calc = 400;
	unsigned long int collision_tick_calc = 200;
	*/

	//Calculating GCD
	/*
	unsigned long int tmpGCD = 1;
	tmpGCD = findGCD(key_tick_calc, display_tick_calc);
	tmpGCD = findGCD(tmpGCD, game_start_tick_calc);
	tmpGCD = findGCD(tmpGCD, player_one_tick_calc);
	tmpGCD = findGCD(tmpGCD, player_one_projectile_tick_calc);
	tmpGCD = findGCD(tmpGCD, terrain_tick_calc);
	tmpGCD = findGCD(tmpGCD, enemy_tick_calc);
	tmpGCD = findGCD(tmpGCD, collision_tick_calc);
	*/

	//Greatest common divisor for all tasks or smallest time unit for tasks.
	unsigned long int GCD = tmpGCD;

	//Recalculate GCD periods for scheduler
	/*
	unsigned long int key_tick_period = key_tick_calc/GCD;
	unsigned long int display_tick_period = display_tick_calc/GCD;
	unsigned long int game_start_tick_period = game_start_tick_calc/GCD;
	unsigned long int player_one_tick_period = player_one_tick_calc/GCD;
	unsigned long int player_one_projectile_tick_period = player_one_projectile_tick_calc/GCD;
	unsigned long int terrain_tick_period = terrain_tick_calc/GCD;
	unsigned long int enemy_tick_period = enemy_tick_calc/GCD;
	unsigned long int collision_tick_period = collision_tick_calc/GCD;
	*/

	//Declare an array of tasks
	static task task1, task2, task3, task4, task5, task6, task7, task8;
	task *tasks[] = { &task1, &task2, &task3, &task4, &task5, &task6, &task7, &task8};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);

	// Task 1
	task1.state = FETCHKEY;//Task initial state.
	task1.period = key_tick_period;//Task Period.
	task1.elapsedTime = 0;//Task current elapsed time.
	task1.TickFct = &key_tick;//Function pointer for the tick.


	// Set the timer and turn it on
	TimerSet(GCD);
	TimerOn();

	// initialize LCD and sprites
	LCD_init();
	LCD_Cursor(0);

	unsigned short i; 
	while(1) {
		for ( i = 0; i < numTasks; i++ ) {
			if ( tasks[i]->elapsedTime == tasks[i]->period ) {
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				tasks[i]->elapsedTime = 0;
			}
			tasks[i]->elapsedTime += 1;
		}
		while(!TimerFlag);
		TimerFlag = 0;
	}

	return 0;
}


