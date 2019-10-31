   /****************************************Copyright (c)**************************************************
**                       		     北	京	交	通	大	学
**                                        电气工程学院
**                                         614实验室
** 
**                              
**
**--------------文件信息--------------------------------------------------------------------------------
**文   件   名: user_work.c
**创   建   人: 
**最后修改日期: 
**描        述: 1.5MW双馈变流器核心控制程序----左云风场
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
#include "math.h"

//函数声明
/*********************************************************************************************************
** 函数名称: InitWork
** 功能描述: 初始化WORK部分，包括GIVE，RunCtrl，WAVE，SCOUT
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
void InitWork(void)
{
	M_SetFlag(SL_POWERON);		//置初次上电标志
	_MSG_SCOUT1=MSG_NONE;		//故障信息清除
	_MSG_SCOUT2=MSG_NONE;		//故障信息清除
	MAIN_LOOP.pulse = 0;		//分时脉冲赋初值
	M_ClrFlag(SL_OCS_NPRSTART);
    M_ClrFlag(SL_OCS_MPRSTART);

}


/*********************************************************************************************************
** 函数名称: PwmDrive
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
void PwmDrive(void)
{
    if((M_ChkFlag(SL_POWERON)==0)&&(M_ChkFlag(SL_CHARGEOK)!=0))		//DSP不是正在上电，已上完电,系统预充电完成
 	{ 	
		    //--下面执行的是开关频率配置，在停机状态下允许操作器设置开关频率，每次启动时更新一次开关频率----
			if(M_ChkFlag(SL_OCS_NPRSTART)!=0)      					//主控要求变流器启动
			{
			   if(M_ChkFlag(SL_CONFIGPWM)==0 && M_ChkFlag(SL_NPR_PWMOUT)==0 && M_ChkFlag(SL_MPR_PWMOUT)==0) //没有配置开关频率且脉冲封锁 20100507atzuoyun
		       {								  
				   ConfigPwm();	                					//配置开关频率				
				   if(M_ChkCounter(MAIN_LOOP.cnt_pwmout,DELAY_PWMOUT)>=0)	M_SetFlag(SL_CONFIGPWM);  //脉冲发生允许延时到	20091015 at zy						//脉冲允许,保证EnPwm()只能运行一次       
		       }
			   else	 MAIN_LOOP.cnt_pwmout=0;						//清除系统工作允许延时	
			}
			else   
			{
			   M_ClrFlag(SL_CONFIGPWM);
			   MAIN_LOOP.cnt_pwmout=0;								//清除系统工作允许延时
			}

//----------------下面是真正的脉冲使能最后一关----------------
	        
	 //--------网侧变流器脉冲允许,且没有故障--------------
	        if((M_ChkFlag(SL_NPR_RUNING)!=0)&&(M_ChkFlag(SL_ERRSTOP)==0)&&(M_ChkFlag(SL_SERIESTOP)==0))
	        {
		       if(M_ChkFlag(SL_NPR_PWMOUT)==0)  					//保证Enepwmio_NPR()只能运行一次
		       {
			      Enepwmio_NPR();
			          	
				  M_SetFlag(SL_RUN);
			      M_SetFlag(SL_NPR_PWMOUT);			        
		       }
	        }
			else       												//发生故障就立即封脉冲
	        {
		       Disepwmio_NPR(); 
		              
			   M_ClrFlag(SL_NPR_PWMOUT);
			   M_ClrFlag(SL_RUN);
	        }
     //------机侧变流器脉冲使能,且没有故障---------------
            if((M_ChkFlag(SL_MPR_RUNING)!=0)&&(M_ChkFlag(SL_ERRSTOP)==0)&&(M_ChkFlag(SL_SERIESTOP)==0))
	        {
		       if(M_ChkFlag(SL_MPR_PWMOUT)==0)						//保证Enepwmio_MPR()只能运行一次
		       {
			      Enepwmio_MPR();      
			      M_SetFlag(SL_MPR_PWMOUT);			         
		       }
	        }
	        else                  									//发生故障就立即封脉冲
	        {		       
			   Disepwmio_MPR();       
			   M_ClrFlag(SL_MPR_PWMOUT);        
	        }
	}
    else   //系统正在上电,或预充电闸没有切除
	{      
		DisPwm();
		M_ClrFlag(SL_NPR_PWMOUT);
		M_ClrFlag(SL_MPR_PWMOUT);
		M_ClrFlag(SL_CONFIGPWM);			        				//脉冲封锁
		MAIN_LOOP.cnt_pwmout=0;										//清除系统工作允许延时
	}
} 
//****************************************************************************
//编号：
//名称：SVPWM
//功能：脉冲调制输出
//输入：

//输出：
//注释：
//修改说明:
//****************************************************************************
//------------------网侧变流器SVPWM调制------------------------------------//	
void SVPWM_NPR(float alfa, float beta)
{      
  float tempmin,tempmax,temp1,temp2,temp3;
  float t0,t1,t2;    						 					//空间基本矢量的作用时间
  Uint16 sector;      											//扇区

  vdc = AD_OUT_UDC;  											//vdc为实际直流电压值,cdc取实际直流电压值用于调制
//-----------------------SVPWM 扇区判断和矢量作用时间计算------------------------------------------
  if(beta>=0)
  {
	   if(SQRT3 * alfa >= beta) 
	   {
	      sector=1;
	      t1=SQRT3 * (alfa * SQRT3_2-beta * 0.5)/vdc;   		//SQRT3=sqrt(3)=1.73205081，在宏定义里面实现,0.8660254=SQRT3/2
	      t2=SQRT3 * beta/vdc;
	   }
	   else if(SQRT3 * alfa <= -beta) 
	   {
	      sector=3;                                    
	      t1=SQRT3 * beta/vdc;
	      t2=SQRT3 * (-beta * 0.5 - alfa * SQRT3_2)/vdc;      	//SQRT3_2=sqrt(3)/2
	   }
	   else 
       {
	      sector=2;
	      t1=SQRT3 * (alfa * SQRT3_2 + beta * 0.5)/vdc;
	      t2=SQRT3 * (beta * 0.5 - alfa * SQRT3_2)/vdc;
       }
  }
  else
  {
	   if(SQRT3 * alfa >= -beta)
	   {
		  sector=6;
		  t1=-SQRT3 * beta/vdc;
		  t2=SQRT3 * (beta * 0.5 + alfa * SQRT3_2)/vdc;
	   }
	   else if(SQRT3 * alfa <= beta)	
	   {
	      sector=4; 
		  t1=SQRT3 * (beta * 0.5 - alfa * SQRT3_2)/vdc;	   
	      t2=-SQRT3 * beta/vdc;
	   }
	   else 
	   {
	      sector=5;
	      t1=SQRT3 * (-alfa * SQRT3_2 - beta * 0.5)/vdc;
	      t2=SQRT3 * (-beta * 0.5 + alfa * SQRT3_2)/vdc;
	   }
  }
//---------------------test----test-----test------------------------------------------------------
	zsector = sector;
//---------------------test----test-----test------------------------------------------------------

//--------------------过调制处理------------------------------------------------------------------ 
  	if(t1+t2>=1.0)           									//此时Ts=1，故判断时以1作为比较
  	{
	  t1=t1/(t1+t2);
	  t2=1.0-t1;
  	} 

  	  t0 = 1.0 - t1 - t2;	
  	  t0 = t0 * 0.5;											//零矢量作用时间
//------------------------ 比较寄存器值计算---------------------------------------------------------
  switch(sector)
    {
      	case 1:    temp1=(Uint16) (t0 * SW_NPR);				//SW_NPR为PWM周期寄存器的值,为开关周期的一半
           	       temp2=(Uint16)((t0 + t1) * SW_NPR);
               	   temp3=(Uint16)((t0 + t1 + t2) * SW_NPR);
            	   break;
     	case 2:    temp1=(Uint16)((t0 + t2) * SW_NPR);
           	       temp2=(Uint16) (t0 * SW_NPR);
               	   temp3=(Uint16)((t0 + t1 + t2) * SW_NPR);
       	           break;
        case 3:    temp1=(Uint16)((t0 + t1 + t2) * SW_NPR);           
            	   temp2=(Uint16) (t0 * SW_NPR);
               	   temp3=(Uint16)((t0 + t1) * SW_NPR);  
                   break;
    	case 4:    temp1=(Uint16)((t0 + t1 + t2) * SW_NPR);           
                   temp2=(Uint16)((t0 + t2) * SW_NPR);
               	   temp3=(Uint16) (t0 * SW_NPR);  
                   break;
    	case 5:    temp1=(Uint16)((t0 + t1) * SW_NPR);           
           	       temp2=(Uint16)((t0 + t1 + t2) * SW_NPR);
               	   temp3=(Uint16) (t0 * SW_NPR);  
                   break;
       	case 6:    temp1=(Uint16) (t0 * SW_NPR);           
            	   temp2=(Uint16)((t0 + t1 + t2) * SW_NPR);
              	   temp3=(Uint16)((t0 + t2) * SW_NPR);  
               	   break;  
        default:   break;         
   }
   tempmin = 0.5 * 75 * (_MINONTIME + _DEADTIME);   //_MINONTIME对应最小脉宽的时间，单位us
                                           	  		//_DEADTIME对应死区的时间，单位us
   tempmax = SW_NPR - tempmin;  

   if	  (temp1<tempmin) 	temp1=tempmin;
   else if(temp1>tempmax)	temp1=tempmax;
   if	  (temp2<tempmin) 	temp2=tempmin;
   else if(temp2>tempmax)	temp2=tempmax;
   if	  (temp3<tempmin) 	temp3=tempmin;
   else if(temp3>tempmax)	temp3=tempmax;
   EPwm1Regs.CMPA.half.CMPA = temp1;
   EPwm2Regs.CMPA.half.CMPA = temp2;
   EPwm3Regs.CMPA.half.CMPA = temp3;

}    
//****************************************************************************
//编号：
//名称：SVPWM_MPR
//功能：脉冲调制输出
//输入：

//输出：
//注释： 机侧变流器SVPWM调制
//修改说明:
//****************************************************************************
void SVPWM_MPR( float alfa, float beta)
{
  
   float tempmin,tempmax,temp1,temp2,temp3;
   float t0,t1,t2;
   Uint16 sector;

   vdc = AD_OUT_UDC;
//-----------------------SVPWM 扇区判断和矢量作用时间计算-------------------------------------------	       
   if(beta>=0)
	{
	   if(SQRT3*alfa >= beta) 
	   	{
	      sector=1;
	      t1=SQRT3 * (alfa * SQRT3_2-beta * 0.5)/vdc;   		//SQRT3=sqrt(3)=1.73205081，在宏定义里面实现,0.8660254=SQRT3/2
	      t2=SQRT3 * beta/vdc;
	   	}
	   else if(SQRT3*alfa <= -beta) 
	   	{
	      sector=3;                                    
	      t1=SQRT3 * beta/vdc;
	      t2=SQRT3 * (-beta * 0.5-alfa * SQRT3_2)/vdc;    		//SQRT3_2=sqrt(3)/2
	   	}
	   else 
    	{
	      sector=2;
	      t1=SQRT3 * (alfa * SQRT3_2 + beta * 0.5)/vdc;
	      t2=SQRT3 * (beta * 0.5-alfa * SQRT3_2)/vdc;
     	}
    }
  else
	{
	   if(SQRT3*alfa >= -beta)
	   	{
		  sector=6;
		  t1=-SQRT3 * beta/vdc;
		  t2=SQRT3 * (beta * 0.5 + alfa * SQRT3_2)/vdc;
	    }
	   else if(SQRT3*alfa <= beta)	
	    {
	      sector=4; 
		  t1=SQRT3 * (beta * 0.5 - alfa * SQRT3_2)/vdc;	   
	      t2=-SQRT3 * beta/vdc;
	   	}
	   else 
	   	{
	      sector=5;
	      t1=SQRT3 * (-alfa * SQRT3_2 - beta * 0.5)/vdc;
	      t2=SQRT3 * (-beta * 0.5+alfa * SQRT3_2)/vdc;
	   	}
	} 

  if(t1+t2>=1.0) 
	{
	   t1=t1/(t1+t2);
	   t2=1.0-t1;
	} 
  t0=1.0-t1-t2; 
  t0 = t0 * 0.5;	    										//零矢量作用时间
//------------------------ 比较寄存器值计算---------------------------------------------------------
  switch(sector)
    {
      	case 1:    temp1=(Uint16)(t0 * SW_MPR);
           	       temp2=(Uint16)((t0+t1) * SW_MPR);
               	   temp3=(Uint16)((t0+t1+t2) * SW_MPR);
            	   break;
     	case 2:    temp1=(Uint16)((t0+t2) * SW_MPR);
           	       temp2=(Uint16)(t0 * SW_MPR);
               	   temp3=(Uint16)((t0+t1+t2) * SW_MPR);
       	           break;
        case 3:    temp1=(Uint16)((t0+t1+t2) * SW_MPR);           
            	   temp2=(Uint16)(t0 * SW_MPR);
               	   temp3=(Uint16)((t0+t1) * SW_MPR);  
                   break;
    	case 4:    temp1=(Uint16)((t0+t1+t2) * SW_MPR);           
                   temp2=(Uint16)((t0+t2) * SW_MPR);
               	   temp3=(Uint16)(t0 * SW_MPR);  
                   break;
    	case 5:    temp1=(Uint16)((t0+t1) * SW_MPR);           
           	       temp2=(Uint16)((t0+t1+t2) * SW_MPR);
               	   temp3=(Uint16)(t0 * SW_MPR);  
                   break;
       	case 6:    temp1=(Uint16)(t0 * SW_MPR);           
            	   temp2=(Uint16)((t0+t1+t2) * SW_MPR);
              	   temp3=(Uint16)((t0+t2) * SW_MPR);  
               	   break;  
        default:   break;         
   }
   tempmin = 0.5 * 75 * (_MINONTIME + _DEADTIME);   //_MINONTIME对应最小脉宽的时间，单位us
                                           	  		//_DEADTIME对应死区的时间，单位us
   tempmax = SW_MPR - tempmin;  

   if	  (temp1<tempmin) 	temp1=tempmin;
   else if(temp1>tempmax)	temp1=tempmax;
   if	  (temp2<tempmin) 	temp2=tempmin;
   else if(temp2>tempmax)	temp2=tempmax;
   if	  (temp3<tempmin) 	temp3=tempmin;
   else if(temp3>tempmax)	temp3=tempmax;
   EPwm4Regs.CMPA.half.CMPA = temp1;
   EPwm5Regs.CMPA.half.CMPA = temp2;
   EPwm6Regs.CMPA.half.CMPA = temp3; 
}  

/*********************************************************************************************************
** 函数名称: 坐标变换子函数
** 功能描述: 3s/2s变换，2s/2r变换
** 输　入: 
** 输　出:   
** 注  释: 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日期:20090331
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Transform_3s_2s_2r(struct TRANS_DATA *var1)
{   
    var1->alfa = var1->a;												//采用等幅变换 ualfa=2/3(ua-0.5ub-0.5uc)   
	var1->beta = (var1->b - var1->c) * SQRT3_3;							//ubeta=2/3*sqrt(3)/2 (ub-uc)  SQRT3_3=sqrt(3)/3=0.57735026918962576450914878050196

    var1->d =  var1->alfa * var1->costheta + var1->beta * var1->sintheta;//ud=ualfa*cos(th)+ubeta*sin(th)
    var1->q = -var1->alfa * var1->sintheta + var1->beta * var1->costheta;//uq=-ualfa*sin(th)+ubeta*cos(th)
}  
/*********************************************************************************************************
** 函数名称: 坐标变换子函数
** 功能描述: 2r/2s变换，2s/3s变换
** 输　入: 
** 输　出: 
** 注  释: 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日期:20090331
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Anti_Transform_2r_2s(struct TRANS_DATA *var2)
{
    var2->alfa = var2->d * var2->costheta - var2->q * var2->sintheta;//ualfa=ud*cos(th)-uq*sin(th)   
	var2->beta = var2->d * var2->sintheta + var2->q * var2->costheta;//*ubeta=ud*sin(th)+uq*cos(th)     			
}

/*********************************************************************************************************
** 函数名称: PI_Loop
** 功能描述: PI调节器
** 输　入:   kp 比例放大系数
             ki 积分系数
			 outmax 输出限幅
			 errmax 误差最大值限幅
             errmin 误差最小值限幅
             incrementmax 输出增量限幅

** 输　出:   
** 注  释: 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日期:20090331
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void PI_Loop(struct PI_DATA *var,struct PI_PARA PI_var)
{
	float temp,ptemp,itemp,detemp,outtemp,du;

//----------------------计算本次误差---------------------------------------------------------------
	temp = var->reference - var->feedback;  			

//----------------------次误差正最大值限幅、最小值限幅-------------------------------------------
    if		(temp  	 	>  PI_var.errmax)  temp =  PI_var.errmax; 
    else if	(temp  	 	< -PI_var.errmax)  temp = -PI_var.errmax; 
	else 	 temp = temp;

	var->error = temp;													//修正后误差，抗干扰
    detemp = var->error -  var->errorp;    

//----------------------本次增量、限幅-------------------------------------------------------------
	ptemp = PI_var.kp *  detemp;										//比例项
	itemp = PI_var.kp * PI_var.ki *  var->error * 2.0e-4;				//积分项增量,09.6.17

	du = ptemp + itemp;                                             	//本次增量
	if     (du >  PI_var.incrementmax)    du =  PI_var.incrementmax;	//本次增量限幅
	else if(du < -PI_var.incrementmax)    du = -PI_var.incrementmax;	//本次增量限幅

//----------------------本次输出、限幅------------------------------------------------------------
	outtemp = var->out + du;

	if     (outtemp >  PI_var.outmax)  outtemp =  PI_var.outmax;		//输出限幅
	else if(outtemp < -PI_var.outmax)  outtemp = -PI_var.outmax;		//输出限幅

//---------------------输出赋值、刷新上次误差值--------------------------------------------------	
	var->out    = outtemp;		
	var->errorp = var->error;												
} 


/***************************************************************************************************
** 函数名称: Give_Integral
** 功能描述: 给定积分子函数
** 输　入:   积分步长step，给定指令值give
** 输　出:   给定积分后的指令值out  
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 20090604
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:20090604
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
float Give_Integral(float give,float step,float out)
{
	if(out < give)
	{
		out += step;
		if(out > give)	 	out = give;
	}
	else if(out > give)
	{
		out -=  step;
		if(out < give)		out = give;
	}
	else 	out = give;
		
	return(out);
}  

/*********************************************************************************************************
** 函数名称: Give
** 功能描述: 确定能否工作 cpc修改
** 输　入:   
** 输　出:   
** 注  释: 	 
**-------------------------------------------------------------------------------------------------------
** 作　者: 
** 日　? 
**-------------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:20090801修改
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void Give(void)
{	
	float temp_toqrf,temp_aglrf;
    
	if(M_ChkFlag(SL_NPR_START)==0 && M_ChkFlag(SL_MPR_START)==0)
	{
		M_ClrFlag(SL_NPR_RUN);			    						//网侧工作允许标志清0
		M_ClrFlag(SL_MPR_RUN);			    						//机工作允许标志清0
		GIVE.urf      = _URF;										//网侧给定电压为当前Vdc值
		GIVE.npriqrf  = 0;											//网侧给定电流为0
        GIVE.toqrf    = 0;                  						//机侧给定转矩为0
        GIVE.anglerf  = 0;		            						//机侧给定功率因数角度为0
	}
	else
	{

		if(M_ChkFlag(SL_OCS_NPREIN)!=0)					           //网侧无功并网,网侧给定直流电压和无功电流值
		{
		    M_ClrFlag(SL_MPR_RUN);

			if(M_ChkFlag(SL_NPR_START)!=0)  
			{
				M_SetFlag(SL_NPR_RUN); 								//网侧变流器运行
		    	GIVE.urf      = (int16) _URF;                       //给网侧中间直流电压指令赋值
		    	GIVE.npriqrf  = (int16) _NIQRF;						//给机侧无功电流指令赋值
			}
	        else  M_ClrFlag(SL_NPR_RUN);
		}	
			
		else 		                       							//背靠背工作时网侧直流电压,无功电流给定和机侧转矩和功率因数角给定
		{		
			if(M_ChkFlag(SL_NPR_START)!=0)  		   				//网侧运行判断
			{
			   M_SetFlag(SL_NPR_RUN);                  				//网侧脉冲允许		       
		       GIVE.urf       = (int16) _URF;          				//给中间直流电压指令赋值
			   GIVE.npriqrf   = (int16) _NIQRF;        				//网侧给定感性无功保证并网功率因数为1
//			   GIVE.npriqrf   = 70.0;                  				//网侧给定感性无功保证并网功率因数为1，互馈实验验证需要补70A
			}
		    else     M_ClrFlag(SL_NPR_RUN);


	        if(M_ChkFlag(SL_MPR_START)!=0)  						//机侧运行判断
	        {
	           M_SetFlag(SL_MPR_RUN);                   			//机侧脉冲允许
 
               if(_CANOPER==0)	  
               {
					if(M_ChkFlag(SL_IN1_STATORK)!=0)				//定子接触器闭合以后再接收转矩和角度指令
					{ 
               			GIVE.toqrf  =  (int16) _TOQRF;              //正值为发电
						GIVE.anglerf=  (int16) _AGLRF;  			//给功率因数角指令赋值,-360 -- 360
					}
					else
      		   		{
      		   			GIVE.toqrf    = 0;                  		//机侧给定转矩为0
        				GIVE.anglerf  = 0;		            		//机侧给定功率因数角度为0
      		   		} 
			   }
               else   
               {           
					if(M_ChkFlag(SL_IN1_STATORK)!=0)				//定子接触器闭合以后再接收转矩和角度指令
					{
               			temp_toqrf     =  (int16)SCI_canopen.rx_torque;               //正值为发电
      		   			GIVE.toqrf     =  temp_toqrf * CAN_TEN / 16383.0;             //机侧转矩指令实际值  _TOQRF == -200%-200%

						if(GIVE.toqrf<0)	GIVE.toqrf=0;    //变流器对主控的负转矩指令不予响应 20090815
			   
               			temp_aglrf     =  (int16)SCI_canopen.rx_angle;               
      		   			GIVE.anglerf   =  temp_aglrf * CAN_ANGLEN / 16383.0;  
      		   		}
      		   		else
      		   		{
      		   			GIVE.toqrf    = 0;                  		//机侧给定转矩为0
        				GIVE.anglerf  = 0;		            		//机侧给定功率因数角度为0
      		   		}             				
			   
			   }

			   if     (GIVE.anglerf <-30.0)                      GIVE.anglerf     = -30.0;
			   else if(GIVE.anglerf > 30.0)                      GIVE.anglerf     =  30.0;
			   else  											 GIVE.anglerf     = GIVE.anglerf;
                                         
	        }
            else    M_ClrFlag(SL_MPR_RUN);
		}
	}
}
/*********************************************************************************************************
** 函数名称: RunCtrl
** 功能描述: 工作控制  cpc修改
** 输　入:   
** 输　出:   
** 注  释: 	 
**--------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 
**--------------------------------------------------------------------------------------------------
** 修改人:
** 日　期:20090721修改
**--------------------------------------------------------------------------------------------------
***********************************************************************************************/
void RunCtrl(void)
{
    float udc_max,id_max,iq_max,toq_max,agl_max,temp;
	Uint16 temp_n,temp_ud,temp_iqexi,temp_number;
	float  temp_exi,temp_iqk;
	float  temp_Qc,temp_Qg,temp_tan;

//----------------------------网侧运行给定控制---------------------------------------------------------

//-----网侧正在运行且要求正常停机
    if((M_ChkFlag(SL_NPR_RUNING)!=0)&&(M_ChkFlag(SL_NPR_RUN)==0))  				//网侧无功电流将为0后再停脉冲
    {
		RUN.npriqrf = Give_Integral(0,RUN.npriqstep,RUN.npriqrf);  				//指令减小直至等于0
            
        if(RUN.npriqrf==0)   
        {   
        	if(M_ChkCounter(MAIN_LOOP.cnt_isteady0,DELAY_ISTEADY0)>0)  M_ClrFlag(SL_NPR_RUNING);
		}
		else  MAIN_LOOP.cnt_isteady0= 0;
    }

//-----要求网侧运行
    else if(M_ChkFlag(SL_NPR_RUN)!= 0) 
    {  
       M_SetFlag(SL_NPR_RUNING);                 								//网侧正在运行，开始发网侧脉冲
       MAIN_LOOP.cnt_isteady0= 0;
	                
	   RUN.urf = Give_Integral(GIVE.urf,RUN.ustep,RUN.urf);     				//电压给定积分
       if(RUN.urf == GIVE.urf)	    M_SetFlag(SL_STEADYGV);        				//给定积分完成,置稳定标志位
	   else							M_ClrFlag(SL_STEADYGV);

 	   if(M_ChkFlag(SL_STEADYGV)!=0)											//待电压给定完成后再给定无功电流
	 	   RUN.npriqrf = Give_Integral(GIVE.npriqrf,RUN.npriqstep,RUN.npriqrf); //网侧功电流给定积分	         
    }
//-----待机    
    else                                   										//待机
    {
	   M_ClrFlag(SL_NPR_RUNING);
	   M_ClrFlag(SL_STEADYGV);
	   MAIN_LOOP.cnt_isteady0= 0;

	   RUN.urf   = AD_OUT_UDC;													//给定积分初始值为当前Vdc值
       RUN.npriqrf  = 0;														//给定积分初始值为0
        
       udc_max    =  (float)FUNC[NO_URF].max;									//读取最大值
       iq_max     =  (float)FUNC[NO_NIQRF].max;
       temp       =  (float)_RU_UDCT;
	   RUN.ustep   = abs(udc_max)/temp;											//计算直流电压给定步长
	   temp       =  (float)_RU_IQT;
	   RUN.npriqstep  = abs(iq_max)/temp;										//计算无功电流给定步长

	   PI_NPR_U.reference = 0;
	   PI_NPR_U.feedback = 0;
	   PI_NPR_U.out = 0;
	   PI_NPR_U.integrator = 0;
	   PI_NPR_Id.reference = 0;
	   PI_NPR_Id.feedback = 0;
	   PI_NPR_Id.out = 0;
	   PI_NPR_Id.integrator = 0;
	   PI_NPR_Iq.reference = 0;
	   PI_NPR_Iq.feedback = 0;
	   PI_NPR_Iq.out = 0;
	   PI_NPR_Iq.integrator = 0;
    }

//----------------------------机侧运行有功电流值给定积分-------------------------------------

//-----机侧正在运行并要求停机
    if((M_ChkFlag(SL_MPR_RUNING)!=0)&&(M_ChkFlag(SL_MPR_RUN)==0)) 
    {      
		if(M_ChkFlag(SL_IN1_STATORK)!=0)
		{
		    RUN.toqrf = Give_Integral(0,RUN.toqstep,RUN.toqrf);  				//机侧转矩给定积分
		    RUN.aglrf = Give_Integral(0,RUN.aglstep,RUN.aglrf);  			    //机侧功率因数角给定积分


		    RUN.mpriqrf_g  =  - RUN.toqrf  / (IRQTOTE * TRS_NGS_U.dflt);   		//给定q轴电流指令计算
//	    	RUN.mpriqrf_g  =  RUN.toqrf * STAROTRTO / (1.5 * POLEPAIRES * MPR_Lm * TRS_NGS_U.dflt / (314.15926 *  MPR_Ls));   //给定q轴电流指令计算
 
			RUN_mpridrf_exi  = - (TRS_NGS_U.dflt * SQRT3 * STAROTRTO / (MPR_Lm * 314.15926));  //负号:网压ed与机侧d轴相差180度             
      	    RUN.mpridrf_exi  =  RUN_mpridrf_exi * _eidco;          				//乘调整系数,由操作器给定 
        	temp_ud = (Uint16)(TRS_NGS_U.dflt * SQRT3 / SQRT2);
        	if     (temp_ud < 540) 		temp_ud = 540;
        	else if(temp_ud > 800) 	    temp_ud = 800;
	    	temp_n = (temp_ud - 540) * 127 / 260;
        	temp_exi = TAB_EXI[temp_n];
        	RUN.mpridrf_exi =  RUN.mpridrf_exi * temp_exi;          			//查表调整系数,与空载感应电势有关
/*//20091022atzy
			RUN_mpriq = (Uint16)(- RUN.mpriqrf_g);
			if     (RUN_mpriq < 290)   temp_iqexi = 290;
			else if(RUN_mpriq > 600)   temp_iqexi = 600;
			temp_iqexi = RUN_mpriq;  //20091019atzy
			temp_number = (temp_iqexi - 290) * 127 / 310;
			temp_iqk = TAB_IQEXI[temp_number];
        	RUN.mpridrf_exi = RUN.mpridrf_exi * temp_iqk;          				//查表调整系数,由负载电流Irq给定确定 
*/ //20091022atzy
	 	    RUN.radianrf    = RUN.aglrf * 0.017453292;							//角度转弧度360->2PAI	 0.01745329=2 * PAI / 360
			   	    
//		    temp_Qc = TRS_NGS_U.dflt * 314.15926 * 501.0e-6 / SQRT2;
//		    temp_Qg = PRO.Pgactive * sin(RUN.radianrf) / cos(RUN.radianrf);
//			temp_tan    = (temp_Qg - temp_Qc) / PRO.Psactive;
//		    RUN.mpridrf_var = RUN.mpriqrf_g * temp_tan;

	    	RUN.mpridrf_var = RUN.mpriqrf_g * sin(RUN.radianrf) / cos(RUN.radianrf); //停机时，保证定子电流为0，即功率因数为1
        
 	        RUN.mpridrf_g   = RUN.mpridrf_exi + RUN.mpridrf_var;                                           

			RUN.mpridrf = Give_Integral(RUN.mpridrf_g,RUN.mpridstep,RUN.mpridrf); //机侧并网前d轴给定就为-90A
			RUN.mpriqrf = Give_Integral(RUN.mpriqrf_g,RUN.mpriqstep,RUN.mpriqrf); //指令减小直至等于0	  
		    if(RUN.mpriqrf==0)        M_SetFlag(SL_STACTRIPEN);
			else    			      M_ClrFlag(SL_STACTRIPEN);
		    MAIN_LOOP.cnt_mprsteady0= 0; 		
		}
		else
		{
			RUN.mpriqrf = Give_Integral(0,RUN.mpriqstep,RUN.mpriqrf);  			   	//指令减小直至等于0
			RUN.mpridrf = Give_Integral(0,RUN.mpridstep,RUN.mpridrf);  				//指令减小直至等于0
		    if((RUN.mpridrf==0)&&(RUN.mpriqrf==0))  								//机侧有功和无功电流给定都降零 
		    { 
		        if(M_ChkCounter(MAIN_LOOP.cnt_mprsteady0,DELAY_MPRSTEADY0)>0)   M_ClrFlag(SL_MPR_RUNING); //0.05s
		    }
		    else   MAIN_LOOP.cnt_mprsteady0= 0; 		
		}
		
    }
   	 	   	
//-----机侧要求运行且Vdc稳定
    else if((M_ChkFlag(SL_MPR_RUN)!=0) && (M_ChkFlag(SL_STEADYFB)!=0))  
    {  
        M_SetFlag(SL_MPR_RUNING);
        M_ClrFlag(SL_STACTRIPEN);                 									//机侧正在运行，开始发机侧脉冲
        MAIN_LOOP.cnt_mprsteady0= 0;
 			   
		RUN.toqrf = Give_Integral(GIVE.toqrf,RUN.toqstep,RUN.toqrf);  				//机侧转矩给定积分
		RUN.aglrf = Give_Integral(GIVE.anglerf,RUN.aglstep,RUN.aglrf);  			//机侧功率因数角给定积分
 
	    RUN.mpriqrf_g  =  - RUN.toqrf  / (IRQTOTE * TRS_NGS_U.dflt);   				//给定q轴电流指令计算
//	    RUN.mpriqrf  =  GIVE.toqrf * STAROTRTO / (1.5 * POLEPAIRES * MPR_Lm * TRS_NGS_U.dflt / (314.15926 *  MPR_Ls));   //给定q轴电流指令计算
 
		RUN_mpridrf_exi  = - (TRS_NGS_U.dflt * SQRT3 * STAROTRTO / (MPR_Lm * 314.15926));  //负号:网压ed与机侧d轴相差180度             
        RUN.mpridrf_exi  =  RUN_mpridrf_exi * _eidco;          						//乘调整系数,由操作器给定 
        temp_ud = (Uint16)(TRS_NGS_U.dflt * SQRT3 / SQRT2);							//网压线电压有效值 690V
        if     (temp_ud < 540) 		temp_ud = 540;
        else if(temp_ud > 800) 	    temp_ud = 800;
	    temp_n = (temp_ud - 540) * 127 / 260;
        temp_exi = TAB_EXI[temp_n];
        RUN.mpridrf_exi =  RUN.mpridrf_exi * temp_exi;          					//查表调整系数,由操作器给定 
/*//20091022atzy 
		RUN_mpriq = (Uint16)(- RUN.mpriqrf_g);
		if     (RUN_mpriq < 290)   temp_iqexi = 290;
		else if(RUN_mpriq > 600)   temp_iqexi = 600;
        temp_iqexi = RUN_mpriq;  //20091019atzy
		temp_number = (temp_iqexi - 290) * 127 / 310;
		temp_iqk = TAB_IQEXI[temp_number];
        RUN.mpridrf_exi = RUN.mpridrf_exi * temp_iqk;          						//查表调整系数,由负载电流Irq给定确定 
*///20091022atzy 
   
 	    RUN.radianrf    = RUN.aglrf * 0.017453292;									//角度转弧度360->2PAI	 0.01745329=2 * PAI / 360
	    RUN.mpridrf_var = RUN.mpriqrf_g * sin(RUN.radianrf) / cos(RUN.radianrf); //停机时，保证定子电流为0，即功率因数为1
			   	    																//RUN.aglrf>0,转子励磁增加,发出感性无功;反之,容性
//	    temp_Qc = TRS_NGS_U.dflt * 314.15926 * 501.0e-6 / SQRT2;					//运行时，已总并网功率因数1为目标
//	    temp_Qg = PRO.Pgactive * sin(RUN.radianrf) / cos(RUN.radianrf);
//		temp_tan    = (temp_Qg - temp_Qc) / PRO.Psactive;
//	    RUN.mpridrf_var = RUN.mpriqrf_g * temp_tan;
        
        RUN.mpridrf_g   = RUN.mpridrf_exi + RUN.mpridrf_var;                                           


		RUN.mpridrf = Give_Integral(RUN.mpridrf_g,RUN.mpridstep,RUN.mpridrf);  		//机侧有功电流给定积分
		RUN.mpriqrf = Give_Integral(RUN.mpriqrf_g,RUN.mpriqstep,RUN.mpriqrf);  		//机侧无功电流给定积分

    } 
//-----待机
    else                                    										//待机
    {
	   	M_ClrFlag(SL_MPR_RUNING);
	    MAIN_LOOP.cnt_mprsteady0= 0;
        
        RUN.mpridrf= 0;															    //给定积分初始值为0
        RUN.mpriqrf= 0;															    //给定积分初始值为0
        RUN.mpridrf_g= 0;															//给定积分初始值为0
        RUN.mpriqrf_g= 0;	
        id_max     =  (float)FUNC[NO_MIDRF].max;
	    temp       =  (float)_RU_IDT;
	    RUN.mpridstep  = abs(id_max)/temp;											//计算有功电流给定步长 
        iq_max     =  (float)FUNC[NO_MIQRF].max;
	    temp       =  (float)_RU_IQT;
	    RUN.mpriqstep  = abs(iq_max)/temp;											//计算无功电流给定步长  
 
        RUN.toqrf  = 0;																//给定积分初始值为0
        RUN.aglrf  = 0;																//给定积分初始值为0
        toq_max    =  (float)FUNC[NO_TOQRF].max;
	    temp       =  (float)_RU_TOQT;		//单位ms
//	    temp       =  temp * 1000.0;        //为CPC中心实验台提高有功给定速度 cpc 20090815 让转矩给定以ms为单位
	    RUN.toqstep=  abs(toq_max)/temp;											//计算转矩给定步长 
        agl_max    =  (float)FUNC[NO_AGLRF].max;
	    temp       =  (float)_RU_AGLT;
		temp       =  temp * 1000.0;		//单位s									//为CPC中心实验台降低有功给定速度 cpc test
	    RUN.aglstep=  abs(agl_max)/temp; 											//计算功率因数角给定步长 

	    PI_MPR_U.reference = 0;
	    PI_MPR_U.feedback = 0;
	    PI_MPR_U.out = 0;
	    PI_MPR_U.integrator = 0;
	    PI_MPR_Id.reference = 0;
	    PI_MPR_Id.feedback = 0;
	    PI_MPR_Id.out = 0;
	    PI_MPR_Id.integrator = 0;
	    PI_MPR_Iq.reference = 0;
	    PI_MPR_Iq.feedback = 0;
	    PI_MPR_Iq.out = 0;
 	    PI_MPR_Iq.integrator = 0;

    } 
}  
 
 

