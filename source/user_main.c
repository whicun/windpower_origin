/****************************************Copyright (c)**************************************************
**                       		     北	京	交	通	大	学
**                                         电气工程学院
**                                         614实验室
**
**                              
**
**--------------文件信息--------------------------------------------------------------------------------
**文   件   名: user_main.c
**创   建   人: 
**最后修改日期: 
**描        述: 1.5MW双馈变流器控制软件主程序----左云风场
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
/*---------------------自带头文件-----------------------------*/
#include "IQmathLib.h"
#include "qmath.h"
#include "DSP2833x_Device.h"     // DSP2833x Headerfile Include File
#include "DSP2833x_Examples.h"   // DSP2833x Examples Include File
/*--------------------自定义头文件----------------------------*/
#include "user_header.h"  						//变量常量定义
#include "user_macro.h"							//宏函数
#include "user_database.h"						//数据库   
#include "user_interface.c"						//接口层
#include "user_work.c"							//工作控制
#include "math.h"

/*-----------------------中断声明-----------------------------*/
interrupt void CpuTimer0Isr(void);				//主定时器中断			
interrupt void EPWM1_TZ1_Isr(void);			//TZ1中断
interrupt void EPWM2_TZ2_Isr(void);			//TZ2中断
/*-----------------------函数声明-----------------------------*/

void Protect(void);
void Scout(void);
void et_relay_N(void);
void et_relay_M(void);
void CntCtrl(void);
void SysCtrl(void);
void Bank(void);
void Display(void);
void Datasave(void);	//20091109atzy

