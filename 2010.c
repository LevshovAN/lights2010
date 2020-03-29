//project: lights2010

//#include <inttypes.h>
#include <avr/io.h>
//#include <avr/pgmspace.h>
//#include <avr/eeprom.h>

#define F_CPU0 1000 //частота в килогерцах

//----------- маски портов ----------
#define INDIC1_M 0x7f
#define ADC_IN_M 0x80

#define INDIC2_M 0xC0
#define LED2_M   0x3f

#define LED1_M   0x0f
#define LED3_M   0x10
#define LED4_M   0x20
#define INKN_M1  0x40
#define INKN_M2  0x80

#define STROBPORT PORTC
//------------- константы ------------
#define num_modes 17
#define TIK 1
#define divTC0 64
#define ind_mode 16
//-------------- макросы -------------
#define TO_INDICATOR(x) (PORTA=INDIC1_M&(x)) 
#define RES_INDICATOR (PORTA=0) 
#define RES_LED1 (PORTD|=LED1_M) 
#define SET_LED1(x) (PORTD&=(~LED1_M)|~(x)) 
#define SET_LED134(x) (PORTD&=~((LED1_M|LED3_M|LED4_M)&(x))) 
#define RES_LED3 (PORTD|=LED3_M) 
#define SET_LED3 (PORTD&=~LED3_M) 
#define RES_LED4 (PORTD|=LED4_M)
#define SET_LED4 (PORTD&=~LED4_M)
#define RES_LED134 (PORTD|=LED1_M|LED3_M|LED4_M)
#define RES_LED2 (PORTB|=LED2_M) 
#define SET_LED2(x) (PORTB&=(~LED2_M)|~(x)) 
 
//-------- глобальные переменые -------
unsigned char
 mode=1,     //номер режима
 out_kn,     //код нажатой клавиши
 digit_n,    //номер фазы стробирования
 timer,      //длительность фазы2 в мс
 num_phase,  //количество фаз
 c_phase,    //текущая фаза
 cn_repeat,  //оставшееся число повторов
 mix,        //признак перебора
 init;       //признак инициализации процедуры
unsigned char sost_kn[3];
unsigned char counter=0;       //счетчик длительности фазы 

unsigned char display_mem[16];
const unsigned char segment_code[10]={
0,1,2,4,8,16,32,64,128,0x7f};

 const unsigned char random[256]={
  65, 31, 39,  2, 54, 74, 36, 68, 48, 75, 70, 71, 86, 28, 35,  4,
   5, 58, 76, 31, 49, 38,  9,  4, 89,  9, 41, 71, 34,  3, 19, 12,
  19, 39, 73,  8, 38,  9, 58, 73, 40, 64,  3, 57, 19, 24, 53, 23,
  32,  9 ,28, 36, 62, 36, 87, 67, 52, 54, 70, 35, 30, 81, 45, 26,
  75,  8, 41, 14, 36, 72, 74, 59,  6, 58, 13, 52, 82, 62,  1, 12,
  80,  9, 31,  4, 19,  1, 36, 31, 53, 30, 56, 22, 33, 79, 34, 22,
  29, 57, 57, 57, 69, 46, 38,  0, 31, 82, 53, 85, 13, 15, 79,  8,
  56, 20, 12, 34, 71, 26, 78,  9, 76, 25, 35, 34, 56, 52, 56, 57,
  31, 35, 53, 77,  8, 11, 67, 70, 42, 89,  4, 70, 73, 44, 34, 81,
  55, 35, 36, 86, 76, 20, 61,  1, 64, 84,  3, 78, 88,  6, 42, 12,
  35, 42, 86, 21, 71, 59, 55, 22, 56, 83, 35,  9, 49, 89, 79, 13,
  65, 79, 43, 34, 22, 50, 12,  8, 34, 11, 81,  8, 15, 16, 67, 29,
  25, 40, 33, 16, 10, 67, 31, 82, 22, 41, 31, 53, 58, 79, 30, 22,
  75,  6, 22, 35, 15, 59, 86, 56, 48, 75, 81, 50, 89, 39, 60,  4,
   5, 52, 78, 10, 60, 79, 79, 46, 22, 18, 32, 44, 64, 51, 47, 65,
  87, 34, 43,  4, 62, 57, 51, 16, 65, 80, 75,  7, 63, 23, 77, 57
 };
