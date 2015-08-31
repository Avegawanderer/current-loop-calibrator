

#define CPWM_PORT	MDR_PORTE
#define CPWM_PIN	3

#define LCD_CONTRAST_MIN    0
#define LCD_CONTRAST_MAX    20

#define LCD_MAX_PWM_ARR_STEP    20

void LCD_InitContrastBooster(void);
void LCD_ProcessContrastBooster(void);
uint8_t LCD_SetContrastSetting(int32_t value);
uint8_t LCD_GetContrastSetting(void);
void LCD_RestoreContrastSetting(void);
void LCD_SaveContrastSetting(void);