// These are defined by the linker (see F28335_flash.cmd)
extern Uint16 RamfuncsLoadStart;
extern Uint16 RamfuncsLoadEnd;
extern Uint16 RamfuncsRunStart;
/*********************************************************************************************************
** 函数名称: main
** 功能描述: 系统初始化,主循环
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
void main(void)
{
//--------------------------------系统初始化--------------------------------
	InitSysCtrl();
//---------------------------------IO初始化---------------------------------
	InitGpio();	

	DINT;
   	InitPieCtrl();
	IER = 0x0000;
	IFR = 0x0000;
	InitPieVectTable();
//--------------------------------外设初始化--------------------------------
	InitPeripherals(); 	
//---------------------------------写FLASH程序------------------------------
	MemCopy(&RamfuncsLoadStart, &RamfuncsLoadEnd, &RamfuncsRunStart);
	InitFlash();
//--------------------------------工作初始化--------------------------------
	InitWork();	
//---------------------------用户自定义程序的初始化-------------------------
	InitEeprom();

//	InitRtimer();										//实时时钟初始化只在芯片需要初始化时才进行
//-------------------------------中断地址初始化-----------------------------
	EALLOW;  
	PieVectTable.TINT0       = &CpuTimer0Isr;		  //定时器T0周期中断
	PieVectTable.EPWM1_TZINT = &EPWM1_TZ1_Isr;      //TZ1 功率保护中断 
	PieVectTable.EPWM2_TZINT = &EPWM2_TZ2_Isr;      //TZ2 功率保护中断 

	EDIS;    
	
//------------------------------加载中断使能寄存器--------------------------
    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;		//CPUTIMER0周期中断
    										//INT1(CPU)/INT1.7(PIE)
	PieCtrlRegs.PIEIER2.bit.INTx1 = 1;		//epwm1.tz
											//INT2(CPU)/INT2.1(PIE)
	PieCtrlRegs.PIEIER2.bit.INTx2 = 1;		//epwm2.tz 																	//INT1(CPU)/INT1.1(PIE)
												
    PieCtrlRegs.PIECTRL.bit.ENPIE = 1;  	//使能PIE   
    
   	IER |= (M_INT1|M_INT2);					//加载总中断屏蔽寄存器
    
    EINT;   								//开总中断
    
//-------------------------------定时器开始工作-------------------------------
	CpuTimer0Regs.TCR.all = 0x4001; // Use write-only instruction to set TSS bit = 0//	M_StartCpuTimer0();												//主定时器工作

//----------------------------------主循环---------------------------------------------------------
	for(;;) 
	{				

//----------------------------------每次运行----------------------------------
		EeCtrl();							//eeprom控制
		Sci485Ctrl(); 
		Sci_canopenrx();					//系统输入读取


//-----------------------------一级循环5ms运行一次----------------------------
		if(M_ChkCounter(MAIN_LOOP.cnt1,DELAY_MAIN1)>=0)		
		{
			MAIN_LOOP.cnt1=0;				//清空计数器
			Input();						//系统输入读取
			Output();						//系统输出指示
		}

//-----------------------------二级循环10ms运行一次---------------------------
		if(M_ChkCounter(MAIN_LOOP.cnt2,DELAY_MAIN2)>=0)
		{
			MAIN_LOOP.cnt2=0;
			if(M_ChkFlag(SL_CODEOK)!=0)		//在eeprom正常的情况下
			{
			     Give();					//运行指令给定
			}
		}

//-----------------------------三级循环20ms运行一次---------------------------
		if(M_ChkCounter(MAIN_LOOP.cnt3,DELAY_MAIN3)>=0)
		{
			MAIN_LOOP.cnt3=0;
			SysCtrl();						//主控程序
		}

//-----------------------------四级循环50ms运行一次---------------------------
		if(M_ChkCounter(MAIN_LOOP.cnt4,DELAY_MAIN4)>=0)
		{
			MAIN_LOOP.cnt4=0;
		    Bank();							//显示控制
		}

//----------------------------五级循环100ms运行一次---------------------------
		if(M_ChkCounter(MAIN_LOOP.cnt5,DELAY_MAIN5)>=0)
		{					
			MAIN_LOOP.cnt5=0;
			if((M_ChkFlag(SL_POWERON)!=0)&&(M_ChkCounter(MAIN_LOOP.cnt_poweron,DELAY_POWERON)>0))
			{
				M_ClrFlag(SL_POWERON);
				ClrPdpint();						//PDPINT中断清空	
				EnPdpint();							//PDPINT使能中断
			}			
		}
//-----------------------------六级循环1000ms运行一次---------------------------
		if(M_ChkCounter(MAIN_LOOP.cnt6,DELAY_MAIN6)>=0)
		{
			MAIN_LOOP.cnt6=0;
//			M_NotFlag(SL_DISPLAY4);

			if(M_ChkFlag(SL_CODEOK)!=0)		
				RtRead();							//在eeprom正常的情况下//读取实时时钟,很耗时，要13ms.20090801,CPC				
										
			if(M_ChkCounter(MAIN_LOOP.cnt_senszfstdy,DELAY_SENSZFSTDY)>=0)
				M_SetFlag(SL_SENSZFSTDY);   		//延迟时间到后置零漂滤波稳定标志位


		}
	}  					
} 

//    	_OUT3_TEST |= 0x0001;   
//		*OUT3_ADDR = _OUT3_TEST;
//   	 _OUT3_TEST &= 0xFFFE;   
//		*OUT3_ADDR = _OUT3_TEST; 
//   	 _OUT3_TEST |= 0x0002;   
//		*OUT3_ADDR = _OUT3_TEST; 
//    	 _OUT3_TEST &= 0xFFFD;
//		*OUT3_ADDR = _OUT3_TEST; 
//		if((_OUT3_TEST & 0x0001)!=0)  _OUT3_TEST&=0xFFFE;
//		else      	       _OUT3_TEST|=0x0001; 
//		if((_OUT3_TEST & 0x0002)!=0)  _OUT3_TEST&=0xFFFD;
//     	else      	       _OUT3_TEST|=0x0002; 
/*********************************************************************************************************
** 函数名称: CpuTimer0Isr
** 功能描述: 主定时器周期中断(0.04ms)
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
interrupt void CpuTimer0Isr(void)
{
    Uint16 i;
    
    M_SetFlag(SL_PHASEA);            //测量CPU占有率,测量DSP板上T1端子
    *OUT3_ADDR = _OUT3_DATA;
		
	if(M_ChkFlag(SL_IN1_CBSTS)==0)   //在合主断前网压相序检测子函数 cap5-ubc   cap6-uab
	{
	  PhaseOrderChk();        
	} 
 	QepEncodPos(); 					


    CapQepCtrl();  		  			//测网压相位，网压频率，坐标正、反变换的角度
   	Ad8364Ctrl();             		//有传感器和互感器AD采样和转换

    NPR_CONTROL();            		//网侧变流器控制算法
    MPR_CONTROL();            		//机侧变流器控制算法

	et_relay_N();					//200us执行一次
	et_relay_M();					//200us执行一次

	testtheta = testtheta + 1;		//hoteltest20091109
	if(testtheta>=3721)	testtheta= -1879;//hoteltest20091109

//------------------监视网压跌落20091027atzy-----------------------------------
  if(M_ChkFlag(SL_CHARGEOK)!=0)
  {
//70%
	if((TRS_NGS_U.dflt2<=394) && (_BA_UAB2<65535) && (M_ChkFlag(SL_GRIDLOSE70)==0))			//ed=563V; *0.7=394V
	{
		_BA_UAB2 = _BA_UAB2+1;
		M_SetFlag(SL_DISPLAY6);		//灯指示
	}
	else if(_BA_UAB2!=0)	M_SetFlag(SL_GRIDLOSE70);

//50%
	if((TRS_NGS_U.dflt2<=282) && (_BA_UBC2<65535) && (M_ChkFlag(SL_GRIDLOSE50)==0))			//ed=563V; *0.5=282V
	{
		_BA_UBC2 = _BA_UBC2+1;
		M_SetFlag(SL_DISPLAY7);		//灯指示
	}
	else if(_BA_UBC2!=0)	M_SetFlag(SL_GRIDLOSE50);

//30%
	if((TRS_NGS_U.dflt2<=169) && (_BA_STAUABD<65535) && (M_ChkFlag(SL_GRIDLOSE30)==0))			//ed=563V; *0.3=169V
	{
		_BA_STAUABD = _BA_STAUABD+1;
	}
	else if(_BA_STAUABD!=0)	M_SetFlag(SL_GRIDLOSE30);

//15%
	if((TRS_NGS_U.dflt2<=84.5) && (_BA_STAUBCD<65535) && (M_ChkFlag(SL_GRIDLOSE15)==0))			//ed=563V; *0.15=84.5V
	{
		_BA_STAUBCD = _BA_STAUBCD+1;
	}
	else if(_BA_STAUBCD!=0)	M_SetFlag(SL_GRIDLOSE15);
  }


//---------------------------------分时脉冲计算--------------------------------
	MAIN_LOOP.pulse++;
	if(MAIN_LOOP.pulse>=10)		MAIN_LOOP.pulse=0;
	
//--20级分时脉冲
	switch(MAIN_LOOP.pulse)
	{

//--定时器累加,慢速AD,保护值计算,故障
		case 0:
		{
			Protect();			   //protect calculation		
			Scout();			   //故障保护
			break;
		}
		case 1:
		{	
			PwmDrive();	 		   //输出脉冲控制  				
		    CntCtrl(); 				//计数1ms计一个数，定时器中断周期为200us
			Datasave();				//0.4ms一次20091109
			break;
		}
		case 2:
		{
			Protect();			   //protect calculation		
			Scout();			   //故障保护
			break;
		}
		case 3:
		{	
			PwmDrive();			   //输出脉冲控制				
			RunCtrl();			   //给定积分
			Datasave();				//0.4ms一次20091109
			break;
		}
		case 4:
		{	
			Protect();			   //protect calculation	
			Scout();			   //故障保护
			break;
		}
		case 5:
		{	
			PwmDrive();			   //输出脉冲控制				
			Display();
			Datasave();				//0.4ms一次20091109
			break;
		}
		case 6:
		{	
			CntCtrl(); 	           //计数，1ms计一个数，定时器中断周期为200us
			Protect();			   //protect calculation	
			Scout();			   //故障保护
			break;
		}
		case 7:
		{
			PwmDrive();			   //输出脉冲控制				
			Datasave();				//0.4ms一次20091109
			break;
		}
		case 8:
		{	
			Protect();												//slow AD, protect calculation	
			Scout();												//故障保护
			break;
		}
		case 9:
		{	
			PwmDrive();			   //输出脉冲控制				
			RunCtrl();			   //给定积分
			Datasave();				//0.4ms一次20091109
			break;
		}
//--------------------------------------------------------------------------------------------------
		default:
			break;
	}

//--------------------DA转换------------------------------------------------------------------------
//--------NPR--------
//	zys1 = (int16)(PI_NPR_U.error 	    * 20.0)	+ 2048;
//	zys2 = (int16)(PI_NPR_U.out	        * 2.0) + 2048;
//	zys3 = (int16)(PI_NPR_Id.feedback   * 2.0) + 2048;
//	zys4 = (int16)(PI_NPR_Id.out        * 2.0) + 2048;

//	zys1 = (int16)(TRS_NPR_U.q 	        * 2.0)	+ 2048;
//	zys2 = (int16)(PI_NPR_Iq.reference	* 2.0) + 2048;
//	zys3 = (int16)(PI_NPR_Iq.feedback   * 2.0) + 2048;
//	zys4 = (int16)(PI_NPR_Iq.out        * 2.0) + 2048;

//--------MPR--------
//	zys1 = (int16)(TRS_MPR_U.d          * 5.0) + 2048;
//	zys2 = (int16)(TRS_MPR_I.dflt	    * 5.0) + 2048;
//	zys3 = (int16)(CAP4.omigaslp * MPR_Lr * DM_imrd * 5.0) + 2048;
//	zys4 = (int16)(TRS_MPR_U.q          * 5.0) + 2048;

//	zys1 = (int16)(TRS_MPR_U.d     	    * 2.0) + 2048;
//	zys2 = (int16)(PI_MPR_Id.reference	* 2.0) + 2048;
//	zys3 = (int16)(PI_MPR_Id.feedback   * 2.0) + 2048;
//	zys4 = (int16)(PI_MPR_Id.out        * 2.0) + 2048;

//	zys1 = (int16)(TRS_MPR_U.q     	    * 2.0) + 2048;
//	zys2 = (int16)(PI_MPR_Iq.reference	* 2.0) + 2048;
//	zys3 = (int16)(PI_MPR_Iq.feedback   * 2.0) + 2048;
//	zys4 = (int16)(PI_MPR_Iq.out        * 2.0) + 2048;

//--------Theta--------
//	zys1 = (int16)(CAP4.nprtrstheta     * 200.0) + 2048;   //NPR
//	zys2 = (int16)(QEPDATA.rotposdisp	* 200.0) + 2048;   //rotor
//	zys3 = (int16)(CAP4.mprtrstheta     * 200.0) + 2048;   //MPR
//	zys4 = (int16)(CAP4.mpratitheta     * 200.0) + 2048;   //MPR anti

//--------STAC_SYN TEST--------
	zys1 = (int16)(TRS_NGS_U.dflt2     * 2.0) + 2048;  //ed小滤波，监测网压跌落20091027atzy
	zys2 = (int16)(AD_OUT_STA_U.bc	   * 2.0) + 2048;   //
	zys3 = (int16)(AD_OUT_STAD_U.bc    * 20.0)+ 2048;   //
	zys4 = (int16)(CAP4.mprtrstheta    * 200.0) + 2048;    //
//--------显示滤波后转速-------
//    zys1 = (int16)(PRO.speedflt     * 1.0) + 2048;  //转速

	if		(zys1>4095)  	zys1 = 4095;
	else if	(zys1<0)		zys1 = 0;

	if		(zys2>4095)  	zys2 = 4095;
	else if	(zys2<0)		zys2 = 0;

	if		(zys3>4095)  	zys3 = 4095;
	else if	(zys3<0)		zys3 = 0;

	if		(zys4>4095)  	zys4 = 4095;
	else if	(zys4<0)		zys4 = 0;
		
	*DA_ADD0 =	zys1;
	*DA_ADD1 =	zys2;
	*DA_ADD2 =	zys3;
	*DA_ADD3 =	zys4;


//--应答中断20091109atzy
//	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;	

//canopen
//-----------------------------canopen运行一次----------------------------
	if(M_ChkCounter(MAIN_LOOP.canopen_tx,DELAY_CANOPENTX)>=0)    //13ms		
	{
		MAIN_LOOP.canopen_tx=0;				//清空计数器
		Sci_canopentx();
		M_SetFlag(SL_CANOPENTX);
//			if((_OUT3_TEST & 0x0001)!=0)  	_OUT3_TEST&=0xFFFE;
//			else      	       				_OUT3_TEST|=0x0001;	
//			*OUT3_ADDR = _OUT3_TEST;
	} 

	if(M_ChkFlag(SL_CANOPENTX)!=0)
	{		
	 	if(ScibRegs.SCIFFTX.bit.TXFFST <= 12)						
		{
			for(i=SCICANOPENTXNUM-2;i<SCICANOPENTXNUM;i++)
				ScibRegs.SCITXBUF=SCI_canopen.txb[i];
			M_ClrFlag(SL_CANOPENTX);
		}
	}
	else
	{
		if(ScibRegs.SCIFFTX.bit.TXFFST == 0)						//发送完成?
		{
			if(ScibRegs.SCICTL2.bit.TXEMPTY==1)	
			{
				ScibRegs.SCIFFTX.bit.TXFIFOXRESET=1;	// Re-enable TX FIFO operation
			}
		}
	}

    M_ClrFlag(SL_PHASEA);								//测量CPU占有率,测量DSP板上T1端子
    *OUT3_ADDR = _OUT3_DATA;

//--应答中断20091109atzy
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;	
}

/*********************************************************************************************************
** 函数名称: EPWM1_TZ1_Isr
** 功能描述: 功率保护中断
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
interrupt void EPWM1_TZ1_Isr (void)
{
//----------------------------------------------//处理程序

	Disepwmio_NPR();
	Disepwmio_MPR();
    DisPdpint();                
	M_SetFlag(SL_PDPINTA);
	_BA_EPGRID++;				//20100506临时修改 记录硬故障次数			
//----------------------------------------------//应答中断
	ClrPdpint();
	M_SetFlag(SL_DISPLAY3);    //LED3红灯亮
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP2;
}
/*********************************************************************************************************
** 函数名称: EPWM2_TZ2_Isr
** 功能描述: 功率保护中断
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
interrupt void EPWM2_TZ2_Isr (void)
{
//----------------------------------------------//处理程序
	Disepwmio_NPR();
	Disepwmio_MPR();
    DisPdpint();     
	M_SetFlag(SL_PDPINTB);
//----------------------------------------------//应答中断
	ClrPdpint();
	M_SetFlag(SL_DISPLAY3);    //LED3红灯亮
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP2;
}




/*********************************************************************************************************
** 函数名称: Protect
** 功能描述: 保护值计算
** 输　入: 	 
** 输  出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Protect(void)
{
	float temp;
	
//----------------------------并网电流瞬时值保护---------------------------
	if(abs(AD_OUT_NPR_I.a)>abs( AD_OUT_NPR_I.b))
	{
		if(abs( AD_OUT_NPR_I.a)>abs( AD_OUT_NPR_I.c))	temp=abs(AD_OUT_NPR_I.a);
		else										    temp=abs(AD_OUT_NPR_I.c);
	}
	else
	{
		if(abs( AD_OUT_NPR_I.b)>abs( AD_OUT_NPR_I.c))	temp=abs(AD_OUT_NPR_I.b);
		else										    temp=abs(AD_OUT_NPR_I.c);
	}

	PRO.NPR_iac = temp * 100 / NPR_IACN;			//并网电流瞬时最大值，基准电流的百分值

//----------------------------转子电流瞬时值保护---------------------------
	if(abs(AD_OUT_MPR_I.a)>abs(AD_OUT_MPR_I.b))
	{
		if(abs(AD_OUT_MPR_I.a)>abs(AD_OUT_MPR_I.c))		temp=abs(AD_OUT_MPR_I.a);
		else										temp=abs(AD_OUT_MPR_I.c);
	}
	else
	{
		if(abs(AD_OUT_MPR_I.b)>abs(AD_OUT_MPR_I.c))		temp=abs(AD_OUT_MPR_I.b);
		else										temp=abs(AD_OUT_MPR_I.c);
	}

	PRO.MPR_iac = temp * 100 / MPR_IACN;		//转子电鞔笾担嫉琪的百分值

//------------------------中间直流电压保护值计算-----------------------------

	PRO.udc  = AD_OUT_UDC;	                                 //单位V

//---------------------------网侧线电压有效值计算----------------------------
    PRO.NPR_uab= MEAN_DATA.uab1 * 1.110721;                      //网侧Uab有效值单位V rms=mean*PAI/(2*sqrt(2)) 
	PRO.NPR_ubc= MEAN_DATA.ubc1 * 1.110721;                      //网侧Ubc有效值单位V 1.110721=PAI * SQRT2 / 4
	PRO.GID_uab = MEAN_DATA.uab * 1.110721;        //20091026atzy 电网侧Uab有效值单位V 单位V  rms=mean*PAI/(2*sqrt(2)) ； 1.110721=PAI * SQRT2 / 4
    PRO.GID_ubc = MEAN_DATA.ubc * 1.110721;        //20091026atzy 电网侧Uab有效值单位V 单位V  rms=mean*PAI/(2*sqrt(2)) ； 1.110721=PAI * SQRT2 / 4

//---------------------------定子侧线电压有效值计算----------------------------
    PRO.STA_uab= MEAN_DATA.uab2 * 1.110721;                    //电侧Uab有效值单位V
	PRO.STA_ubc= MEAN_DATA.ubc2 * 1.110721;                    //电机侧Ubc有效值单位V

//------------------功率保护及显示值计算-----------------------------------------
//------------------定子侧dq变换定相角度值--------------------------------------------------------------------
	DIP_STA_I.sintheta = sin(CAP4.nprtrstheta);		//计算定子侧功率 dq变换角度
	DIP_STA_I.costheta = cos(CAP4.nprtrstheta);		//计算定子侧功率 dq变换角度
	DIP_STA_U.sintheta = DIP_STA_I.sintheta;
	DIP_STA_U.costheta = DIP_STA_I.costheta; 

//------------------定子侧电流dq值--------------------------------------------------------------------
   	DIP_STA_I.a     = AD_OUT_STA_I.a;
   	DIP_STA_I.b     = AD_OUT_STA_I.b;
   	DIP_STA_I.c     = AD_OUT_STA_I.c;
	Transform_3s_2s_2r(&DIP_STA_I);
	DataFilter(0.99,&DIP_STA_I.dflt,DIP_STA_I.d); 	//定子侧电流反馈值滤波,Ts=200us,fh=88Hz,滤掉开关频率次
	DataFilter(0.99,&DIP_STA_I.qflt,DIP_STA_I.q); 	//定子侧电流反馈值滤波， Ts=200us,fh=88Hz,滤掉开关频率次

//------------------定子侧电压dq值--------------------------------------------------------------------
   	DIP_STA_U.a     = AD_OUT_NGS_U.a;    //采用网压作为定子电压，因为定子V-LEM反馈电压经过了大常数滤波
   	DIP_STA_U.b     = AD_OUT_NGS_U.b;
   	DIP_STA_U.c     = AD_OUT_NGS_U.c;
	Transform_3s_2s_2r(&DIP_STA_U);
	DataFilter(0.99,&DIP_STA_U.dflt,DIP_STA_U.d); 	//定子侧电压反馈值滤波， Ts=200us,fh=88Hz,滤掉开关频率次
	DataFilter(0.99,&DIP_STA_U.qflt,DIP_STA_U.q); 	//定子侧电压反馈值滤波， Ts=200us,fh=88Hz,滤掉开关频率次

//------------------------定子相电压有效值显示值计算(为功率计算)--------------------------------------------------
    PRO.sta_uar = MEAN_DATA.ua1 * 1.110721;         //单位V 采用网压作为定子电压，因为定子V-LEM反馈电压经过了大常数滤波
	PRO.sta_ubr = MEAN_DATA.ub1 * 1.110721;
	PRO.sta_ucr = MEAN_DATA.uc1 * 1.110721;

//------------------------定子相电流有效值显示值计算(为功率计算)--------------------------------------------------
    PRO.sta_iar = MEAN_DATA.ia3 * 1.110721;			//单位A
	PRO.sta_ibr = MEAN_DATA.ib3 * 1.110721;
	PRO.sta_icr = MEAN_DATA.ic3 * 1.110721;

//------------------定子侧有功和无功计算值--------------------------------------------------------------------
    PRO.Psactive   = 1.5 * SQRT3 * (DIP_STA_U.dflt * DIP_STA_I.dflt + DIP_STA_U.qflt * DIP_STA_I.qflt); 
    PRO.Psreactive = 1.5 * SQRT3 * (DIP_STA_U.qflt * DIP_STA_I.dflt - DIP_STA_U.dflt * DIP_STA_I.qflt); 
	PRO.Ps		   = SQRT3 * (PRO.sta_uar * PRO.sta_iar + PRO.sta_ubr * PRO.sta_ibr + PRO.sta_ucr * PRO.sta_icr);//20091007

//------------------------网侧并网电流有效值显示值计算----------------------------------------------
    PRO.npr_iar = MEAN_DATA.ia1 * 1.110721;     			 //1.110721=PAI * SQRT2 / 4  	//单位A
	PRO.npr_ibr = MEAN_DATA.ib1 * 1.110721;
	PRO.npr_icr = MEAN_DATA.ic1 * 1.110721;

//------------------------网侧相电压有效值显示值计算----------------------------------------------
    PRO.npr_uar = MEAN_DATA.ua1 * 1.110721;      			//1.110721=PAI * SQRT2 / 4  //单位A
	PRO.npr_ubr = MEAN_DATA.ub1 * 1.110721;
	PRO.npr_ucr = MEAN_DATA.uc1 * 1.110721;

//------------------网侧有功和无功计算值--------------------------------------------------------------------
 	PRO.Pnactive   = Pnactive;
	PRO.Pnreactive = Pnreactive;
    PRO.Pn         = PRO.npr_iar * PRO.npr_uar + PRO.npr_ibr * PRO.npr_ubr + PRO.npr_icr * PRO.npr_ucr;

//------------------------------定子侧和网侧总并网功率显示-----------------------------------------------------
	PRO.Pgactive   = PRO.Psactive   + PRO.Pnactive;
	PRO.Pgreactive = PRO.Psreactive + PRO.Pnreactive;

    if(CAP4.omigaslp >= 0)	 PRO.Pg = PRO.Ps - PRO.Pn;
	else 					 PRO.Pg = PRO.Ps + PRO.Pn;

//---------------------------网侧和机侧电感温度值----------------------------
    PRO.NPR_TLOV= AMUX.Lac_temp;                    //网侧电感温度
	PRO.MPR_TLOV= AMUX.Ldudt_temp;                  //机侧电感温度

//--------------------------- 定子侧线电流保护值----------------------------
	PRO.STA_iac = MEAN_DATA.iac3 * 1.110721  * 100/ STA_IACN;
	PRO.STA_iba = MEAN_DATA.iba3 * 1.110721  * 100/ STA_IACN;

//---------------------------转速保护值计算----------------------------------
	PRO.speed   = 9.5492966 * QEPDATA.omigamec;		//单位：转/分；n=60*w/2pai=*w
//	PRO.speed   = 1200;		//单位：转/分；n=60*w/2pai=*w

//	if(_stdby05!=0)   	DataFilter(_stdby01,&PRO.speedflt,PRO.speed); 	//主控转速反馈值滤波， Ts=200us,Tr=248ms 20090815
//	else   				PRO.speedflt= PRO.speed;         //20090816test

//	DataFilter(0.8,&PRO.speedflt,PRO.speed); 				//主控转速反馈值滤波， Ts=200us,Tr=5ms 20091021atzy之前123ms滤波太大了
	DataFilter(0.45,&PRO.speedflt,PRO.speed); 				//主控转速反馈值滤波， Ts=200us,Tr=1ms 20111116
	
	if(PRO.speedflt>700)		M_SetFlag(SL_SPEED_HIGH);	//进入高转速区   20091021atzy 650改为700，加大滞环
    else if(PRO.speedflt<600)	M_ClrFlag(SL_SPEED_HIGH);   //650r/min测周法PRD=845.测频法POSLAT=887.

    if((PRO.speedflt > _SC_MSPEED1) && (PRO.speedflt < _SC_MSPEED2)) 	 M_SetFlag(SL_SPEED_IN_RANGE);  //20091021atzy	
	else													 M_ClrFlag(SL_SPEED_IN_RANGE);

} 
/*********************************************************************************************************
** 函数名称: Scout
** 功能描述: 系统故障检测及处理程序
** 输入：
** 输出:        
**  释: 
**-------------------------------------------------------------------------------------------------------
** 作者: 
** 日期: 
**-------------------------------------------------------------------------------------------------------
** 修改者：
** 日期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Scout(void)
{

  float temp_pgactive,temp_pgreactive;
//------------------根据子程序上报的信息，判断是TAB_MSG中哪一个-------------------------------------
	if(M_ChkFlag(SL_POWERON)==0)										//若电完成故障则检测故障
	{	
//---------------------------------TZ1中断保护------------------------------------------------------
		if(M_ChkFlag(SL_PDPINTA)!=0)	
		{	
			if(M_ChkFlag(SL_IN2_IOVA1)!=0)			M_SetFlag(SL_HIA1);		//网侧A相SKiiP故障
			else if(M_ChkFlag(SL_IN2_IOVB1)!=0)	   	M_SetFlag(SL_HIB1);		//网侧B相SKiiP故障
		    else if(M_ChkFlag(SL_IN2_IOVC1)!=0)	   	M_SetFlag(SL_HIC1);		//网侧C相SKiiP故障
			else if(M_ChkFlag(SL_IN3_VDCOV)!=0)   	M_SetFlag(SL_HUDCOV);	//直流侧过压故障
			else if(M_ChkFlag(SL_IN3_NPRIOV)!=0)  	M_SetFlag(SL_HIACOV1);	//网侧硬件过流故障
			else if(M_ChkFlag(SL_PDPASERIES)!=0)	M_ClrFlag(SL_PDPINTA);	//再允许启动变流器	20091107atzy						
			else									M_SetFlag(SL_PDPASERIES);//发生硬件故障,CPLD没有存到故障
			
			M_SetFlag(SL_ERRDATASAVE);	//触发外部RAM数据转存20091109atzy
		}
		else
		{
			M_ClrFlag(SL_HIA1);   	    									//清故障标志
			M_ClrFlag(SL_HIB1);
			M_ClrFlag(SL_HIC1);
			M_ClrFlag(SL_HUDCOV);
			M_ClrFlag(SL_HIACOV1);  			
			M_ClrFlag(SL_PDPASERIES);  			
		}

//---------------------------------TZ2中断保护-------------------------------------------------------
		if(M_ChkFlag(SL_PDPINTB)!=0)	
		{	
			if(M_ChkFlag(SL_IN2_IOVA2)!=0)			M_SetFlag(SL_HIA2);			//电机侧A相SKiiP故障
			else if(M_ChkFlag(SL_IN2_IOVB2)!=0)	    M_SetFlag(SL_HIB2);			//电机侧B相SKiiP故障
		    else if(M_ChkFlag(SL_IN2_IOVC2)!=0)	    M_SetFlag(SL_HIC2);			//电机侧C相SKiiP故障
		    else if(M_ChkFlag(SL_IN3_MPRIOV)!=0)    M_SetFlag(SL_HIACOV2);		//电机侧硬件过流故障
			else if(M_ChkFlag(SL_PDPBSERIES)!=0)	M_ClrFlag(SL_PDPINTB);		//允许启动变流器	20091107atzy						
			else									M_SetFlag(SL_PDPBSERIES);	//发生硬件故障,CPLD没有存到故障

//			M_SetFlag(SL_ERRDATASAVE);	//触发外部RAM数据转存20091109atzy
		}
		else
		{
			M_ClrFlag(SL_HIA2);    											//清故障标志
			M_ClrFlag(SL_HIB2);	   		
			M_ClrFlag(SL_HIC2);	    
			M_ClrFlag(SL_HIACOV2);
			M_ClrFlag(SL_PDPBSERIES);	
		}

//---------------------------------E-STOP保护-------------------------------------------------------
		if(M_ChkFlag(SL_IN1_EXESTOP)!=0)								//外部急停故障  操作板信号，风场没用								
		{
			if(M_ChkCounter(MAIN_LOOP.cnt_estop,DELAY_ESTOP)>=0)   M_SetFlag(SL_ESTOP);		//紧急停止延迟时间到？
			else M_ClrFlag(SL_ESTOP);									//清标志位
		}
		else 
		{
			MAIN_LOOP.cnt_estop=0;										//清定时器
			M_ClrFlag(SL_ESTOP);
		}  

//---------------------------------外部硬件故障保护-------------------------------------------------
		if((M_ChkFlag(SL_IN1_EXFAULTOK)==0)||(M_ChkFlag(SL_IN1_MIANFAN)==0))	//外部故障动作或者功率缁收� new							
		{
			if(M_ChkCounter(MAIN_LOOP.cnt_exfault,DELAY_EXFAULT)>=0)   M_SetFlag(SL_EXFAIL);	//外部硬件故障延迟时间到？
			else M_ClrFlag(SL_EXFAIL);							     //清标志位
		}
		else 
		{
			MAIN_LOOP.cnt_exfault=0;                               	 //清定时器
			M_ClrFlag(SL_EXFAIL);
		}	

//-----------------------CANOPENOVER CAN通讯故障-----------------------------------------------------------
	    if(_CANOPER!=0) 											//上电且选择CAN控制才进行通讯故障
		{
			if(M_ChkCounter(MAIN_LOOP.cnt_canfault,DELAY_CANFAULT)>=0)		//10s  20090816
		  	{
				if(M_ChkCounter(SCI_canopen.cnt_heartbeat,DELAY_CANOPENOVER)>0)
				{                                   						
			   		 M_SetFlag(SL_CANOPENOVER);
			   		 SCI_canopen.rx_controlword=0;
					 SCI_canopen.rx_torque=0;
					 SCI_canopen.rx_angle=0;
				}
				else M_ClrFlag(SL_CANOPENOVER); 
		  	}   
        }
		else  
		{
			MAIN_LOOP.cnt_canfault=0;
			M_ClrFlag(SL_CANOPENOVER);
		}

//-----------------------CBTRIP保护(主断过流脱扣)---------------------------------------

	    if(M_ChkFlag(SL_IN1_CBRESET)==0)							   	//1=主控发来要求主断复位信号
		{		
			if(M_ChkFlag(SL_IN3_CBTRIP)!=0)								//主断过流脱扣故障	090816	
	    	{
	    		if(M_ChkCounter(MAIN_LOOP.cnt_cbtp,DELAY_CBTP)>=0)	   	//主断脱扣最小时间到?
					M_SetFlag(SL_CBTRIP);	
				else 
					M_ClrFlag(SL_CBTRIP);								//清标志位	    
	    	}
			else   	MAIN_LOOP.cnt_cbtp=0;		
		}
		else if(M_ChkFlag(SL_IN3_CBTRIP)==0)
		{
			M_ClrFlag(SL_CBTRIP);								 		//清标志位
			MAIN_LOOP.cnt_cbtp=0;
		}
		else	MAIN_LOOP.cnt_cbtp=0;
	    	    
//-----------------------主断合闸/分闸/意外断开故障---------------------------------------
	    if((M_ChkFlag(CL_CB)!=0 && M_ChkFlag(SL_IN1_CBSTS)==0)||(M_ChkFlag(CL_CBENGSTRG)==0 && M_ChkFlag(SL_IN1_CBSTS)!=0)||(M_ChkFlag(SL_IN1_CBSTS)==0 && M_ChkFlag(CL_CBENGSTRG)!=0 && M_ChkFlag(SL_CBCLOSED)!=0 && M_ChkFlag(SL_IN3_CBTRIP)==0))	     //							
     	{
			if(M_ChkCounter(MAIN_LOOP.cnt_cberror,DELAY_CBERROR)>=0)	M_SetFlag(SL_ERROR_CB);	  //2s  
			else M_ClrFlag(SL_ERROR_CB);								 //清标志位
		}
		else 
		{
			MAIN_LOOP.cnt_cberror=0;                                    //清定时器
			M_ClrFlag(SL_ERROR_CB);
		} 
		
//-----------------------主接触器闭合/断开故障---------------------------------------
	    if((M_ChkFlag(CL_MAINK)!=0 && M_ChkFlag(SL_IN1_MIANK)==0)||(M_ChkFlag(CL_MAINK)==0 && M_ChkFlag(SL_IN1_MIANK)!=0))	     //							
     	{
			if(M_ChkCounter(MAIN_LOOP.cnt_mainkerror,DELAY_MAINKERROR)>=0)	M_SetFlag(SL_ERROR_MAINK);	
			else M_ClrFlag(SL_ERROR_MAINK);								 //清标志位
		}
		else 
		{
			MAIN_LOOP.cnt_mainkerror=0;                                    //清定时器
			M_ClrFlag(SL_ERROR_MAINK);
		}  
		
//-----------------------主滤波器闭合/断开故障---------------------------------------
	    if((M_ChkFlag(CL_MAINK)!=0 && M_ChkFlag(SL_IN1_MIANFILTER)==0)||(M_ChkFlag(CL_MAINK)==0 && M_ChkFlag(SL_IN1_MIANFILTER)!=0))	     //							
     	{
			if(M_ChkCounter(MAIN_LOOP.cnt_mainferror,DELAY_MAINFERROR)>=0)	M_SetFlag(SL_ERROR_MAINF);	
			else M_ClrFlag(SL_ERROR_MAINF);								 //清标志位
		}
		else 
		{
			MAIN_LOOP.cnt_mainferror=0;                                    //清定时器
			M_ClrFlag(SL_ERROR_MAINF);
		}   

//-----------------------定子接触器闭合/断开故障---------------------------------------
	    if((M_ChkFlag(CL_STATORK)!=0 && M_ChkFlag(SL_IN1_STATORK)==0)||(M_ChkFlag(CL_STATORK)==0 && M_ChkFlag(SL_IN1_STATORK)!=0))	     //							
     	{
			if(M_ChkCounter(MAIN_LOOP.cnt_stacerror,DELAY_STACERROR)>=0)	M_SetFlag(SL_ERROR_STAC);	
			else M_ClrFlag(SL_ERROR_STAC);								 //清标志位
		}
		else 
		{
			MAIN_LOOP.cnt_stacerror=0;                                    //清定时器
			M_ClrFlag(SL_ERROR_STAC);
		}   

//----------------------预充电故障--------------------------------------- 
	    if((M_ChkFlag(CL_PRE)!=0)&&(M_ChkFlag(CL_MAINK)==0))	     //							
     	{
			if(M_ChkCounter(MAIN_LOOP.cnt_preerror,DELAY_PREERROR)>=0)	M_SetFlag(SL_ERROR_PRE);  //15s	
			else M_ClrFlag(SL_ERROR_PRE);								 //清标志位
		}
		else 
		{
			MAIN_LOOP.cnt_preerror=0;                                    //清定时器
			M_ClrFlag(SL_ERROR_PRE);
		}  
 
//-----------------------ENCODFAULT保护(Qep检测,编码器故障)-----------------------------------------
	    if(((M_ChkFlag(SL_QEPPCO)!=0)||(M_ChkFlag(SL_QCAPDISTURB)!=0)||(M_ChkFlag(SL_QEPZDISTRUB)!=0))&&(M_ChkFlag(SL_OCS_EIN)!=0)&&(M_ChkFlag(SL_QCAPSPDIN)!=0))	//QEP上溢或者QCAP、QEP Z信号受干扰故障  20090804于cpc							
//	    if((M_ChkFlag(SL_QEPPCO)!=0)||(M_ChkFlag(SL_QCAPDISTURB)!=0)||(M_ChkFlag(SL_QEPZDISTRUB)!=0))	//QEP上溢或者QCAP、QEP Z信号受干扰故障  20090804于cpc	//201105atzuoyun						
     	{
			M_SetFlag(SL_ENCODFAULT);				     			    //置标志位
		}
		else 
		{
			M_ClrFlag(SL_ENCODFAULT);									//清标志位
		} 

//-----------------------电机工作转速范围判断-----------------------------------------------------
	    if(M_ChkFlag(SL_CHARGEOK)!=0)									//预充电OK后才进行转速超出范围判断 20090815
		{
			if((M_ChkFlag(SL_MSPOUT)==0)&& ((PRO.speedflt<_SC_MSPEED1)||(PRO.speedflt>_SC_MSPEED2)))  //加个延时判断20090817
			{
				if(M_ChkCounter(MAIN_LOOP.cnt_speedout,DELAY_SPEEDOUT)>=0)	M_SetFlag(SL_MSPOUT);	//50ms 20091022atzy									//置超出转速范П曛疚�
			}
			else  MAIN_LOOP.cnt_speedout=0;
		}
		else 
		{
			M_ClrFlag(SL_MSPOUT);
			MAIN_LOOP.cnt_speedout=0;
		}
/*
//-----------------------网侧变流器软件过流判断-----------------------------------------------------
		if((M_ChkFlag(SL_SIAC1)==0)&&(PRO.NPR_iac>_SC_IACOV1))
		{
			M_SetFlag(SL_SIAC1);									//置软件过流标志位
		}
		else if((M_ChkFlag(SL_SIAC1)!=0)&&(PRO.NPR_iac<(_SC_IACOV1-SC_IAC_HW))) 
		{
			M_ClrFlag(SL_SIAC1);									//清软件过流标志位
		}
 

//------------------------电机侧变流器软件过流判断--------------------------------------------------
		if((M_ChkFlag(SL_SIAC2)==0)&&(PRO.MPR_iac>_SC_IACOV2))
		{
			M_SetFlag(SL_SIAC2);									//置软件过流标志位
		}
		else if((M_ChkFlag(SL_SIAC2)!=0)&&(PRO.MPR_iac<(_SC_IACOV2-SC_IAC_HW))) 
		{
			M_ClrFlag(SL_SIAC2);									//清软件过流标志位
		}
*/
//-------------------------中间直流电压软件欠压判断------有问题！-------------------------------------------
	    if((M_ChkFlag(SL_CHARGEOK)!=0)&&(M_ChkFlag(SL_NPR_PWMOUT)!=0))//预充电完成后才进星费古卸�
		{
			if((M_ChkFlag(SL_SUDCLV)==0)&&(PRO.udc<_SC_UDCLV))	//现场暂时改为0了
			{
				M_SetFlag(SL_SUDCLV);									//之前没有欠压，此刻检测出欠，置欠压标志
			}	
			else if((M_ChkFlag(SL_SUDCLV)!=0)&&(PRO.udc>=(_SC_UDCLV+SC_UDC_HW)))	
			{
				M_ClrFlag(SL_SUDCLV);									//之前欠压，此刻检测出超过(欠压值加回差)，清欠压标志
			}		
		}
		else	M_ClrFlag(SL_SUDCLV);	//20100511		

