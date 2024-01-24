#pragma once

namespace Geiger {
	
	const auto VoltageMeasPin    = 22;
	const auto CurrentMeasPin    = 23;
	const auto VoltageControlPin = 11;  // PB3
	const auto ReproPin          =  8;  // PB0
	const auto GeigerCounterPin  = 21;  // PC7
	
	
	// Prepare geigercounter
	uint8_t * const TCB0_base = (uint8_t *)0xa80 ;
	uint16_t* const TCB0_cmp  = (uint16_t*)0xa8c ; // compare register; must be written as 16-bit word - see "Accessing 16-Bit Register"
	
	inline void setTMR(){
		if(0)TCB0_base[0] = (1<<1) + 1; // prescaler 1/2
		else TCB0_base[0] = (0<<1) + 1; // no prescaler
		TCB0_base[1] = 0; // normal mode
		if(0) TCB0_base[5] = 1; // allow interrupt; not now, SetPWM will do it
		}
		
	volatile uint8_t Count_int=0;  uint8_t Count_int_prev=0;
	
	int GetCount(){
		const auto x = Count_int;  // get the increment from previous value
		const int g = uint8_t(x - Count_int_prev);  Count_int_prev = x;
    return g;
		}
	
	void CounterPin_interrupt(){ Count_int++; }
	
	inline void setGeigerInterrupt(){
		if(0)PORTC.PIN7CTRL = (1<<3) + 2 ; // pull-up + rising edge interrupt allowed
		else attachInterrupt( GeigerCounterPin, CounterPin_interrupt, FALLING );  // if ISR is not in asm
		}

	void unused_ISR(){  // PORTC_PORT_vect,ISR_NAKED){
		asm volatile("PUSH r0       \n  IN  r0, 0x3f \n"
		             "PUSH r0       \n"
					 "PUSH r24      \n");
		VPORTC.INTFLAGS = 1<<7; // clear the interrupt (it is not self-resetting)
		Count_int++;
		if(0) VPORTB.IN = 1<<0; // ReproPin toggle; do not do it - the buzzer outputs constant sound when powered, not just a click :-(
		asm volatile("POP  r24      \n"
		             "POP  r0       \n  OUT     0x3f, r0 \n"
		             "POP  r0       \n  RETI             \n");
		}
	
	volatile uint16_t pwm, pwm2;  const uint16_t pwmMax=0x800;

	void SetPWM(uint16_t v){
		TCB0_base[5] = 0; // avoid jump into interrupt
		const auto d=0x30; // protection from missing the compare interrupt - when value is too low
		pwm2 = v+d;  pwm = (pwmMax+d)-v ;  // the interrupt code works somehow "reversed"
		TCB0_base[5] = 1; // allow interrupt
		}

	ISR(TCB0_INT_vect,ISR_NAKED){
		asm(  "PUSH r16         \n"
			  "LDI  r16, 1      \n"
			  "STS 0xa80+6, r16 \n" // clear interrupt flag
			  "LDI  r16, 1<<3   \n"
			  "STS  0x427, r16  \n"  // toggle PB3
			  "LDS  r16, 0x428  \n"
			  "SBRS r16, 3      \n        JMP lbl%=          \n"
				"LDS  r16, %[pwm]     \n  STS  0xa8c  , r16   \n"
				"LDS  r16, %[pwm] +1  \n  STS  0xa8c+1, r16   \n"
			  "POP r16  \n  RETI      \n"
			  "lbl%=:                 \n"
				"LDS  r16, %[pwm2]    \n  STS  0xa8c  , r16   \n"
				"LDS  r16, %[pwm2]+1  \n  STS  0xa8c+1, r16   \n"
			  "POP r16  \n  RETI     \n"
			:: [pwm] "p" (&pwm),  [pwm2] "p" (&pwm2)   );
		}
	void AssignAddLim( uint16_t&a, int16_t b ){
		if     ( b>=0 && int16_t(a)< 0 && int16_t(a)+b >=0 ){ a =0xffff; }
		else if( b< 0 && int16_t(a)>=0 && int16_t(a)+b <=0 ){ a =     0; }
		else                                                  a+=b;
		}

	uint16_t ControlVoltage;
	int16_t DiffVoltage;
	
	uint16_t TimePrev=0;
	uint16_t VInt=90*256;
	const uint16_t Vtarget = 803 ;  const uint8_t Gain=2  ;

	
	void VoltageControl(){
		const uint16_t t = millis();
			if( int16_t(t-TimePrev) >= 3700 ){
				TimePrev = t;
				uint16_t V=0;  const uint8_t Navg=32; // average the voltage:
					for(uint8_t n=0; n<Navg; n++){
						const auto v=analogRead(VoltageMeasPin);
						Serial3.print(int16_t(v-Vtarget));  Serial3.print(" ");
						V+=v;
						}
				Serial3.println("");
				const int16_t Vd =  V - Vtarget*Navg ;
				DiffVoltage = Vd;
				AssignAddLim( VInt, -Vd*Gain );
				const uint16_t v = VInt >> 5 ;
				Geiger::SetPWM(v);  ControlVoltage = v;
				Serial3.print(v);  Serial3.print(" \t");  Serial3.print(Vd);
								   Serial3.print(" \t");  Serial3.println( analogRead(CurrentMeasPin) );
		}
	}
	void Setup(){
		if(0)pinMode(VoltageMeasPin,INPUT);  // pin is input after reset
		pinMode(VoltageControlPin,OUTPUT);
		if(0)pinMode(ReproPin    ,OUTPUT); // will not be used
		setTMR();  setGeigerInterrupt();
		}
}
