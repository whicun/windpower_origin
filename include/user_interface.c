/****************************************Copyright (c)**************************************************
**                       		     北	京	交	通	大	学
**                                        电气工程学院
**                                         614实验室
**
**                              
**
**--------------文件信息--------------------------------------------------------------------------------
**文   件   名: user_interface.c
**创   建   人: 
**最后修改日期: 
**描        述: 1.5MW双馈电机转子侧变流器外设控制程序--双馈--左云风场
				包括对eeprom,sci,spi,ad,da的控制程序
**              
**--------------历史版本信息----------------------------------------------------------------------------
** 创建人: 
** 版  本: 
** 日　期: 
** 描　述: 
**
**--------------当前版本修订------------------------------------------------------------------------------
** 修改人: 
** 日　期: 
** 描　述: 
**
**------------------------------------------------------------------------------------------------------
********************************************************************************************************/
#include "DSP2833x_Device.h"     // Headerfile Include File
#include "DSP2833x_Examples.h"   // Examples Include File
//函数声明
Uint16 		CheckCode(Uint16 index);
Uint16 		SciDatpro(void);
/*********************************************************************************************************
** 函数名称: EeStart
** 功能描述: 开始对eeprom的操作
** 输　入: 
** 输　出:        
** 注  释: 	 时钟线高时数据线下降沿为开始
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void EeStart(void)
{
	EALLOW;
    GpioDataRegs.GPBSET.bit.GPIO32 = 1;     	 //数据高
	GpioCtrlRegs.GPBDIR.bit.GPIO32 = 1;  		//数据线变为输出口
	EDIS;
	DELAY_US(DELAY_EE);
    GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
	DELAY_US(DELAY_EE);
	GpioDataRegs.GPBCLEAR.bit.GPIO32 = 1;		 	//数据低
	DELAY_US(DELAY_EE);
   	GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低
	DELAY_US(DELAY_EE);
}
/*********************************************************************************************************
** 函数名称: EeStop
** 功能描述: 结束对eeprom的操作
** 输　入: 
** 输　出:        
** 注  释: 	 时钟线高时数据线上升沿为结束
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void EeStop(void)
{
	EALLOW;
    GpioDataRegs.GPBCLEAR.bit.GPIO32 = 1;     	 //数据低
	GpioCtrlRegs.GPBDIR.bit.GPIO32 = 1;  		//数据线变为输出口
	EDIS;
	DELAY_US(DELAY_EE);
    GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
	DELAY_US(DELAY_EE);
    GpioDataRegs.GPBSET.bit.GPIO32 = 1;     	 //数据高
	DELAY_US(DELAY_EE);
   	GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低
	EALLOW;
	GpioCtrlRegs.GPBDIR.bit.GPIO32= 0;			//数据线变为输入口
	EDIS;
	DELAY_US(DELAY_EE);
}

/*********************************************************************************************************
** 函数名称: EeWrite
** 功能描述: 将number个字节数据（不包括地址）连续写入到eeprom（一般要求在同一页面）
** 输　入: 	 number,表示要写的字节数
** 输　出:        
** 注  释: 	 EEPROM.data[0]:写控制字;
**			 EEPROM.data[1-2]:待写数据地址; 
**			 EEPROM.data[3-x]:待写数据;
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void EeWrite(unsigned char number)
{
	int16 i,j;
//----------------------------------------------//发送起始位
	EeStart();
//----------------------------------------------//开始发送数据
	for(j=0;j<number+3;j++)						//先发低字节
	{
		for(i=0;i<8;i++)						//每个字节先发高字位
		{
			if((EEPROM.data[j] & ONEBYTE[i])==0)	//要发0
			{
				GpioDataRegs.GPBCLEAR.bit.GPIO32 =1;	//数据低
				DELAY_US(DELAY_EE);					
    			GpioDataRegs.GPBSET.bit.GPIO33 = 1; //时钟高
				DELAY_US(DELAY_EE);				
   	   		    GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;//时钟低
				DELAY_US(DELAY_EE);				
			}
			else									//要发1
			{
				GpioDataRegs.GPBSET.bit.GPIO32 =1;	//数据高
				DELAY_US(DELAY_EE);				
   				GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
				DELAY_US(DELAY_EE);				
   	   		    GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低
				DELAY_US(DELAY_EE);				
			}
		}
//----------------------------------------------//发完一个字节		
		EALLOW;
		GpioCtrlRegs.GPBDIR.bit.GPIO32= 0;		//数据线变为输入口
		EDIS;
    	GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
		DELAY_US(DELAY_EE);
		
		if(GpioDataRegs.GPBDAT.bit.GPIO32==1)	//如果数据线读到1表示没有应答
		{
			M_SetFlag(SL_EE_NOACK);				//置无应答标志
		}
		
   	    GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低

		EALLOW;
		GpioDataRegs.GPBCLEAR.bit.GPIO32= 1;		//数据低
		GpioCtrlRegs.GPBDIR.bit.GPIO32= 1;		//数据线变为输出口
		EDIS;
		DELAY_US(DELAY_EE);
	}
//----------------------------------------------//发送停止位
	EeStop();
}

/*********************************************************************************************************
** 函数名称: EeRead
** 功能描述: 从eeprom连续读出number个字节数据
** 输　入: 	 number,表示要读的字节数
** 输　出:   EEPROM.data[0-1]:读出的数据     
** 注  释: 	 先写写控制字,再写待读数据地址,再写读控制字,在读出数据
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void EeRead(unsigned char number)
{
	int16 i,j;
	
//----------------------------------------------//发送起始位
	EeStart();
//----------------------------------------------//开始发送地址数据
	for(j=0;j<3;j++)							//先发低字节
	{
		for(i=0;i<8;i++)						//每个字节先发高字位
		{
			if((EEPROM.data[j] & ONEBYTE[i])==0)	//要发0
			{
				GpioDataRegs.GPBCLEAR.bit.GPIO32 =1;	//数据低
				DELAY_US(DELAY_EE);				
   				GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
				DELAY_US(DELAY_EE);				
   	    		GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低
				DELAY_US(DELAY_EE);				
			}
			else									//要发1
			{
				GpioDataRegs.GPBSET.bit.GPIO32 =1;	//数据高
				DELAY_US(DELAY_EE);				
    			GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
				DELAY_US(DELAY_EE);				
   	   		    GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低
				DELAY_US(DELAY_EE);				
			}
		}
//----------------------------------------------//发完一个字节		
		EALLOW;
		GpioCtrlRegs.GPBDIR.bit.GPIO32= 0;		//数据线变为输入口
		EDIS;
   	    GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
		DELAY_US(DELAY_EE);
		
		if(GpioDataRegs.GPBDAT.bit.GPIO32==1)	//如果数据线读到1表示没有应答
		{
			M_SetFlag(SL_EE_NOACK);				//置无应答标志
		}
		
   	    GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低
		
		EALLOW;
		GpioDataRegs.GPBCLEAR.bit.GPIO32 =1;		//数据低
		GpioCtrlRegs.GPBDIR.bit.GPIO32= 1;		//数据线变为输出口
		EDIS;
		
		DELAY_US(DELAY_EE);
	}
	
//----------------------------------------------//再次发送起始位
	EeStart();
//----------------------------------------------//再次发送起始位
	EEPROM.data[0] |= 0x01;						//改为读指令
	for(i=0;i<8;i++)							//先发高字位
	{
		if((EEPROM.data[0] & ONEBYTE[i])==0)		//要发0
		{
			GpioDataRegs.GPBCLEAR.bit.GPIO32 =1;		//数据低
			DELAY_US(DELAY_EE);					
   		    GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
			DELAY_US(DELAY_EE);					
   	  	    GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低
			DELAY_US(DELAY_EE);					
		}
		else										//要发1
		{
			GpioDataRegs.GPBSET.bit.GPIO32 =1;		//数据高	
			DELAY_US(DELAY_EE);					
   		    GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
			DELAY_US(DELAY_EE);					
   	        GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低
			DELAY_US(DELAY_EE);					
		}
	}
//----------------------------------------------//发完一个字节
	EALLOW;
	GpioCtrlRegs.GPBDIR.bit.GPIO32 =0;			//数据线变为输入口
	EDIS;
    GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
	DELAY_US(DELAY_EE);
	
	if(GpioDataRegs.GPBDAT.bit.GPIO32==1)		//如果数据线读到1表示没有应答
	{
		M_SetFlag(SL_EE_NOACK);					//置无应答标志
	}
	
   	GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低
	DELAY_US(DELAY_EE);
//----------------------------------------------//开始读数据
	for(j=0;j<number;j++)						//先读低字节
	{
		EEPROM.data[j]=0;
		for(i=0;i<8;i++)						//每个字节先读高位
		{
   		    GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
			DELAY_US(DELAY_EE);	
			if(GpioDataRegs.GPBDAT.bit.GPIO32==1)//数据为1
			{
				EEPROM.data[j] |= ONEBYTE[i];	//为零则不变
			}
   	   	    GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低
			DELAY_US(DELAY_EE);
		}
//----------------------------------------------//收完一个字节
		if(j!=number-1)							//最后一个字节不发出应答
		{
			EALLOW;
			GpioDataRegs.GPBCLEAR.bit.GPIO32 =1;	//输出低来应答
			GpioCtrlRegs.GPBDIR.bit.GPIO32= 1;	//数据线为输出口
			EDIS;
			DELAY_US(DELAY_EE);
		}
   	    GpioDataRegs.GPBSET.bit.GPIO33 = 1;     	//时钟高
		DELAY_US(DELAY_EE);
   	    GpioDataRegs.GPBCLEAR.bit.GPIO33 = 1;     	//时钟低
		EALLOW;
		GpioCtrlRegs.GPBDIR.bit.GPIO32= 0;		//数据线为输入口
		EDIS;
		DELAY_US(DELAY_EE);		
	}
//----------------------------------------------//接收完毕,发送停止位
	EeStop();
}
/*********************************************************************************************************
** 函数名称: EeWpre
** 功能描述: 准备写入EEPROM的地址和数据
** 输　入: 	 index:待写入变量的序号
** 输　出:   EEPROM.data[0-4]:写控制字、待写数据的地址和待写数据    
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void EeWpre(unsigned char index)
{
	EEPROM.data[0]=0xA0;									//Slave ADdress
	EEPROM.data[1]=((index<<1)&0xFF00)>>8;					//MSB ADdress
	EEPROM.data[2]=(index<<1)&0xFF;							//LSB ADdress
	EEPROM.data[3]=*(FUNC[index].para_add) & 0x00ff;		//低8位数据
	EEPROM.data[4]=(*(FUNC[index].para_add) & 0xff00)>>8;	//高8位数据
}
/*********************************************************************************************************
** 函数名称: EeRpre
** 功能描述: 准备读取EEPROM数据的地址
** 输　入: 	 index:待读取变量的序号
** 输　出:   EEPROM.data[0-2]:读控制字和待读取数据的地址    
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void EeRpre(unsigned char index)
{
	EEPROM.data[0]=0xA0;							//Slave ADdress
	EEPROM.data[1]=((index<<1)&0xFF00)>>8;			//MSB ADdress
	EEPROM.data[2]=(index<<1)&0xFF;					//LSB ADdress
}
/*********************************************************************************************************
** 函数名称: EeWrword
** 功能描述: 写一个字的数据到EEPROM并读出来校验
** 输　入: 	 index:待写变量的序号
** 输　出:   
** 注  释: 	 先将待写数据写入eeprom再读出来检验
**			 如果检验不对在置标志位SL_EE_FAIL
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void EeWrword(unsigned char index)
{
	Uint16 data_rd;
	
	EeWpre(index);									
	EeWrite(2);											//写入2字节数据
	EeRead(2);											//将2字节数据读出
	
	data_rd=(EEPROM.data[1]<<8) | EEPROM.data[0];		//data_rd为读出的数据
	
	if(data_rd!=*(FUNC[index].para_add))
		M_SetFlag(SL_EE_FAIL);
}
/*********************************************************************************************************
** 函数名称: InitEeprom
** 功能描述: 初始化eeprom
** 输　入: 	 
** 输　出:   
** 注  释: 	 先将eeprom中的原有数据读出并进行检验
**			 如果出现数据错误则将eeprom中数据全部初始化
**			 检测在初始化过程中是否出现eeprom操作错误
**			 如果没有错误则置SL_CODEOK表示eeprom工作正常,否则清SL_CODEOK表示eeprom工作错误
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void InitEeprom(void)
{
	Uint16 i,j;
//	Uint16 j;
	if(DEBUG_OPER==0)
	{
//----------------------------------------------//将eeprom的数据全部读入ram
		for(i=0;i<PARA_NUM;i++)					
		{
			EeRpre(i);								//控制字和地址填入EEPROM.data[]
			EeRead(2);
												//读出的数据放入RAM
			*FUNC[i].para_add=(EEPROM.data[1]<<8) | EEPROM.data[0];	
		
			if(CheckCode(i)==1)						//检查程序的返回值=1则表示有错误
			{
				for(j=0;j<PARA_NUM;j++)
				{
					*FUNC[j].para_add=FUNC[j].init;	//RAM数据恢复初值
					EeWrword(j);					//写入2字节数据
				}
				break;								//数据校验有错则跳出
			}
		}
	}
	else
	{
//-----------------------------------------//调试时直接将EEPROM初始化!!!
		for(j=0;j<PARA_NUM;j++)
		{
			*FUNC[j].para_add=FUNC[j].init;	//RAM数据恢复初值
			EeWrword(j);					//写入2字节数据
		}
	}
//-----------------------------------------	
	if(M_ChkFlag(SL_EE_FAIL)==0)					
		M_SetFlag(SL_CODEOK);					//EEPROM正常
	else
		M_ClrFlag(SL_CODEOK);					//EEPROM故障
}
/*********************************************************************************************************
** 函数名称: CheckCode
** 功能描述: 检验eeprom中数据是否正确
** 输　入: 	 index,待检验数据的序号
** 输　出:   j,j=1表示数据错误;j=0表示数据正确
** 注  释: 	 检验数据是否在指定的范围之内
**			 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
Uint16 CheckCode(Uint16 index)						
{
	Uint16 j,data,max,min;
	int16  temp,max_temp,min_temp;
	
	data=*FUNC[index].para_add;
//----------------------------------------------//判断该数据的属性	
	if((FUNC[index].attr & MAX_PT)==1)			//具有最大值指针属性 
		max=*FUNC[index].max_add;
	else										//不具有最大值指针属性 
		max=FUNC[index].max;					//读出这个量的最大值
//----------------------------------------------//具有最小值指针属性			
	if((FUNC[index].attr & MIN_PT)==1)
		min=*FUNC[index].min_add;
	else 										//不具有最小值指针属性
		min=FUNC[index].min;					//读出这个量的最小值
//----------------------------------------------//开始校验
	j=0;										//置无故障标志
	if((FUNC[index].attr & SIG)==0)				//无符号数
	{
		if(data>max)				
			j=1;								//大于最大值，有错
		else if(data<min)						//小于最小值
		{
			if((FUNC[index].attr & VA0)==0)		
				j=1;							//又不具有VA0属性，有错
			else if(data!=0)					//具有VA0属性但又不为零，有错
				j=1;
		}
	}					
	else										//有符号数
	{
		temp=(int)data;							//全部转为有符号数
		max_temp=(int)max;
		min_temp=(int)min;
		if(temp>max_temp)						//大于最大值，有错
			j=1;
		else if(temp<min_temp)					//小于最小值，有错
			j=1;								//有符号数没有VA0属性
	}
//----------------------------------------------//返回值为j	
	return j;
}
/*********************************************************************************************************
** 函数名称: EeCtrl
** 功能描述: 程序运行过程中对eeprom数据的操作
** 输　入: 	 
** 输　出:   
** 注  释: 	 在eeprom正确的前提下，检测标志位
**			 SL_INIEE:		是否需要进行初始化eeprom操作;
							如果是则置SL_EEBUSY_INIEE,全部初始化完以后再清SL_INIEE和SL_EEBUSY_INIEE
**			 SL_MCODE:		是否需要修改功能码值
							如果是则置SL_EEBUSY_MCODE,将EEPROM.mcode中指定的数据写入eeprom
							操作完成后再清SL_MCODE和SL_EEBUSY_MCODE
**			 SL_ERRSAVE:	是否需要保存故障信息
							如果是则置SL_EEBUSY_ERRSAVE,将TAB_ERR中的数据写入eeprom
							操作完成后再清SL_ERRSAVE和SL_EEBUSY_ERRSAVE
**			 SL_POFSAVE:	是否需要保存掉电信息
							如果是则置SL_EEBUSY_POFSAVE,将TAB_POF中的数据写入eeprom
							操作完成后再清SL_POFSAVE和SL_EEBUSY_POFSAVE
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void EeCtrl(void)
{
	if(M_ChkFlag(SL_EE_FAIL)==0)					//EEPROM无故障?
	{
//----------------------------------------------//恢复出厂参数进行中
		if(M_ChkFlag(SL_EEBUSY_INI)!=0)			//RAM数据恢复初值
		{
			*FUNC[EEPROM.point].para_add=FUNC[EEPROM.point].init;	
			EeWrword(EEPROM.point);				//写入2字节数据
			EEPROM.point++;
			
			if(EEPROM.point>=PARA_NUM)			//操作完成?
			{
				EEPROM.point=0;
				M_ClrFlag(SL_EEBUSY_INI);
				M_ClrFlag(SL_EEASK_INI);
			}
		}
//----------------------------------------------//修改功能码进行中
		else if(M_ChkFlag(SL_EEBUSY_MCODE)!=0)
		{
			EeWrword(EEPROM.mcode);				//写入2字节数据
			M_ClrFlag(SL_EEBUSY_MCODE);
			M_ClrFlag(SL_EEASK_MCODE);
		}
//----------------------------------------------//保存故障信息进行中
		else if(M_ChkFlag(SL_EEBUSY_ERRSAVE)!=0)
		{
			EeWrword(TAB_ERR[EEPROM.point]);	//写入2字节数据
			EEPROM.point++;
			
			if(EEPROM.point>=ERRO_NUM)			//操作完成?
			{
				EEPROM.point=0;
				M_ClrFlag(SL_EEBUSY_ERRSAVE);
				M_ClrFlag(SL_EEASK_ERRSAVE);
			}
		}
//----------------------------------------------//是否存在保存掉电信息请求
/*
		else if(M_ChkFlag(SL_EEBUSY_POFSAVE)!=0)
		{
			EeWrword(TAB_POF[EEPROM.point]);	//写入2字节数据
			EEPROM.point++;
			
			if(EEPROM.point>=POFF_NUM)		//操作完成?
			{
				EEPROM.point=0;
				M_ClrFlag(SL_EEBUSY_POFSAVE);
				M_ClrFlag(SL_EEASK_POFSAVE);
			}
		}		
*/
//----------------------------------------------//EEPROM没有操作进行中
		else
		{
			EEPROM.point=0;
			if(M_ChkFlag(SL_EEASK_INI)!=0)			//是否存在恢复出厂参数请求
				M_SetFlag(SL_EEBUSY_INI);
			else if(M_ChkFlag(SL_EEASK_MCODE)!=0)	//是否存在修改功能码请求
				M_SetFlag(SL_EEBUSY_MCODE);
			else if(M_ChkFlag(SL_EEASK_ERRSAVE)!=0)	//是否存在保存故障信息请求
				{M_SetFlag(SL_EEBUSY_ERRSAVE);
//				     M_SetFlag(SL_PHASEA);          //测量Save占用时间,测量DSP板上T1端子 20090803
//    				*OUT3_ADDR = _OUT3_DATA;		//测量Save占用时间,测量DSP板上T1端子 20090803
				}
//			M_ClrFlag(SL_PHASEA);           //测量CPU占有率,测量DSP板上T1端子
//    		*OUT3_ADDR = _OUT3_DATA;		//测量Save占用时间,测量DSP板上T1端子 20090803



//			else if(M_ChkFlag(SL_EEASK_POFSAVE)!=0)	//是否存在保存掉电信息请求
//				M_SetFlag(SL_EEBUSY_POFSAVE);
		}
	}
}
/*********************************************************************************************************
** 函数名称: SetRtimer
** 功能描述: 实时时钟设定
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void SetRtimer(void)
{
	Uint16 temp;
	
//----------------------------------------------//写入时间值	
	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x3F;						//LSB ADdress
	EEPROM.data[3]=0x02;						//STATUS register
	EeWrite(1);									//写允许
	
	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x3F;						//LSB ADdress
	EEPROM.data[3]=0x06;						//STATUS register
	EeWrite(1);									//写寄存器允许
	
	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x30;						//LSB ADdress
	
	temp=RTIMER.time[0]/10;
	EEPROM.data[3]=(RTIMER.time[0]-temp*10)|(temp<<4);	//秒
	
	
	temp=RTIMER.time[1]/10;
	EEPROM.data[4]=(RTIMER.time[1]-temp*10)|(temp<<4);	//分
	
	temp=RTIMER.time[2]/10;
	EEPROM.data[5]=0x80 | ((RTIMER.time[2]-temp*10)|(temp<<4));	//时
	
	temp=RTIMER.time[3]/10;
	EEPROM.data[6]=(RTIMER.time[3]-temp*10)|(temp<<4);	//日
	
	temp=RTIMER.time[4]/10;
	EEPROM.data[7]=(RTIMER.time[4]-temp*10)|(temp<<4);	//月
	
	temp=RTIMER.time[5]/10;
	EEPROM.data[8]=(RTIMER.time[5]-temp*10)|(temp<<4);	//年
	
	EEPROM.data[9]=0x05;						//星期
	EEPROM.data[10]=0x20;						//19/20
	EeWrite(8);									//写入时间值

	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x3F;						//LSB ADdress
	EEPROM.data[3]=0x0;							//STATUS register
	EeWrite(1);									//禁止写入
}
/*********************************************************************************************************
** 函数名称: RtRead
** 功能描述: 实时时钟读取
** 输　入: 	 
** 输　出:   RTIMER.time[0~5]--[秒 分 时 日 月 年]
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void RtRead(void)
{
	Uint16 i;

	if(M_ChkFlag(SL_SETRTIMER)!=0)				//需要校正实时时钟?
	{
		SetRtimer();
		M_ClrFlag(SL_SETRTIMER);
	}
	else
	{
		//----------------------------------------------//读取时间值到data[0~7]
		EEPROM.data[0]=0xDE;						//Slave ADdress
		EEPROM.data[1]=0;							//MSB ADdress
		EEPROM.data[2]=0x30;						//LSB ADdress
		EeRead(8);
		//----------------------------------------------
		M_ClrBit(EEPROM.data[2],0x80);				//清除24小时设置位
	
		for(i=0;i<6;i++)
		{
			RTIMER.time[i]=((EEPROM.data[i] & 0xF0)>>4)*10+(EEPROM.data[i] & 0x0F);
		}
	}
}
/*********************************************************************************************************
** 函数名称: InitRtimer
** 功能描述: 实时时钟初始化
** 输　入: 	 
** 输　出:   
** 注  释: 	 只在实时时钟初始化的时候将需要设定的时间值设定到相应位置
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void InitRtimer(void)
{
//----------------------------------------------//写入控制字
	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x3F;						//LSB ADdress
	EEPROM.data[3]=0x0;							//STATUS register
	EeWrite(1);									//禁止写入

	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x3F;						//LSB ADdress
	EEPROM.data[3]=0x02;						//STATUS register
	EeWrite(1);									//写允许

	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x3F;						//LSB ADdress
	EEPROM.data[3]=0x06;						//STATUS register
	EeWrite(1);									//写寄存器允许

	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x10;						//LSB ADdress
	EEPROM.data[3]=0x0;							//Control register 0
	EEPROM.data[4]=0x0;							//Control register 1
	EEPROM.data[5]=0x0;							//Control register 2
	EEPROM.data[6]=0x0;							//Control register 3
	EeWrite(4);									//写入4字节控制字
	
	DELAY_US(10000L);							//延时10ms

//----------------------------------------------//写入时间值	
	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x3F;						//LSB ADdress
	EEPROM.data[3]=0x0;							//STATUS register
	EeWrite(1);									//禁止写入

	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x3F;						//LSB ADdress
	EEPROM.data[3]=0x02;						//STATUS register
	EeWrite(1);									//写允许
	
	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x3F;						//LSB ADdress
	EEPROM.data[3]=0x06;						//STATUS register
	EeWrite(1);									//写寄存器允许
	
	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x30;						//LSB ADdress
	EEPROM.data[3]=0;							//秒
	EEPROM.data[4]=0x00;						//分
	EEPROM.data[5]=0x80 | 0x00;					//时(0x80指的是24小时模式)
	EEPROM.data[6]=0x15;						//日
	EEPROM.data[7]=0x04;						//月
	EEPROM.data[8]=0x09;						//年
	EEPROM.data[9]=0x05;						//星期
	EEPROM.data[10]=0x20;						//19/20
	EeWrite(8);									//写入时间值

	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x3F;						//LSB ADdress
	EEPROM.data[3]=0x0;							//STATUS register
	EeWrite(1);									//禁止写入
	
//----------------------------------------------//读取时间值到data[0~7]
	EEPROM.data[0]=0xDE;						//Slave ADdress
	EEPROM.data[1]=0;							//MSB ADdress
	EEPROM.data[2]=0x30;						//LSB ADdress
	EeRead(8);
}
/*********************************************************************************************************
** 函数名称: Sci485_TxInit
** 功能描述: 485发送初始化
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Sci485_TxInit(void)
{
	Uint32 temp32;
	Uint16 temp16;
	
	M_EN485TXD();
	M_EnTxb();
	temp32=LSPCLK/8;
	temp16=temp32/_SCIB_BAUD-1;				// LSPCLK/(8*BAUD)-1
//----------------------------------------------------------------------------
	SciaRegs.SCIFFTX.all=0xC000;			// Reset TX FIFO's
	SciaRegs.SCICCR.all = 0x0007;			// 1 stop bit, No parity, 8-bit character, No loopback
	
	SciaRegs.SCIHBAUD = (temp16&0xFF00)>>8;	// BAUDRATE
	SciaRegs.SCILBAUD = temp16&0x00FF;
	
	SciaRegs.SCIFFTX.bit.TXFIFOXRESET=1;	// Re-enable TX FIFO operation
	
	//M_EnTxbInt();
	SciaRegs.SCICTL1.all =0x0022;     		// Relinquish SCI from Reset
//----------------------------------------------------------------------------
}
/*********************************************************************************************************
** 函数名称: Sci485_RxInit
** 功能描述: 485接收初始化
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Sci485_RxInit(void)
{
//----------------------------------------------------------------------------
	M_EN485RXD();
	M_EnRxb();
	SciaRegs.SCIFFRX.all=0x000A;			// Reset RX FIFO's
	//ScibRegs.SCICCR.all = 0x0007;			// 1 stop bit, No parity, 8-bit character, No loopback
	
	SciaRegs.SCIFFRX.bit.RXFIFORESET=1;		// Re-enable RX FIFO operation
	
	M_ClrRxFifoOvr();
	
	//M_EnRxbInt();
	SciaRegs.SCICTL1.all =0x0021;     		// Relinquish SCI from Reset
//----------------------------------------------------------------------------
}
/*********************************************************************************************************
** 函数名称: ScibDatpro
** 功能描述: 对sci接收到的数据进行解析校验
** 输　入: 	 
** 输　出:   response=0表示需要不需要回复;response=1表示需要立即回复;response=2表示不立即回复,进程完后再回复(如恢复出厂参数)
** 注  释: 	 通信协议说明如下
				SCI.rxb[0]:报头(0x7E)
				SCI.rxb[1]:下位机地址
				SCI.rxb[2]:命令字(低字节)
				SCI.rxb[3]:命令字(高字节)
				SCI.rxb[4]:功能码序号
				SCI.rxb[5]:功能码数值(低字节)
				SCI.rxb[6]:功能码数值(高字节)
				SCI.rxb[7]:状态字(低字节)
				SCI.rxb[8]:状态字(高字节)
				SCI.rxb[9]:异或校验
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
Uint16 ScibDatpro(void)
{
	Uint16 response,i,xor_data,opration_fail;
	Uint16 rx_command,rx_funcnum,rx_funcval,rx_state;
	Uint16 tx_state;
	
	if(SCI.rxb[0] == COM_HEAD)									//报头=COM_HEAD?
	{
		//发送数准备(报头字节 地址字节)
		SCI.txb[0] = COM_HEAD;									//报头字节
		SCI.txb[1] = SCI.rxb[1];								//地址字节
		
		//地址判断
		if(((SCI.rxb[1]&COM_OPERATOR)!=COM_OPERATOR)&&(SCI.rxb[1]!=_SCIB_ADDR))		//上位机&节点地址不符?
			response=0;											//非发送给本机的命令,不进行处理回复,等待下一帧数据
		else													//操作器控制或上位机控制且节点地址相符
		{
			if(SCI.rxb[1]==0)									//广播模式?
				response=0;										//广播模式不回复
			else
				response=1;										//立即回复
			
			xor_data=SCI.rxb[0];								//异或校验
			for(i=1;i<9;i++)
				xor_data ^= SCI.rxb[i];
			
			if(xor_data != SCI.rxb[9])							//异或校验符合?
				opration_fail=1;								//置操作失败标志
			else
			{
				opration_fail=0;								//清操作失败
				
				//诱数据转存
				rx_command=SCI.rxb[3];							//保存命令字
				rx_command=((rx_command<<8)&0xFF00)|SCI.rxb[2];
				
				rx_funcnum=SCI.rxb[4];							//保存功能码序号
				
				rx_funcval=SCI.rxb[6];							//保存功能码值
				rx_funcval=(SCI.rxb[6]<<8)|SCI.rxb[5];
				
				rx_state=SCI.rxb[8];							//保存状态字
				rx_state=(SCI.rxb[8]<<8)|SCI.rxb[7];
				
				if((rx_state&COM_KEYFWD)==COM_KEYFWD)			//上位机要求
				{
//					M_SetFlag();						//置启动标志
				}
			    if((rx_state&COM_KEYREV)==COM_KEYREV)		    //上位机要求
				{
//					M_SetFlag();						//置启动标志
				}
				if((rx_state&COM_KEYSTOP)==COM_KEYSTOP)			//上位机要求停止?
				{              	    			
//                    M_ClrFlag();	
				}

				//命令字处理
				//if((((rx_command&COM_NEEDSTOP)==COM_NEEDSTOP)&&(M_ChkFlag(SL_RUNING)!=0))||(rx_funcnum>PARA_NUM)||((rx_state&COM_UPNORM)!=COM_UPNORM))	//命令需要停机&正在运行 或 功能码序号超限 或 上位机不正常?
				if((((rx_command&COM_NEEDSTOP)==COM_NEEDSTOP)&&(M_ChkFlag(SL_RUN)!=0))||(rx_funcnum>PARA_NUM)||((rx_state&COM_UPNORM)!=COM_UPNORM))	//命令需要停机&正在运行 或 功能码序号超限 或 上位机不正常? Modified by ljd 05-12-7
					opration_fail=1;							//置操作失败标志
				else
				{
					switch (rx_command&0xFFEF)					//命令解析
					{
						case COM_RD:							//读取功能码(读取下位机EEPROM中的功能码值写入到上位机RAM中)
							if((rx_funcnum>=BANK_FIRST)&&(rx_funcnum<=BANK_END))	//属于监控变量?
							{
								i=*FUNC[rx_funcnum].para_add;			//直接将RAM中的数据发送
								EEPROM.data[0]=i&0x00FF;
								EEPROM.data[1]=(i&0xFF00)>>8;
							}
//							else if((rx_funcnum>=TAB_ERR_FIRST)&&(rx_funcnum<=TAB_ERR_END))	//属于故障变量?
//							{
//								i=*FUNC[rx_funcnum].para_add;			//直接将RAM中的数据发送
//								EEPROM.data[0]=i&0x00FF;
//								EEPROM.data[1]=(i&0xFF00)>>8;
//							}
							else										//普通变量
							{
								EeRpre(rx_funcnum);						//控制字和地址填入EEPROM.data[]
								EeRead(2);								//读出两字节
								*FUNC[rx_funcnum].para_add=(EEPROM.data[1]<<8) | EEPROM.data[0];	//修改RAM中功能码数据
							}
							
							SCI.txb[2]=SCI.rxb[2];						//准备发送数据(操作成功)
							SCI.txb[3]=SCI.rxb[3]|COM_SUCCESS;
							SCI.txb[4]=SCI.rxb[4];
							SCI.txb[5]=EEPROM.data[0];
							SCI.txb[6]=EEPROM.data[1];
							
							break;
							
						case COM_WR:									//修改功能码(修改下位机RAM中的功能码值)
							//if(((FUNC[rx_funcnum].attr&RDO)==RDO)||(((FUNC[rx_funcnum].attr&WR1)==WR1)&&(M_ChkFlag(SL_RUNING)!=0)))		//只读或运行中不可修改?
							if(((FUNC[rx_funcnum].attr&RDO)==RDO)||(((FUNC[rx_funcnum].attr&WR1)==WR1)&&(M_ChkFlag(SL_RUN)!=0)))		//只读或运行中不可修改? Modified by ljd 05-12-7
							{
								opration_fail=1;						//置操作失败标志
							}
							else
							{
								*FUNC[rx_funcnum].para_add=rx_funcval;	//修改RAMRAM中的功能码值
								
								SCI.txb[2]=SCI.rxb[2];					//准备发送数据(操作成功)
								SCI.txb[3]=SCI.rxb[3]|COM_SUCCESS;
								SCI.txb[4]=SCI.rxb[4];
								SCI.txb[5]=SCI.rxb[5];
								SCI.txb[6]=SCI.rxb[6];
							}
							
							break;
							
						case COM_SAVE:									//修改功能码并存储(修改下位机RAM中的功能码值并保存到下位机的EEPROM)
							//if(((FUNC[rx_funcnum].attr&RDO)==RDO)||(((FUNC[rx_funcnum].attr&WR1)==WR1)&&(M_ChkFlag(SL_RUNING)!=0)))		//只读或运行中不可修改?
							if(((FUNC[rx_funcnum].attr&RDO)==RDO)||(((FUNC[rx_funcnum].attr&WR1)==WR1)&&(M_ChkFlag(SL_RUN)!=0)))		//只读或运行中不可修改? Modified by ljd 05-12-7
								opration_fail=1;						//置操作失败标志
							else
							{
								*FUNC[rx_funcnum].para_add=rx_funcval;	//修改RAM中的功能码值
								
								if((rx_funcnum>=TIME_FIRST)&&(rx_funcnum<=TIME_END))
									M_SetFlag(SL_SETRTIMER);			//设实时时钟需要重新设置标志
								else
								{
									EEPROM.mcode=rx_funcnum;			//写入2字节数据
									M_SetFlag(SL_EEASK_MCODE);			//设EEPROM修改功能码请求标志
								}
								
								SCI.txb[2]=SCI.rxb[2];					//准备发送数据(操作成功)
								SCI.txb[3]=SCI.rxb[3]|COM_SUCCESS;
								SCI.txb[4]=SCI.rxb[4];
								SCI.txb[5]=SCI.rxb[5];
								SCI.txb[6]=SCI.rxb[6];
							}
							
							break;
							
						case COM_RESUME:								//恢复出厂参数
						//	if(rx_funcval==RESUME_KEY)					//恢复出厂参数校验码正确?
						//	{
								M_SetFlag(SL_RESUME);					//设恢复出厂参数进行中标志
								
								EEPROM.point=0;							//写入2字节数据
								M_SetFlag(SL_EEASK_INI);				//设EEPROM修改功能码请求标志
								
								SCI.txb[2]=SCI.rxb[2];					//准备发送数据(操作成功)
								SCI.txb[3]=SCI.rxb[3]|COM_SUCCESS;
								SCI.txb[4]=SCI.rxb[4];
								SCI.txb[5]=SCI.rxb[5];
								SCI.txb[6]=SCI.rxb[6];
								
								response=2;								//恢复出厂参数不立即回复,恢复进行完才回复
						//	}
						//	else
						//		opration_fail=1;						//置操作失败标志
							
							break;
							
						case COM_SAVEALL:								//修改功能码并存储(包括只读功能码)
							*FUNC[rx_funcnum].para_add=rx_funcval;		//修改RAM中的功能码值
							
							EEPROM.mcode=rx_funcnum;					//写入2字节数据
							M_SetFlag(SL_EEASK_MCODE);					//设EEPROM修改功能码请求标志
							
							SCI.txb[2]=SCI.rxb[2];						//准备发送数据(操作成功)
							SCI.txb[3]=SCI.rxb[3]|COM_SUCCESS;
							SCI.txb[4]=SCI.rxb[4];
							SCI.txb[5]=SCI.rxb[5];
							SCI.txb[6]=SCI.rxb[6];
							
							break;
/*						
						case COM_VGIVE:									//修改DCDC电压给定值
							if((rx_funcnum == GIVE_FIRST)&&(rx_funcval<=FUNC[NO_VCER].max))
							{
								_DC_VREF2 = rx_funcval;
								SCI.txb[2]=SCI.rxb[2];					//准备发送数据(操作成功)
							    SCI.txb[3]=SCI.rxb[3]|COM_SUCCESS;
								SCI.txb[4]=SCI.rxb[4];
								SCI.txb[5]=SCI.rxb[5];
								SCI.txb[6]=SCI.rxb[6];
							}
							else
							{
								opration_fail=1;						//置操作失败标志
							}
							
							break;
						
						case COM_IGIVE:									//修改DCDC电流给定值
							if((rx_funcnum == (GIVE_FIRST+1))&&(rx_funcval<=FUNC[NO_ICER].max))
							{
								_DC_IREF2 = rx_funcval;
								SCI.txb[2]=SCI.rxb[2];					//准备发送数据(操作成功)
								SCI.txb[3]=SCI.rxb[3]|COM_SUCCESS;
								SCI.txb[4]=SCI.rxb[4];
								SCI.txb[5]=SCI.rxb[5];
								SCI.txb[6]=SCI.rxb[6];
							}
							else
							{
								opration_fail=1;						//置操作失败标志
							}
							
							break;
*/								
						default:
							SCI.txb[2]=SCI.rxb[2];						//准备发送数据(操作成功)
							SCI.txb[3]=SCI.rxb[3]|COM_SUCCESS;
							SCI.txb[4]=SCI.rxb[4];
							SCI.txb[5]=SCI.rxb[5];
							SCI.txb[6]=SCI.rxb[6];
					}
				}
				
				//操作失败的处理
				if(opration_fail==1)									//操作失败?
				{
					SCI.txb[3]=SCI.rxb[3];								//准备发送数据(操作失败)
					SCI.txb[4]=SCI.rxb[4];
					SCI.txb[5]=SCI.rxb[5];
					SCI.txb[6]=SCI.rxb[6];
				}
				
				//下位机反馈状态字处理
				tx_state=0;
				
				if(M_ChkFlag(SL_ERROR)==0)								//工作中无故障?
					tx_state |= COM_SLAVENORM;							//设下位机正常位
				if(M_ChkFlag(SL_CODEOK)!=0)
					tx_state |= COM_SLAVEINIT;							//设下位机初始化完成位
				if(M_ChkFlag(SL_RUN)!=0)								//SL_RUN作为总运行标志位
					tx_state |= COM_SLAVERUN;							//设下位机运行位