//------------------------中间直流电压软件过压判断--------------------------------------------------
		if((M_ChkFlag(SL_SUDCOV)==0)&&(PRO.udc>_SC_UDCOV))	                   
		{
			M_SetFlag(SL_SUDCOV);	 	//
//			M_SetFlag(SL_ERRDATASAVE);	//触发外部RAM数据转存20091109atzy
		}
		else if((M_ChkFlag(SL_SUDCOV)!=0)&&(PRO.udc<=(_SC_UDCOV-SC_UDC_HW)))   M_ClrFlag(SL_SUDCOV);  //之前过压，此刻检测低于过压值减回差	

//------------------------中间直流电压软件过压判断(电网电压跌落) --------------------------------------------------
//		if((M_ChkFlag(SL_SUDCOVH)==0)&&(PRO.udc>_SC_UDCOVH))	                   M_SetFlag(SL_SUDCOVH);	 //之前没有过压，此刻检测龉梗们费贡曛�

//		else if((M_ChkFlag(SL_SUDCOVH)!=0)&&(PRO.udc<=(_SC_UDCOVH-SC_UDC_HW)))     M_ClrFlag(SL_SUDCOVH);  //之前过压，此刻检测低于过压值减回差	

 
//-----------------------网压欠压软件判断(线压有效值)-----------------------------------------------
	   if(M_ChkCounter(MAIN_LOOP.cnt_uaclv1,DELAY_UACLV1)>0)	//10s 上电延时判断网侧软欠压故障
	   {
		if(M_ChkFlag(SL_UACLV1)==0)
		{
			if((PRO.GID_uab<_SC_UACLV1)||(PRO.GID_ubc<_SC_UACLV1))	M_SetFlag(SL_UACLV1);  //20091026atzy 两个线电压任意一个不达要求,置欠标志
		}
		else 
		{
			if((PRO.GID_uab>=(_SC_UACLV1+SC_UAC_HW))&&(PRO.GID_ubc>=(_SC_UACLV1+SC_UAC_HW)))	M_ClrFlag(SL_UACLV1); //20091026atzy 两个线电压均回复到正常后清欠贡曛�	
		}
	   }					
