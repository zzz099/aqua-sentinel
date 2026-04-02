/***
	***************************************************************************************************************************
	*	@file  	lcd_test.c
	*	@version V1.0
	*  @date    2021-3-22
	*	@author  反客科技
	*	@brief   LTDC测试函数	
   ***************************************************************************************************************************
   *  @description
	*
	*	实验平台：反客STM32H743IIT6核心板 （型号：FK743M1-IIT6） + 反客800*480分辨率的RGB屏幕
	*	淘宝地址：https://shop212360197.taobao.com
	*	QQ交流群：536665479
	*
>>>>> 文件说明：
	*
	*	1. 仅用于测试，该文件不是必须，用户移植的时候可以舍弃
	*
	****************************************************************************************************************************
***/

#include "lcd_test.h"
#include "touch_800x480.h"


/*************************************************************************************************
*	函 数 名:	LCD_Test_DoubleLayer
*
*	函数功能:	双层显示
*
*	说    明:	仅开启双层的时候生效			
*************************************************************************************************/

void LCD_Test_DoubleLayer(void)
{
	uint16_t time = 100;	// 延时时间
	uint16_t i = 0;	
	
// 绘制初始界面，包括标题、LOGO以及进度条>>>>>

	LCD_SetBackColor(0xffB9EDF8); 			//	设置背景色，使用自定义颜色
	LCD_Clear(); 									//	清屏，刷背景色
	
	LCD_SetTextFont(&CH_Font32);				// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0xff333333);					//	设置画笔色，使用自定义颜色
	LCD_DisplayText(334, 160,"\xCB\xAB\xB2\xE3\xB2\xE2\xCA\xD4");	// 显示文本
	
	LCD_SetColor(0xfffd7923);					//	设置画笔色，使用自定义颜色
	LCD_DrawImage(  280, 218, 240, 83, Image_FANKE_240x83) ;		// 显示LOGO图片

	LCD_SetColor(LIGHT_YELLOW);		//	设置画笔色
	for(i=0;i<150;i++)
   {
		LCD_FillRect(100,330,4*i,6);	// 绘制矩形，实现简易进度条的效果
		HAL_Delay(10);	
   }	
	
	LCD_SetLayer(1);	//切换到层1
	LCD_SetBackColor(LCD_BLACK & 0x00FFFFFF);  // 透明度100%，高8位的数字越小透明度越高
	LCD_Clear();										 // 此时前景层完全透明，否则无法看到 layer0 的图像	
	
// layer0 显示内容 >>>>>
		
	LCD_SetLayer(0);		//切换到层0	

	LCD_SetBackColor(LCD_BLACK);   
	LCD_Clear();
	
	LCD_SetColor(LCD_WHITE);	
	LCD_SetTextFont(&CH_Font32);				// 设置2424中文字体,ASCII字体对应为2412
	LCD_DisplayText(10,10,"STM32H743 LTDC " "\xB1\xB3\xBE\xB0\xB2\xE3" ", layer0"); 

	HAL_Delay(1000);
	
	LCD_SetColor(LIGHT_CYAN);  	LCD_DrawLine(45, 60,300, 60);	 	HAL_Delay(time);
	LCD_SetColor(LIGHT_MAGENTA);	LCD_DrawLine(45, 90,300, 90);   	HAL_Delay(time);
	LCD_SetColor(LIGHT_YELLOW); 	LCD_DrawLine(45,120,300,120);	 	HAL_Delay(time);
	LCD_SetColor(LIGHT_GREY); 		LCD_DrawLine(45,150,300,150);	 	HAL_Delay(time);
	LCD_SetColor(LIGHT_CYAN);  	LCD_DrawLine(45,180,300,180);	 	HAL_Delay(time);
	LCD_SetColor(LIGHT_MAGENTA);	LCD_DrawLine(45,210,300,210);   	HAL_Delay(time);
	LCD_SetColor(LIGHT_YELLOW); 	LCD_DrawLine(45,240,300,240);	 	HAL_Delay(time);

	LCD_SetColor(LCD_RED);    		LCD_FillCircle(118,350,85);		HAL_Delay(time);
	LCD_SetColor(LCD_GREEN);  		LCD_FillCircle(187,350,85); 	   HAL_Delay(time);
	LCD_SetColor(LCD_BLUE);   		LCD_FillCircle(268,350,85);  		HAL_Delay(time);
	
	LCD_SetColor(LIGHT_RED);	  	LCD_FillRect(387, 53,200,200);	HAL_Delay(time);	  
	LCD_SetColor(LIGHT_GREEN);  	LCD_FillRect(477,141,200,200);  	HAL_Delay(time);  
	LCD_SetColor(LIGHT_BLUE);    	LCD_FillRect(581,248,200,200);  	HAL_Delay(time);
	
	HAL_Delay(1000);
	
// layer1 显示内容 >>>>>

	LCD_SetLayer(1);	//切换到层1

	LCD_SetColor(0Xff348498);		
	LCD_FillRect(100, 80,600,380);		  

	LCD_SetBackColor(0Xff348498);  
	LCD_SetColor(LCD_BLACK);		
	LCD_DisplayText(146,90,"\xC7\xB0\xBE\xB0\xB2\xE3" ", layer1"); 
	HAL_Delay(1000);

#if ColorMode_1 == LTDC_PIXEL_FORMAT_ARGB8888	// 如果layer1 定义为 ARGB8888

	LCD_SetBackColor(LCD_BLACK & 0xE0FFFFFF);  // 设置背景色透明度，高8位的数字越小透明度越高，ARGB8888支持8位透明色，有255种透明状态
	LCD_SetColor(LCD_WHITE);						 // 画笔不透明 
	LCD_DisplayText(110,140,"STM32H743IGT6 LTDC, ARGB8888, layer1"); 
	HAL_Delay(1000);
	
	LCD_SetBackColor(LCD_BLACK & 0xa0FFFFFF);  // 设置背景色透明度，高8位的数字越小透明度越高，ARGB8888支持8位透明色，有255种透明状态
	LCD_SetColor(LCD_WHITE);		 
	LCD_DisplayText(110,200,"STM32H743IGT6 LTDC, ARGB8888, layer1"); 
	HAL_Delay(1000);
		
	LCD_SetBackColor(LCD_BLACK & 0x70FFFFFF );  // 设置背景色透明度，高8位的数字越小透明度越高，ARGB8888支持8位透明色，有255种透明状态
	LCD_SetColor(LCD_WHITE);
	LCD_DisplayText(110,260,"STM32H743IGT6 LTDC, ARGB8888, layer1"); 	
	HAL_Delay(1000);
	
	LCD_SetBackColor(LCD_BLACK & 0x20FFFFFF );  // 设置背景色透明度，高8位的数字越小透明度越高，ARGB8888支持8位透明色，有255种透明状态
	LCD_SetColor(LCD_WHITE);
	LCD_DisplayText(110,320,"STM32H743IGT6 LTDC, ARGB8888, layer1"); 	
	HAL_Delay(1000);	
	
	LCD_SetBackColor(LCD_BLACK);  				 // 将背景色设置为 不透明
	LCD_SetColor(LCD_WHITE & 0x00FFFFFF);		 // 将画笔色设置为 完全透明	 
	LCD_DisplayText(110,380,"STM32H743IGT6 LTDC, ARGB8888, layer1"); 	
	HAL_Delay(2000);	

#elif	ColorMode_1 == LTDC_PIXEL_FORMAT_ARGB4444	// 如果layer1 定义为 ARGB4444

	LCD_SetBackColor(LCD_BLACK & 0xB0FFFFFF);  // 设置背景色透明度，高8位的数字越小透明度越高，ARGB4444支持4位透明色，有16种透明状态
	LCD_SetColor(LCD_WHITE);						 // 画笔不透明 
	LCD_DisplayText(110,140,"STM32H743IGT6 LTDC, ARGB4444, layer1"); 
	HAL_Delay(1000);
	
	LCD_SetBackColor(LCD_BLACK & 0x80FFFFFF);  // 设置背景色透明度，高8位的数字越小透明度越高，ARGB4444支持4位透明色，有16种透明状态
	LCD_SetColor(LCD_WHITE);		 
	LCD_DisplayText(110,200,"STM32H743IGT6 LTDC, ARGB4444, layer1"); 
	HAL_Delay(1000);
		
	LCD_SetBackColor(LCD_BLACK & 0x30FFFFFF );  // 设置背景色透明度，高8位的数字越小透明度越高，ARGB4444支持4位透明色，有16种透明状态
	LCD_SetColor(LCD_WHITE);
	LCD_DisplayText(110,260,"STM32H743IGT6 LTDC, ARGB4444, layer1"); 	
	HAL_Delay(1000);
	
	LCD_SetBackColor(LCD_BLACK  );  				 // 将背景色设置为 不透明
	LCD_SetColor(LCD_WHITE & 0x00FFFFFF);		 // 将画笔色设置为 完全透明	 
	LCD_DisplayText(110,320,"STM32H743IGT6 LTDC, ARGB4444, layer1"); 	
	HAL_Delay(2000);	
	
#elif	ColorMode_1 == LTDC_PIXEL_FORMAT_ARGB1555	// 如果layer1 定义为 ARGB1555


	LCD_SetBackColor(LCD_BLACK & 0xffFFFFF);  // 设置背景色透明度，高8位的数字越小透明度越高，ARGB1555仅支持一位透明色，即仅有透明和不透明两种状态
	LCD_SetColor(LCD_WHITE);						// 画笔不透明 
	LCD_DisplayText(110,140,"STM32H743IGT6 LTDC, ARGB1555, layer1"); 
	HAL_Delay(1000);
	
	LCD_SetBackColor(LCD_BLACK & 0x00FFFFFF);  // 设置背景色透明度，高8位的数字越小透明度越高，ARGB1555仅支持一位透明色，即仅有透明和不透明两种状态
	LCD_SetColor(LCD_WHITE);	  					 // 画笔不透明 
	LCD_DisplayText(110,200,"STM32H743IGT6 LTDC, ARGB1555, layer1"); 
	HAL_Delay(1000);
		
	LCD_SetBackColor(LCD_BLACK  & 0xffFFFFF);  // 将背景色设置为 不透明
	LCD_SetColor(LCD_WHITE & 0x00FFFFFF);		 // 将画笔色设置为 完全透明	 
	LCD_DisplayText(110,260,"STM32H743IGT6 LTDC, ARGB1555, layer1"); 	
	HAL_Delay(2000);	
	
#endif

}