//=====================================================

void obrab_input(void){

 out_kn=0;
 if(digit_n<2){
  if((~PIND)&(INKN_M1)){ 
   sost_kn[digit_n]++;
   if(sost_kn[digit_n]>250){
    out_kn=digit_n+1;
    sost_kn[digit_n]=150;
   }
   if(sost_kn[digit_n]==10) out_kn=digit_n+1;
  }else sost_kn[digit_n]=0;
  if((sost_kn[0]>100)&&(sost_kn[1]>100)) out_kn=3;	 
 }

 if(digit_n==2){              
  if((PIND)&(INKN_M2)){       //определение подключения к лин входу
   if(sost_kn[2]<150) sost_kn[2]++;
   if(sost_kn[2]==100) out_kn=4;
  }else{                      //определение отключения от лин входа
   if(sost_kn[2]>5) sost_kn[2]--;
   if(sost_kn[2]==50) out_kn=5;
  }
 }
}

void init_timer(void){
 TCNT0=-(TIK*F_CPU0/divTC0);
 TIFR=0;
}  

void timer_proc(){
 digit_n++;                            //переключчение переменной стробирования
 digit_n&=7; 

 if(digit_n==0){                       //если номер фазы стробирования = 0 (доп. деление частоты  на 8)
  counter++;                           //инкремент счетчика таймера
  if(counter==timer){                  //при совпадении значения счетчика заданному значению
   counter=0;                          //сброс счетчика
   c_phase++;                          //переключение номера фазы
   if(c_phase>=num_phase){
			 c_phase=0;   //если номер фазы больше количества фаз сброс номера фазы
    cn_repeat--;  
   }    
  }
 }
}

void indicator_proc(){                 //вывод цифры номера режима
unsigned char tmp, digit2;             //врем.перем., вторая(старшая) цифра

 PORTB&=~INDIC2_M;                     //выключение стробирующих выводов 
 PORTA=0;                              //подача нулей на все сегменты индикатора 

 digit2=0;                             //вычисление цифр
 tmp=mode;
 while(tmp>9){                         //пока номер режима двузначное число 
  tmp-=10;                             //вычитать десятки
  digit2++;                            //и считать их
 }
 if(digit_n&1){                        //если младшая цифра вывод единиц
  TO_INDICATOR(segment_code[tmp]);     //подача информации на индикатор
  PORTB|=0x40;                         //включение стробирующего вывода единиц
 }else{                                //иначе старшая цифра вывод десятков
  TO_INDICATOR(segment_code[digit2]);  //подача информации на индикатор
  PORTB|=0x80;                         //включение стробирующего вывода десятков
 }
}

void initADC(){
 ADCSRA=0b10000101;		//7: Включение АЦП;6-запуск; 5-непрерывно/однократное преобразование ;4,3-без прерываний;0-2-CK/64 
 ADMUX =0b11000111;		//67-источник оп. напр. (внутр.2,56В);  5:0-выравнивание вправо; 0-4: номер порта (седьмой канал)
}

void init0(){
 ADCSRA=0b00000101;		//7: Выключение АЦП 
}