//----------------------网压过压软件判断(线压有效值)------------------------------------------------
		if(M_ChkFlag(SL_UACOV1)==0)
		{
			if((PRO.NPR_uab>_SC_UACOV1)||(PRO.NPR_ubc>_SC_UACOV1))	 M_SetFlag(SL_UACOV1);

		}
		else
 		{
   			if((PRO.NPR_uab<=(_SC_UACOV1-SC_UAC_HW))&&(PRO.NPR_ubc<=(_SC_UACOV1-SC_UAC_HW)))	M_ClrFlag(SL_UACOV1);

		}

/*
//-----------------------定子侧网压欠压软件判断(线压有效值)-----------------------------------------------
		if(M_ChkFlag(SL_UACLV2)==0)
		{
			if((PRO.STA_uab<_SC_UACLV2)||(PRO.STA_ubc<_SC_UACLV2))	 M_SetFlag(SL_UACLV2);	  //两个线电压任意一个不达要求,置欠压标志

		}
		else 
		{
			if((PRO.STA_uab>=(_SC_UACLV2+SC_UAC_HW))&&(PRO.STA_ubc>=(_SC_UACLV2+SC_UAC_HW)))  M_ClrFlag(SL_UACLV2);		//两个线电压均回复到正常后清欠压标志

		}
						
//----------------------定子侧网压过压软件判断(线压有效值)------------------------------------------------
		if(M_ChkFlag(SL_UACOV2)==0)
		{
			if((PRO.STA_uab>_SC_UACOV2)||(PRO.STA_ubc>_SC_UACOV2))	 M_SetFlag(SL_UACOV2);

		}
		else
 		{
   			if((PRO.STA_uab<=(_SC_UACOV2-SC_UAC_HW))&&(PRO.STA_ubc<=(_SC_UACOV2-SC_UAC_HW)))   M_ClrFlag(SL_UACOV2);

		}
*/ 
//------------------------电网频率故障--------------------------------------------------------------
		if(M_ChkFlag(SL_IN1_CBSTS)!=0)     									//主断闭合再判断网频故障 20090816
		{
			if(M_ChkCounter(MAIN_LOOP.cnt_cbfreq,DELAY_CBFREQ)>=0)      	//1s
			{
				if(M_ChkFlag(SL_GRDFQE)!=0)									//10个网压周期(200ms),则置频率错误标志
				{
					if(M_ChkCounter(MAIN_LOOP.cnt_freq,DELAY_FREQ)>=0)	 M_SetFlag(SL_FE1);	
				}	
				else
				{
					M_ClrFlag(SL_FE1);									   	//否则清频率错误标志
					MAIN_LOOP.cnt_freq=0;                                  	//计时器每1ms加1
				}
			}
		}
		else   	
		{
			MAIN_LOOP.cnt_cbfreq=0;	
			M_ClrFlag(SL_FE1);
		}
	
//-----------------------定子电流过流判断-----------------------------------------------------------
		if((M_ChkFlag(SL_SIOVST)==0)&&((PRO.STA_iac>_SC_IACOVST)||(PRO.STA_iba>_SC_IACOVST)))
		{
			M_SetFlag(SL_SIOVST);									//置定子电流过流标志位
		}
		else if((M_ChkFlag(SL_SIOVST)!=0)&&(PRO.STA_iac<(_SC_IACOVST-SC_IAC_HW))&&(PRO.STA_iba<(_SC_IACOVST-SC_IAC_HW))) 
		{
			M_ClrFlag(SL_SIOVST);									//清定子电流过流标志位
		}

//-----------------------中间电压不稳定-------------------------------------------------------------
        if((M_ChkFlag(SL_STEADYFB)!=0)&&(M_ChkFlag(SL_NPR_PWMOUT)!=0))
		{
		   if((AD_OUT_UDC<(_URF - 70))||(AD_OUT_UDC>(_URF + 70)))  //中间电压超出范围后置故障标志位 new
		   {
		       if(M_ChkCounter(MAIN_LOOP.cnt_steadyfb,DELAY_UDCWAVE)>0)
			   {                                   //DELAY_STEADYFB，cnt_steadyfb与sysctrl里面的是同一个
			   	   M_SetFlag(SL_UDCWAVE);  
			   }	   
		   }
		   else
	   	   {
	      		   MAIN_LOOP.cnt_steadyfb=0;
		  		   M_ClrFlag(SL_UDCWAVE);
	   	   }
		}
		else   M_ClrFlag(SL_UDCWAVE);

//-----------------------软件温度反馈SKIIP超温故障判断----------------------------------------------
		if(AMUX.NPR_tempa > AMUX.NPR_tempb)	
		{	if(AMUX.NPR_tempa > AMUX.NPR_tempc)	    AMUX.NPR_skiiptemp = AMUX.NPR_tempa;
			else									AMUX.NPR_skiiptemp = AMUX.NPR_tempc;
		}	
		else
		{   if(AMUX.NPR_tempb > AMUX.NPR_tempc)	    AMUX.NPR_skiiptemp = AMUX.NPR_tempb;
			else									AMUX.NPR_skiiptemp = AMUX.NPR_tempc;
		}
		if(AMUX.MPR_tempa > AMUX.MPR_tempb)
		{	if(AMUX.MPR_tempa > AMUX.MPR_tempc)	    AMUX.MPR_skiiptemp = AMUX.MPR_tempa;
			else								    AMUX.MPR_skiiptemp = AMUX.MPR_tempc;
		}
		else
		{	if(AMUX.MPR_tempb > AMUX.MPR_tempc)	    AMUX.MPR_skiiptemp = AMUX.MPR_tempb;
			else									AMUX.MPR_skiiptemp = AMUX.MPR_tempc;
		}   
   	 	if(AMUX.NPR_skiiptemp > AMUX.MPR_skiiptemp)	AMUX.skiiptempmax = AMUX.NPR_skiiptemp;    
		else                          				AMUX.skiiptempmax = AMUX.MPR_skiiptemp; 

		if((M_ChkFlag(SL_SKTOV)==0)&&(AMUX.skiiptempmax > 100.0))
		{
			M_SetFlag(SL_SKTOV);									//置SKIIP超温故障标志位
		}
		else if((M_ChkFlag(SL_SKTOV)!=0)&&(AMUX.skiiptempmax < 90.0)) 
		{
			M_ClrFlag(SL_SKTOV);									//清SKIIP超温故障标志位
		}
        
//----------------------网侧SKIIP超温故障----------------------------------------------------------			
		if((M_ChkFlag(SL_IN2_TAOV)!=0)&&(M_ChkFlag(SL_TAOVONCE)==0))
		{	
			M_SetFlag(SL_TAOVONCE);           		//置温度检测抗干扰标志位
		}
		else if((M_ChkFlag(SL_IN2_TAOV)!=0)&&(M_ChkFlag(SL_TAOVONCE)!=0))
		{
			M_SetFlag(SL_TAOV); 					//置超温标志位
		}		
       	else
		{
			M_ClrFlag(SL_TAOVONCE); 				//清超温检测抗干扰标志位
			M_ClrFlag(SL_TAOV);						//清超温标志位
		}

//--------------------电机侧SKIIP超温故障-----------------------------------------------------------	
		if((M_ChkFlag(SL_IN2_TBOV)!=0)&&(M_ChkFlag(SL_TBOVONCE)==0))
		{	
			M_SetFlag(SL_TBOVONCE);            		//置温度检测抗干扰标志位
		}
		else if((M_ChkFlag(SL_IN2_TBOV)!=0)&&(M_ChkFlag(SL_TBOVONCE)!=0))
		{
		   	M_SetFlag(SL_TBOV); 					//置超温标志位
		}		
       	else
		{
			M_ClrFlag(SL_TBOVONCE); 				//清超温检测抗干扰标志位
			M_ClrFlag(SL_TBOV);						//清超温标志位
		}


//----------------------网侧电感超温故障----------------------------------------------------------			
		if((M_ChkFlag(SL_NPR_TLOV)==0)&&(PRO.NPR_TLOV>_SC_NPR_TLOV))
		{
			M_SetFlag(SL_NPR_TLOV);									//置网侧电感超温标志位
		}
		else if((M_ChkFlag(SL_NPR_TLOV)!=0)&&(PRO.NPR_TLOV<(_SC_NPR_TLOV - 10))) 
		{
			M_ClrFlag(SL_NPR_TLOV);									//清网侧电感超温标志位
		} 


//--------------------电机侧电感超温故障-----------------------------------------------------------	
		if((M_ChkFlag(SL_MPR_TLOV)==0)&&(PRO.MPR_TLOV>_SC_MPR_TLOV))
		{
			M_SetFlag(SL_MPR_TLOV);									//置机侧电感超温标志位
		}
		else if((M_ChkFlag(SL_MPR_TLOV)!=0)&&(PRO.MPR_TLOV<(_SC_MPR_TLOV - 10))) 
		{
			M_ClrFlag(SL_MPR_TLOV);									//清机侧电感超温标志位
		} 

//-----------------------变流器有功功率过载判断-----------------------------------------------------------
		temp_pgactive = abs(PRO.Pgactive);							//20090816
		temp_pgactive = temp_pgactive * 0.001;
		if((M_ChkFlag(SL_PGOV)==0)&&(temp_pgactive>_SC_PGOV))
		{
			M_SetFlag(SL_PGOV_COM);
			if(M_ChkCounter(MAIN_LOOP.cnt_pgovload,DELAY_PGOVLOAD)>0)  	//10s
				M_SetFlag(SL_PGOV);										//置变流器有功功率过载标志位
		}
		else if((M_ChkFlag(SL_PGOV)!=0)&&(temp_pgactive<(_SC_PGOV-SC_POWOROV_HW))) 
		{
			M_ClrFlag(SL_PGOV);
			M_ClrFlag(SL_PGOV_COM);										//清变流器有功功率过载标志位
			MAIN_LOOP.cnt_pgovload=0;
		}
		else  MAIN_LOOP.cnt_pgovload=0;

//-----------------------变流器无功功率过载判断-----------------------------------------------------------
        temp_pgreactive = abs(PRO.Pgreactive);							//20090816
        temp_pgreactive = temp_pgreactive * 0.001;
		if((M_ChkFlag(SL_QGOV)==0)&&(temp_pgreactive>_SC_QGOV))
		{
			M_SetFlag(SL_QGOV_COM);										//20091007
			if(M_ChkCounter(MAIN_LOOP.cnt_qgovload,DELAY_QGOVLOAD)>0)  	//10s
				M_SetFlag(SL_QGOV);										//置变流器无功功率过载标志位
		}
		else if((M_ChkFlag(SL_QGOV)!=0)&&(temp_pgreactive<(_SC_QGOV-SC_POWOROV_HW))) 
		{
			M_ClrFlag(SL_QGOV);	
			M_ClrFlag(SL_QGOV_COM);										//清变流器无功功率过载标志位
			MAIN_LOOP.cnt_qgovload=0;
		} 
		else MAIN_LOOP.cnt_qgovload=0;
 
//--------------------------------得到TAB_MSG中的故障序号-------------------------------------------
		_MSG_SCOUT2 = MSG_NONE;												//先将MSG清零
		
		if(M_ChkFlag(SL_CODEOK)==0)				_MSG_SCOUT2=MSG_CODEOK;      //1=功能码未校验完毕
		
		else if(M_ChkFlag(SL_EE_FAIL)!=0) 		_MSG_SCOUT2=MSG_EE_FAIL;     //2=EEPROM故障

		else if(M_ChkFlag(SL_CANOPENOVER)!=0) 	_MSG_SCOUT2=MSG_CAN_FAIL;    //3=CAN通讯故障

		else if(M_ChkFlag(SL_PHORDE)!=0) 		_MSG_SCOUT2=MSG_PHORDE;      //4=相序错误

		else if(M_ChkFlag(SL_ENCODFAULT)!=0) 	_MSG_SCOUT2=MSG_ENCODFAULT;  //5=编码器故障

		else if(M_ChkFlag(SL_ESTOP)!=0) 	    _MSG_SCOUT2=MSG_ESTOP;       //6=紧急停机故障

		else if(M_ChkFlag(SL_CBTRIP)!=0) 	    _MSG_SCOUT2=MSG_CBTRIP;      //7=主断脱扣故障

		else if(M_ChkFlag(SL_EXFAIL)!=0) 	 	_MSG_SCOUT2=MSG_EXFAULT;     //8=外部硬件故障

		else if(M_ChkFlag(SL_ERROR_CB)!=0) 	 	_MSG_SCOUT2=MSG_CBERROR;     //9=鞫虾险⒐收�

		else if(M_ChkFlag(SL_ERROR_PRE)!=0) 	_MSG_SCOUT2=MSG_PREERROR;    //10=预充电故障

		else if(M_ChkFlag(SL_ERROR_MAINK)!=0) 	_MSG_SCOUT2=MSG_MAINKERROR;  //11=主接触器闭合故障

		else if(M_ChkFlag(SL_ERROR_MAINF)!=0) 	_MSG_SCOUT2=MSG_MAINFERROR;  //12=主滤波器闭合故障

		else if(M_ChkFlag(SL_ERROR_STAC)!=0) 	_MSG_SCOUT2=MSG_STACERROR;     //13=定子哟テ鞅蘸瞎收�

		else if(M_ChkFlag(SL_MSPOUT)!=0) 	    _MSG_SCOUT2=MSG_MSPEEDOUT;     //14=转速超出范围故障
					
		else if(M_ChkFlag(SL_HIA1)!=0)			_MSG_SCOUT2=MSG_HIA1;     //15=网侧变流器A相SKIIP故障

		else if(M_ChkFlag(SL_HIB1)!=0)			_MSG_SCOUT2=MSG_HIB1;     //16=网侧变流器B相SKIIP故障

		else if(M_ChkFlag(SL_HIC1)!=0)			_MSG_SCOUT2=MSG_HIC1;     //17=网侧变流器C相SKIIP故障
		
		else if(M_ChkFlag(SL_HIA2)!=0)			_MSG_SCOUT2=MSG_HIA2;     //18=电机侧变流器A相SKIIP故�

		else if(M_ChkFlag(SL_HIB2)!=0)			_MSG_SCOUT2=MSG_HIB2;     //19=电机侧变流器B相SKIIP故障

		else if(M_ChkFlag(SL_HIC2)!=0)			_MSG_SCOUT2=MSG_HIC2;     //20=电机侧变流器C相SKIIP故障

		else if(M_ChkFlag(SL_HUDCOV)!=0)		_MSG_SCOUT2=MSG_UDCOV;    //21=变流器直流母线硬件过压故障

        else if(M_ChkFlag(SL_HIACOV1)!=0)		_MSG_SCOUT2=MSG_HIAC1;    //22=网侧硬件过流故障

		else if(M_ChkFlag(SL_HIACOV2)!=0)		_MSG_SCOUT2=MSG_HIAC2;    //23=电机侧硬件过流故障

        else if(M_ChkFlag(SL_PDPASERIES)!=0)	_MSG_SCOUT2=MSG_PDPASERIES;//24=网侧严重故障

		else if(M_ChkFlag(SL_PDPBSERIES)!=0)	_MSG_SCOUT2=MSG_PDPBSERIES;//25=网侧严重故障

		else if(M_ChkFlag(SL_SIAC1)!=0)			_MSG_SCOUT2=MSG_SIAC1;    //26=网侧软件过流故障

		else if(M_ChkFlag(SL_SIAC2)!=0)			_MSG_SCOUT2=MSG_SIAC2;    //27=电机侧软件过流故障

		else if(M_ChkFlag(SL_FE1)!=0)			_MSG_SCOUT2=MSG_FE1;      //28=网侧频率不符故障

		else if(M_ChkFlag(SL_SUDCOV)!=0)		_MSG_SCOUT2=MSG_SUDCOV;   //29=软件中间直流电压过压
		
		else if(M_ChkFlag(SL_UACOV1)!=0)		_MSG_SCOUT2=MSG_SUACOV1;  //30=软件网压交流过压

