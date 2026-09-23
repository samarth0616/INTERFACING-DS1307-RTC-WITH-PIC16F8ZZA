#include <xc.h>

#define _XTAL_FREQ 4000000UL

//==================================================
// CONFIGURATION
//==================================================
#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config BOREN = ON
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF


//==================================================
// INITIAL RTC TIME
//==================================================

#define INITIAL_HOUR    11
#define INITIAL_MINUTE  45
#define INITIAL_SECOND  0

// 1 = set RTC every time PIC starts
// 0 = do not reset RTC at startup
#define SET_RTC_AT_STARTUP  1


//==================================================
// LCD CONNECTIONS
//==================================================

#define LCD_RS RD0
#define LCD_EN RD1

// RD4 -> LCD D4
// RD5 -> LCD D5
// RD6 -> LCD D6
// RD7 -> LCD D7


//==================================================
// BUTTONS
//==================================================

#define SET_BUTTON   RB0
#define UP_BUTTON    RB1
#define DOWN_BUTTON  RB2

#define BUZZER       RB3


//==================================================
// DS1307 ADDRESS
//==================================================

#define DS1307_WRITE  0xD0
#define DS1307_READ   0xD1


//==================================================
// VARIABLES
//==================================================

unsigned char current_hour = 0;
unsigned char current_minute = 0;
unsigned char current_second = 0;

unsigned char alarm_hour = 12;
unsigned char alarm_minute = 0;

unsigned char mode = 0;

unsigned char alarm_triggered = 0;


//==================================================
// DECIMAL TO BCD
//==================================================

unsigned char Decimal_To_BCD(unsigned char value)
{
    unsigned char tens;
    unsigned char units;

    tens = value / 10;
    units = value % 10;

    return (unsigned char)((tens << 4) | units);
}


//==================================================
// BCD TO DECIMAL
//==================================================

unsigned char BCD_To_Decimal(unsigned char value)
{
    unsigned char tens;
    unsigned char units;

    tens = (unsigned char)(value >> 4);
    units = (unsigned char)(value & 0x0F);

    return (unsigned char)((tens * 10) + units);
}


//==================================================
// LCD PULSE
//==================================================

void LCD_Pulse(void)
{
    LCD_EN = 1;

    __delay_us(5);

    LCD_EN = 0;

    __delay_us(50);
}


//==================================================
// SEND 4 BITS TO LCD
//==================================================

void LCD_Send4Bits(unsigned char data)
{
    RD4 = (unsigned char)((data >> 0) & 1);
    RD5 = (unsigned char)((data >> 1) & 1);
    RD6 = (unsigned char)((data >> 2) & 1);
    RD7 = (unsigned char)((data >> 3) & 1);

    LCD_Pulse();
}


//==================================================
// LCD COMMAND
//==================================================

void LCD_Command(unsigned char command)
{
    LCD_RS = 0;

    LCD_Send4Bits((unsigned char)(command >> 4));
    LCD_Send4Bits((unsigned char)(command & 0x0F));

    if(command == 0x01 || command == 0x02)
    {
        __delay_ms(2);
    }
}


//==================================================
// LCD CHARACTER
//==================================================

void LCD_Char(unsigned char data)
{
    LCD_RS = 1;

    LCD_Send4Bits((unsigned char)(data >> 4));
    LCD_Send4Bits((unsigned char)(data & 0x0F));
}


//==================================================
// LCD STRING
//==================================================

void LCD_String(const char *str)
{
    while(*str)
    {
        LCD_Char(*str);
        str++;
    }
}


//==================================================
// LCD CURSOR
//==================================================

void LCD_SetCursor(unsigned char row, unsigned char column)
{
    if(row == 1)
    {
        LCD_Command((unsigned char)(0x80 + column));
    }
    else
    {
        LCD_Command((unsigned char)(0xC0 + column));
    }
}


//==================================================
// DISPLAY TWO DIGITS
//==================================================

void LCD_TwoDigits(unsigned char number)
{
    LCD_Char((unsigned char)((number / 10) + '0'));
    LCD_Char((unsigned char)((number % 10) + '0'));
}


//==================================================
// LCD INITIALIZATION
//==================================================

void LCD_Init(void)
{
    __delay_ms(20);

    LCD_RS = 0;
    LCD_EN = 0;

    LCD_Send4Bits(0x03);
    __delay_ms(5);

    LCD_Send4Bits(0x03);
    __delay_us(150);

    LCD_Send4Bits(0x03);

    LCD_Send4Bits(0x02);

    LCD_Command(0x28);
    LCD_Command(0x0C);
    LCD_Command(0x06);
    LCD_Command(0x01);

    __delay_ms(2);
}


//==================================================
// I2C WAIT
//==================================================

void I2C_Wait(void)
{
    while((SSPCON2 & 0x1F) || (SSPSTAT & 0x04))
    {
        ;
    }
}


