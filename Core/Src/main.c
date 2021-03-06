/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define RFID_PORT GPIOA
#define RFID_PIN GPIO_PIN_6     // Порт выходного сигнала

//#define RFID_PORT GPIOA
//#define RFID_PIN GPIO_PIN_3     // Если нужно, порт тактирования 125 КГц
// Таймер TIM2 в любом случае выдает туда сигнал PWM

#define MESSAGE_LENGTH 16       // Длина повторяющегося сообщения, включая метку данных

#define DEBUG_MODE 0
// 0 - без отладки
// 1 - регистр сдвига
// 2 - биты в прерывании компаратора TIM3 CHANNEL_1

// Кодирование по IEEE 802.3
// При кодировании по по Томасу поменять местами "^0" и "^1" в прерываниях TIM3

#define BIG_ENDIAN 1
#define BIT_ORDER BIG_ENDIAN
//#define LITTLE_ENDIAN 2
//#define BIT_ORDER LITTLE_ENDIAN
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
__IO uint8_t shift_bit;     // Бит данных ноль или единица из буфера один на два прерывания
__IO uint8_t TIM3_IRQ_cnt;  // Счетчик прерываний, чтобы не пропустить следующий бит данных
// Один и тот же бит данных используется в двух прерываниях таймера
// Поэтому мы можем загрузить следующий бит только после второго прерывания
uint8_t RFID_buff[16] = {"00ABCDEFGHIJKLMN"};   // Нули впереди для сигнатуры синхронизации данных
uint8_t shift_reg = 0;     // Сдвиговый регистр
uint8_t  bits_counter = 0;
uint8_t  bytes_counter = 0;

#if (DEBUG_MODE > 0)
__IO uint8_t Debug_buff[260] = {0,};
__IO uint16_t Debug_cnt = 0;
__IO uint32_t cnt = 0;
#endif

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
// Метка данных из девяти() единиц  
  RFID_buff[0] = 0xFF;        // 0b11111111
  #if (BIT_ORDER == BIG_ENDIAN)
                      
                      RFID_buff[1] = 0xE0;        // 0b11100000
                      
#elif (BIT_ORDER == LITTLE_ENDIAN)
                      
                      RFID_buff[1] = 0x07;        // 0b00000111
                      
#endif
 
  shift_reg = RFID_buff[0];
// Здесь можно загрузить данные в shift_bit, поскольку по HAL_TIM_Base_Start_IT(&htim3);
// программа сразу уйдет в прерывание. Однако мне показалось более изящным блокировать
// первые два прерывания и подавать данные сразу в цикле
  
HAL_TIM_Base_Start_IT(&htim3);
HAL_TIM_OC_Start_IT(&htim3, TIM_CHANNEL_1);
HAL_TIM_Base_Start(&htim2);
HAL_TIM_PWM_Start_IT(&htim2, TIM_CHANNEL_4);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    
// Регистр сдвига
        if (TIM3_IRQ_cnt == 1)  // Пока не обработается прерывание по компаратору
        {
          TIM3_IRQ_cnt = 0;
                    
            if (bits_counter > 7)
                    {
                      bits_counter = 0;
                      if (bytes_counter == MESSAGE_LENGTH - 1)
                          {
                            shift_reg = RFID_buff[0];
                            bytes_counter = 0;

#if (DEBUG_MODE > 0)                            
                            cnt++;
                                    if (cnt == 3)
                                    {
                                      asm("nop");     // удобная точка останова при отладке
                                      cnt = 0;
                                    }
#endif
                          
                          }
                      else
                          {
                          shift_reg = RFID_buff[++bytes_counter];
                          }
                        }
            if (bits_counter++ > 0)     // Нулевой бит не двигаем
                    {
                      
#if (BIT_ORDER == BIG_ENDIAN)
                      
                      shift_reg = shift_reg << 1;       // Остальные влево
                      
#elif (BIT_ORDER == LITTLE_ENDIAN)
                      
                      shift_reg = shift_reg >> 1;       // Остальные вправо
                      
#endif
                    }
            
#if (BIT_ORDER == BIG_ENDIAN)
            
                    shift_bit = 0x80 & shift_reg;       // Уничтожаем все, кроме левого бита
                    shift_bit = shift_bit >> 7;         // Превращаем бит в число
                    
#elif (BIT_ORDER == LITTLE_ENDIAN)
                    
                    shift_bit = 0x01 & shift_reg;       // Уничтожаем все, кроме правого бита
                    
#endif

#if (DEBUG_MODE == 1)                    
                    Debug_buff[Debug_cnt++] = shift_bit;        // Биты из регистра сдвига
                    if (Debug_cnt == 255) Debug_cnt = 0;
#endif
        }

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLL_DIV3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 255;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 127;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 63;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OC_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;
  sSlaveConfig.InputTrigger = TIM_TS_ITR1;
  if (HAL_TIM_SlaveConfigSynchro(&htim3, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 31;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
// Прерывание по переполнению
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  static uint8_t start_flag_UIF = 0;    // Флаг игнорирования прерывания по старту
  uint8_t shift_bit_UIF;                // Данные для установки (сброса) выхода
  // Поскольку shift_bit на два прерывания, его нельзя портить

        if(htim->Instance == TIM3)
        {
          TIM3_IRQ_cnt = 0;     // Блокировать подачу данных
          
          switch (start_flag_UIF)
                  {
                  case 1:
                    
                              shift_bit_UIF = shift_bit ^ 0;    // shift_bit будет нам нужен в следующем прерывании
                              
                              switch (shift_bit_UIF)
                                    {
                                    case 0:
                                            HAL_GPIO_WritePin(RFID_PORT, RFID_PIN, GPIO_PIN_RESET);
                                            break;
                                    case 1:
                                            HAL_GPIO_WritePin(RFID_PORT, RFID_PIN, GPIO_PIN_SET);
                                            break;
                                    }
                          break;
                          
                  case 0:       // Игнорировать первое прерывание
                    
                            start_flag_UIF = 1;         // Никогда больше сюда не заходить
                            return;
                  }
            
        }
}

// Прерывание по сравнению
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
  static uint8_t start_flag_CMP = 0;
  uint8_t shift_bit_CMP;

        if(htim->Instance == TIM3)
        {
          if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
          {

          TIM3_IRQ_cnt = 1;     // Разрешить подачу данных
          
          switch (start_flag_CMP)
                  {
                  case 1:
                
                              shift_bit_CMP = shift_bit ^ 1;
                              
#if (DEBUG_MODE == 2)
                              Debug_buff[Debug_cnt++] = shift_bit;
                              if (Debug_cnt == 255) Debug_cnt = 0;
#endif
                              
                              switch (shift_bit_CMP)
                                    {
                                    case 0:
                                            HAL_GPIO_WritePin(RFID_PORT, RFID_PIN, GPIO_PIN_RESET);
                                            break;
                                    case 1:
                                            HAL_GPIO_WritePin(RFID_PORT, RFID_PIN, GPIO_PIN_SET);
                                            break;
                                    }
                          break;
                          
                  case 0:
                    
                            start_flag_CMP = 1;
                            return;
                  }
          }
        }
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
