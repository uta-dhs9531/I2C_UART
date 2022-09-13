// I2C Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz
// Stack:           4096 bytes (needed for snprintf)

// Hardware configuration:
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port
//   Configured to 115,200 baud, 8N1

// I2C devices on I2C bus 0 with 2kohm pullups on SDA (PB3) and SCL (PB2)

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "tm4c123gh6pm.h"
#include "clock.h"
#include "uart0.h"
#include "i2c0.h"
#include "gpio.h"

// Range of polled devices
// 0 for general call, 1-3 for compatible i2c variants
// 120-123 are for 10-bit address mode
// 123-127 reserved
#define MIN_I2C_ADD 0x20
#define MAX_I2C_ADD 0x2F

#define IO_EXPANDER_GREEN_LED_MASK   (0x01)
#define IO_EXPANDER_RED_LED_MASK     (0x02)
#define IO_EXPANDER_PUSH_BUTTON_MASK (0x04)
#define ADDRESSI2C                   (0x20)

#define OPCODER  0x47
#define OPCODEW  0x46
#define IODIR    0x00
#define IPOL     0x01
#define GPINTEN  0x02
#define DEFVAL   0x03
#define INTCON   0x04
#define IOCON    0x05
#define GPPU     0x06
#define INTF     0x07
#define INTCAP   0x08
#define GPIO     0x09
#define OLAT     0x0A
#define DEFAULT  0x00

// PortB masks
#define SDA_MASK 8
#define SCL_MASK 4

// Pins
#define I2C0SCL PORTB,2
#define I2C0SDA PORTB,3

#define GPIOE_VECTOR_INTERRUPT 20

uint8_t INTCAPregValue;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();
}

void getsUart0(char str[], uint8_t size)
{
    uint8_t count = 0;
    bool end = false;
    char c;
    while(!end)
    {
        c = getcUart0();
        end = (c == 13) || (count == size);
        if (!end)
        {
            if ((c == 8 || c == 127) && count > 0)
                count--;
            if (c >= ' ' && c < 127)
                str[count++] = c;
        }
    }
    str[count] = '\0';
}

uint8_t asciiToUint8(const char str[])
{
    uint8_t data;
    if (str[0] == '0' && tolower(str[1]) == 'x')
        sscanf(str, "%hhx", &data);
    else
        sscanf(str, "%hhu", &data);
    return data;
}

void GPIOE_isr()
{
    writeI2c0Register(ADDRESSI2C, GPIO, IO_EXPANDER_GREEN_LED_MASK);
    waitMicrosecond(1000000);
    writeI2c0Register(ADDRESSI2C, GPIO, IO_EXPANDER_RED_LED_MASK);
    clearPinInterrupt(PORTE, 3);
    INTCAPregValue= readI2c0Register(ADDRESSI2C, INTCAP);

}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