//				if(_WORK_MODE==1)										//充电
//					tx_state |= COM_SLAVEFWD;							
//				if(_WORK_MODE==2)										//放电
//					tx_state |= COM_SLAVEREV;							
//				if(_WORK_MODE==3)										
//					tx_state |= COM_SLAVEDEB;							//调试
				
				SCI.txb[7]=tx_state&0x00FF;
				SCI.txb[8]=(tx_state&0xFF00)>>8;
				
				//异或校验字节
				xor_data=SCI.txb[0];									//异或校验
				for(i=1;i<9;i++)
					xor_data ^= SCI.txb[i];
				
				SCI.txb[9] = xor_data;
			}
		}
	}
	
	return response;
}

/*********************************************************************************************************
** 函数名称: Sci485Ctrl
** 功能描述: 对sci的接收发送进行综合控制
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Sci485Ctrl(void)
{
	Uint16 i,response;
	
	if(M_ChkCounter(SCI.cnt_sciover,DELAY_SCIOVER)>=0)				//发送/接收超时?
	{
		SCI.cnt_sciover=0;
		M_ClrFlag(SL_TX);
		M_ClrFlag(SL_RX);
		Sci485_RxInit();											//485接收初始化
	}
	else if(M_ChkFlag(SL_TX)!=0)									//发送?
	{
		if(SciaRegs.SCIFFTX.bit.TXFFST == 0)						//发送完成?
		{
			SCI.cnt_sciover=0;										//清除发送/接收超时定时器
			if(SciaRegs.SCICTL2.bit.TXEMPTY==1)						//发送寄存器为空?
		 	{
				M_ClrFlag(SL_TX);
				Sci485_RxInit();									//485接收初始化
			}
		}
	}
	else															//接收
	{
		if(SciaRegs.SCIFFRX.bit.RXFFST == 0)						//未开始接收或接收完成?
		{
			SCI.cnt_sciover=0;										//清除发送/接收超时定时器
			M_ClrRxFifoOvr();
			
			if(M_ChkFlag(SL_RX)!=0)									//接收完成?
			{
				if(M_ChkCounter(SCI.cnt_scispace,DELAY_SCISPACE)>=0)//接收到发送间隔到时?
	    		{
					if(M_ChkFlag(SL_RESUME)!=0)						//恢复出厂参数进行中?
					{
						if(M_ChkFlag(SL_EEASK_INI)==0)				//EEPROM修改功能码操作完成?
						{
							M_ClrFlag(SL_RESUME);					//清恢复出厂参数进行中标志
							M_SetFlag(SL_TX);						//置发送任务标志位
							M_ClrFlag(SL_RX);
							
							Sci485_TxInit();						//485发送初始化
							
							for(i=0;i<SCI485NUM;i++)
								SciaRegs.SCITXBUF=SCI.txb[i];
						}
					}
					else
					{
						response=ScibDatpro();						//调用数据解析程序
						
						if(response==1)								//表示要立即回复
						{
							M_SetFlag(SL_TX);						//置发送任务标志位
							M_ClrFlag(SL_RX);
							
							Sci485_TxInit();						//485发送初始化
							
							//for(i=0;i<SCI485NUM;i++)				//发送缓存等于接收缓存(调试用)
							//	SCI.txb[i]=SCI.rxb[i];
							
							for(i=0;i<SCI485NUM;i++)
								SciaRegs.SCITXBUF=SCI.txb[i];
						}
						else if(response==0)						//不需要回复
						{
							M_ClrFlag(SL_RX);
							Sci485_RxInit();						//485接收初始化
						}
					}
				}
			}
		}
		else if((SciaRegs.SCIFFRX.bit.RXFFST >= 1)&&(M_ChkFlag(SL_HEADOK)==0))	//开始接收且还没有收到报头?
		{
			SCI.rxb[0]=SciaRegs.SCIRXBUF.all&0x00FF;
			if(SCI.rxb[0]==COM_HEAD)
				M_SetFlag(SL_HEADOK);
			else
				Sci485_RxInit();									//485接收初始化
		}
		else if(SciaRegs.SCIFFRX.bit.RXFFST >= SCI485NUM-1)			//接收完成?
		{
			SCI.cnt_sciover=0;										//清除发送/接收超时定时器
			M_DisTxRxb();
			M_ClrRxFifoOvr();
			
			for(i=1;i<SCI485NUM;i++)								//读出接收缓存(不包括报头)
				SCI.rxb[i]=SciaRegs.SCIRXBUF.all&0x00FF;
			
			M_ClrFlag(SL_HEADOK);
			M_SetFlag(SL_RX);										//置接收完成标志位
			SCI.cnt_scispace=0;										//清除接收到发送间隔定时器
		}
	}
}

/*********************************************************************************************************
** 函数名称: Sci_canopenrx
** 功能描述: sci_CANOPEN初始化
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Scicanopen_Init(void)
{
//----------------------------------------------------------------------------
	ScibRegs.SCICCR.all =0x0007;   			// 1 stop bit,No loopback 
                                  			// no parity,8 char bits
	ScibRegs.SCICTL1.all =0x0003;  			// Disable TX, RX, internal SCICLK, 
                                  			// Disable RX_ERR, SLEEP, TXWAKE
	ScibRegs.SCICTL2.all =0;		 		// fifo mode,they are ignored 

   	ScibRegs.SCIFFTX.all=0xC000;			// disable tx232_int,reset scia_fifo 
    ScibRegs.SCIFFRX.all=0x000A;			// disable rx232_int 
    ScibRegs.SCIFFCT.all=0x00;
    
	ScibRegs.SCICTL1.all =0x0023;     		// Relinquish SCI from Reset 

	ScibRegs.SCIFFTX.bit.TXFIFOXRESET=1;
	ScibRegs.SCIFFRX.bit.RXFIFORESET=1;
//----------------------------------------------------------------------------
}  
/*********************************************************************************************************
** 函数名称: Sci_canopenrx
** 功能描述: 对sci_CANOPEN的接收发送进行综合控制
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Sci_canopenrx(void)
{
	Uint16 i,heartbeat,xor_data;
	
	if(M_ChkCounter(SCI_canopen.cnt_sciover,DELAY_SCICANOPENOVER)>=0)				//接收超时?
	{
		SCI_canopen.cnt_sciover=0;
		Scicanopen_Init();
	}
	else
	{
		if((ScibRegs.SCIFFRX.bit.RXFFST >= 1)&&(M_ChkFlag(SL_CANOPENHEADOK)==0))	//开始接收且还没有收到报头?
		{
			SCI_canopen.cnt_sciover=0;

			SCI_canopen.rxb[0]=ScibRegs.SCIRXBUF.all&0x00FF;
			if(SCI_canopen.rxb[0]==CANOPENCOM_HEAD)
				M_SetFlag(SL_CANOPENHEADOK);
			else
			{	
				ScibRegs.SCIFFRX.bit.RXFIFORESET=1;		// Re-enable RX FIFO operation
				ScibRegs.SCIFFRX.bit.RXFFOVRCLR=1;
			}
		}
		else if(ScibRegs.SCIFFRX.bit.RXFFST >= SCICANOPENRXNUM-1)			//接收完成?
		{
			
			//	ScibRegs.SCICTL1.bit.RXENA =0;
				for(i=1;i<SCICANOPENRXNUM;i++)								//读出接收缓存(不包括报头)
					SCI_canopen.rxb[i]=ScibRegs.SCIRXBUF.all&0x00FF;
				M_ClrFlag(SL_CANOPENHEADOK);
				
				heartbeat= SCI_canopen.rxb[2] & ONEBYTE[0];
				if(heartbeat!=SCI_canopen.heartbeat)
					SCI_canopen.cnt_heartbeat=0;
				
				SCI_canopen.heartbeat = heartbeat;

				xor_data=SCI_canopen.rxb[0];								//异或校验
				for(i=1;i<(SCICANOPENRXNUM-1);i++)
					xor_data ^= SCI_canopen.rxb[i];
			
				if(xor_data == SCI_canopen.rxb[SCICANOPENRXNUM-1])							//异或校验符合?
				{
					if((SCI_canopen.rxb[1]|SCI_canopen.rxb[2]|SCI_canopen.rxb[3]|SCI_canopen.rxb[4]|SCI_canopen.rxb[5]|SCI_canopen.rxb[6]|SCI_canopen.rxb[7]|SCI_canopen.rxb[8])!=0) //剔出数据都等于0的坏包20090817
					{			
					//数据转存
					SCI_canopen.rx_controlword=(SCI_canopen.rxb[2]<<8)|SCI_canopen.rxb[1];							//controlword
				
					SCI_canopen.rx_torque=(SCI_canopen.rxb[4]<<8)|SCI_canopen.rxb[3];							//torque_ref
				
					SCI_canopen.rx_angle=(SCI_canopen.rxb[6]<<8)|SCI_canopen.rxb[5];							//angle_ref
					}	
					
					if(M_ChkFlag(SL_CANOPENOVER)!=0)	//201105atzuoyun
					{
			 	  		 SCI_canopen.rx_controlword=0;
						 SCI_canopen.rx_torque=0;
						 SCI_canopen.rx_angle=0; 
					}						
				}

		//		ScibRegs.SCICTL1.bit.RXENA =1;
				ScibRegs.SCIFFRX.bit.RXFIFORESET=1;		// Re-enable RX FIFO operation
				ScibRegs.SCIFFRX.bit.RXFFOVRCLR=1;
				SCI_canopen.cnt_sciover=0;

		}
	}
} 

/*********************************************************************************************************
** 函数名称: Sci_canopenrx
** 功能描述: 对sci_CANOPEN的接收发送进行综合控制
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Sci_canopentx(void)
{
	Uint16 i,xor_data;

//if(SCI_canopen.tx_state1!=0)  SCI_canopen.tx_state1=0x0000;   //cantest
//else SCI_canopen.tx_state1=0xFFFF;                            //cantest
	
		SCI_canopen.txb[0] = CANOPENCOM_HEAD;
//		SCI_canopen.txb[1] = CANOPENCOM_TX1;
		SCI_canopen.txb[1]=SCI_canopen.tx_torque&0x00FF;
		SCI_canopen.txb[2]=(SCI_canopen.tx_torque&0xFF00)>>8;
		SCI_canopen.txb[3]=SCI_canopen.tx_speed&0x00FF;
		SCI_canopen.txb[4]=(SCI_canopen.tx_speed&0xFF00)>>8;
		SCI_canopen.txb[5]=SCI_canopen.tx_state1&0x00FF;
		SCI_canopen.txb[6]=(SCI_canopen.tx_state1&0xFF00)>>8;
		SCI_canopen.txb[7]=SCI_canopen.tx_state2&0x00FF;
		SCI_canopen.txb[8]=(SCI_canopen.tx_state2&0xFF00)>>8;
		SCI_canopen.txb[9]=SCI_canopen.tx_watertempin&0x00FF;
		SCI_canopen.txb[10]=(SCI_canopen.tx_watertempin&0xFF00)>>8;
		SCI_canopen.txb[11]=SCI_canopen.tx_watertempout&0x00FF;
		SCI_canopen.txb[12]=(SCI_canopen.tx_watertempout&0xFF00)>>8;
		SCI_canopen.txb[13]=SCI_canopen.tx_skiiptempmax&0x00FF;
		SCI_canopen.txb[14]=(SCI_canopen.tx_skiiptempmax&0xFF00)>>8;
		SCI_canopen.txb[15]=SCI_canopen.tx_demand&0x00FF;
		SCI_canopen.txb[16]=(SCI_canopen.tx_demand&0xFF00)>>8;

		xor_data=SCI_canopen.txb[0];									//异或校验
		for(i=1;i<SCICANOPENTXNUM-1;i++)
			xor_data ^= SCI_canopen.txb[i];
				
		SCI_canopen.txb[SCICANOPENTXNUM-1] = xor_data;
		
		for(i=0;i<SCICANOPENTXNUM-2;i++)
			ScibRegs.SCITXBUF=SCI_canopen.txb[i];
		
} 
/*********************************************************************************************************
** 函数名称: DataFilter
** 功能描述: 数据滤波
** 输　入: 	Y(k-1)为上次滤波结果，X(k)为新采样值。。
** 输　出: ：Y(k)为本次滤波结果。  
** 注  释: 	 滤波公式为：Y(k)=cY(k-1)+(1-c)X(k),其中，c=1/(1+2*PAI*fh/fs),fh为低通滤波器的截止频率，fs为采样频率.
			在一阶低通滤波中，X(k)即为Y(k)。
			直流量误差1％为稳定时间。
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void DataFilter( float c,float *out,float in)
{
   *out = c * *out + (1 - c) * in;
} 

/*********************************************************************************************************
** 函数名称: Ad8364Ctrl
** 功能描述: 读取并处理前一次的转换结果，同时启动下一次AD转换
** 输　入: 	 
** 输　出:   
** 注  释: 	 每次都运行
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Ad8364Ctrl(void)
{
	float tempa,tempb,tempc;

//------------------------------数据读取(共18路)---------------------------------
//该组AD数据是100us之前的结果
	AD.dat[0] = *AD_ASTART;	       // STA.Ubc 电机定子侧电压Ubc
	AD.dat[1] = *(AD_ASTART+1);    // AMUX，慢速信号，16选1过来的信号
	AD.dat[2] = *(AD_ASTART+2);    // GRD.Ubc 主断前Ubc
//	AD.dat[3] = *(AD_ASTART+3);    // Ic 备用SKIIP电流反馈
	AD.dat[4] = *(AD_ASTART+4);    // GRD.Uab 主断前Uab
	AD.dat[5] = *(AD_ASTART+5);    // MPR.ic, 机侧变流器MPR的电流

	AD.dat[6]  = *AD_BSTART;       // NGS.Uab 网侧电压Ubc大滤波通道
	AD.dat[7]  = *(AD_BSTART+1);   // MPR.ib，机侧变流器NPR的电流
	AD.dat[8]  = *(AD_BSTART+2);   // NGS.Ubc 网侧电压Ubc大滤波通道 
	AD.dat[9]  = *(AD_BSTART+3);   // MPR.ia，机侧变流器NPR的电流 
	AD.dat[10] = *(AD_BSTART+4);   // Udc 中间直流电压 
	AD.dat[11] = *(AD_BSTART+5);   // NPR.ic，网侧变流器NPR的电流

//	AD.dat[12] = *AD_CSTART;       // Vdc2 
	AD.dat[13] = *(AD_CSTART+1);   // NPR.ib，网侧变流器NPR的电流
	AD.dat[14] = *(AD_CSTART+2);   // NGS.Uab 网侧电压Uab
	AD.dat[15] = *(AD_CSTART+3);   // NPR.ia，网侧变流器NPR的电流
	AD.dat[16] = *(AD_CSTART+4);   // STA.Uab 电机定子侧电压Uab
	AD.dat[17] = *(AD_CSTART+5);   // NGS.Ubc 网侧电压Ubc

//----------------------------数据读取结束------------------------------
										
	ADFINAL.ia1  = AD.dat[15];		// NPR.ia，网侧变流器NPR的电流
	ADFINAL.ib1  = AD.dat[13];	    // NPR.ib，网侧变流器NPR的电流
	ADFINAL.ic1  = AD.dat[11];	    // NPR.ic，网侧变流器NPR的电流

	ADFINAL.ia2  = AD.dat[9];	    // MPR.ia，机侧变流器NPR的电流
	ADFINAL.ib2  = AD.dat[7];		// MPR.ib，机侧变流器NPR的电流	
	ADFINAL.ic2  = AD.dat[5];		// MPR.ic, 机侧变流器MPR的电流	

	ADFINAL.uab   = AD.dat[4];       // GRD.Uab 主断前Uab
    ADFINAL.ubc   = AD.dat[2];		 // GRD.Ubc 主断前Ubc

	ADFINAL.uab1 = AD.dat[14];		// Uab 网侧电压		
	ADFINAL.ubc1 = AD.dat[17];		// Ubc 网侧电压
	
	ADFINAL.uab2 = AD.dat[16];		// Uab 电机定子侧电压		
	ADFINAL.ubc2 = AD.dat[0];		// Ubc 电机定子侧电压

	ADFINAL.uab3 = AD.dat[6];		// Uab 网侧电压	大滤波通道 	
	ADFINAL.ubc3 = AD.dat[8];		// Ubc 网侧电压 大滤波通道 


	ADFINAL.udc  = AD.dat[10];		//中间直流电压检测

    if(ADFINAL.udc < 0) ADFINAL.udc=0;

	ADFINAL.AMUX = AD.dat[1];       //慢速AD输入

//-----------------------------------------------------------
                 
	switch(_OUT_AMUX)
	{
		 case(8): {AMUX.NPR_tempa=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//NPR的A相温度，Skiip反馈,=10V*10/(32768) +20 			   				
		 case(10):{AMUX.NPR_tempb=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//NPR的B相温度，Skiip反馈,=10V*10/(32768) +20		    
		 case(12):{AMUX.NPR_tempc=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//NPR的C相温度，Skiip反馈,=10V*10/(32768) +20			    
		 case(11):{AMUX.MPR_tempa=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//MPR的A相温度，Skiip反馈,=10V*10/(32768) +20			    
		 case(9): {AMUX.MPR_tempb=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//MPR的B相温度，Skiip反馈,=10V*10/(32768) +20	     
		 case(2): {AMUX.MPR_tempc=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//MPR的C相温度，Skiip反馈,=10V*10/(32768) +20
         case(1): {AMUX.Lac_temp =ADFINAL.AMUX   * 0.007825;		 break; }	//网侧电感温度，PT100(Rt=100R+0.39*T),=10V*50R/(32768*5V*0.39)
		 case(0): {AMUX.Ldudt_temp= ADFINAL.AMUX * 0.007825;	     break; }	//机侧du/dt的温度，PT100
		 case(5): {AD_OUT_STA_I.ac=ADFINAL.AMUX  * 0.0732422;		 break; }	//电机定子侧A相线电流，互感器1200A:1A,Rt=5R,=1200A*10V/(5R*1A*32768)			     
		 case(4): {AD_OUT_STA_I.ba=ADFINAL.AMUX  * 0.0732422;		 break; }	//电机定子侧B相线电流，互感器
	     default: break; 			       
	}


	      
	_OUT_AMUX++;
	if(_OUT_AMUX > 12) _OUT_AMUX=0;
			
	_OUT4_DATA = _OUT_AMUX;
	*OUT4_ADDR = _OUT4_DATA;	      
/*
//--------------可参考改进---------------------------------------------	      
	switch(_OUT4_DATA)
	{
		case(8): {AMUX.NPR_tempa=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//NPR的A相温度，Skiip反馈,=10V*10/(32768) +20 			   				
		case(10):{AMUX.NPR_tempb=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//NPR的B相温度，Skiip反馈,=10V*10/(32768) +20		    
		case(12):{AMUX.NPR_tempc=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//NPR的C相温度，Skiip反馈,=10V*10/(32768) +20			    
		case(11):{AMUX.MPR_tempa=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//MPR的A相温度，Skiip反馈,=10V*10/(32768) +20			    
		case(9): {AMUX.MPR_tempb=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//MPR的B相温度，Skiip反馈,=10V*10/(32768) +20	     
		case(2): {AMUX.MPR_tempc=ADFINAL.AMUX   * 0.0030517 + 20;   break; }	//MPR的C相温度，Skiip反馈,=10V*10/(32768) +20
        case(1): {
                  AMUX.Lac_R=(ADFINAL.AMUX+49152) /(491.52- 0.001* ADFINAL.AMUX);//new PCB PT100(Rt=100R+0.39*T)
             	  AMUX.Lac_temp=2.5641 * (AMUX.Lac_R - 100);
             	  break; 
             	 }
        case(0): {
                  AMUX.Ldudt_R=(ADFINAL.AMUX+49152) /(491.52- 0.001* ADFINAL.AMUX);//new PCB PT100(Rt=100R+0.39*T)
             	  AMUX.Ldudt_temp=2.5641 * (AMUX.Ldudt_R - 100);
             	  break; 
             	 }
		case(5): {AD_OUT_STA_I.ac=ADFINAL.AMUX  * 0.0732422;		break; }	//电机定子侧A相线电流，互感器1200A:1A,Rt=5R,=1200A*10V/(5R*1A*32768)			     
		case(4): {AD_OUT_STA_I.ba=ADFINAL.AMUX  * 0.0732422;		break; }	//电机定子侧B相线电流，互感器
		case(7): {AD_OUT_SCR_I.a =ADFINAL.AMUX  * 0.0305176;		break; }	//ActiveCROWBAR中A相SCR电流，电流LEM/电压型反馈4A--400V =10V*400A/(32768*4V)			     
		case(6): {AD_OUT_SCR_I.b =ADFINAL.AMUX  * 0.0305176;		break; }	//ActiveCROWBAR中B相SCR电流，电流LEM/电压型反馈4A--400V =10V*400A/(32768*4V)
		default: break; 			       
	}

	if(M_ChkCounter(MAIN_LOOP.cnt_AMUX,DELAY_AMUX)>=0)
	{
	    MAIN_LOOP.cnt_AMUX=0;                         							//5ms读一次慢速AD	     
	     _OUT_AMUX1++;
		 if(_OUT_AMUX1 >= 12) _OUT_AMUX1=0;
		 _OUT4_DATA = _OUT_AMUX1;
		 *OUT4_ADDR = _OUT4_DATA;
    } 
	else
	{
		_OUT_AMUX2++;
		if((_OUT_AMUX2 > 7) || (_OUT_AMUX2 < 4)) _OUT_AMUX2=4; 
		_OUT4_DATA = _OUT_AMUX2;
		*OUT4_ADDR = _OUT4_DATA;	
	}
*/
//-------------------定子电流（单位A）-------------------------------------------------------------
	AD_OUT_STA_I.a  =  (AD_OUT_STA_I.ac - AD_OUT_STA_I.ba) * 0.3333333;
	AD_OUT_STA_I.b  =   AD_OUT_STA_I.a  + AD_OUT_STA_I.ba;	
    AD_OUT_STA_I.c  = - AD_OUT_STA_I.a  - AD_OUT_STA_I.b; 

