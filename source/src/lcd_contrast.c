/*******************************************************************
	Module systick
	
	LCD contrast charge pump control

********************************************************************/

#include "MDR32F9Qx_config.h"
#include "MDR32F9Qx_port.h"
#include "MDR32F9Qx_timer.h"
#include "lcd_contrast.h"
#include "adc.h"
#include "eeprom.h"
#include "utils.h"

static uint8_t contrastSetting;
static uint16_t contrastAdcCode;
static uint8_t contrastBoosterState;


void LCD_InitContrastBooster(void) {

	TIMER_CntInitTypeDef sTIM_CntInit;
	TIMER_ChnInitTypeDef sTIM_ChnInit;
	TIMER_ChnOutInitTypeDef sTIM_ChnOutInit;
	PORT_InitTypeDef PORT_InitStructure;
	
	// Initialize timer 2 counter
	TIMER_CntStructInit(&sTIM_CntInit);
	sTIM_CntInit.TIMER_Prescaler                = 31;		// CLK = F_CPU / (prescaler + 1)
	sTIM_CntInit.TIMER_Period                   = 1000;		// 1MHz / 100 = 10kHz
	TIMER_CntInit (MDR_TIMER2,&sTIM_CntInit);

	// Initialize timer 2 channel 3 - used for LCD contrast
	TIMER_ChnStructInit(&sTIM_ChnInit);
	sTIM_ChnInit.TIMER_CH_Mode                = TIMER_CH_MODE_PWM;
	sTIM_ChnInit.TIMER_CH_REF_Format          = TIMER_CH_REF_Format6;
	sTIM_ChnInit.TIMER_CH_CCR_UpdateMode      = TIMER_CH_CCR_Update_On_CNT_eq_0;
	sTIM_ChnInit.TIMER_CH_Number              = TIMER_CHANNEL3;			
	TIMER_ChnInit(MDR_TIMER2, &sTIM_ChnInit);
    
	// Initialize timer 2 channel 3 output
	TIMER_ChnOutStructInit(&sTIM_ChnOutInit);
	sTIM_ChnOutInit.TIMER_CH_NegOut_Polarity          = TIMER_CHOPolarity_NonInverted;
	sTIM_ChnOutInit.TIMER_CH_NegOut_Source            = TIMER_CH_OutSrc_REF;
	sTIM_ChnOutInit.TIMER_CH_NegOut_Mode              = TIMER_CH_OutMode_Output;
	sTIM_ChnOutInit.TIMER_CH_Number                   = TIMER_CHANNEL3;
	TIMER_ChnOutInit(MDR_TIMER2, &sTIM_ChnOutInit);
	
	// Set contrast PWM duty cycle 0%
	MDR_TIMER2->CCR3 = 0;
    
	// Enable TIMER2 counter clock
	TIMER_BRGInit(MDR_TIMER2,TIMER_HCLKdiv1);

	// Enable TIMER2
	TIMER_Cmd(MDR_TIMER2,ENABLE);
	
	
    // Setup GPIO
    PORT_ResetBits(CPWM_PORT, (1<<CPWM_PIN));
    
    PORT_StructInit(&PORT_InitStructure);
    PORT_InitStructure.PORT_Pin = (1 << CPWM_PIN);
	PORT_InitStructure.PORT_FUNC = PORT_FUNC_ALTER;
	PORT_InitStructure.PORT_MODE = PORT_MODE_DIGITAL;
    PORT_InitStructure.PORT_OE = PORT_OE_OUT;
    PORT_InitStructure.PORT_SPEED = PORT_SPEED_FAST;
	PORT_Init(CPWM_PORT, &PORT_InitStructure);
    
    contrastBoosterState = 0;
    
    // Default setting
    LCD_SetContrastSetting(10);
}

void LCD_ProcessContrastBooster(void) {
	uint16_t temp16u;
    int32_t temp32;
	uint8_t next_state = contrastBoosterState + 1;
    static uint16_t arr_value = 1000;
    switch (contrastBoosterState) {
        case 0:
            ADC_Contrast_Start();
            break;
        case 1:
            temp16u = ADC_Contrast_GetResult();
            // -3.3V -> 0, 3.3V -> 4095
            temp16u = 4095 - temp16u;
            // -3.3V -> 4095, 3.3V -> 0
            // contrastAdcCode = setting [1000 - 3000]
            // Get difference
            temp32 = - (int32_t)contrastAdcCode + (int32_t)temp16u;
            // Bound step
            if (temp32 > LCD_MAX_PWM_ARR_STEP)
                temp32 = LCD_MAX_PWM_ARR_STEP;
            else if (temp32 < -LCD_MAX_PWM_ARR_STEP)
                temp32 = -LCD_MAX_PWM_ARR_STEP;
        
            // Integrate error
            arr_value += temp32;

            // Bound frequency
            if (arr_value < 50)
                arr_value = 50;
            else if (arr_value > 2000)
                arr_value = 2000;
        
            MDR_TIMER2->ARR = arr_value;
            MDR_TIMER2->CCR3 = MDR_TIMER2->ARR >> 1;
			next_state = 0;
            break;
		default:
            next_state = 0;
            break;
    }
    contrastBoosterState = next_state;
    
	//TIMER_SetCntAutoreload(MDR_TIMER2, 900);
	// Set contrast PWM duty cycle 50%
	//MDR_TIMER2->CCR3 = MDR_TIMER2->ARR >> 1;
}

uint8_t LCD_SetContrastSetting(int32_t value) {
	uint8_t result = verify_int32(&value, LCD_CONTRAST_MIN, LCD_CONTRAST_MAX);
	contrastSetting = value;
    contrastAdcCode = (10 + contrastSetting) * 100;
	return result;
}

uint8_t LCD_GetContrastSetting(void) {
	return contrastSetting;
}


void LCD_RestoreContrastSetting(void) {
    LCD_SetContrastSetting(system_settings.contrast_level);
}

void LCD_SaveContrastSetting(void) {
    system_settings.contrast_level = contrastSetting;
}





