#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "aht20.h"

#define AHT_SCL 4
#define AHT_SDA 5

#define SDA_Hight gpio_set_level(AHT_SDA, 1)
#define SDA_Low   gpio_set_level(AHT_SDA, 0)

#define SCL_Hight gpio_set_level(AHT_SCL, 1)
#define SCL_Low   gpio_set_level(AHT_SCL, 0)

#define delay_us(x) vTaskDelay(x)
#define delay_ms(x) vTaskDelay(x/portTICK_PERIOD_MS)

static void ant_gpio_out_config(int num)
{
	gpio_config_t io_conf = {
		.mode = GPIO_MODE_OUTPUT,
		.pin_bit_mask = 1ULL << num,
	};
	gpio_config(&io_conf);
}

static void SDA_OUT(void)
{
	gpio_set_direction(AHT_SDA, GPIO_MODE_OUTPUT);
}
static void SDA_IN(void)
{
    gpio_set_direction(AHT_SDA, GPIO_MODE_INPUT);
}

void AHT20_GPIO_Init(void)
{
	ant_gpio_out_config(AHT_SCL);
	ant_gpio_out_config(AHT_SDA);
	ant_gpio_out_config(GPIO_NUM_11);

	gpio_set_level(GPIO_NUM_11, 0);

	gpio_set_level(AHT_SDA, 1);
	gpio_set_level(AHT_SCL, 1);
}

uint8_t   ack_status=0;
uint8_t   readByte[6];
uint8_t   AHT20_status=0;

uint32_t  H1=0;  //Humility
uint32_t  T1=0;  //Temperature

uint8_t  AHT20_OutData[4];
uint8_t  AHT20sendOutData[10] = {0xFA, 0x06, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF};

void IIC_Init(void)
{					     
	AHT20_GPIO_Init(); 
}

uint8_t  Receive_ACK(void)
{
	uint8_t result=0;
	uint8_t cnt=0;

	SCL_Low;
	SDA_IN(); 
	delay_us(4);

	SCL_Hight;
	delay_us(4);

	while(gpio_get_level(AHT_SDA) && (cnt<100))
	{
		cnt++;
	}

	SCL_Low;
	delay_us(4);

	if(cnt<100)
	{
		result=1;
	}
	return result;
}



void  Send_ACK(void)
{
	SDA_OUT();
	SCL_Low;
	delay_us(4);

	SDA_Low;
	delay_us(4);

	SCL_Hight;
	delay_us(4);
	SCL_Low;
	delay_us(4);

	SDA_IN();
}



void  SendNot_Ack(void)
{
	SDA_OUT();
	SCL_Low;
	delay_us(4);

	SDA_Hight;
	delay_us(4);

	SCL_Hight;
	delay_us(4);

	SCL_Low;
	delay_us(4);

	SDA_Low;
	delay_us(4);
}


void I2C_WriteByte(uint8_t  input)
{
	uint8_t  i;
	SDA_OUT();
	for(i=0; i<8; i++)
	{
		SCL_Low;
		delay_ms(5);

		if(input & 0x80)
		{
			SDA_Hight;
			//delaymm(10);
		}
		else
		{
			SDA_Low;
			//delaymm(10);
		}

		SCL_Hight;
		delay_ms(5);

		input = (input<<1);
	}

	SCL_Low;
	delay_us(4);

	SDA_IN();
	delay_us(4);
}	


uint8_t I2C_ReadByte(void)
{
	uint8_t  resultByte=0;
	uint8_t  i=0, a=0;

	SCL_Low;
	SDA_IN();
	delay_ms(4);

	for(i=0; i<8; i++)
	{
		SCL_Hight;
		delay_ms(3);

		a=0;
		if(gpio_get_level(AHT_SDA))
		{
			a=1;
		}
		else
		{
			a=0;
		}

		//resultByte = resultByte | a;
		resultByte = (resultByte << 1) | a;

		SCL_Low;
		delay_ms(3);
	}

	SDA_IN();
	delay_ms(10);

	return   resultByte;
}