/*************************************************************************************************
*	函 数 名:	LCD_Test_Clear
*
*	函数功能:	清屏测试
*
*	说    明:	无	
*************************************************************************************************/

void LCD_Test_Clear(void)
{
	uint16_t time = 1000;	// 延时时间
	uint8_t	i = 0;			// 计数变量
	
// 绘制初始界面，包括标题、LOGO以及进度条>>>>>
	
	LCD_SetBackColor(0xffB9EDF8); 			//	设置背景色，使用自定义颜色
	LCD_Clear(); 									//	清屏，刷背景色
	
	LCD_SetTextFont(&CH_Font32);				// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0xff333333);					//	设置画笔色，使用自定义颜色
	LCD_DisplayText(334, 160,"\xCB\xA2\xC6\xC1\xB2\xE2\xCA\xD4");	// 显示文本
	
	LCD_SetColor(0xfffd7923);					//	设置画笔色，使用自定义颜色
	LCD_DrawImage(  280, 218, 240, 83, Image_FANKE_240x83) ;		// 显示LOGO图片

	LCD_SetColor(LIGHT_YELLOW);		//	设置画笔色
	for(i=0;i<150;i++)
   {
		LCD_FillRect(100,330,4*i,6);	// 绘制矩形，实现简易进度条的效果
		HAL_Delay(10);	
   }	

		
// 刷屏测试>>>>>
		
	LCD_SetTextFont(&CH_Font32);			// 设置2424中文字体,ASCII字体对应为2412
	LCD_SetColor(LCD_BLACK);				// 设置画笔颜色

	for(i=0;i<8;i++)
	{
		switch (i)		// 切换背景色
		{
			case 0: LCD_SetBackColor(LIGHT_RED); 		break;	
			case 1: LCD_SetBackColor(LIGHT_GREEN); 	break;				
			case 2: LCD_SetBackColor(LIGHT_BLUE); 		break;
			case 3: LCD_SetBackColor(LIGHT_YELLOW); 	break;
			case 4: LCD_SetBackColor(LIGHT_CYAN);		break;
			case 5: LCD_SetBackColor(LIGHT_GREY); 		break;
			case 6: LCD_SetBackColor(LIGHT_MAGENTA); 	break;
			case 7: LCD_SetBackColor(LCD_WHITE); 		break;			
			default:	break;			
		}
		LCD_Clear();		// 清屏
		LCD_DisplayText(112, 84,"STM32H743 LTDC " "\xCB\xA2\xC6\xC1\xB2\xE2\xCA\xD4");
		LCD_DisplayText(112,134,"\xBA\xCB\xD0\xC4\xB0\xE5\xD0\xCD\xBA\xC5\xA3\xBA" "FK743M1");
		LCD_DisplayText(112,184,"\xC6\xC1\xC4\xBB\xB7\xD6\xB1\xE6\xC2\xCA\xA3\xBA" "800*480");	
		HAL_Delay(time);	// 延时
	}
}

/*************************************************************************************************
*	函 数 名:	LCD_Test_Text
*
*	函数功能:	文本显示测试
*
*	说    明:	无	
*************************************************************************************************/