//		else if(M_ChkFlag(SL_SUDCLV)!=0)		_MSG_SCOUT2=MSG_SUDCLV;   //31=软件中间直流电压欠压 20100507atzuoyun
				
		else if(M_ChkFlag(SL_UACLV1)!=0)		_MSG_SCOUT2=MSG_SUACLV1;  //32=软件网压交流欠压
		
//		else if(M_ChkFlag(SL_UDCWAVE)!=0)		_MSG_SCOUT2=MSG_UDCWAVE;  //33=中间电压波动故障

        else if(M_ChkFlag(SL_SIOVST)!=0)		_MSG_SCOUT2=MSG_SIOVST;   //34=软件检测定子过流故障
		
		else if(M_ChkFlag(SL_TAOV)!=0)			_MSG_SCOUT2=MSG_TOV1;	  //35=网侧SKIIP超温
		
		else if(M_ChkFlag(SL_TBOV)!=0)			_MSG_SCOUT2=MSG_TOV2;  	  //36=电机侧SKIIP超温

		else if(M_ChkFlag(SL_SKTOV)!=0)			_MSG_SCOUT2=MSG_SKTOV;     //37=软件判断SKIIP超温

        else if(M_ChkFlag(SL_NPR_TLOV)!=0)		_MSG_SCOUT2=MSG_TLOV1;     //38=电网侧电感超温

		else if(M_ChkFlag(SL_MPR_TLOV)!=0)		_MSG_SCOUT2=MSG_TLOV2;     //39=电机侧电感超温

        else if(M_ChkFlag(SL_PGOV)!=0)		    _MSG_SCOUT2=MSG_PGOV;      //40=变流器有功功率过载

		else if(M_ChkFlag(SL_QGOV)!=0)	    	_MSG_SCOUT2=MSG_QGOV;      //41=变流器无功功率过载

//------------------------故障处理---------------------------------------------------------

//NO1-------原来没有故障本次有故障(或)原来有故障但本次级别更高-------------------（故障升级）------
		if(((_MSG_SCOUT2!=MSG_NONE)&&(M_ChkFlag(SL_ERROR)==0))||((_MSG_SCOUT2!=MSG_NONE)&&(M_ChkFlag(SL_ERROR)!=0)&&(TAB_MSG[_MSG_SCOUT2].rank > TAB_MSG[_MSG_SCOUT1].rank)))
		{													
			M_SetFlag(SL_ERROR);							//置故障标志位
			M_SetFlag(SL_DISPLAY5);                         //置系统故障指示

//----------------------------新故障属性标示-------------------------------------------------------												
			if((TAB_MSG[_MSG_SCOUT2].attr & SHUT)==SHUT)		M_SetFlag(SL_SHUT);		//停机属性
			else												M_ClrFlag(SL_SHUT);
			
			if((TAB_MSG[_MSG_SCOUT2].attr & N_RCVR)==N_RCVR)	M_SetFlag(SL_NRCVR);	//不能恢复属性
			else												M_ClrFlag(SL_NRCVR);
			
			if((TAB_MSG[_MSG_SCOUT2].attr & I_RCVR)==I_RCVR)	M_SetFlag(SL_IRCVR);	//立即恢复属性
			else												M_ClrFlag(SL_IRCVR);
			
			if((TAB_MSG[_MSG_SCOUT2].attr & D_RCVR)==D_RCVR)	M_SetFlag(SL_DRCVR);	//延时恢复属性
			else												M_ClrFlag(SL_DRCVR);
						
			if((TAB_MSG[_MSG_SCOUT2].attr & CNT)==CNT)			M_SetFlag(SL_CNT);		//计次数属性
			else												M_ClrFlag(SL_CNT);
			
			if((TAB_MSG[_MSG_SCOUT2].attr & OT_SER)==OT_SER)	M_SetFlag(SL_OTSER);	//超时严重属性
			else												M_ClrFlag(SL_OTSER);
			
			if((TAB_MSG[_MSG_SCOUT2].attr & SAVE)==SAVE)		M_SetFlag(SL_SAVE);		//需要存储
			else												M_ClrFlag(SL_SAVE);		

			if((TAB_MSG[_MSG_SCOUT2].attr & WARNING)==WARNING)	M_SetFlag(SL_WARNING);	//报警属性
			else												M_ClrFlag(SL_WARNING);	
	
			if((TAB_MSG[_MSG_SCOUT2].attr & OFFCB)==OFFCB)		M_SetFlag(SL_OFFCB);	//先断主断属性
			else												M_ClrFlag(SL_OFFCB);	

			if((TAB_MSG[_MSG_SCOUT2].attr & REPORT)==REPORT)	M_SetFlag(SL_REPORT);	//上报主控属性
			else												M_ClrFlag(SL_REPORT);	

//----------------------------------根据故障属性判断系统动作---------------------------------------
						
			if(M_ChkFlag(SL_NRCVR)!=0)		//不可重试故障
			{
				M_SetFlag(SL_FORBIDRESET);	//故障次数超出限制,禁止主控复位 201105atzuoyun	
				M_SetFlag(SL_ERRSTOP);		
				M_SetFlag(SL_SERIESTOP);    //严重故障，须手动复位
				M_ClrFlag(SL_NRCVR);		
			}

			if(M_ChkFlag(SL_OFFCB)!=0)		//严重故障,直接断主断 201105atzuoyun
			{
				M_SetFlag(SL_ERRSTOP);		
				M_SetFlag(SL_SERIESTOP);    //严重故障，允许主控复位
				M_ClrFlag(SL_OFFCB);	
			}


        	if(M_ChkFlag(SL_REPORT)!=0)   //该故障需要上报给主控201105atzuoyun
			{
				M_SetFlag(SL_REPORT_OCS);  //上报给主控			
				M_ClrFlag(SL_REPORT);												
			} 
			
            if(M_ChkFlag(SL_SHUT)!=0)
			{
				M_SetFlag(SL_ERRSTOP);												
				M_ClrFlag(SL_SHUT);
			}

/*			if(M_ChkFlag(SL_IRCVR)!=0)		//允许立即恢复故障 被屏蔽,允许故障立即恢复201105atzuoyun
			{
				M_ClrFlag(SL_IRCVR);							
				M_SetFlag(SL_ERRSTOP);
			}											
*/					

			if(M_ChkFlag(SL_CNT)!=0)
			{
				_SY_RTRN++;
//				if(_SY_RTRN>_SC_RTRN)	M_SetFlag(SL_SERIESTOP);		//超过次数,严重故障	
				if(_SY_RTRN>_SC_RTRN)	M_SetFlag(SL_FORBIDRESET);		//故障次数超出限制,禁止主控复位 201105atzuoyun	

				M_ClrFlag(SL_CNT);				
			}

			if(M_ChkFlag(SL_OTSER)!=0)
			{
				if(M_ChkCounter(MAIN_LOOP.cnt_otser,DELAY_OTSER)>=0)	//延时时间到
				{
					M_SetFlag(SL_SERIESTOP);							//严重故障
					M_ClrFlag(SL_OTSER);								//清超时严重标志
				}	
			}	

			MAIN_LOOP.cnt_rcvr=0;			//清延时恢复计数器	
			_MSG_SCOUT1 = _MSG_SCOUT2;		//本次故障信息转存
								
//----------------------------更新故障记录并向上位机报故障------------------------------------------						
			if((M_ChkFlag(SL_SAVE)!=0)&&(M_ChkFlag(SL_EEBUSY_ERRSAVE)==0))			
			{
				MAIN_LOOP.cnt_rcvr=0;			//清延时恢复计数器	

				M_SetFlag(SL_EEASK_ERRSAVE);							//EEPROM操作请求
				M_ClrFlag(SL_SAVE);
				_BA_ERR1 = _BA_ERR2;									//故障信息保存
				_BA_ERR2 = _BA_ERR3;
				_BA_ERR3 = _BA_ERR4;
				_BA_ERR4 = _MSG_SCOUT2;
       			
				_BA_EURF   = (int16)RUN.urf;							//中间直流电压给定
				_BA_EUDC   = (int16)AD_OUT_UDC;							//中间直流电压
				_BA_EMIDRF = (int16)(RUN.mpridrf * 10);					//d轴电流给定
				_BA_ENIQRF = (int16)(RUN.npriqrf * 10);					//q轴电流给定
				_BA_EMIQRF = (int16)(RUN.mpriqrf * 10);					//q轴电流给定
				_BA_ETOQRF = (int16)RUN.toqrf;							//转矩电流给定
                _BA_EAGLRF = (int16)(DISP.aglrf);            			 //无功角度指令

				_BA_EIA1  = (int16)(AD_OUT_NPR_I.a * 10);				//网侧变流器,A相电流瞬时值
				_BA_EIB1  = (int16)(AD_OUT_NPR_I.b * 10);				//网侧变流器,B相电流瞬时值
				_BA_EIC1  = (int16)(AD_OUT_NPR_I.c * 10);				//网侧变流器,C相缌魉彩敝�
				_BA_EIA2  = (int16)(AD_OUT_MPR_I.a * 10);				//电机侧变流器,A相电流瞬时值
				_BA_EIB2  = (int16)(AD_OUT_MPR_I.b * 10);	            //电机侧变流器,B相电流瞬时值
				_BA_EIC2  = (int16)(AD_OUT_MPR_I.c * 10);	            //电机侧变流器,C相电流瞬时值
				
				_BA_EUAB1  = (int16)PRO.NPR_uab;						//网侧ab线电压
				_BA_EUBC1  = (int16)PRO.NPR_ubc;						//网侧bc线电压
				_BA_EUAB2  = (int16)PRO.STA_uab;						//定子侧ab线电压
				_BA_EUBC2  = (int16)PRO.STA_ubc;						//定子侧bc线电压

				_BA_EUAB0    = (int16)DISP.grd_uab;			       		//主断前网侧ab线压
            	_BA_EUBC0    = (int16)DISP.grd_ubc;			       		//主断前网侧bc线压

				_BA_ETLAC  = (int16)PRO.NPR_TLOV;                           //网侧电感温度
				_BA_ETLDUDT= (int16)PRO.MPR_TLOV;                           //机侧电感温度
				_BA_ETSKIIP= (int16)AMUX.skiiptempmax;                      //SKIIP温度	

				_BA_EFREQ  = (int16)(CAP4.freq * 10);						//电网频率
				_BA_ESPEED = (int16)(PRO.speed);							//电机转速 

	            _BA_ENPRUD = (int16)TRS_NGS_U.d;              			//d轴主断后网压反馈20091026atzy
				_BA_ENPRUQ = (int16)TRS_NGS_U.q;          				//q轴主断后网压反馈20091026atzy
	            _BA_ENPRUD2 = (int16)TRS_NGS_U.dflt;          			//d轴主断后网压反馈 滤波后20091026atzy
				_BA_ENPRUQ2 = (int16)TRS_NGS_U.qflt;          			//q轴主断后网压反馈 滤波后20091026atzy
	            _BA_ENUDOUT = (int16)TRS_NPR_U.d;          				//网侧d轴电压输出20091026atzy
				_BA_ENUQOUT = (int16)TRS_NPR_U.q;          				//网侧q轴电压输出20091026atzy

				_BA_EPIONU  = (int16)(PI_NPR_U.out * 10);					//单籄
				_BA_EPIONID = (int16)(PI_NPR_Id.out* 10);					//单位V
				_BA_EPIONIQ	= (int16)(PI_NPR_Iq.out* 10);					//单位V

				_BA_EMEXI   = (int16)(RUN_mpridrf_exi * 10);				//单位A  机侧励磁理论计算值显示 cpc
				_BA_EPIOMID = (int16)(PI_MPR_Id.out* 10);					//单籚
				_BA_EPIOMIQ	= (int16)(PI_MPR_Iq.out* 10);					//单位V

				_BA_ESTAIAC = (int16)(DISP.sta_iac * 10);
				_BA_ESTAIBA = (int16)(DISP.sta_iba * 10);
				_BA_ETOQFB  = (int16)DISP.toqfb;				        	//转矩反馈
				_BA_EPSTA   = (int16)(DISP.Psactive * 0.01);          		//定子侧有功功率显示kW,DOT1 *10/1000=100
				_BA_EPNPR   = (int16)(DISP.Pnactive * 0.01);         		//网侧有功功率显示
//				_BA_EPGRID  = (int16)(DISP.Pgactive * 0.01);          		//并网总的有功功率显示
				_BA_EPGRID  = (int16)(_BA_EPGRID);          				//记录PDP故障次数 20100507atzuoyun

				_BA_TIME1_0=_BA_TIME2_0;									//故障时刻1
				_BA_TIME1_1=_BA_TIME2_1;
				_BA_TIME1_2=_BA_TIME2_2;
				_BA_TIME1_3=_BA_TIME2_3;
				_BA_TIME1_4=_BA_TIME2_4;
				_BA_TIME1_5=_BA_TIME2_5;
				
				_BA_TIME2_0=_BA_TIME3_0;									//故障时刻2
				_BA_TIME2_1=_BA_TIME3_1;
				_BA_TIME2_2=_BA_TIME3_2;
				_BA_TIME2_3=_BA_TIME3_3;
				_BA_TIME2_4=_BA_TIME3_4;
				_BA_TIME2_5=_BA_TIME3_5;
					
				_BA_TIME3_0=_BA_TIME4_0;									//故障时刻3
				_BA_TIME3_1=_BA_TIME4_1;
				_BA_TIME3_2=_BA_TIME4_2;
				_BA_TIME3_3=_BA_TIME4_3;
				_BA_TIME3_4=_BA_TIME4_4;
				_BA_TIME3_5=_BA_TIME4_5;
					
				_BA_TIME4_0=RTIMER.time[0];									//故障时刻4
				_BA_TIME4_1=RTIMER.time[1];                         		//最新故障时间
				_BA_TIME4_2=RTIMER.time[2];
				_BA_TIME4_3=RTIMER.time[3];
				_BA_TIME4_4=RTIMER.time[4];
				_BA_TIME4_5=RTIMER.time[5];
			}
		}

