/*******************************************************************
	Module power_monitor
	
	Power supply monitor
	Saving settings when power is shut down

********************************************************************/
#include <string.h>
#include "MDR32F9Qx_config.h"
#include "MDR32F9Qx_port.h"
#include "MDR32F9Qx_comp.h"
#include "MDR32F9Qx_dac.h"
#include "MDR32F9Qx_iwdg.h"
#include "hw_utils.h"
#include "eeprom.h"
#include "dac.h"
#include "adc.h"
#include "external_adc.h"
#include "power_monitor.h"
#include "lcd_contrast.h"
#include "sound.h"
#include "dwt_delay.h"

uint8_t device_mode;

void PowerMonitor_Init(void) {
    uint32_t temp32u;
	
    COMP_InitTypeDef COMP_InitStructure;
    COMP_CVRefInitTypeDef COMP_CVRefInitStructure;
    PORT_InitTypeDef PORT_InitStructure;
    
    COMP_StructInit(&COMP_InitStructure);
    //COMP_InitStructure.COMP_PlusInputSource = COMP_PlusInput_CVREF;  
    COMP_InitStructure.COMP_PlusInputSource = COMP_PlusInput_IN1;  
    //COMP_InitStructure.COMP_MinusInputSource = COMP_MinusInput_IN1;
    COMP_InitStructure.COMP_MinusInputSource = COMP_MinusInput_IVREF;   // 1.2V
    COMP_InitStructure.COMP_OutInversion = COMP_OUT_INV_Enable;
    COMP_Init(&COMP_InitStructure);
    
    //COMP_CVRefStructInit(&COMP_CVRefInitStructure);
    //COMP_CVRefInitStructure.COMP_CVRefSource = COMP_CVREF_SOURCE_AVdd;
    //COMP_CVRefInitStructure.COMP_CVRefRange = COMP_CVREF_RANGE_Up;
    //COMP_CVRefInitStructure.COMP_CVRefScale = COMP_CVREF_SCALE_14_div_32;
    //COMP_CVRefInit(&COMP_CVRefInitStructure);
    
    COMP_Cmd(ENABLE); 
    //COMP_CVRefCmd(ENABLE);    
    while(COMP_GetCfgFlagStatus(COMP_CFG_FLAG_READY) != SET);
    
    // Setup GPIO
    PORT_StructInit(&PORT_InitStructure);
    PORT_InitStructure.PORT_Pin = (1 << POWER_MONITOR_PIN);
	PORT_Init(POWER_MONITOR_PORT, &PORT_InitStructure);
    
	// Wait until power supply is stable
	temp32u = DWT_StartDelayUs(50000);
	while (DWT_DelayInProgress(temp32u)) {
		if (COMP_GetFlagStatus(COMP_STATUS_FLAG_SY) == SET) {
			// False triggering - restart delay
			temp32u = DWT_StartDelayUs(50000);
		}
	}
    // Read and clear comparator result latch
    COMP_GetResultLatch();
	
    COMP_ITConfig(ENABLE);
	
	NVIC_EnableIRQ(COMPARATOR_IRQn);
}


void COMPARATOR_IRQHandler(void) {
	uint32_t dwt_time_mark;
	
    // Save all settings to EEPROM here
	// When we get here,  main power supply is off and MCU operates from on-board capacitors
	// Disable all power-consuming devices and reduce F_CPU
    __disable_irq();
	
	
	PORT_DeInit(MDR_PORTA);
	PORT_DeInit(MDR_PORTB);
	PORT_DeInit(MDR_PORTC);
	PORT_DeInit(MDR_PORTD);
	PORT_DeInit(MDR_PORTE);
	PORT_DeInit(MDR_PORTF);
	DAC2_Cmd(DISABLE);
	
	hw_Switch_CPU_Clock_to_HSI();
	
	if (device_mode == MODE_NORMAL) {
		// Gather settings and states from all modules (using global variable settings)
		DAC_SaveSettings();
		// Add more modules if required
		
		EE_SaveSettings();
	} else {
		// Gather system settings
		DAC_SaveCalibration();
        ADC_LC_SaveCalibration();
        ADC_LV_SaveCalibration();
        ExtADC_SaveCalibration();
		LCD_SaveContrastSetting();
        Sound_SaveSetting();
		
		EE_SaveSystemSettings();
	}
	
	dwt_time_mark = DWT_StartDelayUs(500000 / 4);	// F_CPU is 32MHz, but here MCU operates on HSI which is 8MHz
	// Safely wait until power suply goes down
	while (DWT_DelayInProgress(dwt_time_mark)) {
		IWDG_ReloadCounter();
	}
	// If power supply is still preset and MCU gets here, there was false triggering
	// Stop resetting watchdog - MCU will soon be rebooted
	while(1);
}