//------------------网侧变流器电流------------------------------------------------------------------
    AD_OUT_NPR_I.a = - (ADFINAL.ia1 * 0.0572204);  // SKIIP反馈电流(流出桥臂为正),底板有一个反向，10V=1875A,=10V*1875A/(32768*10V)
	AD_OUT_NPR_I.b = - (ADFINAL.ib1 * 0.0572204);  // SKIIP反馈电流(流出桥臂为正)，控制算法以流出SKIIP�
	AD_OUT_NPR_I.c = - (ADFINAL.ic1 * 0.0572204);  // SKIIP反馈电流(流出桥臂为正)

//-------------------机侧变流器电流转为实际值------------------------------------------------------
    AD_OUT_MPR_I.a =  (ADFINAL.ia2 * 0.0572204);  // SKIIP反馈电流(流出桥臂为正),底板有一个反向，10V=1875A,=10V*1875A/(32768*10V)
	AD_OUT_MPR_I.b =  (ADFINAL.ib2 * 0.0572204);  // SKIIP反馈电流(流出桥臂为正)，控制算法以流出SKIIP为正
	AD_OUT_MPR_I.c =  (ADFINAL.ic2 * 0.0572204);  // SKIIP反馈电流(流出桥臂为正)

//---------------------直流电压----------------------------------------------------------------------
    AD_OUT_UDC      = ADFINAL.udc * 0.0448788;   // LEM(40mA=1500V),Rt=255R,=10V*1500V/(32768*255R*40mA)
	DataFilter(0.44,&MEAN_DATA.udc,AD_OUT_UDC);  //Vdc直流滤波，fh=1kHz
    AD_OUT_UDC      = MEAN_DATA.udc;
													
