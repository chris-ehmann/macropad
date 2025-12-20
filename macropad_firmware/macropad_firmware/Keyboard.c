/*
             LUFA Library
     Copyright (C) Dean Camera, 2017.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2017  Dean Camera (dean [at] fourwalledcubicle [dot] com)
  Copyright 2010  Denver Gingerich (denver [at] ossguy [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

#include "Keyboard.h"
#include <stdint.h>
#include <util/delay.h>

/** Indicates what report mode the host has requested, true for normal HID reporting mode, \c false for special boot
 *  protocol reporting mode.
 */
static bool UsingReportProtocol = true;

/** Current Idle period. This is set by the host via a Set Idle HID class request to silence the device's reports
 *  for either the entire idle duration, or until the report status changes (e.g. the user presses a key).
 */
static uint16_t IdleCount = 500;

/** Current Idle period remaining. When the IdleCount value is set, this tracks the remaining number of idle
 *  milliseconds. This is separate to the IdleCount timer and is incremented and compared as the host may request
 *  the current idle period via a Get Idle HID class request, thus its value must be preserved.
 */
static uint16_t IdleMSRemaining = 0;

#define DEBOUNCE_THRESHOLD  10
#define LAYER_SWITCH        0x00
#define NUM_COLS            3
#define NUM_ROWS            2
#define PIN_DEF(port, pin)  { &PIN##port, &DDR##port, &PORT##port, pin }
#define layer_count(a)      (sizeof(a) / sizeof(a[0]))
	
typedef struct
{
	volatile uint8_t *pin;
	volatile uint8_t *ddr;
	volatile uint8_t *port;
	uint8_t pin_n;
} gpio_pin_t;

const gpio_pin_t COLS[NUM_COLS]							   = { PIN_DEF(B, 6), PIN_DEF(C, 6), PIN_DEF(C, 7) };
const gpio_pin_t ROWS[NUM_ROWS]						       = { PIN_DEF(B, 4), PIN_DEF(B, 5) };
uint8_t debounce_keys[NUM_ROWS][NUM_COLS]				   = {{0, 0, 0}, {0, 0, 0}}; // Matrix to store the amount of cycles a key has been held down for. Caps out at DEBOUNCE_THRESHOLD to avoid integer overflow
	
// Matrix that stores each layer of our keypad. Key in row 0 column 0 is marked as LAYER_SWITCH as it corresponds
// to a special key on the keyboard that swaps between layers (and doesn't actually send any data)
const unsigned char keycodes[][NUM_ROWS][NUM_COLS] PROGMEM = 
  															 { { {LAYER_SWITCH, HID_KEYBOARD_SC_4_AND_DOLLAR, HID_KEYBOARD_SC_5_AND_PERCENTAGE},
															     {HID_KEYBOARD_SC_1_AND_EXCLAMATION, HID_KEYBOARD_SC_2_AND_AT, HID_KEYBOARD_SC_3_AND_HASHMARK}, },
														
															   { {LAYER_SWITCH, HID_KEYBOARD_SC_9_AND_OPENING_PARENTHESIS, HID_KEYBOARD_SC_0_AND_CLOSING_PARENTHESIS},
															     {HID_KEYBOARD_SC_6_AND_CARET, HID_KEYBOARD_SC_7_AND_AMPERSAND, HID_KEYBOARD_SC_8_AND_ASTERISK} } };
												   
/*  Keyboard layout, as a diagram (! designates SWAP layer key)
	NOTE: Only top layer keys are shown
   +-----------------------------+
   |         |         |         |
   |    !    |    4    |    5    |
   |         |         |         |
   +-----------------------------+
   |         |         |         |
   |    1    |    2    |    3    |
   |         |         |         |
   +-----------------------------+
*/

