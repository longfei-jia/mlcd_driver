/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h> // for rand, srand
#include <stdio.h>
#include "animation.h"
#include "encoder.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_SPI1_Init();
  MX_TIM5_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  MLCD_Init();
  Encoder_Init();
  
  // 演示变量
  int32_t counter = 0;
  char buf[32];
  const char* key_msg = "None";
  uint32_t key_msg_tick = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // 1. 扫描输入
    Encoder_Scan();
    
    // 2. 获取数据
    int32_t diff = Encoder_GetDiff();
    counter += diff;
    
    KeyEvent_t evt = Key_GetEvent();
    if (evt != KEY_EVENT_NONE) {
        key_msg_tick = HAL_GetTick();
        switch(evt) {
            case KEY_EVENT_CLICK:        key_msg = "Click"; break;
            case KEY_EVENT_DOUBLE_CLICK: key_msg = "Double"; break;
            case KEY_EVENT_LONG_PRESS:   key_msg = "LongPress"; break;
            default: break;
        }
    }
    
    // 3. 3秒后清除按键消息
    if (HAL_GetTick() - key_msg_tick > 3000) {
        key_msg = "None";
    }

    // 4. 显示绘制
    MLCD_ClearBuffer();
    
    MLCD_DrawString(10, 10, "--- Encoder Test ---", MLCD_COLOR_BLACK);
    
    // 显示计数值
    sprintf(buf, "Value: %ld", counter);
    MLCD_DrawString(10, 35, buf, MLCD_COLOR_BLACK);
    
    // 显示方向
    if (diff > 0) {
        MLCD_DrawString(10, 55, "Dir: CW  >>>", MLCD_COLOR_BLACK);
    } else if (diff < 0) {
        MLCD_DrawString(10, 55, "Dir: CCW <<<", MLCD_COLOR_BLACK);
    } else {
        MLCD_DrawString(10, 55, "Dir: Stop", MLCD_COLOR_BLACK);
    }
    
    // 显示按键状态
    sprintf(buf, "Key: %s", key_msg);
    MLCD_DrawString(10, 75, buf, MLCD_COLOR_BLACK);
    
    // 绘制一个随编码器旋转的小方块
    int box_x = (counter * 2) % (MLCD_WIDTH - 10);
    if (box_x < 0) box_x += (MLCD_WIDTH - 10);
    MLCD_DrawLine(box_x, 95, box_x + 10, 95, MLCD_COLOR_BLACK);
    MLCD_DrawLine(box_x + 10, 95, box_x + 10, 105, MLCD_COLOR_BLACK);
    MLCD_DrawLine(box_x + 10, 105, box_x, 105, MLCD_COLOR_BLACK);
    MLCD_DrawLine(box_x, 105, box_x, 95, MLCD_COLOR_BLACK);

    MLCD_Refresh();
    
    // 简单的帧率控制
    HAL_Delay(10);
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
#ifdef USE_FULL_ASSERT
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