//---------------------主断前电网电压---------------------------------------------------------------------
	AD_OUT_GRD_U.ab = ADFINAL.uab * 0.0448788;   // LEM(40mA=1500V),Rt=255R,=10V*1500V/(32768*255R*40mA)
	AD_OUT_GRD_U.bc = ADFINAL.ubc * 0.0448788;   // LEM(40mA=1500V),Rt=255R,=10V*1500V/(32768*255R*40mA) 

//---------------------主断后电网电压---------------------------------------------------------------------
	AD_OUT_NGS_U.ab = ADFINAL.uab1 * 0.0448788;   // LEM(40mA=1500V),Rt=255R,=10V*1500V/(32768*255R*40mA)
	AD_OUT_NGS_U.bc = ADFINAL.ubc1 * 0.0448788;   // LEM(40mA=1500V),Rt=255R,=10V*1500V/(32768*255R*40mA)
 
//-------------------电机定子侧线电压---------------------------------------------
	AD_OUT_STA_U.ab  = ADFINAL.uab2 * 0.0448788;  // LEM(40mA=1500V),Rt=255R,=10V*1500V/(32768*255R*40mA)
	AD_OUT_STA_U.bc  = ADFINAL.ubc2 * 0.0448788;  // LEM(40mA=1500V),Rt=255R,=10V*1500V/(32768*255R*40mA)