//==================================================
// I2C INITIALIZATION
//==================================================

void I2C_Init(void)
{
    TRISC3 = 1;
    TRISC4 = 1;

    SSPSTAT = 0x80;

    SSPCON = 0x28;

    SSPCON2 = 0x00;

    // 100 kHz I2C
    // Fosc = 4 MHz
    SSPADD = 9;
}


//==================================================
// I2C START
//==================================================

void I2C_Start(void)
{
    I2C_Wait();

    SEN = 1;

    while(SEN)
    {
        ;
    }
}


//==================================================
// I2C REPEATED START
//==================================================

void I2C_Restart(void)
{
    I2C_Wait();

    RSEN = 1;

    while(RSEN)
    {
        ;
    }
}


//==================================================
// I2C STOP
//==================================================

void I2C_Stop(void)
{
    I2C_Wait();

    PEN = 1;

    while(PEN)
    {
        ;
    }
}


//==================================================
// I2C WRITE
//==================================================

unsigned char I2C_Write(unsigned char data)
{
    I2C_Wait();

    SSPBUF = data;

    while(!SSPIF)
    {
        ;
    }

    SSPIF = 0;

    return ACKSTAT;
}


//==================================================
// I2C READ
//==================================================

unsigned char I2C_Read(unsigned char send_ack)
{
    unsigned char data;

    I2C_Wait();

    RCEN = 1;

    while(!SSPIF)
    {
        ;
    }

    SSPIF = 0;

    data = SSPBUF;

    I2C_Wait();

    if(send_ack)
    {
        ACKDT = 0;
    }
    else
    {
        ACKDT = 1;
    }

    ACKEN = 1;

    while(ACKEN)
    {
        ;
    }

    return data;
}


//==================================================
// SET DS1307 TIME
//==================================================

void DS1307_SetTime(unsigned char hour,
                    unsigned char minute,
                    unsigned char second)
{
    I2C_Start();

    I2C_Write(DS1307_WRITE);

    // Seconds register
    I2C_Write(0x00);

    // Seconds
    I2C_Write(Decimal_To_BCD(second));

    // Minutes
    I2C_Write(Decimal_To_BCD(minute));

    // Hours - 24 hour mode
    I2C_Write(Decimal_To_BCD(hour));

    I2C_Stop();
}


//==================================================
// READ DS1307 TIME
//==================================================

void DS1307_ReadTime(void)
{
    unsigned char sec;
    unsigned char min;
    unsigned char hr;

    I2C_Start();

    I2C_Write(DS1307_WRITE);

    // Start at seconds register
    I2C_Write(0x00);

    I2C_Restart();

    I2C_Write(DS1307_READ);

    // Read seconds
    sec = I2C_Read(1);

    // Read minutes
    min = I2C_Read(1);

    // Read hours
    hr = I2C_Read(0);

    I2C_Stop();

    // Remove CH bit
    sec = (unsigned char)(sec & 0x7F);

    // Remove 12/24 hour mode bits
    hr = (unsigned char)(hr & 0x3F);

    current_second = BCD_To_Decimal(sec);
    current_minute = BCD_To_Decimal(min);
    current_hour = BCD_To_Decimal(hr);
}


//==================================================
// UPDATE RTC AFTER TIME ADJUSTMENT
//==================================================

void Update_RTC_Time(void)
{
    DS1307_SetTime(
        current_hour,
        current_minute,
        current_second
    );
}


//==================================================
// NORMAL CLOCK DISPLAY
//==================================================

void Display_Normal(void)
{
    LCD_SetCursor(1, 0);

    LCD_String("TIME: ");

    LCD_TwoDigits(current_hour);
    LCD_Char(':');

    LCD_TwoDigits(current_minute);
    LCD_Char(':');

    LCD_TwoDigits(current_second);

    LCD_String(" ");

    LCD_SetCursor(2, 0);

    LCD_String("ALARM:");

    LCD_TwoDigits(alarm_hour);
    LCD_Char(':');
    LCD_TwoDigits(alarm_minute);

    LCD_String("   ");
}


//==================================================
// SETTING DISPLAY
//==================================================

void Display_Setting(void)
{
    LCD_Command(0x01);

    if(mode == 1)
    {
        LCD_SetCursor(1, 0);
        LCD_String("SET TIME HOUR");

        LCD_SetCursor(2, 0);
        LCD_String("HOUR: ");

        LCD_TwoDigits(current_hour);
    }

    else if(mode == 2)
    {
        LCD_SetCursor(1, 0);
        LCD_String("SET TIME MIN");

        LCD_SetCursor(2, 0);
        LCD_String("MIN: ");

        LCD_TwoDigits(current_minute);
    }

    else if(mode == 3)
    {
        LCD_SetCursor(1, 0);
        LCD_String("SET ALARM HOUR");

        LCD_SetCursor(2, 0);
        LCD_String("HOUR: ");

        LCD_TwoDigits(alarm_hour);
    }

    else if(mode == 4)
    {
        LCD_SetCursor(1, 0);
        LCD_String("SET ALARM MIN");

        LCD_SetCursor(2, 0);
        LCD_String("MIN: ");

        LCD_TwoDigits(alarm_minute);
    }
}


