/* avr */
#include <avr/io.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdint.h>

#include "LUFA/Common/Common.h"
#include "descriptors.h"

#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>

void EVENT_USB_Device_Connect(void);
void EVENT_USB_Device_Disconnect(void);
void EVENT_USB_Device_ConfigurationChanged(void);
void EVENT_USB_Device_ControlRequest(void);

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
    SWITCH_1,
    SWITCH_2,
    SWITCH_3,
    SWITCH_4,
} keyswitch_t;

/* setting col and row to 1 */
#define COL0 (1 << PORTF0) 
#define COL1 (1 << PORTF1)

#define ROW0 (1 << PORTF4)
#define ROW1 (1 << PORTF5)

#define DELAY_MS 10
#define MAX_SWITCHES 4

uint8_t matrix_scan();

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
 */

static FILE USBSerialStream;

void daoboard_init(void) {
    // Set columns as input
    DDRF &= ~(COL0 | COL1);

    // Enable the internal pull-up resistors
    PORTF |= (COL0 | COL1);

    // Set rows as output
    DDRF |= (ROW0 | ROW1);

    // Drive ROWs as LOW (initially)
    PORTF &= ~(ROW0 | ROW1);

    MCUSR &= ~(1 << WDRF);
    wdt_disable();
	clock_prescale_set(clock_div_1);
    USB_Init();
    
    CDC_Device_CreateStream(&usb_cdc_interface, &USBSerialStream);
    GlobalInterruptEnable();
}

int main(void) {
    daoboard_init();

    // scanning for a key press
    for(;;) {
        
        CDC_Device_USBTask(&usb_cdc_interface);
        USB_USBTask();
        
        CDC_Device_ReceiveByte(&usb_cdc_interface);
        _delay_ms(1000);
        CDC_Device_SendString(&usb_cdc_interface, "ZYHRU\r\n");
        
        #if 0
        uint8_t key_pressed = matrix_scan();
        switch(key_pressed) {
            case SWITCH_1:
                break;
            case SWITCH_2:
                break;
            case SWITCH_3:
                break;
            case SWITCH_4:
                break;

        }

        _delay_ms(DELAY_MS);
        #endif

    }
    return 0;
}

//TODO: Redo this
uint8_t matrix_scan(void) {
    uint8_t key = 0;
    
    PORTF |= COL0;
    PORTF &= ~COL1;
    _delay_ms(DELAY_MS);

    if(PINF & ROW0) {
        // sw1
        key = 0;
    }

    if(PINF & ROW1) {
        // sw2
        key = 1;
    }

    // disable COL0 now
    PORTF &= ~COL0;
    PORTF |= COL1;
    _delay_ms(DELAY_MS);
    
    if(PINF & ROW0) {
        // sw3
        key = 2;
    }

    if(PINF & ROW0) {
        // sw4
        key = 3;
    }

    PORTF &= ~(COL0 | COL1);
    return key;
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