void  set_AHT20sendOutData(void)
{
	/* --------------------------
	 * 0xFA 0x06 0x0A temperature(2 Bytes) humility(2Bytes) short Address(2 Bytes)
	 * And Check (1 byte)
	 * -------------------------*/
	AHT20sendOutData[3] = AHT20_OutData[0];
	AHT20sendOutData[4] = AHT20_OutData[1];
	AHT20sendOutData[5] = AHT20_OutData[2];
	AHT20sendOutData[6] = AHT20_OutData[3];

//	AHT20sendOutData[7] = (drf1609.shortAddress >> 8) & 0x00FF;
//	AHT20sendOutData[8] = drf1609.shortAddress  & 0x00FF;

//	AHT20sendOutData[9] = getXY(AHT20sendOutData,10);
}


void  I2C_Start(void)
{
	SDA_OUT();
	SCL_Hight;
	delay_ms(4);

	SDA_Hight;
	delay_ms(4);
	SDA_Low;
	delay_ms(4);

	SCL_Low;
	delay_ms(4);
}



void  I2C_Stop(void)
{
	SDA_OUT();
	SDA_Low;
	delay_ms(4);

	SCL_Hight;
	delay_ms(4);

	SDA_Hight;
	delay_ms(4);
}


void read_AHT20(uint8_t *temp, uint8_t *hum)
{
	uint8_t i;
	uint8_t retry_cnt=0;

	for(i=0; i<6; i++)
	{
		readByte[i]=0;
	}

read_AHT20:
	//-------------
	I2C_Start();

	I2C_WriteByte(0x71);
	ack_status = Receive_ACK();

	readByte[0]= I2C_ReadByte();
	Send_ACK();
	if (readByte[0] & 0x80) {
		SendNot_Ack();
		I2C_Stop();
		if (retry_cnt > 5) {
			ESP_LOGE("aht20", "AHT20 read fail");
			return;
		}
		delay_ms(50);
		retry_cnt++;
		goto read_AHT20;
	}

	readByte[1]= I2C_ReadByte();
	Send_ACK();

	readByte[2]= I2C_ReadByte();
	Send_ACK();

	readByte[3]= I2C_ReadByte();
	Send_ACK();

	readByte[4]= I2C_ReadByte();
	Send_ACK();

	readByte[5]= I2C_ReadByte();
	SendNot_Ack();
	//Send_ACK();

	I2C_Stop();

	//--------------
	if( (readByte[0] & 0x80) != 0x80 )
	{
		H1 = readByte[1];
		H1 = (H1<<8) | readByte[2];
		H1 = (H1<<8) | readByte[3];
		H1 = H1>>4;

		H1 = (H1*1000)/1024/1024;

		T1 = readByte[3];
		T1 = T1 & 0x0000000F;
		T1 = (T1<<8) | readByte[4];
		T1 = (T1<<8) | readByte[5];

		T1 = (T1*2000)/1024/1024 - 500;

		AHT20_OutData[0] = (H1>>8) & 0x000000FF;
		AHT20_OutData[1] = H1 & 0x000000FF;

		AHT20_OutData[2] = (T1>>8) & 0x000000FF;
		AHT20_OutData[3] = T1 & 0x000000FF;
	}
	else
	{
		AHT20_OutData[0] = 0xFF;
		AHT20_OutData[1] = 0xFF;

		AHT20_OutData[2] = 0xFF;
		AHT20_OutData[3] = 0xFF;
		printf("读取失败");

	}
	printf("\r\n");
	printf("温度:%ld%ld.%ld",T1/100,(T1/10)%10,T1%10);
	printf("湿度:%ld%ld.%ld",H1/100,(H1/10)%10,H1%10);
	printf("\r\n");

    *temp = (T1+5) /10;
    *hum = (H1+5) /10;
}