/*************************************************************************************************
** 函数名称:  QepEncodPos
** 功能描述:  调整位置计数器为增计数模式，编码器位置检测
** 输　入:    稳态下的转子电流，定子电压
** 输　�:    编码器位置rad  
** 注  释:    定子接触器合闸前运行，合闸后禁止运行。每个采样周期运行一次。非特别指明的角度均为电角度
**--------------------------------------------------------------------------------------------------
** 作　者: 	 
** 日　期: 	 20090330
**--------------------------------------------------------------------------------------------------
** 修改人:
** 日  期:  20090409
**--------------------------------------------------------------------------------------------------
***********************************************************************************************/
 void QepEncodPos(void)
{
//	Uint16 temp1;
	float temp_pos;
/*//20091021atzy经检测判断，计数AB信号不必对调，原先就是增计数，现场将对调程序屏蔽了
//----------------------判断POSCNT计数方向，确保增计数模式------------------------------------------
	if(M_ChkFlag(SL_QCAPSPDIN)!=0 && M_ChkFlag(SL_SPEED_HIGH)!=0)	//20091020atzy风机待机时电机轴会左右转动//转速检测正常后，调整编码器工作模式
	{
		temp1 = EQep2Regs.QEPSTS.bit.QDF;   				//计数方向  0-逆时针-减   1-顺时针-增
		if (temp1 == 0 && M_ChkFlag(SL_QEPSWP)==0) 			//如果当前处于减计数模式，并且未对调过AB
		{
			temp1 = EQep2Regs.QDECCTL.bit.SWAP;				//不论AB是否对调过，对调当前的AB信号
		
			if (temp1 == 0) 
			{
				EQep2Regs.QPOSCNT = 1000;					//防止对调后立马报上溢故障			
				EQep2Regs.QDECCTL.bit.SWAP  = 1;			//交换A、B输入信号			
				EQep2Regs.QPOSCNT = 0; 					
			}
			else 
			{ 
				EQep2Regs.QPOSCNT = 1000;				 	//防止对调后立马报上溢故障
				EQep2Regs.QDECCTL.bit.SWAP  = 0;			//还原A、B输入信号
				EQep2Regs.QPOSCNT = 0; 						
			} 
		
			M_SetFlag(SL_QEPSWP);					    	//置AB对调完成标志位，防止多次对调AB
		}
//		else if (temp1==0 && M_ChkFlag(SL_QEPSWP)!=0)
//			M_SetFlag(SL_QEPPCDE);  						//置位置计数器方向错误标志位,为提高抗干扰性能,该故障不做判断 20090804于cpc

	}

*/
//--------QCAP正常工作后计数编码器位置--------------------------------------------------------------
	if(M_ChkFlag(SL_QCAPSPDIN)!=0)			//转速检测正常后，计算编码器位置
	{
  		temp_pos = _encodpos;
		QEPDATA.encodpos =  - temp_pos;   	//由操作器直接给定初始位置角度										
	}
	
} 