// Set up appropriate GPIO pins for keypad matrix
static void io_config()
{
	// Initially set up our row pins (which will be used as output pins) as input (Hi-Z)
	for(int i = 0; i < NUM_ROWS; i++)
	{
		*ROWS[i].ddr &= ~(1 << ROWS[i].pin_n);
	}
	
	// Set up our column pins to be input and activate the internal pull-up resistor
	for(int i = 0; i < NUM_COLS; i++)
	{
		*COLS[i].ddr &= ~(1 << COLS[i].pin_n);
		*COLS[i].port |= (1 << COLS[i].pin_n);
	}
}

static bool is_key_pressed(uint8_t row, uint8_t col)
{
	return debounce_keys[row][col] == DEBOUNCE_THRESHOLD;
}

// Performs a scan over the entire keyboard matrix and updates key states accordingly
static void scan_matrix()
{
	for(uint8_t row = 0; row < NUM_ROWS; row++)
	{
		// Set current row to output; drive it low
		*ROWS[row].ddr |= (1 << ROWS[row].pin_n);
		*ROWS[row].port &= ~(1 << ROWS[row].pin_n);
			
		// Slight delay to allow row to settle
		_delay_us(20);
			
		// Iterate over each column to detect which keys are pressed in this row
		for(uint8_t col = 0; col < NUM_COLS; col++)
		{
			uint8_t pressed = 0;
			pressed = (*COLS[col].pin & (1 << COLS[col].pin_n)) == 0; // Check if pin corresponding to column is low (meaning the key is pressed) or high (key is not pressed)
			if(pressed)
			{
				if(debounce_keys[row][col] < DEBOUNCE_THRESHOLD)
				{
					debounce_keys[row][col]++;
				}
				continue;
			}

			if(debounce_keys[row][col] > 0)
			{
				debounce_keys[row][col]--;
			}
		}
		
		// Switch row back to being input
		*ROWS[row].ddr &= ~(1 << ROWS[row].pin_n);
	}	
}

int main(void)
{
	SetupHardware();
	io_config();
	GlobalInterruptEnable();

	while(1)
	{
		scan_matrix();
		
		// Send data (either pressed keys, or nothing) through USB connection
		HID_Task();
		USB_USBTask();
		
		// Half a millisecond delay between full scans corresponds to about 5ms to fully debounce a key (given DEBOUNCE_THRESHOLD = 10)
		_delay_us(500);
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
	USB_Init();
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host sets the current configuration
 *  of the USB device after enumeration, and configures the keyboard device endpoints.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	/* Setup HID Report Endpoints */
	ConfigSuccess &= Endpoint_ConfigureEndpoint(KEYBOARD_IN_EPADDR, EP_TYPE_INTERRUPT, KEYBOARD_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(KEYBOARD_OUT_EPADDR, EP_TYPE_INTERRUPT, KEYBOARD_EPSIZE, 1);

	/* Turn on Start-of-Frame events for tracking HID report period expiry */
	USB_Device_EnableSOFEvents();
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
	/* Handle HID Class specific requests */
	switch (USB_ControlRequest.bRequest)
	{
		case HID_REQ_GetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				USB_KeyboardReport_Data_t KeyboardReportData;

				/* Create the next keyboard report for transmission to the host */
				CreateKeyboardReport(&KeyboardReportData);

				Endpoint_ClearSETUP();

				/* Write the report data to the control endpoint */
				Endpoint_Write_Control_Stream_LE(&KeyboardReportData, sizeof(KeyboardReportData));
				Endpoint_ClearOUT();
			}

			break;
		case HID_REQ_SetReport:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Wait until the LED report has been sent by the host */
				while (!(Endpoint_IsOUTReceived()))
				{
					if (USB_DeviceState == DEVICE_STATE_Unattached)
					  return;
				}

				Endpoint_ClearOUT();
				Endpoint_ClearStatusStage();
			}

			break;
		case HID_REQ_GetProtocol:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Write the current protocol flag to the host */
				Endpoint_Write_8(UsingReportProtocol);

				Endpoint_ClearIN();
				Endpoint_ClearStatusStage();
			}

			break;
		case HID_REQ_SetProtocol:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				/* Set or clear the flag depending on what the host indicates that the current Protocol should be */
				UsingReportProtocol = (USB_ControlRequest.wValue != 0);
			}

			break;
		case HID_REQ_SetIdle:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				/* Get idle period in MSB, IdleCount must be multiplied by 4 to get number of milliseconds */
				IdleCount = ((USB_ControlRequest.wValue & 0xFF00) >> 6);
			}

			break;
		case HID_REQ_GetIdle:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Write the current idle duration to the host, must be divided by 4 before sent to host */
				Endpoint_Write_8(IdleCount >> 2);

				Endpoint_ClearIN();
				Endpoint_ClearStatusStage();
			}

			break;
	}
}