//-------------------主断后电网电压,大滤波通道---------------------------------------------
	AD_OUT_NGF_U.ab  = ADFINAL.uab3 * 0.0448788;  // LEM(40mA=1500V),Rt=255R,=10V*1500V/(32768*255R*40mA)
	AD_OUT_NGF_U.bc  = ADFINAL.ubc3 * 0.0448788;  // LEM(40mA=1500V),Rt=255R,=10V*1500V/(32768*255R*40mA)

/* 屏蔽零漂滤波20100507atzuoyun
//---------------------零漂滤波-------------------------------------------------------------------	
	DataFilter(0.999985,&MEAN_DATA.zfia1,AD_OUT_NPR_I.a); //网侧电流传感器	零漂滤波60S
	DataFilter(0.999985,&MEAN_DATA.zfib1,AD_OUT_NPR_I.b); //网侧电流传感器	零漂滤波60S
	DataFilter(0.999985,&MEAN_DATA.zfic1,AD_OUT_NPR_I.c); //网侧电流传感器	零漂滤波60S

	DataFilter(0.999985,&MEAN_DATA.zfia3,AD_OUT_STA_I.a); //定子侧电流传感器	零漂滤波60S
	DataFilter(0.999985,&MEAN_DATA.zfib3,AD_OUT_STA_I.b); //定子侧电流传感器	零漂滤波60S
	DataFilter(0.999985,&MEAN_DATA.zfic3,AD_OUT_STA_I.c); //定子侧电流传感器	零漂滤波60S

	DataFilter(0.999985,&MEAN_DATA.zfuab,AD_OUT_GRD_U.ab); //主断前网压电压传感器	零漂滤波60S
	DataFilter(0.999985,&MEAN_DATA.zfubc,AD_OUT_GRD_U.bc); //主断前网压电压传感器	零漂滤波60S 

	DataFilter(0.999985,&MEAN_DATA.zfuab1,AD_OUT_NGS_U.ab); //网压电压传感器	零漂滤波60S
	DataFilter(0.999985,&MEAN_DATA.zfubc1,AD_OUT_NGS_U.bc); //网压电压传感器	零漂滤波60S


//------------------除去零漂---------------------------------------------------------------------
	if(M_ChkFlag(SL_SENSZFSTDY)!=0)
	{
		AD_OUT_NPR_I.a = AD_OUT_NPR_I.a - MEAN_DATA.zfia1;  //网侧电流
		AD_OUT_NPR_I.b = AD_OUT_NPR_I.b - MEAN_DATA.zfib1;
		AD_OUT_NPR_I.c = AD_OUT_NPR_I.c - MEAN_DATA.zfic1;

		AD_OUT_STA_I.a = AD_OUT_STA_I.a - MEAN_DATA.zfia3;  //定子侧电流
		AD_OUT_STA_I.b = AD_OUT_STA_I.b - MEAN_DATA.zfib3; 
		AD_OUT_STA_I.c = AD_OUT_STA_I.c - MEAN_DATA.zfic3; 

		AD_OUT_GRD_U.ab = AD_OUT_GRD_U.ab - MEAN_DATA.zfuab; //主断前网压电压
		AD_OUT_GRD_U.bc = AD_OUT_GRD_U.bc - MEAN_DATA.zfubc; 

		AD_OUT_NGS_U.ab = AD_OUT_NGS_U.ab - MEAN_DATA.zfuab1; //网压电压
		AD_OUT_NGS_U.bc = AD_OUT_NGS_U.bc - MEAN_DATA.zfubc1;	
	}
*/
//-------------------主断前电网电压，由线压转为相压------------------------------------------------------
	AD_OUT_GRD_U.b  = (AD_OUT_GRD_U.bc - AD_OUT_GRD_U.ab) * 0.3333333;
	AD_OUT_GRD_U.a  = AD_OUT_GRD_U.b + AD_OUT_GRD_U.ab;	
	AD_OUT_GRD_U.c  = - AD_OUT_GRD_U.a - AD_OUT_GRD_U.b; 