void LCD_Test_Text(void)
{
	uint16_t i;					// 计数变量
	uint16_t time = 80;	// 延时时间
	
// 绘制初始界面，包括标题、LOGO以及进度条>>>>>

	LCD_SetBackColor(0xffB9EDF8); 			//	设置背景色，使用自定义颜色
	LCD_Clear(); 									//	清屏，刷背景色
	
	LCD_SetTextFont(&CH_Font32);				// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0xff333333);					//	设置画笔色，使用自定义颜色
	LCD_DisplayText(334, 160,"\xCE\xC4\xB1\xBE\xCF\xD4\xCA\xBE");	// 显示文本
	
	LCD_SetColor(0xfffd7923);					//	设置画笔色，使用自定义颜色
	LCD_DrawImage(  280, 218, 240, 83, Image_FANKE_240x83) ;		// 显示LOGO图片

	LCD_SetColor(LIGHT_YELLOW);		//	设置画笔色
	for(i=0;i<150;i++)
   {
		LCD_FillRect(100,330,4*i,6);	// 绘制矩形，实现简易进度条的效果
		HAL_Delay(10);	
   }	
		
	
// 显示文本，包括各种字体大小的中文和ASCII字符 >>>>>

	LCD_SetBackColor(LCD_BLACK); 			//	设置背景色
	LCD_Clear(); 								// 清屏
	
	LCD_SetColor(LCD_WHITE);	// 设置画笔，白色
	LCD_SetFont(&Font32); LCD_DisplayString(0,  0,"!#$%&'()*+,-.0123456789:;<=>?@ABCDEFGHIJKLMNOPQRST"); HAL_Delay(time);		
	LCD_SetFont(&Font24); LCD_DisplayString(0, 32,"!#$%&'()*+,-.0123456789:;<=>?@ABCDEFGHIJKLMNOPQRST"); HAL_Delay(time);
	LCD_SetFont(&Font20); LCD_DisplayString(0, 56,"!#$%&'()*+,-.0123456789:;<=>?@ABCDEFGHIJKLMNOPQRST"); HAL_Delay(time);	
	LCD_SetFont(&Font16); LCD_DisplayString(0, 76,"!#$%&'()*+,-.0123456789:;<=>?@ABCDEFGHIJKLMNOPQRST"); HAL_Delay(time);	
	LCD_SetFont(&Font12); LCD_DisplayString(0, 92,"!#$%&'()*+,-.0123456789:;<=>?@ABCDEFGHIJKLMNOPQRST"); HAL_Delay(time);	
																																		  
	LCD_SetColor(LCD_CYAN);                                                                              
	LCD_SetFont(&Font12); LCD_DisplayString(0,104,"!#$%&'()*+,-.0123456789:;<=>?@ABCDEFGHIJKLMNOPQRST"); HAL_Delay(time);	
	LCD_SetFont(&Font16); LCD_DisplayString(0,116,"!#$%&'()*+,-.0123456789:;<=>?@ABCDEFGHIJKLMNOPQRST"); HAL_Delay(time);	
	LCD_SetFont(&Font20); LCD_DisplayString(0,132,"!#$%&'()*+,-.0123456789:;<=>?@ABCDEFGHIJKLMNOPQRST"); HAL_Delay(time);		
	LCD_SetFont(&Font24); LCD_DisplayString(0,152,"!#$%&'()*+,-.0123456789:;<=>?@ABCDEFGHIJKLMNOPQRST"); HAL_Delay(time);		
	LCD_SetFont(&Font32); LCD_DisplayString(0,176,"!#$%&'()*+,-.0123456789:;<=>?@ABCDEFGHIJKLMNOPQRST"); HAL_Delay(time);	

	LCD_SetTextFont(&CH_Font24);			// 设置2424中文字体,ASCII字体对应为2412
	LCD_SetColor(LCD_YELLOW);				// 设置画笔，黄色
	LCD_DisplayText(0, 230,"\xCE\xC4\xB1\xBE\xCF\xD4\xCA\xBE\xA3\xAC\xBF\xC9\xCF\xD4\xCA\xBE\xD6\xD0\xCE\xC4\xBA\xCD" "ASCII" "\xD7\xD6\xB7\xFB\xBC\xAF");
	LCD_DisplayText(0, 260,"\xD3\xC3\xBB\xA7\xBF\xC9\xB8\xF9\xBE\xDD\xD0\xE8\xC7\xF3\xA3\xAC\xB6\xD4\xD7\xD6\xBF\xE2\xBD\xF8\xD0\xD0\xD4\xF6\xCC\xED\xBA\xCD\xC9\xBE\xBC\xF5");	

	
	LCD_SetTextFont(&CH_Font12);			// 设置1212中文字体,ASCII字体对应为1206
	LCD_SetColor(0Xff8AC6D1);						// 设置画笔
	LCD_DisplayText(28, 290,"1212" "\xD6\xD0\xCE\xC4\xD7\xD6\xCC\xE5\xA3\xBA\xB7\xB4\xBF\xCD\xBF\xC6\xBC\xBC");	
	
	LCD_SetTextFont(&CH_Font16);			// 设置1616中文字体,ASCII字体对应为1608
	LCD_SetColor(0XffC5E1A5);						// 设置画笔
	LCD_DisplayText(28, 310,"1616" "\xD6\xD0\xCE\xC4\xD7\xD6\xCC\xE5\xA3\xBA\xB7\xB4\xBF\xCD\xBF\xC6\xBC\xBC");		
	
	// LCD_SetTextFont(&CH_Font20);			// 设置2020中文字体,ASCII字体对应为2010
	// LCD_SetColor(0Xff2D248A);						// 设置画笔
	// LCD_DisplayText(28, 335,"2020" "\xD6\xD0\xCE\xC4\xD7\xD6\xCC\xE5\xA3\xBA\xB7\xB4\xBF\xCD\xBF\xC6\xBC\xBC");

	LCD_SetTextFont(&CH_Font24);			// 设置2424中文字体,ASCII字体对应为2412
	LCD_SetColor(0XffFF585D);						// 设置画笔
	LCD_DisplayText(28, 365,"2424" "\xD6\xD0\xCE\xC4\xD7\xD6\xCC\xE5\xA3\xBA\xB7\xB4\xBF\xCD\xBF\xC6\xBC\xBC");		

	LCD_SetTextFont(&CH_Font32);			// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0XffF6003C);						// 设置画笔
	LCD_DisplayText(28, 405,"3232" "\xD6\xD0\xCE\xC4\xD7\xD6\xCC\xE5\xA3\xBA\xB7\xB4\xBF\xCD\xBF\xC6\xBC\xBC");		

	HAL_Delay(2000);	// 延时等待
}

/*************************************************************************************************
*	函 数 名:	LCD_Test_Variable
*
*	函数功能:	变量显示，包括整数和小数
*
*	说    明:	无	
*************************************************************************************************/