void change_mode(){
 unsigned char              //   0, 1, 2, 3, 4, 5, 6, 7, 8,  9,10,11,12,13, 14, 15, 16,17,18,19,20
  timer_of_mode[num_modes]     ={2,30,25,25,25,25,25,25,25,  2,25,25,25, 7,/* 25,*/  7, 10,/*40,*/25   },      
  num_phase_of_mode[num_modes] ={2,64,64,64,64,64,64,64,64,128,64,56,72,21,/*242,*/255,160,/*16,*/64   },  
  num_repeat_of_mode[num_modes]={1, 1, 1, 1, 1, 1, 1, 1, 1, 12, 2, 2, 2, 5,/*  1,*/  1,  1,/* 5,*/ 2   };  

static unsigned char save_mode,old_mode=0;

 switch(out_kn){                       //переключение режима
  case 1:                              //нажата кнопка "+"
   mode+=1;                            //перейти к след режиму
   if(mix){                            //если включен режим перебора
			 mix=0;                             //выключить его
				mode=1;                            //перейти в первый режим  
   }
  break;
  case 2:                              //нажата кнопка "-"
   mode-=1;                            //перейти к предыдущему режиму
   if(mode==0) mode=num_modes;         //после первого - последний
   if(mix){                            //если включен режим перебора
			 mix=0;                             //выключить его
				mode=num_modes-1;                  //перейти в предпоследний режим  
   }
  break;
  case 3:                              //нажаты обе кнопки 
   mix=0;                              //выключение перебора режимов  
   mode=0;                             //переход в тестовый режим
  break;
  case 4:                              //подключение к лин входу 
   mix=0;                              //выключение перебора режимов  
   save_mode=mode;                     //сохранение номера режима
   mode=ind_mode;                      //переход в режим индикатора уровня
  break;
  case 5:                              //отключение от лин входа 
   if(mode==ind_mode) mode=save_mode;        //если режим индикатор уровня - переход в сохраненый режим
 }
 if(mode>=num_modes){                  //при переходе в последний режим 
	 cn_repeat=0;                         //для переключения в первый режим            
  mix=1;                               //включить перебор режимов
 }
 if(mix){                              //в режиме пребора
	 if(cn_repeat==0){                    //если закончились повторы одного режима перейти к другому 
	  mode+=1;                            //след режим
   if(mode==ind_mode)                        //если переход на индикатор уровня
			 if(sost_kn[2]<100) mode+=1;        //при неподключеном входе - переход на след режим  		
  } 
  if(mode>=num_modes) mode=1;          //после последнего - первый
 }
 if(mode!=old_mode){                   //если режим изменился - настроить временые параметры в соответствии с режимом
  timer=timer_of_mode[mode];           //загрузка значения длительности фазы      
  num_phase=num_phase_of_mode[mode];   //загрузка значения количества фаз  
  cn_repeat=num_repeat_of_mode[mode];  //загрузка значения количества повторов  
  counter=0;                           //сброс счетчика длительности фазы
  c_phase=0;                           //начальная фаза 0   
  init=0;
  old_mode=mode;
  switch(mode){                        //настройка контроллера для нового режима
   case ind_mode:	initADC(); break; 
   default: init0();   break; 
  }
 }
}

void light_1led(unsigned char number){
 STROBPORT=0;                    //выкл. строб
 RES_LED134;                     //выкл. светодиоды 1,3,4 кругов
 RES_LED2;                       //выкл. светодиоды 2 круга
 number&=0x7f;
 STROBPORT=1<<(number&7); 
 number>>=3; 
 if(number<4) SET_LED1(1<<number);
 else{
  number-=4;
  if(number<6) SET_LED2(1<<number);
  else{
   if(number==6) SET_LED3; 
   if(number==7) SET_LED4;   
  }
 }
}

void turn_off_leds(){
 STROBPORT=0;                    //выкл. строб
 RES_LED134;                     //выкл. светодиоды 1,3,4 кругов
 RES_LED2;                       //выкл. светодиоды 2 круга
}

void write_1led(unsigned char number,unsigned char vol){ // записывает состояние светодиода в память
 unsigned char addres,n_circle,strobe;

 if(number<89){
  n_circle=number>>3; 
  strobe=number&7;
  if(n_circle<6) addres=strobe;
  else{
   addres=strobe+8;
   n_circle-=6;
  }
  if(vol) display_mem[addres]|=(1<<n_circle);
  else display_mem[addres]&=~(1<<n_circle);
 }
}

void clear_display(){ // гасит все светодиоды, при работе через display_leds() 
 unsigned char i;
 for(i=0;i<16;i++) display_mem[i]=0;
}

