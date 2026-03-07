/* avr */
#include <avr/io.h>
#include <avr/cpufunc.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>

#include "LUFA/Common/Common.h"
#include "descriptors.h"

#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>

void EVENT_USB_Device_Connect(void);
void EVENT_USB_Device_Disconnect(void);
void EVENT_USB_Device_ConfigurationChanged(void);
void EVENT_USB_Device_ControlRequest(void);

uint8_t pinf_state;


USB_ClassInfo_CDC_Device_t usb_cdc_interface =
	{ .Config =
			{
				.ControlInterfaceNumber   = INTERFACE_ID_CDC_CCI,
				.DataINEndpoint           =
					{
						.Address          = CDC_TX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.DataOUTEndpoint =
					{
						.Address          = CDC_RX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.NotificationEndpoint =
					{
						.Address          = CDC_NOTIFICATION_EPADDR,
						.Size             = CDC_NOTIFICATION_EPSIZE,
						.Banks            = 1,
					},
			},
	};

typedef enum {
    DEFAULT,  // 0
    SWITCH_1, // 1
    SWITCH_2, // 2
    SWITCH_3, // 3
    SWITCH_4, // 4
} KeySwitch;

typedef struct {
    char* str;
    KeySwitch sw;
} KeyLayout;

#define ROWS 2
#define COLS 2
KeyLayout kl[ROWS][COLS] = {
    { {}, {}              },
    { {}, {}              },
};

/* setting col and row to 1 */
#define COL0 (1 << PORTF0) 
#define COL1 (1 << PORTF1)

#define ROW0 (1 << PORTF4)
#define ROW1 (1 << PORTF5)

#define DELAY_MS 10
#define MAX_SWITCHES 4

/*
 * COL are input
 * COL are HIGH
 * ROW are LOW
 * ROW are output
 *
 * When scanning the keyboard matrix check
 * if COL and ROW are BOTH high
 *
 * When scanning one column, disable the other columns 
 * to avoid two button presses
 *
 * DDRx 1 = Output
 * DDRx 0 = Input
 *
 *
 */

static FILE USBSerialStream;

/*
 * NOTE: Reading the Pin Value:
 * Port pin can be read throug the PINF Register Bit
 * Example:
 * PINXN := x = letter of the register, n = number of pin
 * PINF0
 * PINF1
 * PINF2
 *
 * When reading back a software assigned pin value 
 * you must use a nop instruction.
 *
 *
 */
static void send_buffer(uint8_t state, uint8_t reg, double ms)  {
    char buffer[50];
    char str[10];
    
    if(reg == PORTF) strcpy(str, "PORTF");
    if(reg == PINF) strcpy(str, "PINF");
    if(reg == MCUCR) strcpy(str, "MCUCR");
    state = reg;
        
     _NOP(); // Synchronization
    sprintf(buffer, "TEST: %s (2/27/26): 0x%02X\r\n", str, state);
    CDC_Device_SendString(&usb_cdc_interface, buffer);
    _delay_ms(ms);
}

static void send_pinf_buffer(char *msg, double ms) {
    char buffer[50];
    _NOP();
    snprintf(buffer, sizeof(buffer), "%s\r\n", msg);
    CDC_Device_SendString(&usb_cdc_interface, buffer);
    _delay_ms(ms);
}

/*
 *
PORTF (2/27/26): 0x30
MCUCR (2/27/26): 0x00
PINF (2/27/26): 0x02
 *
 */
void daoboard_init(void) {
    // input + pull-ups
    PORTF |= (1 << PF1 | 1 << PF0);
    DDRF &= ~((1 << PF1) | (1 << PF0)); 

    // disabling jtag bits
    MCUCR |= (1 << JTD);
    MCUCR |= (1 << JTD);
   
    // output high
    PORTF |= (1 << PF5 | 1 << PF4);
    DDRF |= (1 << PF5 | 1 << PF4);  
   
    MCUSR &= ~(1 << WDRF);
    wdt_disable();
	clock_prescale_set(clock_div_1);
    USB_Init();
    
    CDC_Device_CreateStream(&usb_cdc_interface, &USBSerialStream);
    GlobalInterruptEnable();
}

// set PORTFx to 0 as input
void disable_pull_ups(uint8_t pin) { 
    PORTF &= ~(pin);
}

// set PORTFx to 1 as input
void enable_pull_ups(uint8_t pin) { 
    PORTF |= pin;
}

void send_string(char *msg) {
    CDC_Device_SendString(&usb_cdc_interface, msg);
}

/*
 * Drive COLUMNS low
 * PF0
 * Read PINF and check if PF4 is low: if so sw1 is pressed
 * Read PINF and check if PF5 is low: is so sw3 is pressed
 *
 *
 * PF1
 * Read PINF and check if PF4 is low: if so sw2 is pressed
 * Read PINF and check if PF5 is low: is so sw4 is pressed
 */
uint8_t daoboard_scan(void) {
    uint8_t key = 0;
   
    // pinf will 0x73 -> 0b1110011
    // 0b01110011 AND
    // 0b00010000
    // ----------
    // 0b00010000
    // !0b00010000 = 0
   
    uint8_t portf_state;
    uint8_t pin_state;
    
    // ================================== //
    /* scanning COL0  */
    PORTF &= ~(1 << PF4); // ROW0
    _NOP();

    // 0,0
    if(!(PINF & (1 << PF0))) {
        key = 1;     
        send_pinf_buffer("(0,0)", 20);
    }

    // 0,1
    if(!(PINF & (1 << PF1))) {
        send_pinf_buffer("(0,1)", 20);
        key = 3;     
    }
    
    PORTF |= (1 << PF4);
    _NOP();
   // --------------------------------- // 

    /* scanning COL1 */
    // 1,0
    PORTF &= ~(1 << PF5); // Drive PF5 LOW
    _NOP();
    if(!(PINF & (1 << PF0))) {
        send_pinf_buffer("(1,0)", 20);
        key = 2;     
    }
    
    // 1,1
    if(!(PINF & (1 << PF1))) {
        send_pinf_buffer("(1,1)", 20);
        key = 4;     
    }
    PORTF |= (1 << PF5);
    _NOP();
    
    return key;
}

int main(void) {
    daoboard_init();
    uint8_t pin_state;
    uint8_t port_state;
    uint8_t jtag_state;
    
    char pin_buffer[50];
    char port_buffer[50];

    for(;;) {
        CDC_Device_USBTask(&usb_cdc_interface);
        USB_USBTask();
        CDC_Device_ReceiveByte(&usb_cdc_interface);
        //send_buffer(pin_state, PINF, 500);
        //send_buffer(port_state, PORTF, 500);
        //send_buffer(jtag_state, MCUCR, 500);

        uint8_t key_pressed = daoboard_scan();
        switch(key_pressed) {
            case SWITCH_1:
                CDC_Device_SendString(&usb_cdc_interface, "Pressed Switch 1\r\n");
                _delay_ms(10);
                break;
            case SWITCH_2:
                CDC_Device_SendString(&usb_cdc_interface, "Pressed Switch 2\r\n");
                _delay_ms(10);
                break;
            case SWITCH_3:
                CDC_Device_SendString(&usb_cdc_interface, "Pressed Switch 3\r\n");
                _delay_ms(10);
                break;
            case SWITCH_4:
                CDC_Device_SendString(&usb_cdc_interface, "Pressed Switch 4\r\n");
                _delay_ms(10);
                break;
            default:
                break;
        }
    }
    return 0;
}

// COL0 has sw1 and sw2
// COL1 has sw3 and sw4

/*
* Rows are inputs with pull-ups 
* Columns are output driven LOW one at a time during scanning
* 
* When scanning the MCU pulls one columns LOW and checks to
* see if the row went low
*
* the row would go high bc its being tied to the col which 
* is already high
*
*
* DDRF = Direction switch (input ← → output)
* PORTF = Output knob (LOW ← → HIGH) OR pull-up switch
* PINF = Voltage meter (reads current state)
*/

void EVENT_USB_Device_Connect(void)
{
	//LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	//LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&usb_cdc_interface);

	//LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&usb_cdc_interface);
}

/** CDC class driver callback function the processing of changes to the virtual
 *  control lines sent from the host..
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t *const CDCInterfaceInfo)
{
	/* You can get changes to the virtual CDC lines in this callback; a common
	   use-case is to use the Data Terminal Ready (DTR) flag to enable and
	   disable CDC communications in your application when set to avoid the
	   application blocking while waiting for a host to become ready and read
	   in the pending data from the USB endpoints.
	*/
	bool HostReady = (CDCInterfaceInfo->State.ControlLineStates.HostToDevice & CDC_CONTROL_LINE_OUT_DTR) != 0;

	(void)HostReady;
}