/***************************************************************************************************
** 函数名称: PhaseOrderChk
** 功能描述: 检查相序是否正确。
** 输　入:
** 输　出:   
** 注  释: 在合主断前运行，合主断后不再调用此子函数 cap5-ubc   cap6-uab
**--------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期: 	20090627
**--------------------------------------------------------------------------------------------------
** 修改人:	
** 日  期:	
**--------------------------------------------------------------------------------------------------
***********************************************************************************************/
 void PhaseOrderChk(void)
{
	float temp,phaseshift;

//------------计算网压频率、角频率、CAP单位时间内网压相位增量--------------------------------------
	temp=(float)ECap5Regs.CAP1;									//记录捕获值

	if(temp>0)
	{
		CAP5.prd = temp;										
		CAP5.freqtmp   = 150.0e6/CAP5.prd;						//计算本次频率
	}

	if (abs(CAP5.freqtmp-50)<1)									//如果频率正常则更新频率
	{
		CAP5.freq   = CAP5.freqtmp;    							//更新
		CAP5.omigasyn  = TWOPAI * CAP5.freq;						
		CAP5.radpertb  = TWOPAI/CAP5.prd;						//计算一个计数周期角度增量
		M_ClrFlag(SL_GRDFQE);									//清频率失败标志
	}
	else	M_SetFlag(SL_GRDFQE);								//否则置频率有误标志位	
		

//----------------计算Ubc，Uab相位-----------------------------------------------------------------
	temp = (float)ECap5Regs.TSCTR;
	CAP5.phase = temp * CAP5.radpertb;							//主断前网压Ubc相位 
	temp = (float)ECap6Regs.TSCTR;
	temp = temp * CAP5.radpertb;								//主断前网压Uab相位
	
//---------------计算相位差------------------------------------------------------------------------
	phaseshift = temp - CAP5.phase;								//计算主断前网压Uab与Ubc的相位差
	if (phaseshift<0)   phaseshift = phaseshift + TWOPAI;		//相差限制在0到2 * PAI之间

//---------------检查相序是否正确------------------------------------------------------------------
	if(abs(phaseshift - TWOPAI_3) >  PAI_3)  					//当相差误差超过PAI/3时报错
	{
		if(M_ChkFlag(SL_POWERON)==0)	M_SetFlag(SL_PHORDE);	//DSP上电后	相序有误标志位置1 20090817
	}
	else 
		M_ClrFlag(SL_PHORDE);									// 清零相序有误标志位 
}  