void scroll_display(){ // прокрутка дисплея
 unsigned char i,tmp1,tmp2;
 tmp1=display_mem[7];
 tmp2=display_mem[15];  
 for(i=7;i>0;i--){
  display_mem[i]<<=2;
		display_mem[i]|=(display_mem[i-1]<<2)&0x0C;
 } 
 for(i=15;i>8;i--){
  display_mem[i]<<=3;
		display_mem[i]|=(display_mem[i-1]<<3)&0x38;
 }
 display_mem[0]<<=2;
	display_mem[0]|=(tmp1<<2)&0x0C;
 display_mem[8]<<=3;
	display_mem[8]|=(tmp2<<3)&0x38;
}

void display_leds(){
 STROBPORT=0;                    //выкл. строб
 RES_LED134;                     //выкл. светодиоды 1,3,4 кругов
 RES_LED2;                       //выкл. светодиоды 2 круга
 SET_LED134(display_mem[digit_n]);
 SET_LED2(display_mem[8+digit_n]);
 STROBPORT=(1<<digit_n);          //включить светодиоды всех кругов в соответствии с фазой	 
}

void proc0(void){               // тест
 unsigned char strob_mask;
 STROBPORT=0;                   //выкл. строб
 strob_mask=1<<digit_n;         //вычисление нового строба
 RES_LED134;                    //выкл. все светодиоды
 STROBPORT=strob_mask;          //включить строб
 SET_LED1(LED1_M);              //включить светодиоды 1 круга
 SET_LED3;                      //включить светодиоды 3 круга	 
 SET_LED2(LED2_M);              //включить светодиоды 2 круга
 SET_LED4;                      //включать центральный светодиод
}

void proc1(void){               //8 бегущих огней по первому кругу
 unsigned char tmp_phase;
 tmp_phase=c_phase;
 if(tmp_phase>=32) tmp_phase=32-tmp_phase;
 STROBPORT=0xFF;                //вкл. все стробы
 RES_LED134;                    //выкл. светодиоды 1,3,4 кругов
 RES_LED2;                      //выкл. светодиоды 2 круга
 SET_LED1(1<<(tmp_phase&3));    //включить светодиоды внешнего круга в соответствии с фазой	 
 SET_LED4;                      //включать центральный светодиод
}

void proc2(void){               //8 бегущих огней по первому кругу
 unsigned char tmp_phase;
	const unsigned char led134_arr[4]={0b00000011,0b00000110,0b00001100,0b00001001};    
 tmp_phase=c_phase;
 if(tmp_phase>=32) tmp_phase=32-tmp_phase;
 STROBPORT=0xFF;                //вкл. все стробы
 RES_LED134;                    //выкл. светодиоды 1,3,4 кругов
 RES_LED2;                      //выкл. светодиоды 2 круга
 SET_LED1(led134_arr[tmp_phase&3]);    //включить светодиоды внешнего круга в соответствии с фазой	 
 SET_LED4;                      //включать центральный светодиод
}

void proc3(){               
 const unsigned char led134_arr[6][4]={
  {0b00110111,0b00110001,0b00100000,0b00101100},//мельница   
  {0b00110000,0b00110000,0b00101000,0b00100010},//косая вертушка  4 ветки, строб - 2фазы, 4 фазы 		
  {0b00001001,0b00001110,0b00000011,0b00000100},//звездочка		
  {         0,         0,         0,         0},//бегущие огни 2		
  {0b00010100,0b00010001,0b00010100,0b00010001},//вилка
  {0b00100100,0b00100001,0b00100100,0b00100001}};//солнышко    

 const unsigned char led2_arr[6][4]={
  {0b00111111,0b00000111,0b00000000,0b00111000},
  {0b00100000,0b00010100,0b00001010,0b00000001},		
  {0b00100111,0b00101100,0b00100101,0b00111100},		
  {0b00001000,0b00000001,0b00010000,0b00000010},
  {0b00011110,0b00000011,0b00011000,0b00110011},
  {0b00011010,0b00010011,0b00011010,0b00010011}};  

 unsigned char tmp_phase, strob;

 STROBPORT=0x00;                  //вкл. все стробы
 RES_LED134;                      //выкл. светодиоды 1,3,4 кругов
 RES_LED2;                        //выкл. светодиоды 2 круга
 tmp_phase=c_phase;
 if(tmp_phase>=32) tmp_phase=63-tmp_phase;
 if((digit_n&1)==0)	strob=0x55;  //включить светодиоды всех кругов в соответствии с фазой	 
 else{
  tmp_phase+=2;
  strob=0xAA;                    //включить светодиоды всех кругов в соответствии с фазой	 
 }
 tmp_phase&=3;

 SET_LED134(led134_arr[mode-3][tmp_phase]);     //включить светодиоды 1 круга
 SET_LED2(led2_arr[mode-3][tmp_phase]);         //включить светодиоды 2 круга
 STROBPORT=strob;                   //включить светодиоды всех кругов в соответствии с фазой	 
}

