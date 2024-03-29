/****************************************************************//*
	@brief Module ADC
	
	Functions for internal voltage and current ADCs.
	Loop status is determined by result of current ADC, not hardware comparators
    
    
********************************************************************/

#include <stdlib.h>
#include "MDR32F9Qx_config.h"
#include "MDR32F9Qx_adc.h"
#include "MDR32F9Qx_port.h"
#include "linear_calibration.h"
#include "led.h"
#include "adc.h"
#include "dac.h"
#include "eeprom.h"
#include "power_monitor.h"

#define CURRENT_ADC_OVERSAMPLE  4
#define VOLTAGE_ADC_OVERSAMPLE  4
#define BLINK_PERIOD			25	// in units of ADC_UpdateLoopMonitor() call period

static calibration_t adc_voltage_calibration;
static calibration_t adc_current_calibration;
static uint32_t adc_current_code;		// required for calibration
static uint32_t adc_voltage_code;
static uint32_t loop_voltage;
static uint32_t loop_current;
static uint8_t loop_status;
static uint8_t blink_counter;
//static uint32_t temperature;

void ADC_Initialize(void) {
	
	ADC_InitTypeDef sADC;
	ADCx_InitTypeDef sADCx;
	PORT_InitTypeDef PORT_InitStructure;
	
	ADC_StructInit(&sADC);
	sADC.ADC_TempSensor           = ADC_TEMP_SENSOR_Enable;
	sADC.ADC_TempSensorAmplifier  = ADC_TEMP_SENSOR_AMPLIFIER_Enable;
	sADC.ADC_TempSensorConversion = ADC_TEMP_SENSOR_CONVERSION_Enable;
	ADC_Init (&sADC);
	
	// ADC1 Configuration 
	ADCx_StructInit (&sADCx);
	sADCx.ADC_ClockSource      = ADC_CLOCK_SOURCE_ADC;
	//sADCx.ADC_ChannelNumber    = ADC_CH_TEMP_SENSOR;		
	sADCx.ADC_VRefSource       = ADC_VREF_SOURCE_EXTERNAL;
	sADCx.ADC_Prescaler        = ADC_CLK_div_512;
	sADCx.ADC_DelayGo          = 7;
	ADC1_Init (&sADCx);
    
    // Using AVDD as reference for ADC2
    sADCx.ADC_VRefSource       = ADC_VREF_SOURCE_INTERNAL;
	ADC2_Init (&sADCx);
	
	// ADC1 enable
	ADC1_Cmd (ENABLE);
	ADC2_Cmd (ENABLE);
	
	// Setup GPIO
    PORT_StructInit(&PORT_InitStructure);
    PORT_InitStructure.PORT_Pin = ((1 << ADC_PIN_CURRENT) | (1 << ADC_PIN_VOLTAGE) | (1 << ADC_PIN_CONTRAST));
	PORT_Init(ADC_PORT, &PORT_InitStructure);
    
   	// Default calibration
	adc_voltage_calibration.point1.value = 0;
	adc_voltage_calibration.point1.code = 0;
	adc_voltage_calibration.point2.value = 20000;
	adc_voltage_calibration.point2.code = 3276 * VOLTAGE_ADC_OVERSAMPLE;
    adc_voltage_calibration.scale = 10000L;
	CalculateCoefficients(&adc_voltage_calibration);
    
    adc_current_calibration.point1.value = 0;
	adc_current_calibration.point1.code = 0;
	adc_current_calibration.point2.value = 20000;
	adc_current_calibration.point2.code = 3276 * CURRENT_ADC_OVERSAMPLE;
    adc_current_calibration.scale = 10000L;
	CalculateCoefficients(&adc_current_calibration);
	
	blink_counter = 0;
}

//-----------------------------------------------------------------//
// Loop current

// Using ADC1
void ADC_UpdateLoopCurrent(void) {
    uint32_t temp32u;
	int32_t temp32;
    uint8_t i;
    temp32u = 0;
	ADC1_SetChannel(ADC_PIN_CURRENT);
    for (i=0; i<CURRENT_ADC_OVERSAMPLE; i++) {
        ADC1_Start();
        while (ADC1_GetFlagStatus(ADCx_FLAG_END_OF_CONVERSION) == RESET);
        temp32u += ADC1_GetResult() & 0xFFF;
    }
	adc_current_code = temp32u;
    temp32 = GetValueForCode(&adc_current_calibration, temp32u);  
	loop_current  = (temp32 < 0) ? 0 : temp32;
}