/**************************************************************************************************
** 函数名称: CapQepCtrl
** 功能描述: compute vector position and frequency
** 输  入:	  
** 输　出:    
** 注  释:    
			//Cap4.adsmptsctr=ECap4Regs.TSCTR; 		//加在AD采样启动语句后
			//QEPDATA.adsmposcnt=EQep2Regs.QPOSCNT;	//加在AD采样启动语句后
**--------------------------------------------------------------------------------------------------
** 作　者: 
** 日　期:	20090409 
**--------------------------------------------------------------------------------------------------
** 修改人:
** 日  期:	20090812 at cpc
**--------------------------------------------------------------------------------------------------
**************************************************************************************************/
 void CapQepCtrl(void)
{
	float	temp,temp1,radpert0;
	Uint32  temp2,temp3;

//------------计算网侧网压率、髦角频率、CAP单位时间内网压相位增量-----------------------------------
	temp2 = ECap4Regs.CAP1;
	temp  =	(float)temp2;									//捕获事件周期值
	
	if(temp>0)
	{
		CAP4.prd = temp;									
		CAP4.freqtmp   = 150.0e6/CAP4.prd;					//计算本次网压频率
	}

	if (abs(CAP4.freqtmp - 50)<1)							//如果频率正常则更新网压频率
	{
		CAP4.freq      = CAP4.freqtmp;				    	//更新网压频率 CAP4.freqtmp是实际实时网频
		CAP4.omigasyn  = TWOPAI * CAP4.freq;						
		CAP4.radpertb  = TWOPAI * CAP4.freq / 150.0e6;		//计算一个计数周期角度增量
		M_ClrFlag(SL_GRDFQE);					   	   	 	//清频率失败标志
	}
	else	M_SetFlag(SL_GRDFQE);							//频率有误标志位置1	
		


//-----------计算网侧算法正变换角度----------------------------------------------------------------                         
	
	radpert0       = TWOPAI * CAP4.freq * 0.2e-3;   		//计算一个T0中断周期角度增量 T0=0.2ms
  
    if(M_ChkFlag(SL_GRDFQE)!=0)
	{
	   CAP4.nprtrstheta =CAP4.nprtrstheta + radpert0;              
       if(CAP4.nprtrstheta >= TWOPAI)	CAP4.nprtrstheta = CAP4.nprtrstheta - TWOPAI; 
	}
	else	
	{	
	  if(CAP4.adsmptsctr > 3.5e6)
	  {
	  	 CAP4.nprtrstheta =CAP4.nprtrstheta + radpert0;               
       	 if(CAP4.nprtrstheta >= TWOPAI)	CAP4.nprtrstheta = CAP4.nprtrstheta - TWOPAI; 
	  }
	  else
	  {
	     CAP4.nprtrstheta = (float)CAP4.adsmptsctr * CAP4.radpertb;
	  	 if(CAP4.nprtrstheta >= TWOPAI)	CAP4.nprtrstheta = CAP4.nprtrstheta - TWOPAI; 
      }
	}
//-------------定子磁链位置-------------------------------------------------------------------------
//	CAP4.stavectheta = 	CAP4.nprtrstheta + TWOPAI_3;			//定子磁链相对于A相轴线的位置


//-------------转子机械角频率和电角频率------------------------------------------------------------	
//	QEPDATA.qcapprd = EQep2Regs.QCPRD;	                		//对eQEP模块的QCLK进行512分频，QCAP时钟采用SYSCLKOUT/128  20090817
//	temp = 300.0e6 *  PAI/(PLSPRVL  * (float)QEPDATA.qcapprd); 	//转子机械角频率rad/s  QEPDATA.omigamec = 2 * PAI * (150M/128) *  512/(QCPRD * PLSPRVL * 4 )  20090817	 						                            

//    if(M_ChkFlag(SL_SPEED_HIGH)!=0)		temp = 2400.0e6 *  PAI/(PLSPRVL  * (float)QEPDATA.qcapprd);  //转子机械角频率rad/s  QEPDATA.omigamec = 2 * PAI * 37.5M * 128/(QCPRD * PLSPRVL * 4 )  20090815atcpc
//    else  								temp = 150.0e6 *  PAI/(PLSPRVL  * (float)QEPDATA.qcapprd);   //转子机械角频率rad/s  QEPDATA.omigamec = 2 * PAI * 37.5M * 8/(QCPRD * PLSPRVL * 4 )  20090815atcpc	

	if(M_ChkFlag(SL_SPEED_HIGH)!=0)
	{
		QEPDATA.qposlat2 = EQep2Regs.QPOSLAT;                 		//采用SYSCLK=150M，QUPRD=1.5M,单位频率为100Hz
/*
		if(QEPDATA.qposlat2 < QEPDATA.qposlat1)
			QEPDATA.qposlat  = QEPDATA.qposlat2 + PLSPRVL * 4 - QEPDATA.qposlat1;  //20090817
		else
			QEPDATA.qposlat  = QEPDATA.qposlat2 - QEPDATA.qposlat1;
		
		QEPDATA.qposlat1  = QEPDATA.qposlat2;
		
		temp = 50.0 *  PAI * (float)QEPDATA.qposlat / PLSPRVL; 		//转子机械角频率rad/s  QEPDATA.omigamec = 2 * PAI * (150M/1.5M) *  QEPDATA.qposlat/(PLSPRVL * 4 )  20090817	 						                            		
	}
	else
	{
		QEPDATA.qcapprd = EQep2Regs.QCPRD;	                		//对eQEP模块的QCLK进行32分频，QCAP时钟采用SYSCLKOUT/128  20090817
		temp = 37.5e6 *  PAI/(PLSPRVL  * (float)QEPDATA.qcapprd); //转子机械角频率rad/s  QEPDATA.omigamec = 2 * PAI * (150M/128) *  64/(QCPRD * PLSPRVL * 4 )  20090817	 						                            		
	}
*/

//====================重要修正 201105atzuoyun======================================================================
		if(QEPDATA.qposlat2 < QEPDATA.qposlat1)				//201011LVRT DSP内部的EQep2Regs.QPOSLAT更新速度没有T0这么快！刷新100Hz和采样为5kHz
		{
			QEPDATA.qposlat  = QEPDATA.qposlat2 + PLSPRVL * 4 - QEPDATA.qposlat1;  //20090817
			temp = 50.0 *  PAI * (float)QEPDATA.qposlat / PLSPRVL; 		//转子机械角频率rad/s  QEPDATA.omigamec = 2 * PAI * (150M/1.5M) *  QEPDATA.qposlat/(PLSPRVL * 4 )  20090817	 						                            		
		}			
		else if(QEPDATA.qposlat2 == QEPDATA.qposlat1)
		{
			temp = QEPDATA.omigamec;
		}
		else
		{
			QEPDATA.qposlat  = QEPDATA.qposlat2 - QEPDATA.qposlat1;
			temp = 50.0 *  PAI * (float)QEPDATA.qposlat / PLSPRVL; 		//转子机械角频率rad/s  QEPDATA.omigamec = 2 * PAI * (150M/1.5M) *  QEPDATA.qposlat/(PLSPRVL * 4 )  20090817	 						                            		
		}
			
		QEPDATA.qposlat1  = QEPDATA.qposlat2;
	}
	else
	{
		QEPDATA.qcapprd = EQep2Regs.QCPRD;	                		//对eQEP模块的QCLK进行32分频，QCAP时钟采用SYSCLKOUT/128  20090817
		temp = 37.5e6 *  PAI/(PLSPRVL  * (float)QEPDATA.qcapprd); //转子机械角频率rad/s  QEPDATA.omigamec = 2 * PAI * (150M/128) *  64/(QCPRD * PLSPRVL * 4 )  20090817	 						                            		
	}

	temp2 = EQep2Regs.QEPSTS.bit.COEF;						//Capture overflow error--0.43ms question
	if(temp2!=0)	
	{
		EQep2Regs.QEPSTS.bit.COEF = 1;						//clear overflow flag
		M_ClrFlag(SL_QCAPSPDIN); 
		MAIN_LOOP.cnt_qcapspdin = 0;
	}	 		
	else
	{
		if(M_ChkCounter(MAIN_LOOP.cnt_qcapspdin,DELAY_QCAPSPDIN)>=0)  //120ms 20090817
			M_SetFlag(SL_QCAPSPDIN); 
	} 			
		 
   	if(M_ChkFlag(SL_QCAPSPDIN)==0)			//20090813 cpc
   	{
   		QEPDATA.omigamec = 0;  				//检测范围之外 
		M_ClrFlag(SL_SPEED_HIGH);           //20090817
		MAIN_LOOP.cnt_qcapdisturb=0;
	}
	else
	{
		if(QEPDATA.omigamec==0)		
		{	
			QEPDATA.omigamec = temp;
			MAIN_LOOP.cnt_qcapdisturb=0;
		}
		else if(temp >= (0.75 * QEPDATA.omigamec) && temp <= (1.25 * QEPDATA.omigamec))	//8分频时丢失1个A或B信号测速将是上一次的1.25倍，多出1个则是0.75倍
		{
			QEPDATA.omigamec = temp;												//如果转速在正确范围内，更新;否则保持上一次的值 20090804于cpc
			MAIN_LOOP.cnt_qcapdisturb=0;
			M_ClrFlag(SL_QCAPDISTURB);		
		}
		else
		{
			if(M_ChkCounter(MAIN_LOOP.cnt_qcapdisturb,DELAY_QCAPDISTURB)>=0)		//延时100ms,连续10次均被干扰,报故障 20090817cpc
			{
				M_SetFlag(SL_QCAPDISTURB);
				QEPDATA.omigamec = temp;    //20090817
			}
		}
	}
	 
   		  		
	QEPDATA.omigarote = POLEPAIRES * QEPDATA.omigamec;								//转子电角频率rad/s
//	DataFilter(0.985,&QEPDATA.omigaroteflt,QEPDATA.omigarote); 						//转速反馈值滤波， Ts=200us,Tr=100ms 20090815
	QEPDATA.rotradpret0 = QEPDATA.omigarote * 0.2e-3;								//每次T0周期转子电角度增量，单位弧度 20090815

//-----------转差角频率----------------------------------------------------------------------------
	CAP4.omigaslp = CAP4.omigasyn - QEPDATA.omigarote;  	//转差角频率  20090815


//------------检测编码器是否有故障------------------------------------------------------------------	
	temp = EQep2Regs.QFLG.bit.PCO;
//	if(temp!=0)		
	if((temp!=0) && M_ChkFlag(SL_QCAPSPDIN)!=0)		//201105atzuoyun
	{
	   _NQEPPCO++;
	   if(_NQEPPCO >10)		   M_SetFlag(SL_QEPPCO);    	//置计数器上溢标志位
	   EQep2Regs.QCLR.bit.PCO = 1;
	   MAIN_LOOP.cnt_qepcntok=0;
	}
	else
	{
		if(M_ChkCounter(MAIN_LOOP.cnt_qepcntok,DELAY_QEPCNTOK)>=0) 				//1s 连续1s未发生上溢错误
		{
			_NQEPPCO=0;
			M_ClrFlag(SL_QEPPCO);
		}
	}
/*						
	temp = EQep2Regs.QFLG.bit.PHE;
	if(temp!=0)		
	{
	    M_SetFlag(SL_QEPPHE);								//QEP的AB信号相位故障,相差非90度,故障程序中不再检测该故障
	    EQep2Regs.QCLR.bit.PHE = 1; 
	}
*/
//----------机侧算法正变换角度--------------------------------------------------------------------- 
    QEPDATA.posilat = EQep2Regs.QPOSILAT;								//Z信号到来时POSCNT的计数值
	if(QEPDATA.posilat < (PLSPRVL * 4 -20))
	{	
		QEPDATA.rotpos = QEPDATA.rotpos + QEPDATA.rotradpret0;
		if(QEPDATA.rotpos > 2* TWOPAI) QEPDATA.rotpos = QEPDATA.rotpos - 2* TWOPAI;
//		if((M_ChkCounter(MAIN_LOOP.cnt_qepzdisturb,DELAY_QEPZDISTURB)>=0)&&(M_ChkFlag(SL_QCAPSPDIN)!=0))    //1s //转速检测正常后，计算编码器位置
		if(M_ChkCounter(MAIN_LOOP.cnt_qepzdisturb,DELAY_QEPZDISTURB)>=0)   //1s
			M_SetFlag(SL_QEPZDISTRUB);
	}
	else
	{														//temp1 = POLEPAIRES  * 2 * PAI * QEPDATA.adsmposcnt/(PLSPRVL * 4);
		temp1 = 1.53398e-3 * QEPDATA.adsmposcnt; 			// POLEPAIRES=2, PLSPRVL=2048cpc								
		QEPDATA.rotpos  = 	temp1;                          //0--4PIE
		MAIN_LOOP.cnt_qepzdisturb=0;
		M_ClrFlag(SL_QEPZDISTRUB);
	}

	temp1 = QEPDATA.rotpos - QEPDATA.encodpos;				//AD采样时刻转子位置 - 初始位置角度
                                                            // -2PIE -- 6PIE
	if 		(temp1 < 0) 	  	temp1 = temp1 + TWOPAI;	
	else if (temp1 > TWOPAI)  	temp1 = temp1 - TWOPAI;		//AD采样时刻转子位置取模(0-TWOPAI)
	if 		(temp1 < 0) 	  	temp1 = temp1 + TWOPAI;	
	else if (temp1 > TWOPAI)  	temp1 = temp1 - TWOPAI;		//AD采样时刻转子位置取模(0-TWOPAI)
							
	temp = TWOPAI_3 + CAP4.nprtrstheta - temp1;				//正变换角度，未取模，（-1.3PAI---2.7PAI） 
															//定子绕组角接,B相电压过零时，电压矢量为30degree
	if		(temp > TWOPAI)	 	temp = temp - TWOPAI;	
	else if	(temp < 0)	  		temp = temp + TWOPAI;		//取模 (0-TWOPAI)
//	if		(temp > TWOPAI)	 	temp = temp - TWOPAI;	
//	else if	(temp < 0)	  		temp = temp + TWOPAI;		//取模 (0-TWOPAI)
		
	CAP4.mprtrstheta = temp;								//机侧正变换角度

	if 		(QEPDATA.rotpos < 0) 	  	QEPDATA.rotposdisp = QEPDATA.rotpos + TWOPAI;	
	else if (QEPDATA.rotpos > TWOPAI)  	QEPDATA.rotposdisp = QEPDATA.rotpos - TWOPAI;	//QEPDATA.rotposdisp用于DA显示


//-----------计算网侧反变换角度----201105atzuoyun----------------------------------------------------------------
	if(M_ChkT1Direction() == 0)  							//CTR减计数 
    	temp3 = EPwm1Regs.TBPRD + EPwm1Regs.TBCTR;
	else 													//CTR增计数
		temp3 = 3 * EPwm1Regs.TBPRD - EPwm1Regs.TBCTR;

	temp3 = temp3 * 2; 										//变换到CAP4的时钟频率：150M
//	temp2 = ECap4Regs.TSCTR;     //problem:不能有效抗网频干扰
//	temp2 = temp2 + temp3;									//估计下次脉冲发时ECap4Regs.TSCTR的值		
//	temp  = temp2 * CAP4.radpertb;							//网侧反变换角度，未取模，约（0---2PAI）
	temp  = CAP4.nprtrstheta + (float)temp3 * CAP4.radpertb;//采用抗干扰后角度计算反变换角度201105atzuoyun		

//	temp2 =  temp3 * CAP4.radpertb;									
//	temp  = CAP4.nprtrstheta + temp2;						//网侧反变换角度，未取模，约（0---2PAI）
	
	if (temp > TWOPAI)	temp = temp - TWOPAI;				//取模2 * PAI
			
	CAP4.npratitheta = temp;								//网侧反变换角度
	
//----------计算机侧反变换角度---201105atzuoyun------------------------------------------------------------------
/*	temp2 = ECap4Regs.TSCTR;           //problem:不能有效抗网频干扰
	temp  = (float)temp2;
	temp  = temp * CAP4.radpertb + TWOPAI_3;  				//当前定子磁链位置rad PAI_2=PAI/2=1.5707963	定子绕组角接 zlquestion
    													
	temp2 = EQep2Regs.QPOSCNT;
	temp1 = (float)temp2;									//QEP模块位置计数器的值

	temp1 = 1.53398e-3 * temp1;								//转子相对于周期信号的位置rad(电角度) //temp1 = POLEPAIRES  * PAI * temp1/(PLSPRVL * 2.0);
              												//0.002618=POLEPAIRES  * PAI/(PLSPRVL * 2.0); POLEPAIRES=2, PLSPRVL=2048cpc                          						
	temp1 = temp1 - QEPDATA.encodpos;						//当前转子实际位置rad	

	if 		(temp1 < 0) 	   	temp1 = temp1 + TWOPAI;
	else if (temp1 > TWOPAI) 	temp1 = temp1 - TWOPAI;		//当前转子位置取模
		
	temp = temp - temp1; 	 								//当前反变换角度
*/	
	temp = CAP4.mprtrstheta; 	 							//当前反变换角度 201105atzuoyun

	if(M_ChkT3Direction() == 0) temp1 = EPwm4Regs.TBPRD + EPwm4Regs.TBCTR;		//CTR减计数  	
	else 						temp1 = 3 * EPwm4Regs.TBPRD - EPwm4Regs.TBCTR;	//CTR增计数
		
	temp1 = temp1 * CAP4.omigaslp * 1.333333e-8; 			//剩余时间对应的角度增量PWMclk:75M, 1/75.0e6=1.3333e-8

	temp = temp + temp1; 									//反变换角度，未取模，约（-1.5PAI---2.5PAI）		

	if		(temp > TWOPAI)		temp = temp - TWOPAI;		
	else if	(temp < 0)		  	temp = temp + TWOPAI;		//取模2 * PAI
	if		(temp > TWOPAI)		temp = temp - TWOPAI;		
	else if	(temp < 0)		  	temp = temp + TWOPAI;		//取模2 * PAI 201005atcpc
		
	CAP4.mpratitheta = temp; 								//机侧反变换角度

//--------------------赋值给3/2变换结构体变量-------------------------------------------------------

	TRS_NPR_I.sintheta = sin(CAP4.nprtrstheta);	
	TRS_NPR_I.costheta = cos(CAP4.nprtrstheta);				//网侧定向角度
	TRS_NGS_U.costheta = TRS_NPR_I.costheta;	
	TRS_NGS_U.sintheta = TRS_NPR_I.sintheta;				//网侧定向角度
	TRS_NPR_U.sintheta = sin(CAP4.npratitheta);	
	TRS_NPR_U.costheta = cos(CAP4.npratitheta);				//网侧控制电压角度 

	TRS_MPR_I.sintheta = sin(CAP4.mprtrstheta);				//机侧控制用
	TRS_MPR_I.costheta = cos(CAP4.mprtrstheta);		
	TRS_MPR_U.sintheta = sin(CAP4.mpratitheta);				//转子控制电压角度 
	TRS_MPR_U.costheta = cos(CAP4.mpratitheta);		
 
}