void LCD_Test_Variable (void)
{

	uint16_t i;					// 计数变量

	int32_t	a = 0;			// 定义整数变量，用于测试
	int32_t	b = 0;			// 定义整数变量，用于测试
	int32_t	c = 0;			// 定义整数变量，用于测试

	double p = 3.1415926;	// 定义浮点数变量，用于测试
	double f = -1234.1234;	// 定义浮点数变量，用于测试
	
	
// 绘制初始界面，包括标题、LOGO以及进度条>>>>>	
		
	LCD_SetBackColor(0xffB9EDF8); 			//	设置背景色，使用自定义颜色
	LCD_Clear(); 									//	清屏，刷背景色
	
	LCD_SetTextFont(&CH_Font32);				// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0xff333333);					//	设置画笔色，使用自定义颜色
	LCD_DisplayText(334, 160,"\xB1\xE4\xC1\xBF\xCF\xD4\xCA\xBE");	// 显示文本
	
	LCD_SetColor(0xfffd7923);					//	设置画笔色，使用自定义颜色
	LCD_DrawImage(  280, 218, 240, 83, Image_FANKE_240x83) ;		// 显示LOGO图片

	LCD_SetColor(LIGHT_YELLOW);		//	设置画笔色
	for(i=0;i<150;i++)
   {
		LCD_FillRect(100,330,4*i,6);	// 绘制矩形，实现简易进度条的效果
		HAL_Delay(10);	
   }	
			
	
// 显示正整数、负整数、小数>>>>>	
	
	LCD_SetBackColor(LCD_BLACK); 			//	设置背景色
	LCD_Clear(); 								// 清屏
	
	LCD_SetTextFont(&CH_Font32);					// 设置2424中文字体,ASCII字体对应为2412
	LCD_SetColor(LCD_WHITE);						// 设置画笔,白色
	
	LCD_DisplayText(28, 20,"\xB6\xE0\xD3\xE0\xCE\xBB\xCC\xEE\xB3\xE4\xBF\xD5\xB8\xF1");	// 显示文本	
	LCD_DisplayText(400,20,"\xB6\xE0\xD3\xE0\xCE\xBB\xCC\xEE\xB3\xE4" "0");		// 显示文本		
	
	LCD_SetColor(LIGHT_CYAN);					// 设置画笔，蓝绿色
	LCD_DisplayText(28, 60,"\xD5\xFD\xD5\xFB\xCA\xFD\xA3\xBA");				
	LCD_DisplayText(28,100,"\xD5\xFD\xD5\xFB\xCA\xFD\xA3\xBA");				
	LCD_DisplayText(28,140,"\xB8\xBA\xD5\xFB\xCA\xFD\xA3\xBA");					
				
	LCD_SetColor(LIGHT_YELLOW);				// 设置画笔，亮黄色		
	LCD_DisplayText(400, 60,"\xD5\xFD\xD5\xFB\xCA\xFD\xA3\xBA");	
	LCD_DisplayText(400,100,"\xD5\xFD\xD5\xFB\xCA\xFD\xA3\xBA");	
	LCD_DisplayText(400,140,"\xB8\xBA\xD5\xFB\xCA\xFD\xA3\xBA");	
			
	LCD_SetColor(LIGHT_RED);					// 设置画笔	，亮红色		
	LCD_DisplayText(28, 200,"\xD5\xFD\xD0\xA1\xCA\xFD\xA3\xBA");	
	LCD_DisplayText(28, 240,"\xB8\xBA\xD0\xA1\xCA\xFD\xA3\xBA");		
	
	for(i=0;i<100;i++)
   {
		LCD_SetColor(LIGHT_CYAN);									// 设置画笔	，蓝绿色	
		LCD_ShowNumMode(Fill_Space);								// 多余位填充空格
		LCD_DisplayNumber( 160, 60, a+i*125, 8) ;				// 显示变量		
		LCD_DisplayNumber( 160,100, b+i, 	 6) ;				// 显示变量			
		LCD_DisplayNumber( 160,140, c-i,     6) ;				// 显示变量			
		
		LCD_SetColor(LIGHT_YELLOW);								// 设置画笔，亮黄色	
		LCD_ShowNumMode(Fill_Zero);								// 多余位填充0
		LCD_DisplayNumber( 560, 60, a+i*125, 8) ;				// 显示变量		
		LCD_DisplayNumber( 560,100, b+i, 	 6) ;				// 显示变量			
		LCD_DisplayNumber( 560,140, c-i,     6) ;				// 显示变量				
		
		LCD_SetColor(LIGHT_RED);									// 设置画笔，亮红色			
		LCD_ShowNumMode(Fill_Space);								// 多余位填充空格		
		LCD_DisplayDecimals( 160, 200, p+i*0.1,  6,3);		// 显示小数	
		LCD_DisplayDecimals( 160, 240, f+i*0.01, 11,4);		// 显示小数		
		
		HAL_Delay(30);				
   }
	HAL_Delay(2500);		
	
}



/*************************************************************************************************
*	函 数 名:	LCD_Test_FillRect
*
*	函数功能:	矩形填充测试
*
*	说    明:	无	
*************************************************************************************************/

void LCD_Test_FillRect(void)
{
	uint16_t i;					// 计数变量
	
// 绘制初始界面，包括标题、LOGO以及进度条>>>>>
		
	LCD_SetBackColor(0xffB9EDF8); 			//	设置背景色，使用自定义颜色
	LCD_Clear(); 									//	清屏，刷背景色
	
	LCD_SetTextFont(&CH_Font32);				// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0xff333333);					//	设置画笔色，使用自定义颜色
	LCD_DisplayText(334, 160,"\xBE\xD8\xD0\xCE\xBB\xE6\xD6\xC6");	// 显示文本
	
	LCD_SetColor(0xfffd7923);					//	设置画笔色，使用自定义颜色
	LCD_DrawImage(  280, 218, 240, 83, Image_FANKE_240x83) ;		// 显示LOGO图片

	LCD_SetColor(LIGHT_YELLOW);		//	设置画笔色
	for(i=0;i<150;i++)
   {
		LCD_FillRect(100,330,4*i,6);	// 绘制矩形，实现简易进度条的效果
		HAL_Delay(10);	
   }	
		
	
// 矩形填充>>>>>	
	
	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色

	LCD_SetFont(&Font16);
	LCD_SetColor(LCD_BLUE);	  	  LCD_FillRect(200,  3,500,16);	LCD_DisplayString(0,  3,"LCD_BLUE");		
	LCD_SetColor(LCD_GREEN);  	  LCD_FillRect(200, 25,500,16);  LCD_DisplayString(0, 25,"LCD_GREEN");		
	LCD_SetColor(LCD_RED);    	  LCD_FillRect(200, 47,500,16);  LCD_DisplayString(0, 47,"LCD_RED");			
	LCD_SetColor(LCD_CYAN);   	  LCD_FillRect(200, 69,500,16);  LCD_DisplayString(0, 69,"LCD_CYAN");		
	LCD_SetColor(LCD_MAGENTA);	  LCD_FillRect(200, 91,500,16);  LCD_DisplayString(0, 91,"LCD_MAGENTA");	
	LCD_SetColor(LCD_YELLOW); 	  LCD_FillRect(200,113,500,16);  LCD_DisplayString(0,113,"LCD_YELLOW");		
	LCD_SetColor(LCD_GREY);   	  LCD_FillRect(200,135,500,16);	LCD_DisplayString(0,135,"LCD_GREY");		

	LCD_SetColor(LIGHT_BLUE);	  LCD_FillRect(200,157,500,16);  LCD_DisplayString(0,157,"LIGHT_BLUE");		
	LCD_SetColor(LIGHT_GREEN);   LCD_FillRect(200,179,500,16);  LCD_DisplayString(0,179,"LIGHT_GREEN");	
	LCD_SetColor(LIGHT_RED);     LCD_FillRect(200,201,500,16);  LCD_DisplayString(0,201,"LIGHT_RED");	   
	LCD_SetColor(LIGHT_CYAN);    LCD_FillRect(200,223,500,16);  LCD_DisplayString(0,223,"LIGHT_CYAN");	   
	LCD_SetColor(LIGHT_MAGENTA); LCD_FillRect(200,245,500,16);  LCD_DisplayString(0,245,"LIGHT_MAGENTA");	
	LCD_SetColor(LIGHT_YELLOW);  LCD_FillRect(200,267,500,16);  LCD_DisplayString(0,267,"LIGHT_YELLOW");	
	LCD_SetColor(LIGHT_GREY);    LCD_FillRect(200,289,500,16);	LCD_DisplayString(0,289,"LIGHT_GREY");  	

	LCD_SetColor(DARK_BLUE);	  LCD_FillRect(200,311,500,16);  LCD_DisplayString(0,311,"DARK_BLUE");		
	LCD_SetColor(DARK_GREEN);    LCD_FillRect(200,333,500,16);  LCD_DisplayString(0,333,"DARK_GREEN");		
	LCD_SetColor(DARK_RED);      LCD_FillRect(200,355,500,16);  LCD_DisplayString(0,355,"DARK_RED");		
	LCD_SetColor(DARK_CYAN);     LCD_FillRect(200,377,500,16);  LCD_DisplayString(0,377,"DARK_CYAN");		
	LCD_SetColor(DARK_MAGENTA);  LCD_FillRect(200,399,500,16);  LCD_DisplayString(0,399,"DARK_MAGENTA");	
	LCD_SetColor(DARK_YELLOW);   LCD_FillRect(200,421,500,16);  LCD_DisplayString(0,421,"DARK_YELLOW");	
	LCD_SetColor(DARK_GREY);     LCD_FillRect(200,443,500,16);	LCD_DisplayString(0,443,"DARK_GREY");	

	HAL_Delay(3000);
}