void ADC_UpdateLoopMonitor(void) {
    loop_status = LOOP_OK;
	if (device_mode == MODE_NORMAL) {
		// Check loop break
		if (DAC_GetOutputState()) {
			if (loop_current <= LOOP_BREAK_TRESHOLD) {
				loop_status |= LOOP_BREAK;
			}	
			blink_counter = 0;
		} else {
			blink_counter = (blink_counter < BLINK_PERIOD - 1) ? blink_counter + 1 : 0;
			if (blink_counter < BLINK_PERIOD / 2) {
				loop_status |= LOOP_BREAK;
			}
		}
		// Check loop error
        if (DAC_GetOutputState()) {
            // Output enabled
            if (DAC_GetMode() == DAC_MODE_CONST) {
                if (abs((int32_t)loop_current - (int32_t)DAC_GetSettingConst()) > LOOP_ERROR_TRESHOLD)
                    loop_status |= LOOP_ERROR;
            } else {
                if (((int32_t)loop_current < (int32_t)DAC_GetSettingWaveMin() - LOOP_ERROR_TRESHOLD) || 
                    ((int32_t)loop_current > (int32_t)DAC_GetSettingWaveMax() + LOOP_ERROR_TRESHOLD))
                    loop_status |= LOOP_ERROR;
            }
        } else {
            // Output disabled
            if (loop_current > LOOP_ERROR_TRESHOLD)
                loop_status |= LOOP_ERROR;
        }
	}
	LED_Set(LED_ERROR, loop_status & LOOP_ERROR);
	LED_Set(LED_BREAK, loop_status & LOOP_BREAK);
}

uint8_t ADC_GetLoopStatus(void) {
    return loop_status;
}

uint32_t ADC_GetLoopCurrent(void) {
	return loop_current;
}

void ADC_SaveLoopCurrentCalibrationPoint(uint8_t pointNum, uint32_t measuredValue) {
	calibration_point_t *p = (pointNum == 1) ? &adc_current_calibration.point1 : &adc_current_calibration.point2;
	p->value = measuredValue;
	p->code = adc_current_code;
}

void ADC_LoopCurrentCalibrate(void) {
	CalculateCoefficients(&adc_current_calibration);
}

void ADC_LC_ApplyCalibration(void) {          
	adc_current_calibration.point1.value = system_settings.adc_current.point1.value;
	adc_current_calibration.point1.code  = system_settings.adc_current.point1.code;
	adc_current_calibration.point2.value = system_settings.adc_current.point2.value;
	adc_current_calibration.point2.code  = system_settings.adc_current.point2.code;
	CalculateCoefficients(&adc_current_calibration);
}

void ADC_LC_SaveCalibration(void) {           
	system_settings.adc_current.point1.value = adc_current_calibration.point1.value;
	system_settings.adc_current.point1.code = adc_current_calibration.point1.code;
	system_settings.adc_current.point2.value = adc_current_calibration.point2.value;
	system_settings.adc_current.point2.code = adc_current_calibration.point2.code;
}

//-----------------------------------------------------------------//
// Loop voltage

// Using ADC1
void ADC_UpdateLoopVoltage(void) {
	uint32_t temp32u;
	int32_t temp32;
    uint8_t i;
    temp32u = 0;
    ADC1_SetChannel(ADC_PIN_VOLTAGE);
    for (i=0; i<VOLTAGE_ADC_OVERSAMPLE; i++) {
        ADC1_Start();
        while (ADC1_GetFlagStatus(ADCx_FLAG_END_OF_CONVERSION) == RESET);
        temp32u += ADC1_GetResult() & 0xFFF;
    }
	adc_voltage_code = temp32u;
	temp32 = GetValueForCode(&adc_voltage_calibration, temp32u);
    loop_voltage = (temp32 < 0) ? 0 : temp32;
}

uint32_t ADC_GetLoopVoltage(void) {
	return (loop_voltage < 500) ? 0 : loop_voltage;		// actual zero may float a bit
}

void ADC_SaveLoopVoltageCalibrationPoint(uint8_t pointNum, uint32_t measuredValue) {
	calibration_point_t *p = (pointNum == 1) ? &adc_voltage_calibration.point1 : &adc_voltage_calibration.point2;
	p->value = measuredValue;
	p->code = adc_voltage_code;
}

void ADC_LoopVoltageCalibrate(void) {
	CalculateCoefficients(&adc_voltage_calibration);
}

void ADC_LV_ApplyCalibration(void) {          
	adc_voltage_calibration.point1.value = system_settings.adc_voltage.point1.value;
	adc_voltage_calibration.point1.code  = system_settings.adc_voltage.point1.code;
	adc_voltage_calibration.point2.value = system_settings.adc_voltage.point2.value;
	adc_voltage_calibration.point2.code  = system_settings.adc_voltage.point2.code;
	CalculateCoefficients(&adc_voltage_calibration);
}

void ADC_LV_SaveCalibration(void) {           
	system_settings.adc_voltage.point1.value = adc_voltage_calibration.point1.value;
	system_settings.adc_voltage.point1.code = adc_voltage_calibration.point1.code;
	system_settings.adc_voltage.point2.value = adc_voltage_calibration.point2.value;
	system_settings.adc_voltage.point2.code = adc_voltage_calibration.point2.code;
}


//-----------------------------------------------------------------//
// Other

// Using ADC1
/*
void ADC_UpdateMCUTemperature(void) {
	ADC1_SetChannel(ADC_CH_TEMP_SENSOR);
	ADC1_Start();
	while (ADC1_GetFlagStatus(ADCx_FLAG_END_OF_CONVERSION) == RESET);
	temperature = ADC1_GetResult();
	temperature &= 0xFFF;
}
*/

// Using ADC2
void ADC_Contrast_Start(void) {
	ADC2_SetChannel(ADC_PIN_CONTRAST);
    ADC2_Start();
}

uint16_t ADC_Contrast_GetResult(void) {
    uint16_t temp16u;
    temp16u = ADC2_GetResult();
    temp16u &= 0xFFF;
    return temp16u;
}