uint8_t aht20_get_status(void)
{
	uint8_t status;
	I2C_Start();

	I2C_WriteByte(0x71);
	ack_status = Receive_ACK();
	status = I2C_ReadByte();
	SendNot_Ack();
	//Send_ACK();

	I2C_Stop();

	ESP_LOGE("aht20", "status = %d", status);
	return status;
}

void  reset_AHT20(void)
{

	I2C_Start();

	I2C_WriteByte(0x70);
	ack_status = Receive_ACK();
	if(ack_status) printf("1");
	else printf("1-n-");
	I2C_WriteByte(0xBA);
	ack_status = Receive_ACK();
		if(ack_status) printf("2");
	else printf("2-n-");
	I2C_Stop();

	/*
	AHT20_OutData[0] = 0;
	AHT20_OutData[1] = 0;
	AHT20_OutData[2] = 0;
	AHT20_OutData[3] = 0;
	*/
}


void  init_AHT20(void)
{
	I2C_Start();

	I2C_WriteByte(0x70);
	ack_status = Receive_ACK();
	if(ack_status) printf("3");
	else printf("3-n-");	
	I2C_WriteByte(0xBE);
	ack_status = Receive_ACK();
	if(ack_status) printf("4");
	else printf("4-n-");
	I2C_WriteByte(0x08);
	ack_status = Receive_ACK();
	if(ack_status) printf("5");
	else printf("5-n-");
	I2C_WriteByte(0x00);
	ack_status = Receive_ACK();
	if(ack_status) printf("6");
	else printf("6-n-");
	I2C_Stop();
}


void  startMeasure_AHT20(void)
{
	//------------
	I2C_Start();

	I2C_WriteByte(0x70);
	ack_status = Receive_ACK();
	//if(ack_status) printf("7");
	//else printf("7-n-");
	I2C_WriteByte(0xAC);
	ack_status = Receive_ACK();
	//if(ack_status) printf("8");
	//else printf("8-n-");
	I2C_WriteByte(0x33);
	ack_status = Receive_ACK();
	//if(ack_status) printf("9");
	//else printf("9-n-");
	I2C_WriteByte(0x00);
	ack_status = Receive_ACK();
	//if(ack_status) printf("10");
	//else printf("10-n-");
	I2C_Stop();
}


/**********
*ÉÏÃæ²¿·ÖÎªIO¿ÚÄ£¿éI2CÅäÖÃ
*
*´ÓÕâÒÔÏÂ¿ªÊ¼ÎªAHT20µÄÅäÖÃI2C
*º¯ÊýÃûÓÐIICºÍI2CµÄÇø±ð£¬Çë×¢Òâ£¡£¡£¡£¡£¡
*
*2020/2/23×îºóÐÞ¸ÄÈÕÆÚ
*
***********/
void  read_AHT20_once(uint8_t *temp, uint8_t *hum)
{
    uint8_t _temp,_hum;

	//reset_AHT20();
	//delay_ms(10);

	startMeasure_AHT20();
	delay_ms(80);

	read_AHT20(&_temp,&_hum);

	*temp = _temp;
	*hum = _hum;

	delay_ms(5);
}

aht20_data aht_data;

void aht20_update_data(void)
{
	read_AHT20_once(&aht_data.temperature, &aht_data.humidity);

	aht_data.status = true;
}

static void aht20_read_task(void *pvParameters)
{
    while(1)
    {
        read_AHT20_once(&aht_data.temperature, &aht_data.humidity);

		aht_data.status = true;

        delay_ms(60000);
    }
}

void AHT20_Init(void)
{
	uint8_t status;

	memset(&aht_data, 0x00, sizeof(aht_data));

    IIC_Init();

get_status:
	status = aht20_get_status();
	if ((status & 0x08) == 0) {
		reset_AHT20();

		delay_ms(10);

		init_AHT20();
		ESP_LOGI("aht20", "init_AHT20");
		goto get_status;
	}

	delay_ms(10);

	aht20_update_data();
    // xTaskCreate(aht20_read_task, "aht20_read_task", 8192, NULL, 4, NULL);
}