/*************************************************************************************************
*	函 数 名:	LCD_Test_Color
*
*	函数功能:	颜色测试
*
*	说    明:	无	
*************************************************************************************************/

void LCD_Test_Color(void)
{
	uint16_t i;
	
	
// 绘制初始界面，包括标题、LOGO以及进度条>>>>>

	LCD_SetBackColor(0xffB9EDF8); 			//	设置背景色，使用自定义颜色
	LCD_Clear(); 									//	清屏，刷背景色
	
	LCD_SetTextFont(&CH_Font32);				// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0xff333333);					//	设置画笔色，使用自定义颜色
	LCD_DisplayText(334, 160,"\xD1\xD5\xC9\xAB\xBB\xE6\xD6\xC6");	// 显示文本
	
	LCD_SetColor(0xfffd7923);					//	设置画笔色，使用自定义颜色
	LCD_DrawImage(  280, 218, 240, 83, Image_FANKE_240x83) ;		// 显示LOGO图片

	LCD_SetColor(LIGHT_YELLOW);		//	设置画笔色
	for(i=0;i<150;i++)
   {
		LCD_FillRect(100,330,4*i,6);	// 绘制矩形，实现简易进度条的效果
		HAL_Delay(10);	
   }		
	
// 颜色测试>>>>>
		
	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色
	
	LCD_SetTextFont(&CH_Font32);			// 设置2424中文字体,ASCII字体对应为2412
	LCD_SetColor(LCD_WHITE);				// 设置画笔颜色
	
	LCD_DisplayText(42,70,"\xBA\xCB\xD0\xC4\xB0\xE5\xD0\xCD\xBA\xC5\xA3\xBA" "FK743M1");
	LCD_DisplayText(42,110,"\xC6\xC1\xC4\xBB\xB7\xD6\xB1\xE6\xC2\xCA\xA3\xBA" "800*480");		
	LCD_DisplayText(42,150,"RGB" "\xC8\xFD\xBB\xF9\xC9\xAB\xC9\xAB\xBD\xD7\xB2\xE2\xCA\xD4");	
	
	//使用画线函数绘制三基色色条
	for(i=0;i<255;i++)
	{
		LCD_SetColor( LCD_RED-(i<<16) );
		LCD_DrawLine(30+2*i,  240,30+2*i,  280);	
		LCD_DrawLine(30+2*i+1,240,30+2*i+1,280);
	}
	for(i=0;i<255;i++)
	{
		LCD_SetColor( LCD_GREEN-(i<<8) );
		LCD_DrawLine(30+2*i,  290,30+2*i,  330);	
		LCD_DrawLine(30+2*i+1,290,30+2*i+1,330);
	}
	for(i=0;i<255;i++)
	{
		LCD_SetColor( LCD_BLUE-i );
		LCD_DrawLine(30+2*i,  350,30+2*i,  390);	
		LCD_DrawLine(30+2*i+1,350,30+2*i+1,390);
	}	
	HAL_Delay(3000);	
	
	
}

/*************************************************************************************************
*	函 数 名:	LCD_Test_GrahicTest
*
*	函数功能:	2D图形绘制
*
*	说    明:	无	
*************************************************************************************************/

void LCD_Test_GrahicTest(void)
{
	uint16_t time = 80;		// 延时时间
	uint16_t i;					// 计数变量
		
// 绘制初始界面，包括标题、LOGO以及进度条>>>>>

	LCD_SetBackColor(0xffB9EDF8); 			//	设置背景色，使用自定义颜色
	LCD_Clear(); 									//	清屏，刷背景色
	
	LCD_SetTextFont(&CH_Font32);				// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0xff333333);					//	设置画笔色，使用自定义颜色
	LCD_DisplayText(334, 160,"\xBB\xE6\xCD\xBC\xB2\xE2\xCA\xD4");	// 显示文本
	
	LCD_SetColor(0xfffd7923);					//	设置画笔色，使用自定义颜色
	LCD_DrawImage(  280, 218, 240, 83, Image_FANKE_240x83) ;		// 显示LOGO图片

	LCD_SetColor(LIGHT_YELLOW);		//	设置画笔色
	for(i=0;i<150;i++)
   {
		LCD_FillRect(100,330,4*i,6);	// 绘制矩形，实现简易进度条的效果
		HAL_Delay(10);	
   }			
	
// 2D图形绘制>>>>>>>
	
	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色	

	LCD_SetColor(LCD_WHITE);	
	LCD_DrawLine(0,0,799,0);		// 在屏幕的边界画线
	LCD_DrawLine(0,479,799,479);
	LCD_DrawLine(0,0,0,479);
	LCD_DrawLine(799,0,799,479);	
	
	LCD_SetColor(LCD_RED);    LCD_FillCircle(120,350,80);		//填充圆形
	LCD_SetColor(LCD_GREEN);  LCD_FillCircle(170,350,80); 	
	LCD_SetColor(LCD_BLUE);   LCD_FillCircle(220,350,80);  	
	
	LCD_SetColor(LIGHT_GREY);
	LCD_DrawLine(5,5,400,5);	HAL_Delay(time);			//画直线
	LCD_DrawLine(5,10,300,10);	HAL_Delay(time);
	LCD_DrawLine(5,15,200,15); HAL_Delay(time);
	LCD_DrawLine(5,20,100,20);	HAL_Delay(time);	

	LCD_SetColor(LIGHT_CYAN);
	LCD_DrawCircle(600,120,100);	HAL_Delay(time);		//绘制圆形
	LCD_DrawCircle(600,120,80);   HAL_Delay(time);
	LCD_DrawCircle(600,120,60);   HAL_Delay(time);
	LCD_DrawCircle(600,120,40);   HAL_Delay(time);
	
	LCD_SetColor(LCD_RED);	
	LCD_DrawRect(5,35,400,150);  HAL_Delay(time);			//绘制矩形
	LCD_DrawRect(30,50,350,120); HAL_Delay(time);
	LCD_DrawRect(55,65,300,90);  HAL_Delay(time);
	LCD_DrawRect(80,80,250,60);  HAL_Delay(time);

	LCD_SetColor(LIGHT_MAGENTA);	
	LCD_DrawEllipse(590,350,200,100); HAL_Delay(time);	//绘制椭圆
	LCD_DrawEllipse(590,350,170,80);  HAL_Delay(time);
	LCD_DrawEllipse(590,350,140,60);  HAL_Delay(time);
	LCD_DrawEllipse(590,350,110,40);  HAL_Delay(time);

	HAL_Delay(2000);		
}
/*************************************************************************************************
*	函 数 名:	LCD_Test_Image
*
*	函数功能:	图片显示测试
*
*	说    明:	无	
*************************************************************************************************/

