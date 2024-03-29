
#include "linear_calibration.h"

#ifndef __DAC_H_
#define __DAC_H_

#define DAC_PORT        MDR_PORTE
#define DAC_OUTPUT_PIN  0

#define DAC_PROFILE_COUNT	2
#define DAC_MAX_SETTING     24000   // uA
#define DAC_MIN_SETTING     100     // uA
#define DAC_CYCLES_MAX      99999
#define DAC_CYCLES_MIN      1
#define DAC_PERIOD_MIN      100     // ms
#define DAC_PERIOD_MAX      500000  // ms
#define DAC_MAX_CODE		4013	// = 2.45V at shunt (a bit lower than hardware protection threshold, which is 2.5V)



enum DacModes {DAC_MODE_CONST, DAC_MODE_WAVEFORM};
enum SignalWaveforms {WAVE_MEANDR, WAVE_SAW_DIRECT, WAVE_SAW_REVERSED, WAVE_TRIANGULAR};

void DAC_Initialize(void);

void DAC_SaveCalibrationPoint(uint8_t pointNum, uint32_t measuredValue);
void DAC_Calibrate(void);
void DAC_ApplyCalibration(void);
void DAC_SaveCalibration(void);
void DAC_RestoreSettings(void);
void DAC_SaveSettings(void);

void DAC_SetOutputState(uint8_t isEnabled);
uint8_t DAC_SetProfile(uint32_t value);
uint8_t DAC_SetSettingConst(int32_t value);
uint8_t DAC_SetSettingWaveMax(int32_t value);
uint8_t DAC_SetSettingWaveMin(int32_t value);
void DAC_SetWaveform(uint8_t newWaveForm);
uint8_t DAC_SetPeriod(int32_t value);
void DAC_SetMode(uint8_t new_mode);
uint8_t DAC_SetTotalCycles(uint32_t value);
void DAC_RestartCycles(void);
		
uint8_t DAC_GetOutputState(void);
uint32_t DAC_GetSettingConst(void);
uint8_t DAC_GetActiveProfile(void);
uint32_t DAC_GetSettingWaveMax(void);
uint32_t DAC_GetSettingWaveMin(void);
uint8_t DAC_GetWaveform(void);
uint32_t DAC_GetPeriod(void);
uint8_t DAC_GetMode(void);
uint32_t DAC_GetTotalCycles(void);
uint32_t DAC_GetCurrentCycle(void);


#endif