/**************************************************************************************************
** 函数名称: 	MPR_CONTROL
** 功能描述:  	机侧变流器控制算法
** 输　入:   	
** 输　出: 		 
** 注  释: 		鞒鲎游�	
**-------------------------------------------------------------------------------------------------
** 作　者: 		
** 日　期: 	
**-------------------------------------------------------------------------------------------------
** 修改人:
** 日期:	 
**-------------------------------------------------------------------------------------------------
***********************************************************************************************/
void MPR_CONTROL(void)
{
   float temp_d,temp_q,temp1,temp2;

//------------------机侧电流dq值--------------------------------------------------------------------
   	TRS_MPR_I.a     = AD_OUT_MPR_I.a;
   	TRS_MPR_I.b     = AD_OUT_MPR_I.b;
   	TRS_MPR_I.c     = AD_OUT_MPR_I.c;
	Transform_3s_2s_2r(&TRS_MPR_I);
	DataFilter(0.4,&TRS_MPR_I.dflt,TRS_MPR_I.d); 			//机侧电流反馈值滤波， Ts=200us,fh=1.2kHz,滤掉开关频率次
	DataFilter(0.4,&TRS_MPR_I.qflt,TRS_MPR_I.q); 			//机侧电流反馈值滤波， Ts=200us,fh=1.2kHz,滤掉开关频率次


//----------------运行机侧电流环-------------------------------------------------------------------
	if(M_ChkFlag(SL_MPR_RUNING)!=0)							//机侧调节需要运行
	{	

		PI_MPR_Id.reference      = RUN.mpridrf;
		PI_MPR_Id.feedback       = TRS_MPR_I.dflt; 			//机侧d轴电流环
		PI_Loop(&PI_MPR_Id,PI_PARA_MPRID);   

		PI_MPR_Iq.reference      = RUN.mpriqrf;		  	  
		PI_MPR_Iq.feedback       = TRS_MPR_I.qflt;			//机侧q轴电流环
  		PI_Loop(&PI_MPR_Iq,PI_PARA_MPRIQ);
    }
   
//--------------MPR输出电压算�---------------------------------------------------------------------

    DM_imrd = - (TRS_NGS_U.dflt * SQRT3 * STAROTRTO / (MPR_Lm * CAP4.omigasyn));
//    DM_imrq = - (TRS_NGS_U.qflt * SQRT3 * STAROTRTO / (MPR_Lm * CAP4.omigasyn));  //网压q轴前馈解耦项

	temp_d = - PI_MPR_Id.out + SIGMA * CAP4.omigaslp * MPR_Lr * PI_MPR_Iq.feedback;	//解耦项计算

//	temp_d = temp_d	- CAP4.omigaslp * MPR_Lr * DM_imrq;								//网压q轴前馈解耦项

  	temp_q = - PI_MPR_Iq.out - SIGMA * CAP4.omigaslp * MPR_Lr * PI_MPR_Id.feedback;	//解耦项计算
	temp_q = temp_q	- CAP4.omigaslp * MPR_Lr * DM_imrd;                               


	temp_d = temp_d - MPR_Rr * PI_MPR_Id.feedback;
	temp_q = temp_q - MPR_Rr * PI_MPR_Iq.feedback;									//转子电阻压降
  
   	TRS_MPR_U.d = temp_d / STAROTRTO2;
	TRS_MPR_U.q = temp_q / STAROTRTO2;
    
    temp1 = (float)_SC_MPR_UD;
	temp2 = (float)_SC_MPR_UQ;

	if     (TRS_MPR_U.d >  temp1)     TRS_MPR_U.d =  temp1;	    //jutsttest
	else if(TRS_MPR_U.d < -temp1)     TRS_MPR_U.d = -temp1;	    //jutsttest
	if     (TRS_MPR_U.q >  temp2)     TRS_MPR_U.q =  temp2;	    //jutsttest
	else if(TRS_MPR_U.q < -temp2)     TRS_MPR_U.q = -temp2;	    //jutsttest

	Anti_Transform_2r_2s(&TRS_MPR_U);							//坐标反变换到两相静止系

//------------SVM脉宽计算和脉冲发生-----------------------------------------------------------------	
	SVPWM_MPR(TRS_MPR_U.alfa,TRS_MPR_U.beta);					//脉冲发生

//------------Te实际转矩反馈值计算-----------------------------------------------------------------	
//	Te_feedback = 1.5 * POLEPAIRES * MPR_Lm * TRS_NGS_U.dflt * SQRT3 * TRS_MPR_I.qflt / (314.15926 * STAROTRTO * MPR_Ls);   // Te=1.5*np*(Us/w1)*(Irq/K)*(Lm/Ls)
//	Te_feedback = IRQTOTE * TRS_NGS_U.dflt *  PI_MPR_Iq.feedback;   // Te=1.5*np*(Us/w1)*(Irq/K)*(Lm/Ls)
//	DataFilter(0.999,&DIP_MPR_I.qflt,TRS_MPR_I.q); 					//定子侧功率计算,机侧q电流， Ts=200us,fh=Hz
	DataFilter(0.8,&DIP_MPR_I.qflt,TRS_MPR_I.q); 					//20111116 减小转矩滤波时间常数 由1s改为4ms
	Te_feedback = - IRQTOTE * DIP_NPR_U.dflt *  DIP_MPR_I.qflt;     // Te=-1.5*np*(Us/w1)*(Irq/K)*(Lm/Ls),MPR_i为负值
	if(Te_feedback<=0)	Te_feedback=0;   							//20090817
}  