void LCD_Test_Image(void)
{
	uint16_t i;					// 计数变量
		
// 绘制初始界面，包括标题、LOGO以及进度条>>>>>
	
	LCD_SetBackColor(0xffB9EDF8); 			//	设置背景色，使用自定义颜色
	LCD_Clear(); 									//	清屏，刷背景色
	
	LCD_SetTextFont(&CH_Font32);				// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0xff333333);					//	设置画笔色，使用自定义颜色
	LCD_DisplayText(334, 160,"\xCD\xBC\xC6\xAC\xBB\xE6\xD6\xC6");	// 显示文本
	
	LCD_SetColor(0xfffd7923);					//	设置画笔色，使用自定义颜色
	LCD_DrawImage(  280, 218, 240, 83, Image_FANKE_240x83) ;		// 显示LOGO图片

	LCD_SetColor(LIGHT_YELLOW);		//	设置画笔色
	for(i=0;i<150;i++)
   {
		LCD_FillRect(100,330,4*i,6);	// 绘制矩形，实现简易进度条的效果
		HAL_Delay(10);	
   }		
// 图片绘制>>>>>>>	

	LCD_SetBackColor(LCD_BLACK); 			//	设置背景色
	LCD_Clear(); 								// 清屏
	
	LCD_SetColor( 0xffF6E58D);
	LCD_DrawImage( 185, 116, 83, 83, Image_Android_83x83) ;	// 显示图片
	
	LCD_SetColor( 0xffFFAB91);
	LCD_DrawImage( 359, 124, 83, 83, Image_Cloud_83x83) ;		// 显示图片
	
	LCD_SetColor( 0xff8AC6D1);
	LCD_DrawImage( 533, 124, 83, 83, Image_Folder_83x83) ;	// 显示图片

	LCD_SetColor( 0xffDFF9FB);
	LCD_DrawImage( 185, 270, 83, 83, Image_Message_83x83) ;	// 显示图片
	
	LCD_SetColor( 0xff9DD3A8);
	LCD_DrawImage( 359, 270, 83, 83, Image_Toys_83x83) ;		// 显示图片
	
	LCD_SetColor( 0xffFF8753);
	LCD_DrawImage( 533, 270, 83, 83, Image_Video_83x83) ;		// 显示图片

	HAL_Delay(2000);
	
	LCD_SetBackColor(LCD_WHITE); 			//	设置背景色
	LCD_Clear(); 								// 清屏
	LCD_SetColor( LCD_BLACK);				// 设置画笔
	LCD_DrawImage( 159, 120, 480, 239, Image_FANKE_480x239) ;	// 显示图片
	HAL_Delay(2000);
	
}


/*************************************************************************************************
*	函 数 名:	LCD_Test_Vertical
*
*	函数功能:	竖屏测试
*
*	说    明:	无	
*************************************************************************************************/