void proc9(void){              //мигалка сине-красная-белая
 const unsigned char //         0          1,         2,3,4	      
  led2_arr[5]=        {0b00100100,0b00010010,0b00001001,0,0};      
 unsigned char tmp_phase; 
 STROBPORT=0xFF;                //вкл. все стробы
 RES_LED134;                    //выкл. светодиоды 1,3,4 кругов
 RES_LED2;                      //выкл. светодиоды 2 круга
 tmp_phase=c_phase&0x1f;
 if((tmp_phase==0)||(tmp_phase==6)||(tmp_phase==12)||(tmp_phase==18))
  SET_LED2(led2_arr[c_phase>>5]); //включить светодиоды 2 круга
}

void proc10(void){
               //кольцо от центра к краю
 const unsigned char //0,     1,     2,         3,         4,         5,     6,	      
  led134_arr1[8]=     {0,LED4_M,LED3_M,         0,         0,         0,LED1_M,0},      
  led2_arr1[8]  =     {0,     0,     0,0b00100100,0b00010010,0b00001001,     0,0};      

               //кольца от края к центру
 const unsigned char     //0,         1,         2,         3,         4,         5,        6,	      
  led134_arr2[28]={        0,    LED1_M,         0,         0,         0,    LED3_M,   LED4_M,
                  0b00100000,0b00100001,0b00100000,0b00100000,0b00100000,0b00110000,
                  0b00110000,0b00111111,0b00110000,0b00110000,0b00110000,
                  0b00110000,0b00111111,0b00110000,0b00110000,
                  0b00110000,0b00111111,0b00110000,
                  0b00110000,0b00111111,
																		0b00111111},
																		
  led2_arr2[28] ={         0,         0,0b00001001,0b00010010,0b00100100,         0,        0,
                           0,         0,0b00001001,0b00010010,0b00100100,         0,
                           0,         0,0b00001001,0b00010010,0b00100100,
                  0b00100100,0b00100100,0b00101101,0b00110110,
                  0b00110110,0b00110110,0b00111111,
                  0b00111111,0b00111111,																											
																		0b00111111};      

 unsigned char tmp_phase,strob,tmp_led134,tmp_led2;

 tmp_phase=c_phase;


 RES_LED134;                      //выкл. светодиоды 1,3,4 кругов
 RES_LED2;                        //выкл. светодиоды 2 круга
 if(mode==10){
  if(tmp_phase>=32) tmp_phase=63-tmp_phase;
  tmp_phase&=7;
  tmp_led134=led134_arr1[tmp_phase];
  tmp_led2=led2_arr1[tmp_phase];
  strob=0x55<<(digit_n&1);
 }else{
  if(mode==11){
   if(tmp_phase>=28) tmp_phase=55-tmp_phase;
   strob=0x11<<(digit_n&3);
		}else{
   if(tmp_phase>=36) tmp_phase=71-tmp_phase;
   tmp_phase-=digit_n;
   strob=1<<digit_n;
   if(tmp_phase>=28) tmp_phase=27;
		}

  if(tmp_phase>=28) tmp_phase=0;
  tmp_led134=led134_arr2[tmp_phase];
  tmp_led2=led2_arr2[tmp_phase];
 } 
 SET_LED134(tmp_led134); //включить светодиоды 1 круга
 SET_LED2(tmp_led2);     //включить светодиоды 2 круга
 STROBPORT=strob;        //вкл. стробы
}