/**************************************************************************************************
** 函数名称: 	NPR_CONTROL
** 功能描述:  	网侧变流器控制算法
** 输　入:   	
** 输　出: 		 
** 注  释: 		电流流向电网为正	
**-------------------------------------------------------------------------------------------------------
** 作　者: 		
** 日　期: 		
**-------------------------------------------------------------------------------------------------------
** 修改者:
** 日期:
**------------------------------------------------------------------------------------------------------
***********************************************************************************************/
void NPR_CONTROL(void)
{ 
	float temp_d,temp_q;

//-----------计算网侧变流器电流dq值-----------------------------------------------------------------
	TRS_NPR_I.a = AD_OUT_NPR_I.a;  							//获取网侧变流器电流
	TRS_NPR_I.b = AD_OUT_NPR_I.b;
	TRS_NPR_I.c = AD_OUT_NPR_I.c;		
	Transform_3s_2s_2r(&TRS_NPR_I);							//坐标变换
	DataFilter(0.4,&TRS_NPR_I.dflt,TRS_NPR_I.d); 			//网侧电流反馈值滤波， Ts=200us,fh=1.2kHz,滤掉开关频率次
	DataFilter(0.4,&TRS_NPR_I.qflt,TRS_NPR_I.q); 			//网侧电流反馈值滤波， Ts=200us,fh=1.2kHz，20090615改

//----------计算网压dq值----------------------------------------------------------------------------
	TRS_NGS_U.a = AD_OUT_NGS_U.a;							//获取电网电压
	TRS_NGS_U.b = AD_OUT_NGS_U.b;
	TRS_NGS_U.c = AD_OUT_NGS_U.c;
	Transform_3s_2s_2r(&TRS_NGS_U);							//坐标变换

	DataFilter(0.4,&TRS_NGS_U.dflt,TRS_NGS_U.d); 			//网压反馈值滤波，Ts=200us,fh=1.2kHz t=132us 20090608change to ok
	DataFilter(0.4,&TRS_NGS_U.qflt,TRS_NGS_U.q); 			//网压反馈值滤波，Ts=200us,fh=1.2kHz t=132us 20090608change to ok
//	DataFilter(0.062,&TRS_NGS_U.dflt,TRS_NGS_U.d); 			//网压反馈值滤波，Ts=200us,fh=12kHz 20100507change at zuoyun
//	DataFilter(0.062,&TRS_NGS_U.qflt,TRS_NGS_U.q); 			//网压反馈值滤波，Ts=200us,fh=12kHz 20100507change at zuoyun

	DataFilter(0.1,&TRS_NGS_U.dflt2,TRS_NGS_U.d); 			//网压反馈值滤波，Ts=200us,fh=7.9kHz,126us,为监测网跌，20091026


	if(M_ChkFlag(SL_NPR_RUNING)!=0)							//网侧PI运行控制条件
	{

//----------运行电压外环----------------------------------------------------------------------------
	  if(M_ChkFlag(SL_STEADYGV)==0)  						//Vdc没有稳定，且采用稳态PI参数
	  {

		PI_NPR_U.reference     = - RUN.urf;   				//获取中间电压指令
		PI_NPR_U.feedback      = - AD_OUT_UDC;				//获取中间电压反馈值
    	PI_Loop(&PI_NPR_U,PI_PARA_DYNU);
      }	
	  else                                                   //采用稳态PI参数
	  {
		PI_NPR_U.reference     = - RUN.urf;   				//获取中间电压指令
		PI_NPR_U.feedback      = - AD_OUT_UDC;				//获取中间电压反馈值
    	PI_Loop(&PI_NPR_U,PI_PARA_NPRU); 
      }
    	
//---------运行d轴电流环----------------------------------------------------------------------------
   		PI_NPR_Id.reference      = PI_NPR_U.out; 			//获取d轴电流噶睿⒁庹汉�
		PI_NPR_Id.feedback       = TRS_NPR_I.dflt;			//获取d轴电流反馈滤波值
		PI_Loop(&PI_NPR_Id,PI_PARA_NPRID);

//---------运行q轴电流环----------------------------------------------------------------------------
		PI_NPR_Iq.reference      = RUN.npriqrf; 			//q轴电流指令
		PI_NPR_Iq.feedback       = TRS_NPR_I.qflt;	  		//获取q轴电流反馈滤波值
		PI_Loop(&PI_NPR_Iq,PI_PARA_NPRIQ);	  	  
	}

//---------扑阈迪翹PR控制电压------------------------------------------------------------------

	temp_d = PI_NPR_Id.out  + TRS_NGS_U.dflt - CAP4.omigasyn * NPR_L * PI_NPR_Iq.feedback;
	temp_q = PI_NPR_Iq.out  + TRS_NGS_U.qflt + CAP4.omigasyn * NPR_L * PI_NPR_Id.feedback;
	
  	TRS_NPR_U.d = temp_d;
  	TRS_NPR_U.q = temp_q;

	Anti_Transform_2r_2s(&TRS_NPR_U); 						//antitransform to static axis

//---------SVM脉宽计算-----------------------------------------------------------------------------
	SVPWM_NPR(TRS_NPR_U.alfa,TRS_NPR_U.beta);				//脉冲发生

//------------网侧有功功率计算(3相)-----------------------------------------------------------------	
	DataFilter(0.99,&DIP_NPR_I.dflt,TRS_NPR_I.d); 			//定子侧功率计算,网压ed， Ts=200us,fh=88Hz
	DataFilter(0.99,&DIP_NPR_I.qflt,TRS_NPR_I.q); 			//定子侧功率计算,机侧q电流， Ts=200us,fh=88Hz
	DataFilter(0.99,&DIP_NPR_U.qflt,TRS_NGS_U.q); 			//定子侧功率计算,机侧q电流， Ts=200us,fh=88Hz
	DataFilter(0.995,&DIP_NPR_U.dflt,TRS_NGS_U.d); 			//定子侧功率计算,网压ed， Ts=200us,Tr=250ms

	Pnactive   = 1.5 * (DIP_NPR_U.dflt * DIP_NPR_I.dflt + DIP_NPR_U.qflt * DIP_NPR_I.qflt);   	// Pnactive=3*Ud*Id/(squt2*squt2)
	Pnreactive = 1.5 * (DIP_NPR_U.qflt * DIP_NPR_I.dflt - DIP_NPR_U.dflt * DIP_NPR_I.qflt);   	// Pnactive=3*Ud*Id/(squt2*squt2)

}