//NO2------原有故障现在也有，且级别相同或者低于之前级别的故障------（故障平级非零、降级非零）----
		else if((_MSG_SCOUT2!=MSG_NONE)&&(M_ChkFlag(SL_ERROR)!=0)&&((TAB_MSG[_MSG_SCOUT2].rank <= TAB_MSG[_MSG_SCOUT1].rank)))					
		{
			MAIN_LOOP.cnt_rcvr=0;
			_MSG_SCOUT1 = _MSG_SCOUT2;		//本次故障信息转存

			if(M_ChkFlag(SL_OTSER)!=0)
			{
				if(M_ChkCounter(MAIN_LOOP.cnt_otser,DELAY_OTSER)>=0)	//延时时间到
				{
					M_SetFlag(SL_ERRSTOP);								//故障
					M_SetFlag(SL_SERIESTOP);							//严重故障
					M_ClrFlag(SL_OTSER);								//清超时严重标志
				}	
			}
			else	MAIN_LOOP.cnt_otser=0;				
		}

//NO3---------原来有故障，本次没有故障----------------------------------------（故障降级为零）------			
		else if((_MSG_SCOUT2==MSG_NONE)&&(M_ChkFlag(SL_ERROR)!=0))					
		{				
		
			if(M_ChkFlag(SL_OTSER)!=0)		
			{
				M_ClrFlag(SL_OTSER);								//超时严重,本次没有故障
				MAIN_LOOP.cnt_otser=0;  							//清超时严重延时计数器
			}

			if(M_ChkFlag(SL_IRCVR)!=0)								//允许立即恢复故障
			{
				M_ClrFlag(SL_IRCVR);								//清立即恢复标志
				if(M_ChkFlag(SL_FORBIDRESET)==0)
				{
					M_ClrFlag(SL_ERROR);							//删除故障信号
					M_ClrFlag(SL_ERRSTOP);							//删除故障信号  new
					M_ClrFlag(SL_SERIESTOP);						//延时恢复允许清除现毓收媳曛�201105atzuoyun
					M_ClrFlag(SL_REPORT_OCS); 						//不再持续故障上报主控	201105atzuoyun
				} 
			}
			
			if(M_ChkFlag(SL_DRCVR)!=0)								//允许延时恢复且本次没有故障
			{
				PRO.rcvr = _SC_RTRT * 1000;							//单位变换:s->ms
				if(M_ChkCounter(MAIN_LOOP.cnt_rcvr,PRO.rcvr)>=0)	//延时时间到
				{
					M_ClrFlag(SL_DRCVR);							//清延时恢复标志
					if(M_ChkFlag(SL_FORBIDRESET)==0)
					{
						M_ClrFlag(SL_ERROR);							//删除故障信号
						M_ClrFlag(SL_ERRSTOP);							//删除故障信号  new
						M_ClrFlag(SL_SERIESTOP);						//延时恢复允许清除现毓收媳曛�201105atzuoyun
						M_ClrFlag(SL_REPORT_OCS); 						//不再持续故障上报主控	201105atzuoyun
					}
				}
			}
			else	
				MAIN_LOOP.cnt_rcvr=0;		
		}
//NO4---------------------------------原来没有故障本次也没有故障--------------（故障平级为零）------
		else if((_MSG_SCOUT2==MSG_NONE)&&(M_ChkFlag(SL_ERROR)==0))	
		{
			M_ClrFlag(SL_WARNING);
			M_ClrFlag(SL_SHUT);
			M_ClrFlag(SL_OTSER);
			M_ClrFlag(SL_IRCVR);
			M_ClrFlag(SL_NRCVR);			
			M_ClrFlag(SL_DRCVR);
			M_ClrFlag(SL_OFFCB);	//201105atzuoyun
			M_ClrFlag(SL_REPORT);  	//201105atzuoyun
			MAIN_LOOP.cnt_rcvr=0;
			MAIN_LOOP.cnt_otser=0;
			_MSG_SCOUT1=0;
			_MSG_SCOUT2=0;
			M_ClrFlag(SL_DISPLAY5);                         //清系统故障指示
		}
	}//if((M_ChkFlag(SL_POWERON)==0)&&(M_ChkFlag(SL_ERRSTOP)==0))

//-------------------------故障停机后复位处理-------------------------------------------------------
	if(((M_ChkFlag(SL_ERRSTOP)!=0)||(M_ChkFlag(SL_SERIESTOP)!=0)) &&(M_ChkFlag(SL_RESET)!=0)&&(_MSG_SCOUT2==0)&&(M_ChkFlag(SL_FORBIDRESET)==0))   //没有故障时才能复位
	{																											//一定时间内故障超出次数则禁止主控复位 201105atzuoyun
			M_ClrFlag(SL_SERIESTOP);				//清除严重停机故障标志
			M_ClrFlag(SL_ERRSTOP);					//清除停机故障标志
			M_ClrFlag(SL_ERROR);					//清除故障标志
//			_SY_RTRN=0;								//故障计数器清零		201105atzuoyun
			M_ClrFlag(SL_REPORT_OCS); 				//不再持续故障上报主控	201105atzuoyun
			
			M_ClrFlag(SL_OCS_NPRSTART);	          	//清主控命令	 
			M_ClrFlag(SL_OCS_MPRSTART);				//清主控命令								

			M_ClrFlag(SL_EE_FAIL);					//清除EEROM故障标志

			M_ClrFlag(SL_PDPINTA);					//清除网侧TZ1_PDP故障标志
			M_ClrFlag(SL_PDPINTB);					//清除机侧TZ2_PDP故障标志
			M_ClrFlag(SL_QEPPCO);
//			M_ClrFlag(SL_QEPPHE);
//			M_ClrFlag(SL_QEPPCDE);

			M_ClrFlag(SL_DISPLAY3);             	//灭PDP故障指示灯
			ClrPdpint();							//PDPINT中断清空	
			EnPdpint();								//PDPINT中断使能
	
			_MSG_SCOUT1=0;											//清故障信息位
			_MSG_SCOUT2=0;
			MAIN_LOOP.cnt_rcvr=0;                                   //故障延时恢复计时
			MAIN_LOOP.cnt_otser=0;                                  //超时严重延时	

//			M_ClrFlag(SL_CANOPENOVER);               //CAN通讯
	}

	if(_SY_RTRN!=0)
	{
		PRO.reset = _stdby05 * 1000;							//单位变换:s->ms

		if(M_ChkCounter(MAIN_LOOP.cnt_resetrn,PRO.reset)>=0)	//由备用_STDBY5设定0-32767,单位s
//		if(M_ChkCounter(MAIN_LOOP.cnt_resetrn,86400000)>=0)	//1 day
//		if(M_ChkCounter(MAIN_LOOP.cnt_resetrn,300000)>=0)	//5 min test 
		{
			_SY_RTRN=0;	
			MAIN_LOOP.cnt_resetrn=0;
		}
	}
	else	MAIN_LOOP.cnt_resetrn=0;


}
/*********************************************************************************************************
** 函数名称: et_relay
** 功能描述: 过载保护计算-NPR
** 输　入: 	 
** 输:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void et_relay_N(void)
{
	int32 et1,et2,ev1,ev2,ev,et;
//网侧	
	ev = (int32)PRO.NPR_iac;
	et = ((ev-_SC_IACOV1) * 512) /_SC_IACOV1;

//200us
	if(abs(et)>255)
	{
		if(et>=0)
		{
			et1=et-256;
			if(et1>255) et1=255;   //超出200%的话就算作200%
		
			ev1=TAB_ET1[et1];
		}
		else 
		{
			et1=et+256;
			if(et1<-255) et1=-255;
	
			et1=-et1;
			ev1=TAB_ET1[et1];
			ev1=-ev1;
		}
	}
	else ev1=0; 
//1s
	if(M_ChkCounter(MAIN_LOOP.et_relay1,1000)>=0)
	{
		MAIN_LOOP.et_relay1=0;
		et2=et;
		
		if(et2>=0)
		{
			if(et2>255) ev2=0;
			else ev2=TAB_ET2[et2];
		}
		else
		{
			if(et2<-255) ev2=0;
			else
			{
			 et2=-et2;
			 ev2=TAB_ET2[et2];
			 ev2=-ev2;
			}
		}

	}
	else ev2=0;

	ET_SUM1=ET_SUM1+ev1+ev2;
	if(ET_SUM1>65535) ET_SUM1=65535;

	if(ET_SUM1<=0)					//小于额定值，清零
	{
		M_ClrFlag(SL_SIAC1);
		ET_SUM1=0;
	}
	else
	{
		if(ET_SUM1>et_gate)
		{
			M_SetFlag(SL_SIAC1);
			ET_SUM1=et_gate;
//			M_SetFlag(SL_ERRDATASAVE);	//触发外部RAM数据转存20091109atzy
		}
		else M_ClrFlag(SL_SIAC1);
	}
}  
/*********************************************************************************************************
** 函数名称: et_relay
** 功能描述: 过载保护计算--MPR
** 输　入: 	 
** 输:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void et_relay_M(void)
{
	int32 et1,et2,ev1,ev2,ev,et;
//机侧	
	ev = (int32)PRO.MPR_iac;
	et = ((ev-_SC_IACOV2) * 512) /_SC_IACOV2;

//200us
	if(abs(et)>255)
	{
		if(et>=0)
		{
			et1=et-256;
			if(et1>255) et1=255;
		
			ev1=TAB_ET1[et1];
		}
		else 
		{
			et1=et+256;
			if(et1<-255) et1=-255;
	
			et1=-et1;
			ev1=TAB_ET1[et1];
			ev1=-ev1;
		}
	}
	else ev1=0; 
//1s
	if(M_ChkCounter(MAIN_LOOP.et_relay2,1000)>=0)
	{
		MAIN_LOOP.et_relay2=0;
		et2=et;
		
		if(et2>=0)
		{
			if(et2>255) ev2=0;
			else ev2=TAB_ET2[et2];
		}
		else
		{
			if(et2<-255) ev2=0;
			else
			{
			 et2=-et2;
			 ev2=TAB_ET2[et2];
			 ev2=-ev2;
			}
		}

	}
	else ev2=0;

	ET_SUM2=ET_SUM2+ev1+ev2;
	if(ET_SUM2>65535) ET_SUM2=65535;

	if(ET_SUM2<=0)
	{
		M_ClrFlag(SL_SIAC2);
		ET_SUM2=0;
	}
	else
	{
		if(ET_SUM2>et_gate)
		{
			M_SetFlag(SL_SIAC2);
			ET_SUM2=et_gate;
//			M_SetFlag(SL_ERRDATASAVE);	//触发外部RAM数据转存20091109atzy
		}
		else M_ClrFlag(SL_SIAC2);
	}
}  

/*********************************************************************************************************
** 函数名: CntCtrl
** 功能描述: 计数控制
** 输　入:
** 输　出       
** 注  释: 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void CntCtrl(void)
{
	if(MAIN_LOOP.cnt1!=65535)			MAIN_LOOP.cnt1++;
	if(MAIN_LOOP.cnt2!=65535)			MAIN_LOOP.cnt2++;
	if(MAIN_LOOP.cnt3!=65535)			MAIN_LOOP.cnt3++;
	if(MAIN_LOOP.cnt4!=65535)			MAIN_LOOP.cnt4++;
	if(MAIN_LOOP.cnt5!=65535)			MAIN_LOOP.cnt5++;
	if(MAIN_LOOP.cnt6!=65535)			MAIN_LOOP.cnt6++;

	if(MAIN_LOOP.cnt_poweron!=65535)	MAIN_LOOP.cnt_poweron++;			
	if(MAIN_LOOP.cnt_pwmout!=65535)		MAIN_LOOP.cnt_pwmout++;
	if(MAIN_LOOP.cnt_freq!=65535)		MAIN_LOOP.cnt_freq++;
	if(MAIN_LOOP.cnt_freq2!=65535)		MAIN_LOOP.cnt_freq2++;							
	if(MAIN_LOOP.cnt_cbfreq!=65535)		MAIN_LOOP.cnt_cbfreq++;							
					
	if(MAIN_LOOP.cnt_nprlamp!=65535)	MAIN_LOOP.cnt_nprlamp++;  //指示网侧变流器工作灯
	if(MAIN_LOOP.cnt_mprlamp!=65535)	MAIN_LOOP.cnt_mprlamp++;  //指示机侧变流器工作灯
				
	if(MAIN_LOOP.cnt_rcvr!=65535)		MAIN_LOOP.cnt_rcvr++;
	if(MAIN_LOOP.cnt_otser!=65535)		MAIN_LOOP.cnt_otser++;

	if(MAIN_LOOP.cnt_estop!=65535)		MAIN_LOOP.cnt_estop++;
	if(MAIN_LOOP.cnt_exfault!=65535)	MAIN_LOOP.cnt_exfault++;
	if(MAIN_LOOP.cnt_qgovload!=65535)	MAIN_LOOP.cnt_qgovload++;
	if(MAIN_LOOP.cnt_pgovload!=65535)	MAIN_LOOP.cnt_pgovload++;
	if(MAIN_LOOP.cnt_canfault!=65535)	MAIN_LOOP.cnt_canfault++;

	if(MAIN_LOOP.cnt_offcb!=65535)		MAIN_LOOP.cnt_offcb++;
	if(MAIN_LOOP.cnt_cbtp!=65535)		MAIN_LOOP.cnt_cbtp++;
	if(MAIN_LOOP.cnt_cberror!=65535)		MAIN_LOOP.cnt_cberror++;
	if(MAIN_LOOP.cnt_mainkerror!=65535)		MAIN_LOOP.cnt_mainkerror++;
	if(MAIN_LOOP.cnt_mainferror!=65535)		MAIN_LOOP.cnt_mainferror++;
	if(MAIN_LOOP.cnt_stacerror!=65535)		MAIN_LOOP.cnt_stacerror++;
	if(MAIN_LOOP.cnt_preerror!=65535)		MAIN_LOOP.cnt_preerror++;

	if(SCI.cnt_sciover!=65535)			SCI.cnt_sciover++;
	if(SCI.cnt_scispace!=65535)			SCI.cnt_scispace++;

	if(MAIN_LOOP.cnt_isteady0!=65535)		MAIN_LOOP.cnt_isteady0++;     //RunCtrl网侧停机给定延时
	if(MAIN_LOOP.cnt_mprsteady0!=65535)		MAIN_LOOP.cnt_mprsteady0++;   //RunCtrl机侧停机给定延时

	if(MAIN_LOOP.cnt_senszfstdy!=60001)		MAIN_LOOP.cnt_senszfstdy++;		//传感器零漂滤波计数器
	if(MAIN_LOOP.cnt_speedout!=65535)		MAIN_LOOP.cnt_speedout++;		//20090817
	if(MAIN_LOOP.cnt_uaclv1!=65535)		MAIN_LOOP.cnt_uaclv1++;		//200901027atzy

//sysctl_zl_start
	if(MAIN_LOOP.cnt_opencontac!=65535)		MAIN_LOOP.cnt_opencontac++;    //严重不可恢复故障下，发出断主断指令后到发出断定子接触器和主接触器的延时
	
	if(MAIN_LOOP.cnt_mkoff!=65535)	    	MAIN_LOOP.cnt_mkoff++;         //接收到系统停机指令后，延时断主接触器计数
	if(MAIN_LOOP.cnt_cboff!=65535)	    	MAIN_LOOP.cnt_cboff++;         //接收到系统停机指令后，延时断主断计数
      
	if(MAIN_LOOP.cnt_closecb!=65535)		MAIN_LOOP.cnt_closecb++;        //正常启动，主断储能到主断合闸的延时计数
	if(MAIN_LOOP.cnt_precok!=65535)      	MAIN_LOOP.cnt_precok++;         //正常启动，预充电时间

	if(MAIN_LOOP.cnt_steadyfb!=65535)      	MAIN_LOOP.cnt_steadyfb++;       //正常启动/停止，检测到Vdc稳定在1100V到允许机侧脉冲的延时计数
	if(MAIN_LOOP.cnt_mprstart!=65535)      	MAIN_LOOP.cnt_mprstart++;       //正常启动，Vdc稳定后MPR⒙龀宓难邮奔剖� 
//sysctl_zl_end	

//ADctl_zl_start
    if(MAIN_LOOP.cnt_AMUX!=65535)      	    MAIN_LOOP.cnt_AMUX++;           //慢速AD采样延时及时
//ADctl_zl_end
    if(MAIN_LOOP.cnt_reset!=65535)      	MAIN_LOOP.cnt_reset++;           //上位机I/O复位，延时2s，防止误操作
    if(MAIN_LOOP.cnt_clostacmd!=65535)      MAIN_LOOP.cnt_clostacmd++;       //衔机允许合定子接触器，延时1s，止误操
    if(MAIN_LOOP.cnt_nprcmd!=65535)         MAIN_LOOP.cnt_nprcmd++;         //上位机允许NPR发脉冲邮�1s，防止误操作
    if(MAIN_LOOP.cnt_mprcmd!=65535)         MAIN_LOOP.cnt_mprcmd++;         //上位机允许MPR发脉冲，延时1s，防刮蟛僮�
    if(MAIN_LOOP.cnt_fanstar!=65535)        MAIN_LOOP.cnt_fanstar++;          //风机星三角变换延时计数
    if(MAIN_LOOP.cnt_fantriangle!=65535)    MAIN_LOOP.cnt_fantriangle++;      //风机星三角变换延时计数 
//    if(MAIN_LOOP.cnt_fanstop!=65535)        MAIN_LOOP.cnt_fanstop++;          //系统停机后，控制风机以星接工作延时计数 
	if(MAIN_LOOP.cnt_qcapspdin!=65535)		MAIN_LOOP.cnt_qcapspdin++;   
	if(MAIN_LOOP.cnt_async!=65535)		    MAIN_LOOP.cnt_async++;   
	if(MAIN_LOOP.cnt_qcapdisturb!=65535)	MAIN_LOOP.cnt_qcapdisturb++;		//QEP抗扇�   
	if(MAIN_LOOP.cnt_qepcntok!=65535)		MAIN_LOOP.cnt_qepcntok++;   		//QEP抗干扰   
	if(MAIN_LOOP.cnt_qepzdisturb!=65535)	MAIN_LOOP.cnt_qepzdisturb++;   		//QEP抗干扰   
	   
//canopen
	if(MAIN_LOOP.canopen_tx!=65535)			MAIN_LOOP.canopen_tx++;   
	if(MAIN_LOOP.canopen_rx!=65535)			MAIN_LOOP.canopen_rx++;      

	if(SCI_canopen.cnt_heartbeat!=65535)	SCI_canopen.cnt_heartbeat++;      
	if(SCI_canopen.cnt_sciover!=65535)		SCI_canopen.cnt_sciover++;   
	if(MAIN_LOOP.cnt_cbreset!=65535)		MAIN_LOOP.cnt_cbreset++;  

	if(MAIN_LOOP.et_relay1!=65535)		    MAIN_LOOP.et_relay1++;      
	if(MAIN_LOOP.et_relay2!=65535)		    MAIN_LOOP.et_relay2++;   

	if(MAIN_LOOP.cnt_ocsein1!=65535)		MAIN_LOOP.cnt_ocsein1++;   
	if(MAIN_LOOP.cnt_ocsein2!=65535)		MAIN_LOOP.cnt_ocsein2++;  
	if(MAIN_LOOP.cnt_ocssysrun1!=65535)		MAIN_LOOP.cnt_ocssysrun1++;      
	if(MAIN_LOOP.cnt_ocssysrun2!=65535)		MAIN_LOOP.cnt_ocssysrun2++;   

	if(MAIN_LOOP.cnt_datasave!=65535)		MAIN_LOOP.cnt_datasave++;   
    if(MAIN_LOOP.cnt_resetrn!=65535)    	MAIN_LOOP.cnt_resetrn++;    //201105atzuoyun

//test----------------------------------------------------------------------------------------------
/*
if((M_ChkFlag(SL_NPR_RUNING)!=0))//&&(i_cnt1<=99)
{
	i_cnt2++;
	if(i_cnt2>=1)	 i_cnt2=0;
	if(i_cnt2==0)
	{
		draw1[i_cnt1] = CAP4.freq;      //
		draw2[i_cnt1] = TRS_NPR_U.d;        //

		draw3[i_cnt1] = TRS_NGS_U.qflt;       //
		draw4[i_cnt1] = TRS_NPR_U.q;            //
	
		draw5[i_cnt1] = TRS_NPR_U.alfa;//
		draw6[i_cnt1] = TRS_NPR_U.beta;//

		draw5[i_cnt1] = PI_NPR_Id.out;//
		draw6[i_cnt1] = PI_NPR_Iq.out;//

		i_cnt1++;
    	if(i_cnt1>=149)	i_cnt1=149;
	}

}
else 
{

	i_cnt2++;
	if(i_cnt2>=1)	 i_cnt2=0;
	if(i_cnt2==0)
	{
		draw1[i_cnt1] = CAP4.freqtmp;//PI_NPR_U.reference;//PI_NPR_Id.reference;//;//;
		draw2[i_cnt1] = CAP4.prd;//PI_NPR_U.feedback;//PI_NPR_Id.feedback;//PI_NPR_Id.feedback;//AD_OUT_NPR_I.c;

		draw3[i_cnt1] = ECap4Regs.TSCTR;//PI_NPR_Id.errortrue;//AD_OUT_NPR_I.c;
		draw4[i_cnt1] = CAP4.freq;//EPwm2Regs.CMPA.half.CMPA*1.0 - EPwm3Regs.CMPA.half.CMPA*1.0;//PI_NPR_Id.error;//AD_OUT_NPR_I.b;
	
		draw5[i_cnt1] = CAP4.nprtrstheta;//PI_NPR_Id.errortrue;//AD_OUT_NPR_I.c;
		draw6[i_cnt1] = GpioDataRegs.GPADAT.bit.GPIO5;//EPwm2Regs.CMPA.half.CMPA*1.0 - EPwm3Regs.CMPA.half.CMPA*1.0;//PI_NPR_Id.error;//AD_OUT_NPR_I.b;
	
		i_cnt1++;
    	if(i_cnt1>=149)	i_cnt1=0;
		i_cnt3=i_cnt1;
	}

}
//test---------------------------------------------------------------------------------------------- 
*/ 
}