void  LCD_Test_Vertical(void)
{
	uint16_t i;
	uint16_t time = 100;	
	
	LCD_DisplayDirection(Direction_V); // 切换到竖屏显示
	
// 绘制初始界面，包括标题、LOGO以及进度条>>>>>
	
	LCD_SetBackColor(0xffB9EDF8); 			//	设置背景色
	LCD_Clear(); 									//	清屏，刷背景色
	
	LCD_SetTextFont(&CH_Font32);				// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0xff333333);					//	设置画笔色
	LCD_DisplayText(180, 260,"\xCA\xFA\xC6\xC1\xCF\xD4\xCA\xBE");	// 显示文本
	
	LCD_SetColor(0xfffd7923);					//	设置画笔色
	LCD_DrawImage( 120, 320, 240, 83, Image_FANKE_240x83) ;		// 显示LOGO图片

	LCD_SetColor(LIGHT_YELLOW);		//	设置画笔色
	for(i=0;i<130;i++)
   {
		LCD_FillRect(45,450,3*i,6);	// 绘制矩形，实现简易进度条的效果
		HAL_Delay(10);	
   }



// 绘制其它内容>>>>>>>		
	
	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色		

	LCD_SetColor(LCD_WHITE);
	LCD_SetFont(&Font32);LCD_DisplayString(0, 0,"!#$%&'()*+,-.0123456789:;<=>"); HAL_Delay(time);		
	LCD_SetFont(&Font24);LCD_DisplayString(0,32,"!#$%&'()*+,-.0123456789:;<=>"); HAL_Delay(time);
	LCD_SetFont(&Font20);LCD_DisplayString(0,56,"!#$%&'()*+,-.0123456789:;<=>"); HAL_Delay(time);	
	LCD_SetFont(&Font16);LCD_DisplayString(0,76,"!#$%&'()*+,-.0123456789:;<=>"); HAL_Delay(time);	
	LCD_SetFont(&Font12);LCD_DisplayString(0,92,"!#$%&'()*+,-.0123456789:;<=>"); HAL_Delay(time);	
                                                                                                      
	LCD_SetColor(LCD_CYAN);                                                                            
	LCD_SetFont(&Font12);LCD_DisplayString(0,104,"!#$%&'()*+,-.0123456789:;<=>");HAL_Delay(time);	
	LCD_SetFont(&Font16);LCD_DisplayString(0,116,"!#$%&'()*+,-.0123456789:;<=>");HAL_Delay(time);	
	LCD_SetFont(&Font20);LCD_DisplayString(0,132,"!#$%&'()*+,-.0123456789:;<=>");HAL_Delay(time);		
	LCD_SetFont(&Font24);LCD_DisplayString(0,152,"!#$%&'()*+,-.0123456789:;<=>");HAL_Delay(time);		
	LCD_SetFont(&Font32);LCD_DisplayString(0,176,"!#$%&'()*+,-.0123456789:;<=>");HAL_Delay(time);	

	LCD_SetTextFont(&CH_Font24);			// 设置2424中文字体,ASCII字体对应为2412
	LCD_SetColor(LCD_YELLOW);				// 设置画笔，黄色
	LCD_DisplayText(0, 230,"\xCE\xC4\xB1\xBE\xCF\xD4\xCA\xBE\xA3\xAC\xBF\xC9\xCF\xD4\xCA\xBE\xD6\xD0\xCE\xC4\xBA\xCD" "ASCII" "\xD7\xD6\xB7\xFB\xBC\xAF");
	LCD_DisplayText(0, 260,"\xD3\xC3\xBB\xA7\xBF\xC9\xB8\xF9\xBE\xDD\xD0\xE8\xC7\xF3\xA3\xAC\xB6\xD4\xD7\xD6\xBF\xE2\xBD\xF8\xD0\xD0\xD4\xF6\xCC\xED\xBA\xCD\xC9\xBE\xBC\xF5");	

	LCD_SetTextFont(&CH_Font12);			// 设置1212中文字体,ASCII字体对应为1206
	LCD_SetColor(0Xff8AC6D1);						// 设置画笔
	LCD_DisplayText(28, 310,"1212" "\xD6\xD0\xCE\xC4\xD7\xD6\xCC\xE5\xA3\xBA\xB7\xB4\xBF\xCD\xBF\xC6\xBC\xBC");	
	
	LCD_SetTextFont(&CH_Font16);			// 设置1616中文字体,ASCII字体对应为1608
	LCD_SetColor(0XffC5E1A5);						// 设置画笔
	LCD_DisplayText(28, 330,"1616" "\xD6\xD0\xCE\xC4\xD7\xD6\xCC\xE5\xA3\xBA\xB7\xB4\xBF\xCD\xBF\xC6\xBC\xBC");		
	
	// LCD_SetTextFont(&CH_Font20);			// 设置2020中文字体,ASCII字体对应为2010
	// LCD_SetColor(0Xff2D248A);						// 设置画笔
	// LCD_DisplayText(28, 355,"2020" "\xD6\xD0\xCE\xC4\xD7\xD6\xCC\xE5\xA3\xBA\xB7\xB4\xBF\xCD\xBF\xC6\xBC\xBC");

	LCD_SetTextFont(&CH_Font24);			// 设置2424中文字体,ASCII字体对应为2412
	LCD_SetColor(0XffFF585D);						// 设置画笔
	LCD_DisplayText(28, 385,"2424" "\xD6\xD0\xCE\xC4\xD7\xD6\xCC\xE5\xA3\xBA\xB7\xB4\xBF\xCD\xBF\xC6\xBC\xBC");		

	LCD_SetTextFont(&CH_Font32);			// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0XffF6003C);						// 设置画笔
	LCD_DisplayText(28, 425,"3232" "\xD6\xD0\xCE\xC4\xD7\xD6\xCC\xE5\xA3\xBA\xB7\xB4\xBF\xCD\xBF\xC6\xBC\xBC");		
	
	LCD_SetTextFont(&CH_Font32);			// 设置3232中文字体,ASCII字体对应为3216
	LCD_SetColor(0xff587058);
	LCD_DisplayText(40,500,   "STM32H743 LTDC " "\xB2\xE2\xCA\xD4");	
	LCD_DisplayText(40,500+40,"\xBA\xCB\xD0\xC4\xB0\xE5\xD0\xCD\xBA\xC5\xA3\xBA" "FK743M1");	
	LCD_DisplayText(40,500+80,"\xC6\xC1\xC4\xBB\xB7\xD6\xB1\xE6\xC2\xCA\xA3\xBA" "800*480");	
	
	HAL_Delay(2000);

	time = 80;
	
	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色	
	
	LCD_SetColor(LCD_RED);    LCD_FillCircle(120,285,80);		// 填充圆
	LCD_SetColor(LCD_GREEN);  LCD_FillCircle(170,285,80); 
	LCD_SetColor(LCD_BLUE);   LCD_FillCircle(220,285,80);  	
	
   LCD_SetColor(LCD_RED);	
	LCD_DrawRect(5,35,350,150);  HAL_Delay(time);		// 画矩形
	LCD_DrawRect(30,50,300,120); HAL_Delay(time);
	LCD_DrawRect(55,65,250,90);  HAL_Delay(time);
	LCD_DrawRect(80,80,200,60);  HAL_Delay(time);
	
	LCD_SetColor(LIGHT_GREY);
	LCD_DrawLine(5,5,400,5);	HAL_Delay(time);	// 画线
	LCD_DrawLine(5,10,300,10);	HAL_Delay(time);
	LCD_DrawLine(5,15,200,15); HAL_Delay(time);
	LCD_DrawLine(5,20,100,20);	HAL_Delay(time);		
	
   
   LCD_SetColor(LCD_YELLOW);	LCD_DrawCircle(100,480,80);   HAL_Delay(time);	// 画圆
	LCD_SetColor(LCD_CYAN);		LCD_DrawCircle(150,480,80);   HAL_Delay(time);
   LCD_SetColor(LCD_MAGENTA); LCD_DrawCircle(200,480,80);   HAL_Delay(time);
   LCD_SetColor(LCD_RED);		LCD_DrawCircle(250,480,80);   HAL_Delay(time);

   LCD_SetColor(LIGHT_MAGENTA);	
	LCD_DrawEllipse(200,680,200,100); HAL_Delay(time);		// 画椭圆
	LCD_DrawEllipse(200,680,170,80);  HAL_Delay(time);
	LCD_DrawEllipse(200,680,140,60);  HAL_Delay(time);
	LCD_DrawEllipse(200,680,110,40);  HAL_Delay(time);
                                                        
	HAL_Delay(2000);	
	
	LCD_SetBackColor(LCD_BLACK); //设置背景色
	LCD_Clear(); //清屏，刷背景色

	LCD_SetFont(&Font16);
	LCD_SetColor(LCD_BLUE);	  	  LCD_FillRect(150,  3,250,16);	LCD_DisplayString(0,  3,"LCD_BLUE"); 			
	LCD_SetColor(LCD_GREEN);  	  LCD_FillRect(150, 25,250,16);  LCD_DisplayString(0, 25,"LCD_GREEN");	
	LCD_SetColor(LCD_RED);    	  LCD_FillRect(150, 47,250,16);  LCD_DisplayString(0, 47,"LCD_RED");	 	
	LCD_SetColor(LCD_CYAN);   	  LCD_FillRect(150, 69,250,16);  LCD_DisplayString(0, 69,"LCD_CYAN");		
	LCD_SetColor(LCD_MAGENTA);	  LCD_FillRect(150, 91,250,16);  LCD_DisplayString(0, 91,"LCD_MAGENTA");	
	LCD_SetColor(LCD_YELLOW); 	  LCD_FillRect(150,113,250,16);  LCD_DisplayString(0,113,"LCD_YELLOW");		
	LCD_SetColor(LCD_GREY);   	  LCD_FillRect(150,135,250,16);	LCD_DisplayString(0,135,"LCD_GREY");		
                                                                                          
	LCD_SetColor(LIGHT_BLUE);	  LCD_FillRect(150,157,250,16);  LCD_DisplayString(0,157,"LIGHT_BLUE");		
	LCD_SetColor(LIGHT_GREEN);   LCD_FillRect(150,179,250,16);  LCD_DisplayString(0,179,"LIGHT_GREEN");	
	LCD_SetColor(LIGHT_RED);     LCD_FillRect(150,201,250,16);  LCD_DisplayString(0,201,"LIGHT_RED");	   
	LCD_SetColor(LIGHT_CYAN);    LCD_FillRect(150,223,250,16);  LCD_DisplayString(0,223,"LIGHT_CYAN");	   
	LCD_SetColor(LIGHT_MAGENTA); LCD_FillRect(150,245,250,16);  LCD_DisplayString(0,245,"LIGHT_MAGENTA");	
	LCD_SetColor(LIGHT_YELLOW);  LCD_FillRect(150,267,250,16);  LCD_DisplayString(0,267,"LIGHT_YELLOW");	
	LCD_SetColor(LIGHT_GREY);    LCD_FillRect(150,289,250,16);	LCD_DisplayString(0,289,"LIGHT_GREY");  	
	                                                                                       
	LCD_SetColor(DARK_BLUE);	  LCD_FillRect(150,311,250,16);  LCD_DisplayString(0,311,"DARK_BLUE");		
	LCD_SetColor(DARK_GREEN);    LCD_FillRect(150,333,250,16);  LCD_DisplayString(0,333,"DARK_GREEN");		
	LCD_SetColor(DARK_RED);      LCD_FillRect(150,355,250,16);  LCD_DisplayString(0,355,"DARK_RED");		
	LCD_SetColor(DARK_CYAN);     LCD_FillRect(150,377,250,16);  LCD_DisplayString(0,377,"DARK_CYAN");		
	LCD_SetColor(DARK_MAGENTA);  LCD_FillRect(150,399,250,16);  LCD_DisplayString(0,399,"DARK_MAGENTA");	
	LCD_SetColor(DARK_YELLOW);   LCD_FillRect(150,421,250,16);  LCD_DisplayString(0,421,"DARK_YELLOW");	
	LCD_SetColor(DARK_GREY);     LCD_FillRect(150,443,250,16);	LCD_DisplayString(0,443,"DARK_GREY");	
	
	//刷颜色条
	for(i=0;i<255;i++){LCD_SetColor( LCD_RED-(i<<16) ); 	LCD_DrawLine(150+i,465,150+i,481);}
	for(i=0;i<255;i++){LCD_SetColor( LCD_GREEN-(i<<8) );	LCD_DrawLine(150+i,487,150+i,503);}
	for(i=0;i<255;i++){LCD_SetColor( LCD_BLUE-i );		 	LCD_DrawLine(150+i,509,150+i,525);}
	for(i=0;i<255;i++){LCD_SetColor( LIGHT_RED-(i<<16) );	LCD_DrawLine(150+i,531,150+i,547);}LCD_DisplayString(0,531,"0xFF8080");
	for(i=0;i<255;i++){LCD_SetColor( LIGHT_GREEN-(i<<8) );LCD_DrawLine(150+i,553,150+i,568);}LCD_DisplayString(0,553,"0x80FF80");
	for(i=0;i<255;i++){LCD_SetColor( LIGHT_BLUE-i );		LCD_DrawLine(150+i,575,150+i,591);}LCD_DisplayString(0,575,"0x8080FF");	
	for(i=0;i<255;i++){LCD_SetColor( DARK_BLUE-(i<<16) );	LCD_DrawLine(150+i,597,150+i,613);}LCD_DisplayString(0,597,"0x000080");	
	for(i=0;i<255;i++){LCD_SetColor( DARK_BLUE-(i<<8) );	LCD_DrawLine(150+i,619,150+i,635);}LCD_DisplayString(0,619,"0x000080");	
	for(i=0;i<255;i++){LCD_SetColor( DARK_RED-i );			LCD_DrawLine(150+i,641,150+i,657);}LCD_DisplayString(0,641,"0x800000");		
	for(i=0;i<255;i++){LCD_SetColor( DARK_RED-(i<<8));		LCD_DrawLine(150+i,663,150+i,679);}LCD_DisplayString(0,663,"0x800000");	
	for(i=0;i<255;i++){LCD_SetColor( DARK_GREEN-(i<<16)); LCD_DrawLine(150+i,685,150+i,701);}LCD_DisplayString(0,685,"0x008000");	
	for(i=0;i<255;i++){LCD_SetColor( DARK_GREEN-i);			LCD_DrawLine(150+i,707,150+i,723);}LCD_DisplayString(0,707,"0x008000");	
	for(i=0;i<255;i++){LCD_SetColor( DARK_CYAN-(i<<16));	LCD_DrawLine(150+i,729,150+i,745);}LCD_DisplayString(0,729,"0x008080");	
	for(i=0;i<255;i++){LCD_SetColor( DARK_YELLOW-i);		LCD_DrawLine(150+i,751,150+i,767);}LCD_DisplayString(0,751,"0x808000");	
	for(i=0;i<255;i++){LCD_SetColor( DARK_MAGENTA-(i<<8));LCD_DrawLine(150+i,773,150+i,789);}LCD_DisplayString(0,773,"0x800080");	

	HAL_Delay(3000);		
	LCD_DisplayDirection(Direction_H); // 切换回横屏显示
}