void proc13(void){    //фейерверк
 unsigned char tmp_phase,strob,tmp_led134,tmp_led2;
 const unsigned char //     0,         1,         2,         3,         4,         5,         6,         7	      
  led134_arr1[7]= {         0,0b00000001,0b00000001,0b00000000,0b00000000,0b00100000,0b00110000},      
  led2_arr1[7]  = {         0,         0,0b00001000,0b00011000,0b00110000,0b00100000,0b00000000},      
  led134_arr2[6]= {0b00100000,0b00110000,0b00010000,         0,         0,0b00000001},      
  led2_arr2[6]  = {         0,         0,0b00100000,0b00110100,0b00011010,0b00001001},      
  led134_arr3[8]= {0b00000101,0b00001010,0b00100101,0b00001000,0b00000010,0b00100100,0b00010001,0b00101000},      
  led2_arr3[8]  = {0b00000101,0b00010010,0b00001001,0b00100000,0b00010001,0b00100010,0b00010000,0b00001000};      

 tmp_phase=c_phase;
 if(tmp_phase<7){
  strob=8;
  tmp_led134=led134_arr1[tmp_phase];
  tmp_led2  =led2_arr1[tmp_phase];
 }else if(tmp_phase<13){
  tmp_phase-=7;  
  strob=0xff;
  tmp_led134=led134_arr2[tmp_phase];
  tmp_led2  =led2_arr2[tmp_phase];
 }else{
  tmp_phase-=13;  
  if((digit_n&1)==0)	strob=0x55;  //включить светодиоды всех кругов в соответствии с фазой	 
  else{
   tmp_phase+=3;
   strob=0xAA;                    //включить светодиоды всех кругов в соответствии с фазой	 
  }
  tmp_phase&=7;
  tmp_led134=led134_arr3[tmp_phase];
  tmp_led2  =led2_arr3[tmp_phase];
 }
 STROBPORT=0x00;                  //выкл. все стробы
 RES_LED134;                      //выкл. светодиоды 1,3,4 кругов
 RES_LED2;                        //выкл. светодиоды 2 круга
 SET_LED134(tmp_led134);          //включить светодиоды 1 3 4 кругов
 SET_LED2(tmp_led2);              //включить светодиоды 2 круга
 STROBPORT=(strob);               //включить светодиоды всех кругов в соответствии с фазой	 
}

void proc15(void){                      //случайные вспышки с паузами случайной длительности
 static unsigned char state7;
 if((c_phase&1)==0){
  light_1led(random[(c_phase>>1)+128]); //включить светодиод
  timer=7;
  state7=1;
 }else{
  if(state7) timer=random[c_phase>>1]&0x0f;
  turn_off_leds(); //выключить светодиоды
  state7=0; 
 }
}

void proc16(void){               //случайное зажигание и гашение светодиодов
 static unsigned char add_phase;
 if(c_phase==0){
  add_phase+=3;
  add_phase&=0x3f;
 }
 if((c_phase&1)==0) write_1led(random[c_phase+add_phase],1); //включить светодиод
 else write_1led(random[c_phase+add_phase+8],0);      //выключить светодиод
 display_leds();
}