#define MAX_CHARS 80
int main(void)
{
    char strInput[MAX_CHARS+1];
    char* token;
    char str[80];
    uint8_t i;
    uint8_t add;
    uint8_t reg;
    uint8_t data;
    uint8_t arg;
    bool regUsed;
    bool found;
    bool valid;
    bool ok;

    // Initialize hardware
    initHw();
    initUart0();
    initI2c0();

    // Setup UART0 baud rate
    setUart0BaudRate(115200, 40e6);

    // Enable clocks port E1 as an Input and enable Interrupt on Tiva

    enablePort(PORTE);
    _delay_cycles(3);
    selectPinDigitalInput(PORTE, 1);
    enablePinPullup(PORTE, 1);

    enableNvicInterrupt(GPIOE_VECTOR_INTERRUPT);
    disablePinInterrupt(PORTE, 1);
    selectPinInterruptLowLevel(PORTE,1);
    enablePinInterrupt(PORTE, 1);
    clearPinInterrupt(PORTE, 1);
    putsUart0("I2C0 Utility\n");

    //Setting I2C Expander config
    writeI2c0Register(ADDRESSI2C, GPINTEN, DEFAULT);
    writeI2c0Register(ADDRESSI2C, IODIR, IO_EXPANDER_PUSH_BUTTON_MASK);
    writeI2c0Register(ADDRESSI2C, DEFVAL, IO_EXPANDER_PUSH_BUTTON_MASK);
    writeI2c0Register(ADDRESSI2C, INTCON, IO_EXPANDER_PUSH_BUTTON_MASK);
    writeI2c0Register(ADDRESSI2C, GPINTEN, IO_EXPANDER_PUSH_BUTTON_MASK);
    writeI2c0Register(ADDRESSI2C, GPIO, IO_EXPANDER_RED_LED_MASK);

    while (true)
    {
        getsUart0(strInput, MAX_CHARS);
        token = strtok(strInput, " \r\n");
        ok = token != NULL;
        valid = false;
        regUsed = false;
        if (strcmp(token, "write") == 0)
        {
            valid = true;
            // Add
            token = strtok(NULL, " ");
            ok = ok && token != NULL;
            add = asciiToUint8(token);
            // Reg or Data
            token = strtok(NULL, " ");
            ok = ok && token != NULL;
            arg = asciiToUint8(token);
            // Optional Data
            token = strtok(NULL, " \r\n");
            // Determine if write add reg data or write add data
            if (strlen(token) > 0)
            {
                reg = arg;
                data = asciiToUint8(token);
                regUsed = true;
            }
            else
                data = arg;
            if (ok)
            {
                if (regUsed)
                {
                    writeI2c0Register(add, reg, data);
                    snprintf(str, sizeof(str), "Writing 0x%02"PRIX8" to address 0x%02"PRIX8", register 0x%02"PRIX8"\n", data, add, reg);
                }
                else
                {
                    writeI2c0Data(add, data);
                    snprintf(str, sizeof(str), "Writing 0x%02"PRIX8" to address 0x%02"PRIX8"\n", data, add);
                }
                putsUart0(str);
            }
            else
                putsUart0("Error in write command arguments\n");
        }
        if (strcmp(token, "read") == 0)
        {
            valid = true;
            // Add
            token = strtok(NULL, " ");
            ok = ok && token != NULL;
            add = asciiToUint8(token);
            // Optional register
            token = strtok(NULL, " \r\n");
            // Determine if read add or read add reg
            if (strlen(token) > 0)
            {
                reg = asciiToUint8(token);
                regUsed = true;
            }
            if (ok)
            {
                if (regUsed)
                {
                    data = readI2c0Register(add, reg);
                    snprintf(str, sizeof(str), "Read 0x%02hhX from address 0x%02"PRIX8", register 0x%02"PRIX8"\n", data, add, reg);
                }
                else
                {
                    data = readI2c0Data(add);
                    snprintf(str, sizeof(str), "Read 0x%02"PRIX8" from address 0x%02"PRIX8"\n", data, add);
                }
                putsUart0(str);
            }
            else
                putsUart0("Error in read command arguments\n");
        }
        if (strcmp(token, "poll") == 0)
        {
            valid = true;
            found = false;
            putsUart0("Devices found: ");
            for (i = MIN_I2C_ADD; i <= MAX_I2C_ADD; i++)
            {
                if (pollI2c0Address(i))
                {
                    found = true;
                    snprintf(str, sizeof(str), "0x%02"PRIX8" ", i);
                    putsUart0(str);
                }
            }
            if (!found)
                putsUart0("(none)");
            putsUart0("\n");
        }
        if (strcmp(token, "help") == 0)
        {
            valid = true;
            putsUart0("Commands:\n");
            putsUart0("    poll                  Poll devices on bus\n");

            putsUart0("    read ADD REG          Read a byte from a device register\n");
            putsUart0("    write ADD REG DATA    Write a byte to a device register\n");

            putsUart0("    read ADD              Read a byte from a device\n");
            putsUart0("    write ADD DATA        Write a byte to a device\n");
        }
        if (!valid)
            putsUart0("Invalid command\n");
    }
}