/**************************************************************************************************
** 名称: SysCtrl
** 功能描述: 1.5MW变流器系统逻辑控制程序--CPC电机实验 	
** 输　入:   	
** 输　出: 		 
** 注  释: 			
**-------------------------------------------------------------------------------------------------
** 作　者: 	 
** 日　期: 		
**-------------------------------------------------------------------------------------------------
** 修改人:
** 日期:	
**-------------------------------------------------------------------------------------------------
***********************************************************************************************/ 
void SysCtrl(void)         
{    

//1-----发生PDP或者严重不可恢复故障-----------------------//     
 if(M_ChkFlag(SL_SERIESTOP)!=0 || M_ChkFlag(SL_SERIESTOPING)!=0)   //系统发生严重故障或者正在停机 20091015 at zy
 {

		M_SetFlag(SL_SERIESTOPING);         //置严重故障正在停机标志 20091015 at zy
//		M_ClrFlag(SL_DISPLAY6);          	//清NPR脉冲待机指示
//    	M_ClrFlag(SL_DISPLAY7);          	//清变流器运行待机指示
		MAIN_LOOP.cnt_cboff=0;           	//if.2
          MAIN_LOOP.cnt_closecb=0;       	//if.3
	  
	   	M_ClrFlag(CL_CB);    		     	//清主断合闸指令
	   	M_ClrFlag(CL_CBENGSTRG);         	//清主断储能指令，同时欠压链断开，主断断开 
	   	M_ClrFlag(CL_CBFCON);            	//故障链断开

	  	M_ClrFlag(SL_OCS_NPRSTART);         //清网侧运行指令 20090815
//	   	M_ClrFlag(SL_STEADYFB);          	//清Vdc稳定标志  20091015atzy
	   	M_ClrFlag(SL_OCS_MPRSTART);	   	 	//清机侧运行指令 20090815
//      M_ClrFlag(SL_CHARGEOK);          	//清预充电完成   20091015atzy
	   	M_ClrFlag(SL_CBCLOSED);			 	//清主断闭合完成 20091022atzy
	   
		 
	   if(M_ChkCounter(MAIN_LOOP.cnt_opencontac,DELAY_OPENCONTAC)>0)	//2s 主断断开延时后再断定子接触器和主接触器
	   {
		   M_ClrFlag(CL_MAINK);    		  	//断主接触器和滤波器
		   M_ClrFlag(CL_PRE); 				//清预充电接触器
		   M_ClrFlag(CL_STATORK);    	  	//断定子接触器	
		   M_ClrFlag(SL_CHARGEOK);          //清预充电完成    20091015atzy
	   }

	   if(AMUX.skiiptempmax<50.0)	 M_ClrFlag(SL_FAN_WORK);          	//关闭功率组件风机  20090816

	   if(M_ChkFlag(SL_IN1_CBSTS)==0 && M_ChkFlag(SL_IN1_MIANK)==0 && M_ChkFlag(SL_IN1_STATORK)==0) M_ClrFlag(SL_SERIESTOPING); //20091015 at zy

 }	//end of NO.1 if

  

//2----接收到SL_OCS_EIN/BIT0=0时，断定子接触器、停脉冲，断主接触器滤波器和主断------------------------- 

  else if((M_ChkFlag(SL_OCS_EIN)==0) || (M_ChkFlag(SL_EINSTOPING)!=0))
  {
	   	MAIN_LOOP.cnt_opencontac=0;        			//if.1
         MAIN_LOOP.cnt_closecb=0;         			//if.3

		M_SetFlag(SL_EINSTOPING);    				//20090817
		M_ClrFlag(SL_OCS_SYSRUN); 					//清系统运行和发脉冲指令，等待脉冲停止和MC断开
    	M_ClrFlag(SL_OCS_NPRSTART);
		M_ClrFlag(SL_OCS_MPRSTART);
  
		if((M_ChkFlag(SL_NPR_PWMOUT)==0)&&(M_ChkFlag(SL_MPR_PWMOUT)==0)&&(M_ChkFlag(SL_CHARGEOK)==0))
		{
	  		if(M_ChkFlag(SL_IN1_CBSTS)!=0)    		//主接触器\主滤波器已经断开 且主断路器闭合状态
	  		{
	    		if(M_ChkCounter(MAIN_LOOP.cnt_cboff,DELAY_CBOFF)>0)  //1s
				{
			  		M_ClrFlag(CL_CB);    		    //清主断合闸指令
			  		M_ClrFlag(CL_CBENGSTRG);      	//清主断储能指令，同时欠压链断开，主断断开
			 		M_ClrFlag(CL_CBFCON);
				}	     
	 
	  		}
	  		else   
	  		{
//	        	M_ClrFlag(SL_DISPLAY6);           	//清NPR脉冲待机指示
//				M_ClrFlag(SL_DISPLAY7);           	//清变流器运行待机指示
				M_ClrFlag(CL_CB);    		      	//清主断合闸指令
		    	M_ClrFlag(CL_CBENGSTRG);          	//清主断储能指令
		    	M_ClrFlag(CL_CBFCON);	
		    	M_ClrFlag(CL_PRE);                	//清预充接触器
		    	M_ClrFlag(SL_CHARGEOK);           	//清预充电完成
            	M_ClrFlag(SL_OCS_NPRSTART);        	//清上位机NPR发脉冲指令
	        	M_ClrFlag(SL_OCS_MPRSTART);        	//清上位机MPR发脉冲指令
				M_ClrFlag(CL_MAINK);    		  	//断主接触器和滤波器
				M_ClrFlag(SL_CBCLOSED);				//清主断闭合完成 
				if(AMUX.skiiptempmax<50.0)	 M_ClrFlag(SL_FAN_WORK);     //关闭功率组件风机 20090816
				M_ClrFlag(SL_EINSTOPING);    		//20090817    		
	  		}
		}
		else    MAIN_LOOP.cnt_cboff=0;
	 	
   } //end of NO.2 if


//3----------- 接收到SL_OCS_EIN/BIT0=1的系统启动指令，合主断，预充电，合主接触器/滤波器，发脉冲

  else                                         		//系统无故障，接收到BIT0=1，执行系统启动指令直至主断闭合 
  {
      	MAIN_LOOP.cnt_opencontac=0;        			//if.1
	   	 MAIN_LOOP.cnt_cboff=0;            			//if.2
   		if(M_ChkFlag(SL_CBCLOSED)==0) 				//主断还没有闭合，执行主断闭合程序
   		{
      		if(M_ChkFlag(SL_IN1_CBSTS)==0)      	//主断路器断开
	  		{
	     	 	M_SetFlag(CL_CBENGSTRG);     		//接收到闭合CB延时到,开始储能
			 	M_SetFlag(CL_CBFCON);

			 	if(M_ChkCounter(MAIN_LOOP.cnt_closecb,DELAY_CLOSE_CB)>0)  M_SetFlag(CL_CB);   //发出CB合闸指令(脉冲即可) 5s
	  		}      
      		else	 
      		{
      			M_SetFlag(SL_CBCLOSED);             //系统主断合闸完成
				M_ClrFlag(CL_CB);  					//主断闭合后，就清除CB合闸(因为合闸仅需要一个脉冲即可)
      		}

   		}
   		else  MAIN_LOOP.cnt_closecb=0;             	//主断路器闭合已经完成
  
  }	 
	  

//4----------主断闭合后，接收到主控变流器运行指令---------------------------------------------------------------------------------

  if(M_ChkFlag(SL_CBCLOSED)!=0)   
  {           
  	if((M_ChkFlag(SL_OCS_SYSRUN)!=0)&&(M_ChkFlag(SL_SERIESTOP)==0)&&(M_ChkFlag(SL_ERRSTOP)==0)&&(M_ChkFlag(SL_SYSSTOPING)==0))
	{
      MAIN_LOOP.cnt_mkoff=0;
      if(M_ChkFlag(SL_CHARGEOK)==0)				//未进行预充电
	  {      	
      	
      	if(M_ChkFlag(SL_SPEED_IN_RANGE)!=0)     //转速在范围内
		{
//	        M_ClrFlag(SL_DISPLAY7);    			//清变流器运行待机指示     			
			M_SetFlag(SL_FAN_WORK);       		//开启功率组件风机  20090816

      		if((M_ChkFlag(SL_IN1_MIANK)==0)||(M_ChkFlag(SL_IN1_MIANFILTER)==0))        //主断路器已闭合,且主接触器/滤波器其一有断开
	  		{	       	     		 		              
	       		M_SetFlag(CL_PRE);          	 //闭合预充电接触器
		   		if((AD_OUT_UDC>=_SC_VDCON)&&(M_ChkCounter(MAIN_LOOP.cnt_precok,DELAY_PRECOK)>=0))  M_SetFlag(CL_MAINK);  //8s      //预充电OK，闭合主接触器 9s   
	  		} 
	  		else
	  		{
		 		M_ClrFlag(CL_PRE);          	      //断开预充电接触器，硬件上会在MF/MC闭合后使其断开，但软件也要蹇刂莆�
		 		M_SetFlag(SL_CHARGEOK);               //系统准备就绪         
      		}
		}
		else	MAIN_LOOP.cnt_precok=0;
	  }
	  else 
	  {
	 		if(AD_OUT_UDC >= 870.0)   M_SetFlag(SL_OCS_NPRSTART);	 //系统上电完成，延时后启动NPR 1s   
		 	MAIN_LOOP.cnt_precok=0;
	  }
	}
	else   //SYSRUN停机过程
	{
   		M_SetFlag(SL_SYSSTOPING);  //20090817
 
  		M_ClrFlag(SL_OCS_NPRSTART);
		M_ClrFlag(SL_OCS_MPRSTART);
  		if((M_ChkFlag(SL_NPR_PWMOUT)==0)&&(M_ChkFlag(SL_MPR_PWMOUT)==0))
		{
  	  		if((M_ChkFlag(SL_IN1_MIANK)!=0)||(M_ChkFlag(SL_IN1_MIANFILTER)!=0))     //主接触器闭合状态
	 		{
         		if(M_ChkCounter(MAIN_LOOP.cnt_mkoff,DELAY_MKOFF)>0)                 //0.2s
	     		{
			  		M_ClrFlag(CL_MAINK);    		  								// 断主接触器和滤波器
	          		M_ClrFlag(CL_PRE);    		  									// 再清一次预充电指令	
		 		}	        	    		  	     
	  		}	
	  		else    
	  		{
	       		M_ClrFlag(SL_CHARGEOK);
				M_ClrFlag(SL_SYSSTOPING);  //20090817
				if(AMUX.skiiptempmax<50.0)	 M_ClrFlag(SL_FAN_WORK);     			//关闭功率组件风机 20090816
//				if(M_ChkFlag(SL_ERRSTOP)==0) M_SetFlag(SL_DISPLAY7);    			//置变流器运行待机指示
		  		MAIN_LOOP.cnt_mkoff=0;
      		}
    	}
		else   MAIN_LOOP.cnt_mkoff=0;
	}
  }
  else
  {
		M_ClrFlag(SL_CHARGEOK);
		MAIN_LOOP.cnt_precok=0;
		MAIN_LOOP.cnt_mkoff=0;
//		M_ClrFlag(SL_DISPLAY7);    					//清变流器运行待机指示 
  }

//5----------- 主接触器/滤波器已闭合且无故障,等待上位机网侧启动指令，发脉�	  
	  
	if((M_ChkFlag(SL_CBCLOSED)!=0)&&(M_ChkFlag(SL_CHARGEOK)!=0)&&(M_ChkFlag(SL_SERIESTOP)==0)&&(M_ChkFlag(SL_ERRSTOP)==0))   
	{
	   if(M_ChkFlag(SL_OCS_NPRSTART)!=0)
	   {	   
//	       M_ClrFlag(SL_DISPLAY6);            									//清NPR脉冲待机指示
         		 
           if(M_ChkFlag(SL_CONFIGPWM)!=0)    M_SetFlag(SL_NPR_START);		   	//pwmdrive配置开关频率完成,网侧变流器运行		  
	       else   	                         M_ClrFlag(SL_NPR_START);		   	//雇啾淞髌髟诵� 

           //---------判断Vdc是否稳定start	          
	       if((AD_OUT_UDC>(_URF - 20))&&(AD_OUT_UDC<(_URF + 20)))   			//判断直流电压是否稳定在_URF(1100V)并且已顺中欢问奔洌允鼓芑嗦龀�
	   	   {   
	   		  M_SetFlag(SL_DISPLAY2);                							//置Vdc稳定指示
	   		  if(M_ChkFlag(SL_MPR_START)==0)         							//只在机侧未发脉冲前检测，机侧运行期间Vdc不稳不再清SL_STEADYFB  
	   		  {                                                					//上下偏差小于20V
	     	    if(M_ChkCounter(MAIN_LOOP.cnt_steadyfb,DELAY_STEADYFB)>0)	  M_SetFlag(SL_STEADYFB);  //2s
		      }
		      else   MAIN_LOOP.cnt_steadyfb=0;
		   }
		   else  
		   {
		   	 MAIN_LOOP.cnt_steadyfb=0;
			 M_ClrFlag(SL_DISPLAY2);             								//清Vdc稳定指示

		   }	        
	       //---------判断Vdc是否稳定end
	     	    
		   if(M_ChkFlag(SL_STEADYFB)!=0)
		   {
		      if(M_ChkCounter(MAIN_LOOP.cnt_mprstart,DELAY_MPRSTART)>=0)   M_SetFlag(SL_OCS_MPRSTART);	 //Vdc已经稳定，延时后启动MPR 1s  
           }
		   else  MAIN_LOOP.cnt_mprstart=0;

	   }
	   else					  													
	   {
	      if(M_ChkFlag(SL_MPR_PWMOUT)==0)   M_ClrFlag(SL_NPR_START);  			//机侧脉冲封锁以后再将网侧变流器运行指令清零

          if(M_ChkFlag(SL_NPR_PWMOUT)==0)      
          {
             M_ClrFlag(SL_STEADYFB);
//             M_SetFlag(SL_DISPLAY6);                  							//网侧脉冲封锁, 置NPR脉冲待机指示
			 M_ClrFlag(SL_DISPLAY2);
		  }
		  MAIN_LOOP.cnt_steadyfb=0;	   
		  MAIN_LOOP.cnt_mprstart=0; 
	   }

    }	   
	else                  
    {   
//	    M_ClrFlag(SL_DISPLAY6);            //清NPR脉冲待机指示
		M_ClrFlag(SL_NPR_START);
		M_ClrFlag(SL_OCS_NPRSTART);	
		M_ClrFlag(SL_STEADYFB);
		M_ClrFlag(SL_DISPLAY2);
		MAIN_LOOP.cnt_steadyfb=0;
		MAIN_LOOP.cnt_mprstart=0;
	}	


//6----------- 主接触器/滤波器已闭合且无故障,等待上位机机侧启动指令，发脉冲	  
	  
	if((M_ChkFlag(SL_CBCLOSED)!=0)&&(M_ChkFlag(SL_CHARGEOK)!=0)&&(M_ChkFlag(SL_SERIESTOP)==0)&&(M_ChkFlag(SL_ERRSTOP)==0)&&(M_ChkFlag(SL_NPR_PWMOUT)!=0))   
	{
      if((M_ChkFlag(SL_OCS_MPRSTART)!=0)&&(M_ChkFlag(SL_STEADYFB)!=0))		  								//网侧运行,且Vdc稳定
	  {
		M_SetFlag(SL_MPR_START);             								//置机侧变流器运行指令

		if(M_ChkFlag(SL_IN1_STATORK)==0)      								//定子接触器处于断开状态
	 	{
		   if((MEAN_DATA.uab_d <= _SC_UDSTAC)&&(MEAN_DATA.ubc_d <= _SC_UDSTAC))          //定子接触器前后半波平均值差在40V以内
		   {
		      M_SetFlag(SL_DISPLAY4);
		      if(_AUTOCSTAC !=0)    M_SetFlag(CL_STATORK);                               //外部控制，为1才允许闭合STAC
			  else					M_ClrFlag(CL_STATORK);		        
		   }
		   else   M_ClrFlag(SL_DISPLAY4);      
	 	}
		else	M_SetFlag(SL_MPR_SYNOK);  									//机侧同步并网完成标志	
	  }
	  else  
	  {
		 M_ClrFlag(SL_MPR_START);
		 M_ClrFlag(SL_DISPLAY4);											// 停机侧变流器运行指令

		 if(M_ChkFlag(SL_IN1_STATORK)!=0)      								//
	 	 {
         	if(M_ChkFlag(SL_STACTRIPEN)!=0)	  M_ClrFlag(CL_STATORK);        //达到定子断开条件, 断开定子接触器		            
	 	 }
		 else      M_ClrFlag(SL_MPR_SYNOK);
		
	  }
	}
	else
	{
	   M_ClrFlag(SL_MPR_START);                								//停止机侧变流器运行指令
	   M_ClrFlag(SL_OCS_MPRSTART);	
	   if(M_ChkFlag(SL_SERIESTOP)==0)	M_ClrFlag(CL_STATORK); 
	   M_ClrFlag(SL_MPR_SYNOK);
	   M_ClrFlag(SL_DISPLAY4);
	}    


//======================系统严重故障发生并且消除以后，外部发出复位信号,只能在系统停机后复位=================//
//======================一般故障发生并且消除以后，外部发出复位信号,在网侧和机侧都停脉冲后即可复位=================//
    if((M_ChkFlag(SL_OCS_RESET)!=0)&& ((M_ChkFlag(SL_SERIESTOP)!=0)||(M_ChkFlag(SL_ERRSTOP)!=0)) &&(M_ChkFlag(SL_CHARGEOK)==0))  //20090815
	{
	   if(M_ChkFlag(SL_SERIESTOP)!=0)   //20091015 at zy
	   {
	   	 if(M_ChkFlag(SL_IN1_CBSTS)==0 && M_ChkFlag(SL_IN1_MIANK)==0 && M_ChkFlag(SL_IN1_STATORK)==0)   M_SetFlag(SL_RESET);
		 else                            																M_ClrFlag(SL_RESET);
	   }
	   else	   M_SetFlag(SL_RESET); 
	}
	else   M_ClrFlag(SL_RESET);

//====================故障发生并且消除以后，操作器发出复位信号---结束==============// 
}

/////////////////no more////////////////////
