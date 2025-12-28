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
#include "menu.h"

// --- 菜单数据定义 ---
static bool setting_sound = true;
static bool setting_vibration = false;
bool setting_dark_mode = false; // 深色模式 (反色显示，全局可见)
static int32_t setting_brightness = 50;
static int32_t setting_contrast = 80;

// 主题切换回调
void Action_ToggleTheme(MenuItem_t *item) {
    // 这里仅切换变量，具体的全屏反色逻辑需要在 Menu_Render 中实现
}

// 阻尼设置变量
static int32_t setting_stiffness = 100; // 刚度 (50-200)
static int32_t setting_damping = 12;    // 阻尼 (1-30)

// 阻尼设置回调 (每次调节数值后重新应用参数)
void Action_ApplyCustomDamping(MenuItem_t *item) {
    // 仅更新参数，不重置位置
    cursor_anim.stiffness = (float)setting_stiffness;
    cursor_anim.damping = (float)setting_damping;
}

void Action_Save(MenuItem_t *item) {
    // 模拟保存动作
}

// 全局菜单指针
MenuPage_t *page_main;
MenuPage_t *page_display;
MenuPage_t *page_damping;
MenuPage_t *page_info;

// 初始化菜单结构
void Setup_Menus(void) {
    // 创建页面
    page_main = Menu_CreatePage("Main Menu");
    page_display = Menu_CreatePage("Display");
    page_damping = Menu_CreatePage("Anim Damping");
    page_info = Menu_CreatePage("System Info");

    // 构建 Main Menu
    Menu_AddSubMenu(page_main, "Display", page_display);
    Menu_AddSubMenu(page_main, "Damping", page_damping);
    Menu_AddToggle(page_main, "Theme", &setting_dark_mode, Action_ToggleTheme);
    Menu_AddToggle(page_main, "Sound", &setting_sound, NULL);
    Menu_AddToggle(page_main, "Vibrate", &setting_vibration, NULL);
    Menu_AddSubMenu(page_main, "Info", page_info);
    Menu_AddAction(page_main, "Save Cfg", Action_Save, NULL);
    Menu_AddAction(page_main, "Reboot", NULL, NULL);

    // 构建 Display Menu
    Menu_AddValue(page_display, "Brightness", &setting_brightness, 0, 100, 5, NULL);
    Menu_AddValue(page_display, "Contrast", &setting_contrast, 0, 100, 1, NULL);
    Menu_AddAction(page_display, "Back", (MenuCallback_t)Menu_Back, NULL); // 使用 Menu_Back 作为回调

    // 构建 Damping Menu
    Menu_AddValue(page_damping, "Stiffness", &setting_stiffness, 50, 200, 10, Action_ApplyCustomDamping);
    Menu_AddValue(page_damping, "Damping", &setting_damping, 1, 30, 1, Action_ApplyCustomDamping);
    Menu_AddAction(page_damping, "Back", (MenuCallback_t)Menu_Back, NULL);

    // 构建 Info Menu
    Menu_AddAction(page_info, "Ver: 1.0.0", NULL, NULL);
    Menu_AddAction(page_info, "Build: Dec28", NULL, NULL);
    Menu_AddAction(page_info, "Back", (MenuCallback_t)Menu_Back, NULL);
}

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
  
  // 动态构建菜单
  Setup_Menus();
  
  // 初始化菜单系统
  Menu_Init(page_main);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Menu_Loop();
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
