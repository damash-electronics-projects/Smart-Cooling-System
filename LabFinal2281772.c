

// This is used to enable the LCD
// Lcd pinout settings
sbit LCD_RS at PORTB2_bit;
sbit LCD_EN at PORTB5_bit;
sbit LCD_D4 at PORTC4_bit;
sbit LCD_D5 at PORTC5_bit;
sbit LCD_D6 at PORTC6_bit;
sbit LCD_D7 at PORTC7_bit;

// Pin direction
sbit LCD_RS_Direction at DDB2_bit;
sbit LCD_EN_Direction at DDB5_bit;
sbit LCD_D4_Direction at DDC4_bit;
sbit LCD_D5_Direction at DDC5_bit;
sbit LCD_D6_Direction at DDC6_bit;
sbit LCD_D7_Direction at DDC7_bit;


//Used for the virtual terminal
#define PROMPT "Enter S for % fan speed and T for temperature: "
#define WRONG_INPUT "Wrong Input! Enter S or T"
//Initializing all ports
void initializePorts()
{
        DDRA = 0xFF;     // Port A  for  LEDs
        DDRB = 0xFF;     // Port B is for LCD
        DDRC |= 0x02;    // Port PC.1 for buzzer
        DDRD = 0x00;     // The input to decide which output to enable

        DDRF = 0x00;          //Port F  input for ADC
        ADMUX = (1<<ADLAR);
        ADCSRA = 0x87;        // ADEN = 0x80 [ADC enable], prescaler 128 [0x07] -> 10MHZ/128 =  78KHz clock

        //Initializing LCD
        Lcd_Init();                 // Turn on the LCD
        Lcd_Cmd(_LCD_CURSOR_OFF);   // Cursor off
        Lcd_Cmd(_LCD_CLEAR);        // Clear display

        TCCR0 = 0x65;               // PWM, phase correct, Non-inverting mode with 128 prescalar

        SREG_I_bit = 1; // Enable Interrupts
        }
//Initialize all ports END

//LCD START

void ShowFanSpeed(char * fanSpeedString)
{
Lcd_Out(1,1,"Fan Speed:");
Lcd_Out(1, 11, fanSpeedString);
}

void ShowTemp(char * tempString)
{
Lcd_Out(2,1,"Temperature:");
Lcd_Out(2, 11, tempString);
}
char fanSpeedString[8];
char tempString[8];
void updateLcd(unsigned int fanSpeed, unsigned int temp)
{

IntToStr(fanSpeed, fanSpeedString);   //Change the integer value into a string
IntToStr(temp, tempString);

//Fan Speed On Lcd
Lcd_Cmd(_LCD_CLEAR); // Clear display
if ((PIND & 1<<0) != 0)
ShowFanSpeed(fanSpeedString+1);

//Temp On Lcd
if ((PIND & 1<<1) != 0)
ShowTemp(tempString+1);
}
//LCD END

//ADC START
unsigned int findTemp(unsigned int High)
{
        return ((unsigned int) (High * 0.3921569)); //This is found using 100 / 255
}

unsigned int readTemp(){
        unsigned int tempAnalogHigh;
        ADCSRA |= (1<<ADSC);                        // start conversion
        while ((ADCSRA & (1<<ADIF))!=0);            // Wait for the conversion to end using ADC interrupt flag
        tempAnalogHigh= ADCH;                       // take the high byte
        ADCSRA |= (1<<ADIF);                        // write 1 to clear the ADIF flag
        return findTemp(tempAnalogHigh);

}
//ADC END

//LightUpLEDs START
char LEDs;
void lightUpNLeds(unsigned int n)
{
        if(n>8){n=8;}
        for (LEDs = 0x01; n>0; n--)
        {
              LEDs = (LEDs*2) +1;
        }
        PORTA = LEDs;
}
// LightUpLEDs END

//Buzzer START
void generateSquareWave(){
TIMSK |= (1<<OCIE2); // Enable interrupt for timer 2.
DDRC |= 0b00000010;  //Used PC.1 for the buzzer
PORTC |= 0b00000010;
TCCR2 = 0x0B; //  CTC with a 64 prescalar
OCR2 = 39; // 1/10MHz =0.1us => 0.1us *64 = 6.4us/cycle, However 1/2KHz =250us
           //The OCR2 = 250us/6.4us = 39.
}
void stopBuzzer()
{
        TCCR2 = 0x00; // Stop timer
        TIMSK &= ~(1<<OCIE2); // Disable interrupt for timer 2.
        PORTC &= ~(1<<1); //Stop buzzer

}