/*************************************************************************************************
*	函 数 名:	LCD_Test_Touch
*
*	函数功能:	触摸测试
*
*	说    明:	1. 进入测试页后会实时显示触点数量和坐标
*					2. 在下方区域拖动可以留下轨迹
*					3. 触摸右上角的 CLEAR 区域可以清空轨迹
*************************************************************************************************/

void LCD_Test_Touch(void)
{
	static const uint32_t pointColors[TOUCH_MAX] = {
		LCD_RED, LCD_GREEN, LCD_BLUE, LCD_YELLOW, LCD_CYAN
	};
	char lineBuffer[48];
	uint8_t i = 0;

	LCD_DisplayDirection(Direction_H);
	LCD_SetBackColor(0xff101820);
	LCD_Clear();

	LCD_SetColor(0xffF2AA4C);
	LCD_SetFont(&Font32);
	LCD_DisplayString(20, 16, "Touch Test");

	LCD_SetColor(LCD_WHITE);
	LCD_SetFont(&Font16);
	LCD_DisplayString(20, 56, "Draw in the box below.");
	LCD_DisplayString(20, 78, "Top-right CLEAR area erases the canvas.");
	LCD_DisplayString(20, 100, "QSPI Chinese font demo ran before this page.");

	LCD_SetColor(LIGHT_GREY);
	LCD_DrawRect(10, 136, 780, 332);

	LCD_SetColor(DARK_RED);
	LCD_DrawRect(640, 10, 140, 36);
	LCD_SetFont(&Font24);
	LCD_DisplayString(670, 16, "CLEAR");

	for (;;)
	{
		Touch_Scan();

		LCD_SetBackColor(0xff101820);
		LCD_ClearRect(20, 112, 760, 20);
		LCD_ClearRect(20, 468, 760, 12);

		LCD_SetColor(LIGHT_CYAN);
		LCD_SetFont(&Font16);
		sprintf(lineBuffer, "touches: %d", touchInfo.flag ? touchInfo.num : 0);
		LCD_DisplayString(20, 112, lineBuffer);

		if(touchInfo.flag != 0U)
		{
			for(i = 0; i < touchInfo.num; i++)
			{
				sprintf(lineBuffer, "P%d=(%03d,%03d)", i + 1, touchInfo.x[i], touchInfo.y[i]);
				LCD_DisplayString(180 + i * 120, 112, lineBuffer);

				if((touchInfo.x[i] >= 640U) && (touchInfo.y[i] <= 46U))
				{
					LCD_SetBackColor(0xff101820);
					LCD_ClearRect(12, 138, 776, 328);
					LCD_SetColor(LIGHT_GREY);
					LCD_DrawRect(10, 136, 780, 332);
					continue;
				}

				if((touchInfo.x[i] >= 12U) && (touchInfo.x[i] <= 788U) &&
				   (touchInfo.y[i] >= 138U) && (touchInfo.y[i] <= 466U))
				{
					LCD_SetColor(pointColors[i % TOUCH_MAX]);
					LCD_FillCircle(touchInfo.x[i], touchInfo.y[i], 5);
				}
			}
		}
		else
		{
			LCD_SetColor(LIGHT_GREY);
			LCD_DisplayString(180, 112, "Release. Touch to draw.");
		}

		HAL_Delay(20);
	}
}