//-------------------电网电压，由线压转为相压------------------------------------------------------
	AD_OUT_NGS_U.b  = (AD_OUT_NGS_U.bc - AD_OUT_NGS_U.ab) * 0.3333333;
	AD_OUT_NGS_U.a  = AD_OUT_NGS_U.b + AD_OUT_NGS_U.ab;	
	AD_OUT_NGS_U.c  = - AD_OUT_NGS_U.a - AD_OUT_NGS_U.b;

//-------------------电机定子侧电压，由线压算相压------------------------------------------------------
	AD_OUT_STA_U.b  = (AD_OUT_STA_U.bc - AD_OUT_STA_U.ab) * 0.3333333;
	AD_OUT_STA_U.a  = AD_OUT_STA_U.b + AD_OUT_STA_U.ab;	
    AD_OUT_STA_U.c  = - AD_OUT_STA_U.a - AD_OUT_STA_U.b;

//---------------------平均值滤波------------------------------------------------------------------	
	tempa = abs(AD_OUT_NPR_I.a);
	tempb = abs(AD_OUT_NPR_I.b);
	tempc = abs(AD_OUT_NPR_I.c);
	DataFilter(0.9999,&MEAN_DATA.ia1,tempa); //网侧电流	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.ib1,tempb); //网侧电流	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.ic1,tempc); //网侧电流	平均值滤波10S

	tempa = abs(AD_OUT_MPR_I.a);
	tempb = abs(AD_OUT_MPR_I.b);
	tempc = abs(AD_OUT_MPR_I.c);
	DataFilter(0.9999,&MEAN_DATA.ia2,tempa); //机侧电流	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.ib2,tempb); //机侧电流	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.ic2,tempc); //机侧电流	平均值滤波10S	

	tempa = abs(AD_OUT_GRD_U.ab);
	tempb = abs(AD_OUT_GRD_U.bc);
	DataFilter(0.9999,&MEAN_DATA.uab,tempa); //主断前网侧电压	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.ubc,tempb); //主断前网侧电压	平均值滤波10S 

	tempa = abs(AD_OUT_NGS_U.ab);
	tempb = abs(AD_OUT_NGS_U.bc);
	DataFilter(0.9999,&MEAN_DATA.uab1,tempa); //网侧电压	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.ubc1,tempb); //网侧电压	平均值滤波10S

	tempa = abs(AD_OUT_STA_U.ab);
	tempb = abs(AD_OUT_STA_U.bc);
	DataFilter(0.9999,&MEAN_DATA.uab2,tempa); //定子侧电压	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.ubc2,tempb); //定子侧电压	平均值滤波10S


	tempa = abs(AD_OUT_NGS_U.a);
	tempb = abs(AD_OUT_NGS_U.b);
	tempc = abs(AD_OUT_NGS_U.c);
	DataFilter(0.9999,&MEAN_DATA.ua1,tempa); //网侧电压	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.ub1,tempb); //网侧电压	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.uc1,tempc); //网侧电压	平均值滤波10S

//--------------求网压和定子电压差值平均值------------------------------------------
    AD_OUT_STAD_U.ab = AD_OUT_NGF_U.ab - AD_OUT_STA_U.ab;
    AD_OUT_STAD_U.bc = AD_OUT_NGF_U.bc - AD_OUT_STA_U.bc;

	tempa = abs(AD_OUT_STAD_U.ab);
	tempb = abs(AD_OUT_STAD_U.bc);
	DataFilter(0.9999,&MEAN_DATA.uab_d,tempa);  //差值平均值    平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.ubc_d,tempb);  //差值平均值	平均值滤波10S
//----------------------------------------------------------------------------------

	tempa = abs(AD_OUT_STA_I.ac);
	tempb = abs(AD_OUT_STA_I.ba);
	DataFilter(0.9999,&MEAN_DATA.iac3,tempa); //定子侧电流	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.iba3,tempb); //定子侧电流	平均值滤波10S

	tempa = abs(AD_OUT_STA_I.a);
	tempb = abs(AD_OUT_STA_I.b);
	tempc = abs(AD_OUT_STA_I.c);
	DataFilter(0.9999,&MEAN_DATA.ia3,tempa); //定子侧电流	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.ib3,tempb); //定子侧电流	平均值滤波10S
	DataFilter(0.9999,&MEAN_DATA.ic3,tempc); //定子侧电流	平均值滤波10S


//--------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
	*AD_DA_CTRL     = AD8364_CONVERT;	               	  	//启动下籄D转换
    CAP4.adsmptsctr = ECap4Regs.TSCTR;						//保存网压定向时间
	QEPDATA.adsmposcnt=EQep2Regs.QPOSCNT;					//保存编码器位置信息  

}
/*********************************************************************************************************
** 函数名称: Output
** 功能描�: 10路信号输出; 8路LED显示输出
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Output(void)
{
//------------------------------数据输出-----------------------------------

   *OUT1_ADDR = _OUT1_DATA;
   *OUT2_ADDR = _OUT2_DATA;

//------------------------------功率风机星三角变换--------------------------------------------------------------------------

	 if(AMUX.skiiptempmax < _SC_FAN_TL)           M_ClrFlag(SL_FANSELECT);   //温度小于40度,星接                      
	 else if(AMUX.skiiptempmax > _SC_FAN_TH)      M_SetFlag(SL_FANSELECT);   //温度大于50度,变为角接        


   if(M_ChkFlag(SL_FAN_WORK)!=0)            								//功率风机星三角变换,根据SKIIP温度判断
   { 
     if(M_ChkFlag(SL_FANSELECT)==0) 
     {
        M_ClrFlag(CL_FANTRIANGLE);
 	    if(M_ChkCounter(MAIN_LOOP.cnt_fanstar,DELAY_FANSTAR)>=0)	    M_SetFlag(CL_FANSTAR);       //延时后再闭合星接接触器
		MAIN_LOOP.cnt_fantriangle=0;		 
     } 
	 else 
	 {
	 	M_ClrFlag(CL_FANSTAR);
	 	if(M_ChkCounter(MAIN_LOOP.cnt_fantriangle,DELAY_FANTRIANGLE)>=0) M_SetFlag(CL_FANTRIANGLE);     //延时后再闭合角接接触器
		MAIN_LOOP.cnt_fanstar=0;	 
	 }
   }
   else
   {
      MAIN_LOOP.cnt_fanstar=0;
	  MAIN_LOOP.cnt_fantriangle=0;
	  M_ClrFlag(CL_FANSTAR);
      M_ClrFlag(CL_FANTRIANGLE);
   }

//---------------------CANOPEN反馈主控的状态变量---------------------------------------------------
/*
    M_SetFlag(SL_CBCLOSED);   //cantest
	M_SetFlag(SL_STEADYFB);   //cantest
	M_SetFlag(SL_MPR_SYNOK);   //cantest
    M_SetFlag(SL_SPEED_IN_RANGE);   //cantest
	M_ClrFlag(SL_SERIESTOP); //cantest
    M_ClrFlag(SL_ERRSTOP); //cantest
	M_SetFlag(SL_PGOV);   //cantest
    M_SetFlag(SL_QGOV);   //cantest
	M_NotFlag(SL_TEST); //cantest
*/
//-----------------------------------tx_state1----------------------------------------	
	if(M_ChkFlag(SL_CBCLOSED)!=0)	  		SCI_canopen.tx_state1 |= COM_NPRREADY;
	else									SCI_canopen.tx_state1 &= COM_NPRREADY_NOT;

	if(M_ChkFlag(SL_STEADYFB)!=0)	  		SCI_canopen.tx_state1 |= COM_NPRON;
	else 									SCI_canopen.tx_state1 &= COM_NPRON_NOT;

	if(M_ChkFlag(SL_MPR_SYNOK)!=0)	  		SCI_canopen.tx_state1 |= COM_READYGENERATION;
	else 									SCI_canopen.tx_state1 &= COM_READYGENERATION_NOT;

	if(M_ChkFlag(SL_SPEED_IN_RANGE)!=0)	  	SCI_canopen.tx_state1 |= COM_SPEEDINRANGE;
	else 									SCI_canopen.tx_state1 &= COM_SPEEDINRANGE_NOT;