//==================================================
// BUTTON RELEASE DELAY
//==================================================

void Wait_Button_Release(void)
{
    __delay_ms(200);
}


//==================================================
// BUTTON HANDLING
//==================================================

void Check_Buttons(void)
{
    //================================================
    // SET / MODE BUTTON
    //================================================

    if(SET_BUTTON == 0)
    {
        __delay_ms(30);

        if(SET_BUTTON == 0)
        {
            mode++;

            if(mode > 4)
            {
                mode = 0;
            }

            Wait_Button_Release();
        }
    }


    //================================================
    // UP BUTTON
    //================================================

    if(UP_BUTTON == 0)
    {
        __delay_ms(30);

        if(UP_BUTTON == 0)
        {
            // TIME HOUR
            if(mode == 1)
            {
                current_hour++;

                if(current_hour >= 24)
                {
                    current_hour = 0;
                }

                Update_RTC_Time();
            }

            // TIME MINUTE
            else if(mode == 2)
            {
                current_minute++;

                if(current_minute >= 60)
                {
                    current_minute = 0;
                }

                Update_RTC_Time();
            }

            // ALARM HOUR
            else if(mode == 3)
            {
                alarm_hour++;

                if(alarm_hour >= 24)
                {
                    alarm_hour = 0;
                }
            }

            // ALARM MINUTE
            else if(mode == 4)
            {
                alarm_minute++;

                if(alarm_minute >= 60)
                {
                    alarm_minute = 0;
                }
            }

            Wait_Button_Release();
        }
    }


    //================================================
    // DOWN BUTTON
    //================================================

    if(DOWN_BUTTON == 0)
    {
        __delay_ms(30);

        if(DOWN_BUTTON == 0)
        {
            // TIME HOUR
            if(mode == 1)
            {
                if(current_hour == 0)
                {
                    current_hour = 23;
                }
                else
                {
                    current_hour--;
                }

                Update_RTC_Time();
            }

            // TIME MINUTE
            else if(mode == 2)
            {
                if(current_minute == 0)
                {
                    current_minute = 59;
                }
                else
                {
                    current_minute--;
                }

                Update_RTC_Time();
            }

            // ALARM HOUR
            else if(mode == 3)
            {
                if(alarm_hour == 0)
                {
                    alarm_hour = 23;
                }
                else
                {
                    alarm_hour--;
                }
            }

            // ALARM MINUTE
            else if(mode == 4)
            {
                if(alarm_minute == 0)
                {
                    alarm_minute = 59;
                }
                else
                {
                    alarm_minute--;
                }
            }

            Wait_Button_Release();
        }
    }
}


//==================================================
// ALARM
//==================================================

void Check_Alarm(void)
{
    if((current_hour == alarm_hour) &&
       (current_minute == alarm_minute))
    {
        if(alarm_triggered == 0)
        {
            BUZZER = 1;

            alarm_triggered = 1;
        }
    }
    else
    {
        BUZZER = 0;

        alarm_triggered = 0;
    }
}


//==================================================
// MAIN
//==================================================

void main(void)
{
    //================================================
    // PORT CONFIGURATION
    //================================================

    // LCD
    TRISD = 0x00;

    // Buttons
    TRISB0 = 1;
    TRISB1 = 1;
    TRISB2 = 1;

    // Buzzer
    TRISB3 = 0;

    // Enable PORTB internal pull-ups
    // NOTE: nRBPU is active LOW
    OPTION_REGbits.nRBPU = 0;

    BUZZER = 0;


    //================================================
    // LCD INITIALIZATION
    //================================================

    LCD_Init();


    //================================================
    // I2C INITIALIZATION
    //================================================

    I2C_Init();


    //================================================
    // SET INITIAL RTC TIME
    //================================================

#if SET_RTC_AT_STARTUP == 1

    DS1307_SetTime(
        INITIAL_HOUR,
        INITIAL_MINUTE,
        INITIAL_SECOND
    );

#endif


    __delay_ms(500);

    LCD_Command(0x01);


    //================================================
    // MAIN LOOP
    //================================================

    while(1)
    {
        // Read RTC
        DS1307_ReadTime();

        // Check buttons
        Check_Buttons();


        // NORMAL MODE
        if(mode == 0)
        {
            Display_Normal();

            Check_Alarm();
        }

        // SETTING MODE
        else
        {
            Display_Setting();

            // Turn buzzer off while setting
            BUZZER = 0;
        }


        __delay_ms(200);
    }
}