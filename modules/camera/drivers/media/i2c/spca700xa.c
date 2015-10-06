/*
 *
 * Copyright (c) 2012 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * */


#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#
#include <linux/spi/spi.h>
#include <linux/freezer.h>

#include "spca700xa.h"
#include "app_i2c_lib_icatch.h"


#define SPCA700XA_SPI_BITS_PER_WORD	8
#define SPCA700XA_CMD_NORMAL		0x55


struct spca700xa *isp;


struct spca700xa {
	struct spi_device	*spi;
	struct spi_message  spi_read_msg;
	struct mutex		mutex;
	spinlock_t		lock;
};

static int spca700xa_enable_dma = 1; // Enable SPI DMA 

static int spca700xa_cmd(struct spca700xa *isp, UINT8 *cmd, UINT32 length)
{
	UINT8 *tx = cmd;
	struct spi_transfer xfer = {
		.tx_buf		= tx,
		.len		= length,
		.bits_per_word	= 8,
	};
	struct spi_message msg;
	int error;

	spi_message_init(&msg);

	if (spca700xa_enable_dma) {
		msg.is_dma_mapped = 1;
	}
	
	spi_message_add_tail(&xfer, &msg);

	error = spi_sync(isp->spi, &msg);
	if (error) {
		printk("%s: error\n", __func__);
		dev_err(&isp->spi->dev, "%s: failed, command: %x, error: %d\n",
			__func__, cmd, error);
		return error;
	}

	return 0;
}


static int spca700xa_cmd_byte(struct spca700xa *isp, u8 cmd)
{
	u8 tx = cmd;
	struct spi_transfer xfer = {
		.tx_buf		= &tx,
		.len		= 1,
		.bits_per_word	= 8,
	};
	struct spi_message msg;
	int error;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	error = spi_sync(isp->spi, &msg);
	if (error) {
		printk("%s: error\n", __func__);
		dev_err(&isp->spi->dev, "%s: failed, command: %x, error: %d\n",
			__func__, cmd, error);
		return error;
	}

	return 0;
}


static int spca700xa_write(struct spca700xa *isp, u8 reg, u16 value)
{
	u32 tx = ((reg) << 16) | value;
	struct spi_transfer xfer = {
		.tx_buf		= &tx,
		.len		= 1,
		.bits_per_word	= 8,
	};
	struct spi_message msg;
	int error;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	error = spi_sync(isp->spi, &msg);
	if (error) {
		dev_err(&isp->spi->dev,
			"%s: failed, register: %x, value: %x, error: %d\n",
			__func__, reg, value, error);
		return error;
	}

	return 0;
}


static int spca700xa_read(struct spca700xa *isp, UINT8 *cmd, UINT32 length)
{
	u8 *rx = cmd;
	struct spi_transfer xfer = {
			.rx_buf = rx,
			.len = length,
			.bits_per_word = 8,
	};
	struct spi_message msg;
	int error;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	error = spi_sync(isp->spi, &msg);
	if (error) {
		printk("%s: error\n", __func__);
		dev_err(&isp->spi->dev, "%s: failed, command: %x, error: %d\n",
			__func__, cmd, error);
		return error;
	}

	return 0;
}



static int spca700xa_read_byte(struct spca700xa *isp, UINT8 *value)
{
	u8 rx = 0;
	struct spi_transfer xfer = {
			.rx_buf = &rx,
			.len = 1,
			.bits_per_word = 8,
	};
	struct spi_message msg;
	int error;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	error = spi_sync(isp->spi, &msg);
	if (error)
		return error;


	*value = rx;
//	printk("%s:%x\n", __func__, *value);
	
	return 0;
}


int spca700xa_SPI_write(UINT8 *ucStartAddr, UINT32 ulTransByteCnt)
{	
	int i, j;

	printk("%s: ulTransByteCnt = %d\n", __func__, ulTransByteCnt);

	if (spca700xa_enable_dma) {
		for (i=0; i<ulTransByteCnt; i=i+2032) {
			j=i+2032;
			if (j<=ulTransByteCnt){
				spca700xa_cmd(isp, &ucStartAddr[i], 2032);
			} else {
				j=j-2032;
				j=ulTransByteCnt-j;
				spca700xa_cmd(isp, &ucStartAddr[i], j);
			}
		}		

	} else {
		spca700xa_cmd(isp, ucStartAddr, ulTransByteCnt);
	}

	return 0;
}

EXPORT_SYMBOL(spca700xa_SPI_write);

int spca700xa_SPI_read(UINT8 *ucStartAddr, UINT32 ulTransByteCnt)
{	
	UINT8 value = 0, error = 0;

	printk("%s: ulTransByteCnt = %d\n", __func__, ulTransByteCnt);

	spca700xa_read(isp, ucStartAddr, ulTransByteCnt);

	return 0;
}

EXPORT_SYMBOL(spca700xa_SPI_read);


struct dw_spi_chip {
	u8 poll_mode;	/* 0 for contoller polling mode */
	u8 type;	/* SPI/SSP/Micrwire */
	u8 enable_dma;
	void (*cs_control)(u32 command);
};


static struct dw_spi_chip chip = {
	.enable_dma = true,
};


static int __devinit spca700xa_probe(struct spi_device *spi)
{
	int error,i;

	
	printk("%s\n", __func__);

	isp = kzalloc(sizeof(*isp), GFP_KERNEL);
	isp->spi = spi;

	spi_set_drvdata(spi, isp);

	/* Set up SPI*/
	spi->bits_per_word = SPCA700XA_SPI_BITS_PER_WORD;
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = 25000000;
	spi->controller_data = &chip;
	
	error = spi_setup(spi);
	if (error < 0) {
		dev_err(&spi->dev, "%s: SPI setup error %d\n",
			__func__, error);
		goto error_buf;
	}	


	return 0;
error_buf:
	kfree(isp);
	return error;
}

static int __devexit spca700xa_remove(struct spi_device *spi)
{
	return 0;
}

#ifdef CONFIG_PM

static int spca700xa_suspend(struct spi_device *spi, pm_message_t state)
{
	return 0;
}

static int spca700xa_resume(struct spi_device *spi)
{
	return 0;
}

#else
#define spca700xa_suspend NULL
#define spca700xa_resume  NULL
#endif

static struct spi_driver spca700xa_driver = {
	.driver = {
		.name		= "spca700xa",
		.owner		= THIS_MODULE,
	},

	.probe		= spca700xa_probe,
	.remove		= __devexit_p(spca700xa_remove),
	.suspend	= spca700xa_suspend,
	.resume		= spca700xa_resume,
};

static int __init spca700xa_init(void)
{
	return spi_register_driver(&spca700xa_driver);
}
module_init(spca700xa_init);

static void __exit spca700xa_exit(void)
{
	spi_unregister_driver(&spca700xa_driver);
}
module_exit(spca700xa_exit);

MODULE_DESCRIPTION("SPCA700XA driver");
MODULE_AUTHOR("ASUS SW3");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:spca700xa");