//201105atzuoyun
	if((M_ChkFlag(SL_SERIESTOP)!=0 || M_ChkFlag(SL_ERRSTOP)!=0) && M_ChkFlag(SL_REPORT_OCS)!=0)	  	  SCI_canopen.tx_state1 |= COM_FAILURE;
	else  if((M_ChkFlag(SL_SERIESTOP)==0 && M_ChkFlag(SL_ERRSTOP)==0)||M_ChkFlag(SL_REPORT_OCS)==0)	  SCI_canopen.tx_state1 &= COM_FAILURE_NOT;

	if(M_ChkFlag(SL_SERIESTOP)!=0 && M_ChkFlag(SL_REPORT_OCS)!=0)	SCI_canopen.tx_state1 |= COM_FAILUREA;
	else 															SCI_canopen.tx_state1 &= COM_FAILUREA_NOT;

	if(M_ChkFlag(SL_ERRSTOP)!=0 && M_ChkFlag(SL_REPORT_OCS)!=0)	  	SCI_canopen.tx_state1 |= COM_FAILUREB;
	else 															SCI_canopen.tx_state1 &= COM_FAILUREB_NOT;
//201105atzuoyun

	if(M_ChkFlag(SL_PGOV_COM)!=0)	  		SCI_canopen.tx_state1 |= COM_PLIM;
    else 									SCI_canopen.tx_state1 &= COM_PLIM_NOT;

	if(M_ChkFlag(SL_QGOV_COM)!=0)	  		SCI_canopen.tx_state1 |= COM_QLIM;
	else 									SCI_canopen.tx_state1 &= COM_QLIM_NOT;
	

	SCI_canopen.tx_state1 &= 0x7FFF;
	SCI_canopen.tx_state1 |= (SCI_canopen.heartbeat <<8);

//-----------------------------------tx_state2----------------------------------------	

//	if(((PRO.STA_iac* 1.05)>_SC_IACOVST)||((PRO.STA_iba* 1.05)>_SC_IACOVST))	  	SCI_canopen.tx_state2 |= COM_ILIM;
//	else																			SCI_canopen.tx_state2 &= COM_ILIM_NOT;

	if(M_ChkFlag(SL_IN1_EXESTOP)!=0)	  	SCI_canopen.tx_state2 |= COM_ESTOP;     //20090815
	else 									SCI_canopen.tx_state2 &= COM_ESTOP_NOT;

	if(M_ChkFlag(SL_CBTRIP)!=0)	  			SCI_canopen.tx_state2 |= COM_CBTRIP;
	else 									SCI_canopen.tx_state2 &= COM_CBTRIP_NOT;

	if(M_ChkFlag(SL_IN1_CBSTS)!=0)	  		SCI_canopen.tx_state2 |= COM_CBCLOSED;			//1=CB closed 0=CB open
	else 									SCI_canopen.tx_state2 &= COM_CBCLOSED_NOT;

//	if(PRO.Pg>1.8e6)	  					SCI_canopen.tx_state2 |= COM_POWLIM;
//    else if(PRO.Pg<1.75e6)					SCI_canopen.tx_state2 &= COM_POWLIM_NOT;	
   