/*********************************************************************************************************
** 函数名称: Display
** 功能描述: 显示值计算
** 输　入: 	 
** 输出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Display(void)
{
	float temp1,temp2,temp3;

//------------------------中间直流电压及指令值显示值计算-------------------------------------------
	DISP.udc  = PRO.udc;							//单位V
	DISP.urf  = RUN.urf;							//单位V												//单位V
	DISP.mpridrf = RUN.mpridrf;							//单位A
	DISP.npriqrf = RUN.npriqrf;							//单位A
	DISP.mpriqrf = RUN.mpriqrf;							//单位A
	DISP.toqrf   = RUN.toqrf;							//单位NM
	DISP.aglrf   = RUN.aglrf;							//单位 角度
    DISP.toqfb   = Te_feedback;    
//---------------------------线电压有效值显示值计算------------------------------------------------
	DISP.npr_uab = PRO.NPR_uab;						//单位V
	DISP.npr_ubc = PRO.NPR_ubc;						//单位V
	DISP.sta_uab = PRO.STA_uab;						//单位V
	DISP.sta_ubc = PRO.STA_ubc;						//单位V

//---------------------------主断前网侧线电压有效值计算-显示---------------------------
	DISP.grd_uab = MEAN_DATA.uab * 1.110721;        //单位V  rms=mean*PAI/(2*sqrt(2)) ； 1.110721=PAI * SQRT2 / 4
    DISP.grd_ubc = MEAN_DATA.ubc * 1.110721;        //单位V  rms=mean*PAI/(2*sqrt(2)) ； 1.110721=PAI * SQRT2 / 4

//---------------------------网侧和机侧电感温度显示----------------------------
    DISP.Lac_temp      = AMUX.Lac_temp;                  //网侧电感温度
	DISP.Ldudt_temp    = AMUX.Ldudt_temp;                //机侧电感温度 
	DISP.NPR_skiptemp  = AMUX.NPR_skiiptemp;              //网侧SKIIP温度
	DISP.MPR_skiptemp  = AMUX.MPR_skiiptemp;              //机侧SKIIP温度


//---------------------------瞬时电流显示值计算----------------------------------------------------
	DISP.npr_iai = AD_OUT_NPR_I.a;					//单位A
	DISP.npr_ibi = AD_OUT_NPR_I.b;					//单位A
	DISP.npr_ici = AD_OUT_NPR_I.c;					//单位A
	DISP.mpr_iai = AD_OUT_MPR_I.a;					//单位A
	DISP.mpr_ibi = AD_OUT_MPR_I.b;					//单位A
	DISP.mpr_ici = AD_OUT_MPR_I.c;					//单位A

//------------------------网侧并网电流有效值显示值计算----------------------------------------------
	DISP.npr_iar = PRO.npr_iar;							//单位A
	DISP.npr_ibr = PRO.npr_ibr;							//单位A
	DISP.npr_icr = PRO.npr_icr;							//单位A

//------------------------机侧电流有效值显示值计算--------------------------------------------------
    temp1 = MEAN_DATA.ia2 * 1.110721;
	temp2 = MEAN_DATA.ib2 * 1.110721;
	temp3 = MEAN_DATA.ic2 * 1.110721;
	DISP.mpr_iar = temp1;							//单位A
	DISP.mpr_ibr = temp2;							//单位A
	DISP.mpr_icr = temp3;							//单位A

//------------------------定子线电流有效值显示值计算--------------------------------------------------
    temp1 = MEAN_DATA.iac3 * 1.110721;
	temp2 = MEAN_DATA.iba3 * 1.110721;
	DISP.sta_iac = temp1;							//单位A
	DISP.sta_iba = temp2;							//单位A

//------------------------------定子侧和网侧和总并网有功功率显示-----------------------------------------------------
	DISP.Psactive   = PRO.Psactive;
	DISP.Psreactive = PRO.Psreactive;
	DISP.Ps		    = PRO.Ps;

	DISP.Pnactive   = PRO.Pnactive;
	DISP.Pnreactive = PRO.Pnreactive;
    DISP.Pn         = PRO.Pn;

	DISP.Pgactive   = PRO.Pgactive;
	DISP.Pgreactive = PRO.Pgreactive;
	DISP.Pg      	= PRO.Pg;

//------------------------------频率和转速显示-----------------------------------------------------	
	if(M_ChkFlag(SL_IN1_CBSTS)==0) 	DISP.freq=0;            //20090815
	else 							DISP.freq = CAP4.freqtmp;

	DISP.speed= PRO.speedflt;		//20090815

//------------------------------定子同步并网前定子前后电压差值显示-----------------------------------------------------
	DISP.uab23 = MEAN_DATA.uab_d;
	DISP.ubc23 = MEAN_DATA.ubc_d;

//------------------------------PI环输出显示-------------------------------------------------------
	DISP.pionu  = PI_NPR_U.out;						//单位A
	DISP.pionid = PI_NPR_Id.out;					//单位V
	DISP.pioniq	= PI_NPR_Iq.out;					//单位V
	DISP.mexi  = RUN_mpridrf_exi;					//单位A  机侧励磁理论计算值显示
	DISP.piomid = PI_MPR_Id.out;				    //单位V
	DISP.piomiq	= PI_MPR_Iq.out;					//单位V
} 
/*********************************************************************************************************
** 函数名称: Bank()
** 功能描述: 显示控制
** 输　入: 	 
** 输出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Bank(void)
{
	
//------------------------------------网侧工作指示灯显示--------------------------------------------
	if(M_ChkFlag(SL_NPR_PWMOUT)!=0)
	{
		if(M_ChkCounter(MAIN_LOOP.cnt_nprlamp,DELAY_NPRRUN)>=0)	//0灯快速闪烁,指示网侧正在发脉冲运行
		{
			M_NotFlag(SL_DISPLAY0);
			MAIN_LOOP.cnt_nprlamp=0;
		}
	}
	else
	{
		if(M_ChkCounter(MAIN_LOOP.cnt_nprlamp,DELAY_NPRSTDBY)>=0)	//0灯慢速了�
		{
		   M_NotFlag(SL_DISPLAY0);
		   MAIN_LOOP.cnt_nprlamp=0;
		} 
	}
			
//------------------------------------机侧工作指示灯显示--------------------------------------------
	if(M_ChkFlag(SL_MPR_PWMOUT)!=0)
	{
		if(M_ChkCounter(MAIN_LOOP.cnt_mprlamp,DELAY_MPRRUN)>=0)	//1灯快速闪烁,甘净嗾诜⒙龀逶诵�
		{
			M_NotFlag(SL_DISPLAY1);
			MAIN_LOOP.cnt_mprlamp=0;
		}
	}
	else
	{
		if(M_ChkCounter(MAIN_LOOP.cnt_mprlamp,DELAY_MPRSTDBY)>=0)	//1灯慢速闪烁
		{
			M_NotFlag(SL_DISPLAY1);
			MAIN_LOOP.cnt_mprlamp=0;
		}
	}

//----------------------------------------运行监控--------------------------------------------------
	_BA_URF   = (int16)DISP.urf;				//中间电压给定值
	_BA_UDC   = (int16)DISP.udc;				//中间直流电压
	_BA_MIDRF  = (int16)(DISP.mpridrf * 10);		//d轴电流指令
	_BA_NIQRF  = (int16)(DISP.npriqrf * 10);		//q轴电流指令
	_BA_MIQRF  = (int16)(DISP.mpriqrf * 10);		//q轴电流指令
	_BA_TOQRF  = (int16)(DISP.toqrf);				//转矩指令
    _BA_AGLRF  = (int16)(DISP.aglrf);             //无功角度指令 20091027atzy

	_BA_IA1   = (int16)(DISP.npr_iar * 10);		//网侧,a相并网电流，改为1位∈�
	_BA_IB1   = (int16)(DISP.npr_ibr * 10);		//网侧,b相并网电流
	_BA_IC1   = (int16)(DISP.npr_icr * 10);		//网侧,c相并网电流
	_BA_IA2   = (int16)(DISP.mpr_iar * 10);		//电机侧,a相电流
	_BA_IB2   = (int16)(DISP.mpr_ibr * 10);		//电机侧,b相电流
	_BA_IC2   = (int16)(DISP.mpr_icr * 10);		//电机侧,c相电流

	_BA_UAB0   = (int16)DISP.grd_uab;			//主断前网侧ab线压
	_BA_UBC0   = (int16)DISP.grd_ubc;			//主断前网侧bc线压				
	_BA_UAB1  = (int16)DISP.npr_uab;			//网侧ab线压
	_BA_UBC1  = (int16)DISP.npr_ubc;			//网侧bc线压
//	_BA_UAB2  = (int16)DISP.sta_uab;			//定子侧ab线压,定子侧 20091027atzy
//	_BA_UBC2  = (int16)DISP.sta_ubc;			//定子侧bc线压,定子侧 20091027atzy
	_BA_GRDUD = (int16)TRS_NGS_U.dflt;          //d轴主断后网压反馈 滤波后
	_BA_GRDUQ = (int16)TRS_NGS_U.qflt;          //q轴主断后网压反馈 滤波后
	_BA_STAID = (int16)TRS_NPR_I.dflt;          //d轴网侧电流反馈 滤波后
	_BA_STAIQ = (int16)TRS_NPR_I.qflt;          //q轴网侧电流反馈 滤波后
    _BA_EXCID = (int16)(TRS_MPR_I.d * 10);             //d轴机侧电流反馈 滤波前
    _BA_EXCIQ = (int16)(TRS_MPR_I.q * 10);             //q轴机侧电流反馈 滤波前
	_BA_STAUD = (int16)TRS_MPR_U.d;
	_BA_STAUQ = (int16)TRS_MPR_U.q;

	_BA_TLAC  = (int16)DISP.Lac_temp;		    //网侧电感温度
	_BA_TLDUDT= (int16)DISP.Ldudt_temp;	        //机侧电感温度
	_BA_TNSKIIP= (int16)DISP.NPR_skiptemp;      //网侧SKIIP温度 摄氏度
	_BA_TMSKIIP= (int16)DISP.MPR_skiptemp;      //机侧SKIIP温度 摄氏度

	_BA_FREQ  = (int16)(DISP.freq * 10);		//电网频率
	_BA_SPEED = (int16)DISP.speed;				//电机转速 

	_BA_PIONU  = (int16)(DISP.pionu  * 10);			    //NPR电压环输出 6.23change_zl改为1位小数
	_BA_PIONID = (int16)(DISP.pionid * 100);			//NPR电流环d输出
	_BA_PIONIQ = (int16)(DISP.pioniq * 100);			//NPR电流环q输出
	_BA_MEXI   = (int16)(DISP.mexi  * 10);			    // 6.23change_zl改为1位小数 改为励磁电流理论值显示cpc
	_BA_PIOMID = (int16)(DISP.piomid * 100);			//MPR电流环d输出
	_BA_PIOMIQ = (int16)(DISP.piomiq * 100);			//MPR电流环q输出

//	_BA_STAUABD = (int16)(DISP.uab23 * 10);             //定子同步并网前定子前后电压差 20091027atzy
//	_BA_STAUBCD = (int16)(DISP.ubc23 * 10);				//定子同步并网前定子前后电压差 20091027atzy
	_BA_STAIAC = (int16)(DISP.sta_iac * 10);            //定子线电流有效值显示
	_BA_STAIBA = (int16)(DISP.sta_iba * 10);			//定子线电流有效值显示

	_BA_TOQFB  = (int16)DISP.toqfb;				        //转矩反馈

	_BA_PSTA  = (int16)(DISP.Psactive * 0.01);          //定子侧有功功率显示kW,DOT1 *10/1000=100
	_BA_PNPR  = (int16)(DISP.Pnactive * 0.01);          //网侧有功功率显示
	_BA_PGRID = (int16)(DISP.Pgactive * 0.01);          //并网总的有功功率显示

	_BA_QSTA  = (int16)(DISP.Psreactive * 0.01);        //定子侧无功功率显示kVAR,DOT1 *10/1000=100
	_BA_QNPR  = (int16)(DISP.Pnreactive * 0.01);        //网侧无功功率显示
	_BA_QGRID = (int16)(DISP.Pgreactive * 0.01);        //并网总的无功功率显示

	_BA_SSTA  = (int16)(DISP.Ps * 0.01);              	//定子侧视在功率显示kVA,DOT1 *10/1000=100
	_BA_SNPR  = (int16)(DISP.Pn * 0.01);              	//网侧视在功率显示
	_BA_SGRID = (int16)(DISP.Pg * 0.01);              	//并网总的视在功率显示 20100506

//通过CANOPEN通讯反馈给主控的监控变量	

//	DISP.toqfb = 8000;  //cantest
//    DISP.speed = 1800;  //cantest
//	AMUX.skiiptempmax = 65; //cantest

	SCI_canopen.tx_torque= (int16)(DISP.toqfb * 16383.0 / CAN_TEN);
	SCI_canopen.tx_speed = (int16)(PRO.speedflt * 16383.0 / CAN_SPEED);

	SCI_canopen.tx_skiiptempmax = (int16)(AMUX.skiiptempmax * 16383.0 / CAN_TEMP);
	SCI_canopen.tx_watertempin  = 0;
	SCI_canopen.tx_watertempout = 0;
	SCI_canopen.tx_demand = 0;
	

} 



/*********************************************************************************************************
** 函数名称: Datasave
** 功  能: 将数据写入外部RAM
** 输　入:
** 输　出:        
** 注  释: 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 20091109atzy
**-------------------------------------------------------------------------------------------------------
** 修改�:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Datasave(void)
{
//--------------------------------External RAM Data Save-----------------------------------------
//	if(_STDBY2!=0)	M_SetFlag(SL_ERRDATASAVE);	//触发外部RAM数据转存20091109athotel

  	if(*(RAM_END - 1) != 0x55AA)
  	{
		if(M_ChkFlag(SL_ERRDATASAVE)==0)  								//没有故障	
			MAIN_LOOP.cnt_datasave=0;
		else if(M_ChkCounter(MAIN_LOOP.cnt_datasave,DELAY_DATASAVE)>=0)	//故障发生后延时2s禁止画图
		{
			M_SetFlag(SL_DRAW);
			M_ClrFlag(SL_ERRDATASAVE);
			*(RAM_END - 1) = 0x55AA;

			*(RAM_END - 2) = RTIMER.time[0];							 //最新故障时间
			*(RAM_END - 3) = RTIMER.time[1];
			*(RAM_END - 4) = RTIMER.time[2];
			*(RAM_END - 5) = RTIMER.time[3];
			*(RAM_END - 6) = RTIMER.time[4];
			*(RAM_END - 7) = RTIMER.time[5];			
		}

		if(M_ChkFlag(SL_DRAW)==0)  										//有故障之后延时2s后停止数据存储
		{
			*(RAM_START+ RAMDATA_POS) = (int16)AD_OUT_UDC;							//0=中间直流电压
//			*(RAM_START+ RAMDATA_POS) = (int16)testtheta;	//1=test
			*(RAM_START+((Uint32)RAM_BIAS * 1 + RAMDATA_POS)) = (int16)CAP4.nprtrstheta;	//1=NPR定向角度
			*(RAM_START+((Uint32)RAM_BIAS * 2 + RAMDATA_POS)) = (int16)AD_OUT_NPR_I.a;		//2=NPR-A电流
			*(RAM_START+((Uint32)RAM_BIAS * 3 + RAMDATA_POS)) = (int16)AD_OUT_NPR_I.b;		//3=NPR-B电流
			*(RAM_START+((Uint32)RAM_BIAS * 4 + RAMDATA_POS)) = (int16)AD_OUT_NPR_I.c;		//4=NPR-C电流
			*(RAM_START+((Uint32)RAM_BIAS * 5 + RAMDATA_POS)) = (int16)AD_OUT_GRD_U.ab;		//5=电网电压Vab
			*(RAM_START+((Uint32)RAM_BIAS * 6 + RAMDATA_POS)) = (int16)AD_OUT_GRD_U.bc;		//6=电网电压Vbc
			*(RAM_START+((Uint32)RAM_BIAS * 7 + RAMDATA_POS)) = (int16)TRS_NGS_U.d;			//7=网压d轴分量ed滤波前
			*(RAM_START+((Uint32)RAM_BIAS * 8 + RAMDATA_POS)) = (int16)TRS_NGS_U.q;			//8=网压q轴分量eq滤波前
			*(RAM_START+((Uint32)RAM_BIAS * 9 + RAMDATA_POS)) = (int16)TRS_NGS_U.dflt;		//9=网压d轴分量ed滤波后
			*(RAM_START+((Uint32)RAM_BIAS * 10 + RAMDATA_POS)) = (int16)TRS_NGS_U.qflt;		//10=网压q轴分量eq滤波后
			*(RAM_START+((Uint32)RAM_BIAS * 11 + RAMDATA_POS)) = (int16)PI_NPR_Id.reference;//11=网侧Id指令
			*(RAM_START+((Uint32)RAM_BIAS * 12 + RAMDATA_POS)) = (int16)TRS_NPR_I.dflt;		//12=网侧Id反馈
			*(RAM_START+((Uint32)RAM_BIAS * 13 + RAMDATA_POS)) = (int16)PI_NPR_Id.out;		//13=网侧IdPI输出
			*(RAM_START+((Uint32)RAM_BIAS * 14 + RAMDATA_POS)) = (int16)TRS_NPR_U.d;		//14=网侧Ud输出
			*(RAM_START+((Uint32)RAM_BIAS * 15 + RAMDATA_POS)) = (int16)PI_NPR_Iq.reference;//15=网侧Iq指令
			*(RAM_START+((Uint32)RAM_BIAS * 16 + RAMDATA_POS)) = (int16)PI_NPR_Iq.feedback;	//16=网侧Iq反馈
			*(RAM_START+((Uint32)RAM_BIAS * 17 + RAMDATA_POS)) = (int16)PI_NPR_Iq.out;		//17=网侧IqPI输出
			*(RAM_START+((Uint32)RAM_BIAS * 18 + RAMDATA_POS)) = (int16)TRS_NPR_U.q;		//18=网侧Uq输出
			*(RAM_START+((Uint32)RAM_BIAS * 19 + RAMDATA_POS)) = (int16)CAP4.mprtrstheta;	//19=MPR定向角度
			*(RAM_START+((Uint32)RAM_BIAS * 20 + RAMDATA_POS)) = (int16)AD_OUT_MPR_I.a;		//20=MPR-A电流
			*(RAM_START+((Uint32)RAM_BIAS * 21 + RAMDATA_POS)) = (int16)AD_OUT_MPR_I.b;		//21=MPR-B电流
			*(RAM_START+((Uint32)RAM_BIAS * 22 + RAMDATA_POS)) = (int16)AD_OUT_MPR_I.c;		//22=MPR-C电流
			*(RAM_START+((Uint32)RAM_BIAS * 23 + RAMDATA_POS)) = (int16)RUN.mpriqrf;		//23=MPR-Iq参考值
			*(RAM_START+((Uint32)RAM_BIAS * 24 + RAMDATA_POS)) = (int16)CAP4.freqtmp;		//24=实际实时网频
			*(RAM_START+((Uint32)RAM_BIAS * 25 + RAMDATA_POS)) = (int16)PRO.speed;			//25=电机转速

			*(RAM_END) = RAMDATA_POS;												//当前数据存储位置转存

			RAMDATA_POS++;
			if(RAMDATA_POS >= RAM_BIAS)  RAMDATA_POS=0;		
		}
  	}
	
	if(_STDBY1!=0)		
	{
		M_ClrFlag(SL_DRAW);							//外部备用标志1非零置画图使能标志
		*(RAM_END - 1) = 0x0000;
		MAIN_LOOP.cnt_datasave=0;					//20100129atbjtu
		_BA_EPGRID = 0;								//记录网侧PDP硬件故障发生次数,清零2010atzuoyun
	} 
//--------------------------------External RAM Data Read-----------------------------------------
} 


/*********************************************************************************************************
** 函数名称: Draw
** 功苊枋�: 绘仆夹�
** 输　入:
** 输　出:        
** 注  释: 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改�:
** 日　期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
/*void Draw(void)
{
	if(M_ChkFlag(SL_DRAW)!=0)
	{
		if(draw<200)
		{
	//		data1[draw]=ADFINAL.ubc1;
	//		data2[draw]=UN.d;
	//		data3[draw]=ADFINAL.uab1;
	//		data4[draw]=UN.q;
	//		data1[draw]=UN.theta;
	//		data2[draw]=UN.theta2;
		//		data1[draw]=UN.d;
		//	data2[draw]=UN.q;
			data1[draw]=ADFINAL.ua1;
			data2[draw]=ADFINAL.ub1;
			data3[draw]=US1.a;
			data4[draw]=US1.b;

		//	data1[draw]=Iq2_LOOP.fb;
		//	data2[draw]=Iq2_LOOP.out;
		//	data3[draw]=EvbRegs.CMPR6;
		//	data4[draw]=EN.q;
			draw++;
		}
	}
	else	draw=0;
} 
*/

//===========================================================================
// No more.
//=========================================================================== 