void proc_ind(void){
 const unsigned char //0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15	      
  led134_arr[16]=     {0,0x20,0x30,0x30,0x30,0x30,0x30,0x30,0x31,0x35,0x37,0x3f,0x3f,0x3f,0x3f,0x3f},      
  led2_arr[16]  =     {0,   0,   0,0x04,0x24,0x26,0x36,0x37,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f};      

 int ADCin;
 static unsigned char sample_arr[128];  //значения от 0 до 255
 static unsigned char ampl_arr[128];    //значения от 0 до 255
 static unsigned char nc_sample;
 static unsigned char nc_ampl;
 static unsigned int Average_ampl=0;   //значения от 0 до 32640
 static unsigned int Average_ampl2=0;  //значения от 0 до 32640
 static unsigned char k,Average2;      //значения от 0 до 127
 unsigned char i,strob_mask,new_sample;
 unsigned char tmp_av_ampl2;           

 if(init==0){
  nc_ampl=0;
  nc_sample=0;
  Average_ampl=0;
  Average_ampl2=0;
  for(i=0; i<128; i++) sample_arr[i]=0; //обнуление массива семплов
  for(i=0; i<128; i++) ampl_arr[i]=0;   //обнуление массива амплитуд
  init=1;
 }

 if((ADCSRA&_BV(ADSC))==0){			         //преобразование завершено?
  ADCin=ADC;                           //считывание кода сигнала
  ADCin>>=1;                           //сдвиг ...
  ADCin-=256;                          //минус половина максимального уровня
  if(ADCin<0) ADCin=~ADCin;            //"выпрямление"
  new_sample=ADCin&0xff;               //копирование в короткую переменную
  nc_sample++;                          //
  nc_sample&=0x7f;

  if(nc_sample==0){                //переключение номера текущего сэмпла
   nc_ampl++;                          //
   nc_ampl&=0x7f;
   Average_ampl2-=ampl_arr[nc_ampl];
   ampl_arr[nc_ampl]=Average_ampl>>7;
   Average_ampl2+=ampl_arr[nc_ampl];
   tmp_av_ampl2=Average_ampl2>>7;
   if(tmp_av_ampl2>6)	k=768/tmp_av_ampl2;
   else k=127;
  }
  Average_ampl-=sample_arr[nc_sample]; //вычитание старого семпла из среднего
  sample_arr[nc_sample]=new_sample;    //запись нового семпла
  Average_ampl+=new_sample;            //прибавление нового семпла к среднему
  Average2=(((Average_ampl>>7)*k)>>7)&0x0f;           // /2048 (значения от 0 до 12)
  ADCSRA|=_BV(ADSC);			                //Начать новое преобразование
 }

 STROBPORT=0x00;                       //выкл. все стробы
 strob_mask=1<<digit_n;                //вычисление нового строба
 RES_LED134;                           //выкл. светодиоды 1,3,4 кругов
 RES_LED2;                             //выкл. светодиоды 2 круга
 SET_LED134(led134_arr[Average2]);     //включить светодиоды 1 круга
 SET_LED2(led2_arr[Average2]);         //включить светодиоды 2 круга
 STROBPORT=strob_mask;                 //включить строб

}

int main (void){
 PORTD=0xFF;         //все выводы - подтянутые входы
 DDRA=INDIC1_M;            // PA0-6 - информационные выходы индикатора 
 PORTA=0;
 DDRB=LED2_M|INDIC2_M;     // PB0-5 - упр. выходы светодиодов средних колец(0 включено); PB6-7 стробирующие выходы индикаторов
 DDRC=0xFF;                // PC - выходы стробирующие светодиоды (0 выключено) 
 DDRD=LED1_M|LED3_M|LED4_M;// PD0-3 - упр. выходы св. внешнего круга, PD4 - упр.выход св. внутреннего круга, PD5 - упр.выход желтого светодиода

 TCCR0|=3;
 init_timer();  
 mode=1;
 digit_n=0;
 out_kn=0;
 while(1){                   //основной цикл
        
  do{                        //задержка
  }while(TCNT0>10);
  init_timer();  
								 
  obrab_input();             //анализ состояния кнопок
  change_mode();             //переключение режима
  timer_proc();              //обработка програмного таймера
  indicator_proc();          //вывод номера режима

  switch(mode){              //переход на подпрограмму текущего режима
   case 0:  proc0();  break; 
   case 1:  proc1();  break; 
   case 2:  proc2();  break;  
   case 3:  
   case 4:
   case 5: 
   case 6:
   case 7: 
   case 8:  proc3();  break; 
   case 9:  proc9();  break; 
   case 10: 
   case 11:	
   case 12:	proc10(); break; 
   case 13:	proc13(); break; 
   case 14:	proc15(); break; 
   case 15:	proc16(); break; 
   case ind_mode:	proc_ind(); break; 
  }
 }
}


