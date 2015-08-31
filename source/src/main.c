/****************************************************************//*
	@brief Module Main
	
	Start-up and setup sequence
	Main loop
    
    
********************************************************************/

#include <string.h>
#include "MDR32F9Qx_iwdg.h"
#include "hw_utils.h"
#include "dwt_delay.h"
#include "lcd_melt20s4.h"
#include "lcd_contrast.h"
#include "buttons.h"
#include "led.h"
#include "encoder.h"
#include "dac.h"
#include "systick.h"
#include "adc.h"
#include "external_adc.h"
#include "power_monitor.h"
#include "gui_top.h"
#include "eeprom.h"
#include "sound.h"
#include "utils.h"

/*
	timer3 -> buzzer
	timer2 -> contrast charge pump
	timer1 -> DMA for waveform
*/

static void displayPowerCycles(void) {
	uint32_t temp32u;
    char str[10];
	
	temp32u = EE_GetPowerCyclesCount();
	i32toa_align_right((int32_t)temp32u, str, 10, 1, -1);
	LCD_InsertCharsXY(11, 3, &str[0], 9);
}


SoftTimer16b_t adcUpdateTimer;

int main(void) {
	uint8_t system_settings_ok;
	uint8_t settings_ok;
	uint32_t dwt_time_mark;
	
    // Initialize system 
	hw_Setup_CPU_Clock();
	hw_Init_DMA();
    DWT_Init();
	dwt_time_mark = DWT_StartDelayUs(1000000);
	
    // Initialize software modules. 
    // All required HW setup is performed inside modules.
	
    // LCD and keyboard controller
    LCD_Init();
	// LCD contrast DC-DC controller
	LCD_InitContrastBooster();
    // Button events driver
    InitButtons();
    // LED driver
    LED_Init();
    // Incremental encoder driver
    Encoder_Init();
    // ADC driver
    ADC_Initialize();
    // SPI ADC driver
    ExtADC_Initialize();
	// DAC driver
    DAC_Initialize();
	// Buzzer
	Sound_Init();
	
	// Get mode of operation
	LCD_CaptureKeyboard();	
	device_mode = (GetRawButtonState() & KEY_OUTPUT_CTRL) ? MODE_CALIBRATION : MODE_NORMAL;
	// Restore system settings
	system_settings_ok = EE_RestoreSystemSettings((device_mode == MODE_CALIBRATION));
	if (system_settings_ok) {
		// Apply system settings:
        if (device_mode != MODE_CALIBRATION)
        {
            // Do not calibrate DAC - use raw output
            // This allows re-calibration when previous was incorrect
            DAC_ApplyCalibration();
        }
        // Raw ADC codes are used for calibration - safe to apply wrong calibration
		ADC_LC_ApplyCalibration();
        ADC_LV_ApplyCalibration();
		ExtADC_ApplyCalibration();
        LCD_RestoreContrastSetting();
        Sound_RestoreSetting();
	} else {
		// Default
		system_settings.output_mode = OUTPUT_UPDATE_NORMAL;
	}
	// Restore settings
	if (device_mode == MODE_NORMAL) {
		settings_ok = EE_RestoreSettings();
		// Use defaults when starting with cancel button pressed
		if ((settings_ok) && (!(GetRawButtonState() & KEY_ESC))) {
			// Apply settings
			DAC_RestoreSettings();
			// other modules
		}
	}
	// Display power on cycles count
	if (GetRawButtonState() & KEY_ESC) {
		displayPowerCycles();
	}
		
    // Setup software timers
	memset(&adcUpdateTimer, 0, sizeof(adcUpdateTimer));
	adcUpdateTimer.top = 4;
	adcUpdateTimer.enabled = 1;
    
	// Power supply monitor
	PowerMonitor_Init();
	// Start ISR-based syncronizer
	Systick_Init();
	
	// Wait a bit for greeting message
	while (DWT_DelayInProgress(dwt_time_mark));
	
	if ((system_settings_ok == 0) && (device_mode == MODE_NORMAL)) {
		// Show error message!
		LCD_Clear();
		LCD_PutStringXY(0,0,"Необходима");
		LCD_PutStringXY(0,1,"     калибровка!");
		// Wait a bit more
		DWT_DelayUs(1000000);
	}
	
	// Setup and start watchdog
    hw_SetupWatchdog(25000);
	// GUI initialization depends on device mode
    GUI_Init();
	// Start main loop
	mainLoopTimer.enabled = 1;	
	Sound_Event(SE_Start);
	
	while (1) {
		// Synchronize
		if (mainLoopTimer.flags.ovfl) {
			__disable_irq();
			mainLoopTimer.flags.ovfl = 0;
			__enable_irq();
            
			IWDG_ReloadCounter();
            
            processSoftTimer16b(&adcUpdateTimer);
			
            if ((adcUpdateTimer.flags.ovfl) || (device_mode == MODE_CALIBRATION)) {
                adcUpdateTimer.flags.ovfl = 0;
                ADC_UpdateLoopVoltage();
                ExtADC_UpdateCurrent();
            }
			ADC_UpdateLoopCurrent();
			ADC_UpdateLoopMonitor();
            
            LCD_CaptureKeyboard();
            ProcessButtons();
			Encoder_UpdateDelta();
			
			GUI_Process();

		}
	}
}




