#pragma once

namespace Meteo {
  int windcount = 0;
  
  //windspeed counter
  void windcounter() {
    windcount ++;
    }

  inline void Setup(){  //anemometer interrupt
    pinMode (23,INPUT);
    attachInterrupt(23,windcounter,RISING);
  
    //wind direction pin
    pinMode (24,INPUT);
    }
      
  inline uint16_t windReadRaw(){ return analogRead(24); }
  
  decltype(millis()) tprev = 0;
  
  //wind speed calculator
  float windspeed() {
  	const auto current = millis();
  	const auto td = current - tprev;
    tprev = current;
  	const auto a = windcount; windcount = 0;
  	
  	if (a > 0)	{ 	return  	((a-1) * 0.33 + 0.66) / td * 1000; }    
  		else      {		return  	0;}
    }
  
  //wind direction, TreshVals starts by the smallest resistor value ==> smallest analogRead value
  float winddir() {
    int v = windReadRaw();
    const unsigned Vals[]			= {5, 		3, 		4, 		7, 		6, 		9, 		8, 		1, 		2, 		11, 	10, 	15, 	0, 		13, 	14, 	12};
    const uint16_t TreshVals[]	= {147,		171,	208,	282,	369,	438,	531,	624,	710,	780,	817,	869,	908,	938,	970,	1024};
    const unsigned N 				= sizeof(TreshVals) / sizeof(TreshVals[0]);
  
    for( unsigned i = 0;  i<N; i++){
      if (v <= TreshVals[i])  {
        Serial3.println(String(TreshVals[i]));
        return Vals[i] * 22.5;  }
      }
  }
}
//  9   8   7   6   5   4    3   2   1   0   15  14  13  12  11  10
//	282 531 438 817 780 1024 938 970 869 908 624 710 171 208 147 369
//	236 463 410 791 768 985  921 954 843 896 597 651 164 180 131 327
//										 N	 N2 clockwise