//-----------------------------显示灯输出----------------------------------
	if(M_ChkFlag(SL_DISPLAY0)!=0) 	GpioDataRegs.GPBCLEAR.bit.GPIO56 = 1;
	else							GpioDataRegs.GPBSET.bit.GPIO56 = 1;

	if(M_ChkFlag(SL_DISPLAY1)!=0)  	GpioDataRegs.GPBCLEAR.bit.GPIO57 = 1;
	else							GpioDataRegs.GPBSET.bit.GPIO57 = 1;
	
	if(M_ChkFlag(SL_DISPLAY2)!=0) 	GpioDataRegs.GPBCLEAR.bit.GPIO58 = 1;
	else							GpioDataRegs.GPBSET.bit.GPIO58 = 1;

	if(M_ChkFlag(SL_DISPLAY3)!=0)  	GpioDataRegs.GPBCLEAR.bit.GPIO59 = 1;
	else							GpioDataRegs.GPBSET.bit.GPIO59 = 1;

	if(M_ChkFlag(SL_DISPLAY4)!=0) 	GpioDataRegs.GPBCLEAR.bit.GPIO60 = 1;
	else							GpioDataRegs.GPBSET.bit.GPIO60 = 1;

	if(M_ChkFlag(SL_DISPLAY5)!=0)  	GpioDataRegs.GPBCLEAR.bit.GPIO61 = 1;
	else							GpioDataRegs.GPBSET.bit.GPIO61 = 1;

	if(M_ChkFlag(SL_DISPLAY6)!=0) 	GpioDataRegs.GPBCLEAR.bit.GPIO62 = 1;
	else							GpioDataRegs.GPBSET.bit.GPIO62 = 1;

	if(M_ChkFlag(SL_DISPLAY7)!=0)  	GpioDataRegs.GPBCLEAR.bit.GPIO63 = 1;
	else							GpioDataRegs.GPBSET.bit.GPIO63 = 1;

} 
/*********************************************************************************************************
** 函数名称: Input
** 功能描述: 16路信号输入; 
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Input(void)
{
	Uint16 tempa,tempb,tempc,tempda,tempdb,tempdc;

//--------------------------------数据输入----------------------------------
	tempa = *IN1_ADDR; 
	tempb = *IN2_ADDR;
	tempc = *IN3_ADDR;

	DELAY_US(100L);        //100us延时，IO输入防抖

	tempda = *IN1_ADDR;    
	tempdb = *IN2_ADDR;
	tempdc = *IN3_ADDR;

//--------------------------------数据输入----------------------------------	
	if((tempa==tempda)&&(tempb==tempdb))	
		_IN12_DATA = (tempa & 0x00FF) | ((tempb<<8) & 0xFF00);

	if(tempc==tempdc)		
		_IN34_DATA = (tempc & 0x00FF);

if(_CANOPER==0)
{
//---------------------上位机要求合主断------------------------------------------------------
	if(_EIN!= 0)							
	{ 
	  if(M_ChkCounter(MAIN_LOOP.cnt_nprcmd,DELAY_NPRCMD)>0)	M_SetFlag(SL_OCS_EIN);     //延时1s，防止误操作
	}				
	else 	
	{
		M_ClrFlag(SL_OCS_EIN);              
		MAIN_LOOP.cnt_nprcmd=0;		
    } 

//---------------------上位机要求复位故障------------------------------------------------------
	if(_RESET != 0)							//上位机要求复位故障,由外部I/O给定
	{ 
	  if(M_ChkCounter(MAIN_LOOP.cnt_reset,DELAY_RESET)>0)	 M_SetFlag(SL_OCS_RESET);     //延时2s，防止误操作
	}				
	else 	
	{
		M_ClrFlag(SL_OCS_RESET);
		MAIN_LOOP.cnt_reset=0;		
    }	
//---------------------上位机要求变流器启动------------------------------------------------------
	if(_SYSRUN!= 0)							
	{ 
	  if(M_ChkCounter(MAIN_LOOP.cnt_clostacmd,DELAY_CLOSTACMD)>0)	 M_SetFlag(SL_OCS_SYSRUN);     //延时1s，防止误操作
	}				
	else 	
	{
		M_ClrFlag(SL_OCS_SYSRUN);
		MAIN_LOOP.cnt_clostacmd=0;		
    }
 
}
else
{ 
//---------------------上位机要求合主断------------------------------------------------------
	if((SCI_canopen.rx_controlword & COM_EIN)==COM_EIN)	    
	{
		if(M_ChkCounter(MAIN_LOOP.cnt_ocsein1,DELAY_OCSEIN1)>0)		M_SetFlag(SL_OCS_EIN);	//20090817 250ms
		MAIN_LOOP.cnt_ocsein2=0;	
	}								
	else 		  											
	{
		if(M_ChkCounter(MAIN_LOOP.cnt_ocsein2,DELAY_OCSEIN2)>0)		M_ClrFlag(SL_OCS_EIN);	//20090817 250ms 
		MAIN_LOOP.cnt_ocsein1=0;
	}

//---------------------上位机要求复位故障------------------------------------------------------
	if((SCI_canopen.rx_controlword & COM_OCSRESET)==COM_OCSRESET)	M_SetFlag(SL_OCS_RESET);									
	else 	   														M_ClrFlag(SL_OCS_RESET);													

	
//---------------------上位机要求变流器启动------------------------------------------------------
	if((SCI_canopen.rx_controlword & COM_SYSRUN)==COM_SYSRUN)	
	{
		if(M_ChkCounter(MAIN_LOOP.cnt_ocssysrun1,DELAY_OCSSYSRUN1)>0)	M_SetFlag(SL_OCS_SYSRUN);	//20090817 250ms
		MAIN_LOOP.cnt_ocssysrun2=0;	
	}									
	else 		  											
	{
		if(M_ChkCounter(MAIN_LOOP.cnt_ocssysrun2,DELAY_OCSSYSRUN2)>0)	M_ClrFlag(SL_OCS_SYSRUN);	//20090817  250ms
		MAIN_LOOP.cnt_ocssysrun1=0;
	}

//---------------------上位机要求网侧变流器单涝诵形薰Σ⑼�------------------------------------------------------
//	if((SCI_canopen.rx_controlword & COM_NPREIN)==COM_NPREIN)	M_SetFlag(SL_OCS_NPREIN);  //暂时屏蔽										
//	else 		  												M_ClrFlag(SL_OCS_NPREIN); 
}	 
//---------------------------接收操作器来的PI环参数-------------------------------------------------
	PI_PARA_NPRU.kp           = _NPR_U_Kp/100.00;			//DOT2属性
    PI_PARA_NPRU.ki           = _NPR_U_Ki/10.00;			//DOT1属性
	PI_PARA_NPRU.kd           = _NPR_U_Kd/1000.00;			//DOT3属性
	PI_PARA_NPRU.outmax       = _NPR_U_outmax;
	PI_PARA_NPRU.errmax       = _NPR_U_errmax/10.00;		//DOT1属性		//NPR电压环参数
	PI_PARA_NPRU.errmin       = _NPR_U_errmin/1000.00;	    //DOT3属性
	PI_PARA_NPRU.incrementmax = _NPR_U_incrementmax/10.00;  //DOT1属性

	PI_PARA_NPRID.kp           = _NPR_ID_Kp/100.00;			//DOT2属性
    PI_PARA_NPRID.ki           = _NPR_ID_Ki/10.00;			//DOT1属性
    PI_PARA_NPRID.kd           = _NPR_ID_Kd/1000.00;			//DOT3属性
	PI_PARA_NPRID.outmax       = _NPR_ID_outmax;		 					    //NPR电流环参数
	PI_PARA_NPRID.errmax       = _NPR_ID_errmax/10.00;		//DOT1属性
	PI_PARA_NPRID.errmin       = _NPR_ID_errmin/1000.00;		//DOT3属性
	PI_PARA_NPRID.incrementmax = _NPR_ID_incrementmax/10.00;	//DOT1属性

	PI_PARA_NPRIQ.kp           = _NPR_IQ_Kp/100.00;			//DOT2属性
    PI_PARA_NPRIQ.ki           = _NPR_IQ_Ki/10.00;			//DOT1属性
    PI_PARA_NPRIQ.kd           = _NPR_IQ_Kd/1000.00;			//DOT3属性
	PI_PARA_NPRIQ.outmax       = _NPR_IQ_outmax;		 					    //NPR电流环参数
	PI_PARA_NPRIQ.errmax       = _NPR_IQ_errmax/10.00;		//DOT1属性
	PI_PARA_NPRIQ.errmin       = _NPR_IQ_errmin/1000.00;		//DOT3属性
	PI_PARA_NPRIQ.incrementmax = _NPR_IQ_incrementmax/10.00;	//DOT1属性

	PI_PARA_MPRID.kp           = _MPR_ID_Kp/100.00;			//DOT2属性
    PI_PARA_MPRID.ki           = _MPR_ID_Ki/10.00;			//DOT1属性
    PI_PARA_MPRID.kd           = _MPR_ID_Kd/1000.00;			//DOT3属性
	PI_PARA_MPRID.outmax       = _MPR_ID_outmax;   							//MPR电流环参数
	PI_PARA_MPRID.errmax       = _MPR_ID_errmax/10.00;		//DOT1属性
	PI_PARA_MPRID.errmin       = _MPR_ID_errmin/1000.00;		//DOT3属性
	PI_PARA_MPRID.incrementmax = _MPR_ID_incrementmax/10.00;	//DOT1属性

	PI_PARA_MPRIQ.kp           = _MPR_IQ_Kp/100.00;			//DOT2属性
    PI_PARA_MPRIQ.ki           = _MPR_IQ_Ki/10.00;			//DOT1属性
    PI_PARA_MPRIQ.kd           = _MPR_IQ_Kd/1000.00;			//DOT3属性
	PI_PARA_MPRIQ.outmax       = _MPR_IQ_outmax;   							//MPR电流环参数
	PI_PARA_MPRIQ.errmax       = _MPR_IQ_errmax/10.00;		//DOT1属性
	PI_PARA_MPRIQ.errmin       = _MPR_IQ_errmin/1000.00;		//DOT3属性
	PI_PARA_MPRIQ.incrementmax = _MPR_IQ_incrementmax/10.00;	//DOT1属性

	PI_PARA_MPRU.kp           = _MPR_U_Kp/100.00;			//DOT2属性
    PI_PARA_MPRU.ki           = _MPR_U_Ki/10.00;			//DOT3属性
    PI_PARA_MPRU.kd           = _MPR_U_Kd/1000.00;			//DOT1属性
	PI_PARA_MPRU.outmax       = _MPR_U_outmax;   							//MPR电压环参数
	PI_PARA_MPRU.errmax       = _MPR_U_errmax/10.00;		//DOT1属性
	PI_PARA_MPRU.errmin       = _MPR_U_errmin/1000.00;		//DOT3属性
	PI_PARA_MPRU.incrementmax = _MPR_U_incrementmax/10.00;  //DOT1属性

	PI_PARA_DYNU.kp           = _DYN_U_Kp/100.00;			//DOT2属性
    PI_PARA_DYNU.ki           = _DYN_U_Ki/1000.00;			//DOT3属性	//将错就错201105atzuoyun
    PI_PARA_DYNU.kd           = _DYN_U_Kd/10.00;			//DOT1属性
	PI_PARA_DYNU.outmax       = _DYN_U_outmax;   							//动态电压环参数
	PI_PARA_DYNU.errmax       = _DYN_U_errmax/10.00;		//DOT1属性
	PI_PARA_DYNU.errmin       = _DYN_U_errmin/1000.00;		//DOT3属性
	PI_PARA_DYNU.incrementmax = _DYN_U_incrementmax/10.00;  //DOT1属性

	_eidco   = _EIDCO/1000.00;
	_encodpos= _ENCODPOS/1000.00;
	_stdby01 = _STDBY1/1000.00;				//备用经过小数点处理后值
	_stdby02 = _STDBY2/100.00;				//备用经过小数点处理后值
	_stdby03 = _STDBY3/10.00;				//备用经过小数点处理后值
    _stdby04 = _STDBY4;			        	//备用
	_stdby05 = _STDBY5;			        	//备用

} 
/*********************************************************************************************************
** 函数名称: Disepwmio_NPR
** 功能描述: 
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　�: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Disepwmio_NPR(void)
{
	EALLOW;
	
	EPwm1Regs.AQCSFRC.bit.CSFA = 1;		//force low  AQCSFRC = Action Qualifier Continuous S/W force Register Set
	EPwm1Regs.AQCSFRC.bit.CSFB = 2;		//force high
	EPwm2Regs.AQCSFRC.bit.CSFA = 1;		//force low
	EPwm2Regs.AQCSFRC.bit.CSFB = 2;		//force high
	EPwm3Regs.AQCSFRC.bit.CSFA = 1;		//force low
	EPwm3Regs.AQCSFRC.bit.CSFB = 2;		//force high

	EPwm1Regs.TBCTR = 0x0000;           // Clear counter  TBCTR = Time-base Counter Register
    EPwm2Regs.TBCTR = 0x0000;           // Clear counter
    EPwm3Regs.TBCTR = 0x0000;           // Clear counter

	EDIS;
}  
/*********************************************************************************************************
** 函数名称: Disepwmio_MPR
** 功能描述: 
** 输∪? 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Disepwmio_MPR(void)
{
	EALLOW;
	
	EPwm4Regs.AQCSFRC.bit.CSFA = 1;		//force low  AQCSFRC = Action Qualifier Continuous S/W force Register Set
	EPwm4Regs.AQCSFRC.bit.CSFB = 2;		//force high
	EPwm5Regs.AQCSFRC.bit.CSFA = 1;		//force low
	EPwm5Regs.AQCSFRC.bit.CSFB = 2;		//force high
	EPwm6Regs.AQCSFRC.bit.CSFA = 1;		//force low
	EPwm6Regs.AQCSFRC.bit.CSFB = 2;		//force high 
  
    EPwm4Regs.TBCTR = 0x0000;           // Clear counter  TBCTR = Time-base Counter Register
    EPwm5Regs.TBCTR = 0x0000;           // Clear counter
    EPwm6Regs.TBCTR = 0x0000;           // Clear counter 

	EDIS;
}  
/*********************************************************************************************************
** 函数名称： DisPwm
** 功能描述：脉冲禁止
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void DisPwm(void)
{
	Disepwmio_NPR();
	Disepwmio_MPR();
} 


/*********************************************************************************************************
** 函数名称: Enepwmio_NPR
** 功能描述: 
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日∑?
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Enepwmio_NPR(void)
{
	EALLOW;

	EPwm1Regs.AQCSFRC.bit.CSFA = 3;		//forcing disabled  AQCSFRC = Action Qualifier Continuous S/W force Register Set
	EPwm1Regs.AQCSFRC.bit.CSFB = 3;		//forcing disabled
	EPwm2Regs.AQCSFRC.bit.CSFA = 3;		//forcing disabled
	EPwm2Regs.AQCSFRC.bit.CSFB = 3;		//forcing disabled
	EPwm3Regs.AQCSFRC.bit.CSFA = 3;		//forcing disabled
	EPwm3Regs.AQCSFRC.bit.CSFB = 3;		//forcing disabled

	EDIS;
}  
/*********************************************************************************************************
** 函数名称：Enepwmio_MPR
** 功能描述: 
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Enepwmio_MPR(void)
{
	EALLOW;
	
	EPwm4Regs.AQCSFRC.bit.CSFA = 3;		//forcing disabled
	EPwm4Regs.AQCSFRC.bit.CSFB = 3;		//forcing disabled
	EPwm5Regs.AQCSFRC.bit.CSFA = 3;		//forcing disabled
	EPwm5Regs.AQCSFRC.bit.CSFB = 3;		//forcing disabled
	EPwm6Regs.AQCSFRC.bit.CSFA = 3;		//forcing disabled
	EPwm6Regs.AQCSFRC.bit.CSFB = 3;		//forcing disabled

	EDIS;
}    
/*********************************************************************************************************
** 函数名称: ConfigPwm
** 功能描述: 进行PWM开关频率和死区时间的设置
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void ConfigPwm(void)						
{
	Disepwmio_NPR();
	Disepwmio_MPR();
	EALLOW;
//----------NPR的PWM1-PWM6设置---------------//
    EPwm1Regs.TBPRD = 3750000/_SW_FR;           // 开关频率为操作器给定,_SW_FR=200对应2kHz
    EPwm2Regs.TBPRD = 3750000/_SW_FR;           // PWM时钟为75MHz
	EPwm3Regs.TBPRD = 3750000/_SW_FR;           // TBPRD = Time-base Period Register
    SW_NPR=3750000.0/_SW_FR;

    EPwm1Regs.DBRED = 75 * _DEADTIME;              //死区，_DEADTIME单位为us 
    EPwm1Regs.DBFED = 75 * _DEADTIME;              //
    EPwm2Regs.DBRED = 75 * _DEADTIME;              //DBRED = Dead-Band Generator Rising Edge Delay Count Register
    EPwm2Regs.DBFED = 75 * _DEADTIME;              //DBFED = Dead-Band Generator Falling Edge Delay Count Register
    EPwm3Regs.DBRED = 75 * _DEADTIME;
    EPwm3Regs.DBFED = 75 * _DEADTIME;   

//----------MPR的PWM7-PWM12设置---------------//
	EPwm4Regs.TBPRD = 3750000/_SW_FR;           // 
    EPwm5Regs.TBPRD = 3750000/_SW_FR;           // 
    EPwm6Regs.TBPRD = 3750000/_SW_FR;           // 
    SW_MPR=3750000.0/_SW_FR;

    EPwm4Regs.DBRED = 75 * _DEADTIME;
    EPwm4Regs.DBFED = 75 * _DEADTIME;
    EPwm5Regs.DBRED = 75 * _DEADTIME;
    EPwm5Regs.DBFED = 75 * _DEADTIME;
    EPwm6Regs.DBRED = 75 * _DEADTIME;
    EPwm6Regs.DBFED = 75 * _DEADTIME; 

   GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1;  // GPIO0 = PWM1A
   GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 1;  // GPIO1 = PWM1B
   GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 1;  // GPIO2 = PWM2A
   GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 1;  // GPIO3 = PWM2B
   GpioCtrlRegs.GPAMUX1.bit.GPIO4 = 1;  // GPIO4 = PWM3A
   GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 1;  // GPIO5 = PWM3B 
   GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 1;  // GPIO6 = PWM4A
   GpioCtrlRegs.GPAMUX1.bit.GPIO7 = 1;  // GPIO7 = PWM4B
   GpioCtrlRegs.GPAMUX1.bit.GPIO8 = 1;  // GPIO8 = PWM5A
   GpioCtrlRegs.GPAMUX1.bit.GPIO9 = 1;  // GPIO9 = PWM5B
   GpioCtrlRegs.GPAMUX1.bit.GPIO10 = 1;  // GPIO10 = PWM6A
   GpioCtrlRegs.GPAMUX1.bit.GPIO11 = 1;  // GPIO11 = PWM6B  
    
	EDIS; 
}

/*********************************************************************************************************
** 函数名称: EnPdpint
** 功能描述: 
** 输　入: 	 
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　? 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void EnPdpint(void)
{
	EALLOW;
	
   // Enable TZ interrupt
    EPwm1Regs.TZEINT.bit.OST = 1;
    EPwm2Regs.TZEINT.bit.OST = 1;
    EPwm3Regs.TZEINT.bit.OST = 1;
	EPwm4Regs.TZEINT.bit.OST = 1;
    EPwm5Regs.TZEINT.bit.OST = 1;
    EPwm6Regs.TZEINT.bit.OST = 1;

	EDIS;
}
/*********************************************************************************************************
** 函数名称: DisPdpint
** 功能描述: 
** 输　入: 	 
** 输　:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 鳌≌�: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void DisPdpint(void)
{
	EALLOW;
	
   // disable TZ interrupt
    EPwm1Regs.TZEINT.bit.OST = 0;
    EPwm2Regs.TZEINT.bit.OST = 0;
    EPwm3Regs.TZEINT.bit.OST = 0;
	EPwm4Regs.TZEINT.bit.OST = 0;
    EPwm5Regs.TZEINT.bit.OST = 0;
    EPwm6Regs.TZEINT.bit.OST = 0;

	EDIS;
} 
/*********************************************************************************************************
** 函数名称: ClrPdpint
** 功能描述: 
** 输　入: 	 
** 输〕?   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void ClrPdpint(void)
{
	EALLOW;
	
    EPwm1Regs.TZCLR.bit.OST = 1;   
    EPwm1Regs.TZCLR.bit.INT = 1;
	EPwm2Regs.TZCLR.bit.OST = 1;   
    EPwm2Regs.TZCLR.bit.INT = 1;
	EPwm3Regs.TZCLR.bit.OST = 1;   
    EPwm3Regs.TZCLR.bit.INT = 1;
	EPwm4Regs.TZCLR.bit.OST = 1;   
    EPwm4Regs.TZCLR.bit.INT = 1;
	EPwm5Regs.TZCLR.bit.OST = 1;   
    EPwm5Regs.TZCLR.bit.INT = 1;
	EPwm6Regs.TZCLR.bit.OST = 1;   
    EPwm6Regs.TZCLR.bit.INT = 1;

	EDIS;
}   

//===========================================================================
// No more.
//==========================================================================