/*
 * main.c
 *
 *  Created on: 2021年8月21日
 *      Author: DON
 */
#include "xparameters.h"
#include "xgpiops.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xplatform_info.h"
#include <xil_printf.h>
#include "sleep.h"

//以下常量映射到xparameters.h文件
#define GPIO_DEVICE_ID XPAR_XGPIOPS_0_DEVICE_ID //PS端GPIO器件ID
#define INTC_DEVICE_ID XPAR_SCUGIC_SINGLE_DEVICE_ID //通用中断控制器ID
#define GPIO_INTERRUPT_ID XPAR_XGPIOPS_0_INTR     //PS端GPIO中断ID

//定义使用到的MIO引脚号
#define KEY 11   //KEY 连接到MIO11
#define LED 0    //LED链接到MIO0

static void intr_handler(void *callback_ref);
int setup_interrupt_system(XScuGic *gic_ins_ptr,XGpioPs *gpio,u16 GpioIntrId);

XGpioPs gpio;   //PS端GPIO驱动实例
XScuGic intc;   //通用中断控制器驱动实例
u32 key_press;  //KEY按键按下的标志
u32 key_val;    //用于控制LED的键值

int main(void)
{
	int status;
	XGpioPs_Config *ConfigPtr;  //PS端GPIO配置信息
	xil_printf("Gpio interrrupt test \r\n");

	//根据器件ID查找配置信息
	ConfigPtr = XGpioPs_LookupConfig(GPIO_DEVICE_ID);
	if(ConfigPtr == NULL){
		return XST_FAILURE;
	}
	//初始化Gpio driver
	XGpioPs_CfgInitialize(&gpio,ConfigPtr,ConfigPtr->BaseAddr);
	//设置KEY所连接的MIO引脚的方向为输入
	XGpioPs_SetDirectionPin(&gpio,KEY,0);
	//设置LED所连接的MIO引脚的方向为输出并使能输出
	XGpioPs_SetDirectionPin(&gpio,LED,1);
	XGpioPs_SetOutputEnablePin(&gpio,LED,1);
	XGpioPs_WritePin(&gpio,LED,0X0);

	//建立中断，出现错误则打印信息并退出
	status = setup_interrupt_system(&intc,&gpio,GPIO_INTERRUPT_ID);
	if(status != XST_SUCCESS){
		xil_printf("Setup interrupt system failed\r\n");
		return XST_FAILURE;
	}
	//中断触发时，key_press为TRUE,延时一段时间后判断按键是否按下，是则反转LED
	while(1){
		if(key_press){
			usleep(20000);
			if(XGpioPs_ReadPin(&gpio,KEY)==0){
				key_val = ~key_val;
				XGpioPs_WritePin(&gpio,LED,key_val);
			}
			key_press = FALSE;
			XGpioPs_IntrClearPin(&gpio,KEY);   //清除按键KEY中断
			XGpioPs_IntrEnablePin(&gpio,KEY);  //使能按键KEY中断
		}
	}
	return XST_SUCCESS;
}

//中断处理函数
//@param CallBackRef 是指向上层回调引用的指针
static void intr_handler(void *callback_ref)
{
	XGpioPs *gpio = (XGpioPs *) callback_ref;

	//读取KEY按键引脚的中断状态，判断是否发生中断
	if(XGpioPs_IntrGetStatusPin(gpio,KEY)){
		key_press = TRUE;
		XGpioPs_IntrDisablePin(gpio,KEY);    //屏蔽按键KEY中断
	}
}

//建立中断系统，使能KEY按键的下降沿中断
//@param GicInstancePtr 是一个指向XScuGic驱动实例的指针
//@param gpio是一个指向连接到中断的GPIO组件实例的指针
//@param GpioIntrId是Gpio中断ID
//@return 如果成功返回XST_SUCCESS,否则返回XST_FAILURE
int setup_interrupt_system(XScuGic *gic_ins_ptr,XGpioPs *gpio,u16 GpioIntrId)
{
	int status;
	XScuGic_Config *IntcConfig;   //中断控制器配置信息
	//查找中断控制器配置信息并初始化中断控制器驱动
	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);
	if(NULL == IntcConfig)
	{
		return XST_FAILURE;
	}
	status = XScuGic_CfgInitialize(gic_ins_ptr,IntcConfig,IntcConfig->CpuBaseAddress);
	if(status != XST_SUCCESS){
		return XST_FAILURE;
	}
	//设置并使能中断异常
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler) XScuGic_InterruptHandler,gic_ins_ptr);
	Xil_ExceptionEnable();
	//为中断设置中断处理函数
	status = XScuGic_Connect(gic_ins_ptr,GpioIntrId,
			(Xil_ExceptionHandler) intr_handler, (void *)gpio);
	if(status != XST_SUCCESS){
		return status;
	}

	//使能来自于Gpio器件的中断
	XScuGic_Enable(gic_ins_ptr,GpioIntrId);

	//设置KEY按键的中断类型为下降沿中断
	XGpioPs_SetIntrTypePin(gpio,KEY,XGPIOPS_IRQ_TYPE_EDGE_FALLING);

	//使能按键KEY中断
	XGpioPs_IntrEnablePin(gpio,KEY);
	return XST_SUCCESS;
}