/** Event handler for the USB device Start Of Frame event. */
void EVENT_USB_Device_StartOfFrame(void)
{
	/* One millisecond has elapsed, decrement the idle time remaining counter if it has not already elapsed */
	if (IdleMSRemaining)
	  IdleMSRemaining--;
}

/** Fills the given HID report data structure with the next HID report to send to the host.
 *
 *  \param[out] ReportData  Pointer to a HID report data structure to be filled
 */
void CreateKeyboardReport(USB_KeyboardReport_Data_t* const ReportData)
{
	uint8_t UsedKeyCodes = 0;
	uint8_t active_layer = is_key_pressed(0, 0) ? 1 : 0;

	/* Clear the report contents */
	memset(ReportData, 0, sizeof(USB_KeyboardReport_Data_t));
	
	for(int row = 0; row < NUM_ROWS; row++)
	{
		for(int col = 0; col < NUM_COLS; col++)
		{
			// The current keypad this firmware is set up for is only 6 keys (5 excluding the layer switch key), 
			// but just to be safe we limit the amount of keys sent to the maximum that can be sent in one report (which is 6, see https://www.usb.org/document-library/device-class-definition-hid-111)
			if(is_key_pressed(row, col) && UsedKeyCodes < 6)
			{
				if(row == 0 && col == 0)
				{
					continue;
				}
				
				ReportData->KeyCode[UsedKeyCodes++] = pgm_read_byte(&keycodes[active_layer][row][col]);
			}
		}
	}
}

/** Sends the next HID report to the host, via the keyboard data endpoint. */
void SendNextReport(void)
{
	static USB_KeyboardReport_Data_t PrevKeyboardReportData;
	USB_KeyboardReport_Data_t        KeyboardReportData;
	bool                             SendReport = false;

	/* Create the next keyboard report for transmission to the host */
	CreateKeyboardReport(&KeyboardReportData);

	/* Check if the idle period is set and has elapsed */
	if (IdleCount && (!(IdleMSRemaining)))
	{
		/* Reset the idle time remaining counter */
		IdleMSRemaining = IdleCount;

		/* Idle period is set and has elapsed, must send a report to the host */
		SendReport = true;
	}
	else
	{
		/* Check to see if the report data has changed - if so a report MUST be sent */
		SendReport = (memcmp(&PrevKeyboardReportData, &KeyboardReportData, sizeof(USB_KeyboardReport_Data_t)) != 0);
	}

	/* Select the Keyboard Report Endpoint */
	Endpoint_SelectEndpoint(KEYBOARD_IN_EPADDR);

	/* Check if Keyboard Endpoint Ready for Read/Write and if we should send a new report */
	if (Endpoint_IsReadWriteAllowed() && SendReport)
	{
		/* Save the current report data for later comparison to check for changes */
		PrevKeyboardReportData = KeyboardReportData;

		/* Write Keyboard Report Data */
		Endpoint_Write_Stream_LE(&KeyboardReportData, sizeof(KeyboardReportData), NULL);

		/* Finalize the stream transfer to send the last packet */
		Endpoint_ClearIN();
	}
}

/** Function to manage HID report generation and transmission to the host, when in report mode. */
void HID_Task(void)
{
	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
	  return;

	/* Send the next keypress report to the host */
	SendNextReport();
}