void buzzerFor2Secs()
{
        TIMSK |= (1<<OCIE1A);//  Enable interrupt for Timer1
        TCCR1A = 0x00;      //1/10MHz =0.1us => 0.1us *1024 = 102.4us/cycle, Then 2s /102.4us => OCR1 = 0x4C4C.
        TCCR1B = 0x0D; ; // CTC mode with prescalar 1024
        OCR1AH = 0x4C; // OCR1 high byte
        OCR1AL = 0x4C; // OCR1 low byte
        generateSquareWave(); // starts generating square wave using timer 2
}

void Timer2Compare_ISR() iv IVT_ADDR_TIMER2_COMP{
     PORTC ^= 0x02;
}

void Timer1Compare_ISR() iv IVT_ADDR_TIMER1_COMPA
{
        stopBuzzer();    //ISR for Compare interrupt using Timer1
 }

// Buzzer END

//  Fan control START
int fanSpeed;
int adjustFanSpeed(unsigned int temp){
        fanSpeed = (int) (temp-30)*(1.429); //linear relationship between temp and fanSpeed 100/70
        if (fanSpeed < 0) fanSpeed=0;
        OCR0 = (int) (fanSpeed * 2.559); //256/100
        return fanSpeed;
}
// Fan control END

//USART START
void initUsart(){

     UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1 << RXCIE0);      //RX & TX Enable,  RX Intrreupt Enable
     UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);                    //8 bit packet size, no parity
     UBRR0H = 0;
     UBRR0L = 65;                                          //baud rate = 9600, Async. mode(Normal speed)
}
void sendTexT(char* string){
     char i = 0;
     while(string[i] != '\0'){                                    // Send character by character for UDR0
                     while(!(UCSR0A & (1<<UDRE0)));
                     UDR0 = string[i];
                     i += 1;
}}
void terminalStart(){

     sendTexT(PROMPT); //fisrt terminal prompt
}


void newLine(){

     while(!(UCSR0A & (1<<UDRE0))); //Wait until UDR0 is empty
     UDR0 = 13; // new line (CR)
}

void  getText(char* string){

    string[0] = UDR0;//Recieve character
}


char controller(char* string){ // input controller, identifies input type

     char type = 0;
     if(string[0] == 't'||string[0] == 'T' ){
                  type = 1;
     }
     else if(string[0] == 's' || string[0] == 'S'){
                  type = 2;
     }
     return type;
}

void request() iv IVT_ADDR_USART0__RX // Recieve character interrupt
{
     char rxText [2];
     getText(rxText);     // Receive the text

        switch(controller(rxText)) //identify which type of output to put on the virtual terminal
     {
       case 0:   //wrong input!
            sendText(WRONG_INPUT);
            newLine();
            sendTexT(PROMPT);
            break;
       case 1:    // Show the Temperature
            sendText(tempString);
            newLine();
            sendTexT(PROMPT);
            break;
       case 2:      // Show the %fan speed
            sendText(fanSpeedString);
            newLine();
            sendTexT(PROMPT);
            break;
     }
}//USART END

// Main  START
char prev_PinD0_bit; // For %Fan speed
char prev_PinD1_bit; // For Temperature
int prev_fanSpeed;
unsigned int prev_temp;
unsigned int temp;
unsigned int buzzerIsPlayed;
char tempStr[4];
void main() {
        initializePorts();
        initUsart();
        terminalStart();  //"Enter S for % fan speed and T for temperature: "
        while (1){
              prev_temp = temp;
                temp = readTemp();
                 //[0]enable temp reading on lcd, [1]enable fan speed reading on lcd
                 //[2]enable temp on LEDs, [3]buzzer

                if ((PIND & 1<<2) == 0)
                         PORTA = 0x00;
                else
                        {lightUpNLeds((int) (temp/12.5));}

                if ((PinD & 1<<3) == 0 || temp < 30){
                   stopBuzzer();
                   buzzerIsPlayed = 0;
                }
                if (temp >= 30 && ((PinD & 1<<3) != 0) && (!buzzerIsPlayed)){
                                 buzzerFor2Secs();
                                 buzzerIsPlayed = 1;
                        }
                prev_fanSpeed = fanSpeed;
                fanSpeed = adjustFanSpeed(temp);

                if ((prev_temp != temp) || (prev_PinD0_bit != PinD0_bit) || (prev_PinD1_bit != PinD1_bit))
                   updateLcd(fanSpeed, temp);

                prev_PinD0_bit = PinD0_bit;
                prev_PinD1_bit = PinD1_bit;

                }
}
// Main END
