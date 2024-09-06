#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "epaper.h"

spi_device_handle_t spi = {0};

spi_bus_config_t buscfg={
	.miso_io_num = -1,
	.mosi_io_num = PIN_NUM_MOSI,
	.sclk_io_num = PIN_NUM_CLK,
	.quadwp_io_num=-1,
	.quadhd_io_num=-1,
	.max_transfer_sz=8
};

spi_device_interface_config_t devcfg={
	.clock_speed_hz = SPI_MASTER_FREQ_8M,			//Clock out at 10 MHz 10*1000*1000 40000000
	.mode			= 0,								//SPI mode 0
	.cs_ena_pretrans= 0,
	.spics_io_num	= -1,				//CS pin
	.queue_size		= 8,				//We want to be able to queue 7 transactions at a time
	.flags          = SPI_DEVICE_3WIRE,
//		.pre_cb 		= olcd_spi_pre_transfer_callback,	//Specify pre-transfer callback to handle D/C line
};

void epaper_spi_init(spi_host_device_t host_id)
{
	esp_err_t ret;

	//Initialize the SPI bus
	ret = spi_bus_initialize(host_id, &buscfg, SPI_DMA_CH_AUTO);
	ESP_ERROR_CHECK(ret);
	//Attach the LCD to the SPI bus
	ret = spi_bus_add_device(host_id, &devcfg, &spi);
	ESP_ERROR_CHECK(ret);

}

void epaper_spi_send_cmd(uint8_t cmd)
{
	esp_err_t ret;
	spi_transaction_t t;

	EPD_DC_Clr();
	EPD_CS_Clr();

	memset(&t, 0, sizeof(t));		//Zero out the transaction
	t.length = 8;					  //Command is 8 bits
	t.tx_buffer = &cmd; 			  //The data is the cmd itself

	ret = spi_device_polling_transmit(spi, &t);  //Transmit!
	assert(ret==ESP_OK);			//Should have had no issues.

	EPD_CS_Set();
}

void epaper_spi_send_data(uint8_t data)
{
	esp_err_t ret;
	spi_transaction_t t;

	EPD_DC_Set();
	EPD_CS_Clr();

	memset(&t, 0, sizeof(t));		//Zero out the transaction
	t.length = 8;					  //Command is 8 bits
	t.tx_buffer = &data; 			  //The data is the cmd itself

	ret = spi_device_polling_transmit(spi, &t);  //Transmit!
	assert(ret==ESP_OK);			//Should have had no issues.

	EPD_CS_Set();
}

#if 1
static void send_lines(spi_device_handle_t spi, const uint8_t *linedata, uint16_t bit_len)
{
    esp_err_t ret;

    static spi_transaction_t trans;

    trans.tx_buffer = linedata;        //finally send the line data
    trans.length = bit_len ;          //Data length, in bits
    trans.flags = 0; //undo SPI_TRANS_USE_TXDATA flag
    trans.rxlength = 0;
    //trans.cmd = 1;

    //ret=spi_device_queue_trans(spi, &trans[5], portMAX_DELAY);
    ret = spi_device_polling_transmit(spi, &trans);
    assert(ret==ESP_OK);

}


void spi_send_data(const uint8_t *linedata, uint32_t nbyte)
{
	const uint8_t *tmp = linedata;
	uint32_t remain_size = nbyte;
	uint16_t max_send_bits = 240 * 16 * 2;
	uint16_t max_send_bytes = 240 * 16 * 2 / 8;

	EPD_DC_Set();
	EPD_CS_Clr();

	while (remain_size / max_send_bytes) {
		send_lines(spi, tmp, max_send_bits);
		remain_size -= max_send_bytes;
		tmp += max_send_bytes;
	}

	if (remain_size)
		send_lines(spi, tmp, remain_size * 8);

	EPD_CS_Set();
}
#endif


