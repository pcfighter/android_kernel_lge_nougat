/***************************************************************************
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *    File  : lgtp_platform_api.c
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/
#define LGTP_MODULE "[MISC]"

/****************************************************************************
* Include Files
****************************************************************************/
#include <linux/input/unified_driver_4/lgtp_common.h>

#include <linux/input/unified_driver_4/lgtp_model_config_misc.h>
#include <linux/input/unified_driver_4/lgtp_model_config_i2c.h>

#include <linux/input/unified_driver_4/lgtp_platform_api_misc.h>
#include <linux/input/unified_driver_4/lgtp_platform_api_i2c.h>


/****************************************************************************
* Manifest Constants / Defines
****************************************************************************/

/****************************************************************************
 * Macros
 ****************************************************************************/

/****************************************************************************
* Type Definitions
****************************************************************************/

/****************************************************************************
* Variables
****************************************************************************/
#if defined ( TOUCH_PLATFORM_QCT )
static int nIrq_num;
atomic_t	touch_irq_mask;
#endif

/****************************************************************************
* Extern Function Prototypes
****************************************************************************/
extern int touch_module;

#if defined (CONFIG_TOUCHSCREEN_UNIFIED_SYNAPTICS_TD4100_PH1)
extern int get_display_id(void);
int ph1_tddi_id;
#endif


/****************************************************************************
* Local Function Prototypes
****************************************************************************/


/****************************************************************************
* Local Functions
****************************************************************************/
#if 0 /* TBD */
#if defined(TOUCH_PLATFORM_QCT)
#define istate core_internal_state__do_not_mess_with_it
#define IRQS_PENDING 0x00000200
#endif
static void TouchClearPendingIrq(void)
{
	#if defined(TOUCH_PLATFORM_QCT)
	unsigned long flags;
	struct irq_desc *desc = irq_to_desc(nIrq_num);

	if (desc) {
		raw_spin_lock_irqsave(&desc->lock, flags);
		desc->istate &= ~IRQS_PENDING;
		raw_spin_unlock_irqrestore(&desc->lock, flags);
	}
	#endif
}
#endif

static void TouchMaskIrq(void)
{
	#if defined(TOUCH_PLATFORM_QCT)
	struct irq_desc *desc = irq_to_desc(nIrq_num);

	if (desc->irq_data.chip->irq_mask)
		desc->irq_data.chip->irq_mask(&desc->irq_data);
	#endif
}

static void TouchUnMaskIrq(void)
{
	#if defined(TOUCH_PLATFORM_QCT)
	struct irq_desc *desc = irq_to_desc(nIrq_num);

	if (desc->irq_data.chip->irq_unmask)
		desc->irq_data.chip->irq_unmask(&desc->irq_data);
	#endif
}


/****************************************************************************
* Global Functions
****************************************************************************/
int TouchGetModuleIndex(void)
{
	int index = FIRST_MODULE;
    #if defined(TOUCH_MODEL_LION_3G)
    index = touch_module;
    #endif
	#if defined (CONFIG_TOUCHSCREEN_UNIFIED_SYNAPTICS_TD4100_PH1)
	index = get_display_id();	//read touch maker ID ( 0 : mit300, 1 : td4100)
	TOUCH_LOG("Display Module Get. Module index = %d\n", get_display_id());
	ph1_tddi_id = index;
	#endif

    TOUCH_LOG("Touch Module Get. Module index = %d\n",index);
	return index;
}

int TouchGetBootMode(void)
{
	int mode = BOOT_NORMAL;

	#if defined(TOUCH_PLATFORM_MSM8916) || defined(TOUCH_PLATFORM_MSM8939)
	{
		enum lge_boot_mode_type lge_boot_mode;

		lge_boot_mode = lge_get_boot_mode();

		if (lge_boot_mode == LGE_BOOT_MODE_CHARGERLOGO)
			mode = BOOT_OFF_CHARGING;
		else if (lge_boot_mode >=  LGE_BOOT_MODE_QEM_56K)
			mode = BOOT_MINIOS;
		else
			mode = BOOT_NORMAL;
	}
	#elif defined(TOUCH_PLATFORM_MSM8210)
	{
		enum lge_boot_mode_type lge_boot_mode;

		lge_boot_mode = lge_get_boot_mode();

		if (lge_boot_mode == LGE_BOOT_MODE_CHARGERLOGO)
			mode = BOOT_OFF_CHARGING;
		else
			mode = BOOT_NORMAL;
	}
	#elif defined(TOUCH_PLATFORM_MTK)

	#else
	#error "Platform should be defined"
	#endif

	return mode;
}

int TouchInitializeGpio(void)
{
	TOUCH_FUNC();

	#if defined(TOUCH_PLATFORM_QCT)
	{
		int ret = 0;

		ret = gpio_request(TOUCH_GPIO_RESET, "touch_reset");
		if (ret < 0)
			TOUCH_ERR("failed at gpio_request(reset pin)\n");

		gpio_direction_output(TOUCH_GPIO_RESET, 1);

		ret = gpio_request(TOUCH_GPIO_INTERRUPT, "touch_int");
		if (ret < 0)
			TOUCH_ERR("failed at gpio_request(interrupt pin)\n");

		gpio_direction_input(TOUCH_GPIO_INTERRUPT);
	}

	#elif defined(TOUCH_MODEL_LION_3G)

	#elif defined(TOUCH_PLATFORM_MTK)

	mt_set_gpio_mode(GPIO_TOUCH_RESET, GPIO_TOUCH_RESET_M_GPIO);
	mt_set_gpio_dir(GPIO_TOUCH_RESET, GPIO_DIR_OUT);

	mt_set_gpio_mode(GPIO_TOUCH_INT, GPIO_TOUCH_INT_M_EINT);
	mt_set_gpio_dir(GPIO_TOUCH_INT, GPIO_DIR_IN);

	#else
	#error "Platform should be defined"
	#endif

	return TOUCH_SUCCESS;
}

void TouchSetGpioReset(int isHigh)
{

	if (isHigh) {
		#if defined(TOUCH_PLATFORM_QCT)
		gpio_set_value(TOUCH_GPIO_RESET, 1);
		#elif defined(TOUCH_MODEL_LION_3G)

		#elif defined(TOUCH_PLATFORM_MTK)
		mt_set_gpio_out(GPIO_TOUCH_RESET, GPIO_OUT_ONE);
		#else
		#error "Platform should be defined"
		#endif

		TOUCH_LOG("Reset Pin was set to HIGH\n");
	} else {
		#if defined(TOUCH_PLATFORM_QCT)
		gpio_set_value(TOUCH_GPIO_RESET, 0);
		#elif defined(TOUCH_MODEL_LION_3G)

		#elif defined(TOUCH_PLATFORM_MTK)
		mt_set_gpio_out(GPIO_TOUCH_RESET, GPIO_OUT_ZERO);
		#else
		#error "Platform should be defined"
		#endif

		TOUCH_LOG("Reset Pin was set to LOW\n");
	}

}

int TouchReadGpioInterrupt(void)
{
	int gpioState = 0;

	#if defined(TOUCH_PLATFORM_QCT)

	gpioState = gpio_get_value(TOUCH_GPIO_INTERRUPT);

	#elif defined(TOUCH_PLATFORM_MTK)

	gpioState = mt_get_gpio_in(GPIO_TOUCH_INT); /* TBD */

	#else
	#error "Platform  should be defined"
	#endif

	return gpioState;
}

#if defined(TOUCH_DEVICE_LU201X) || defined(TOUCH_DEVICE_LU202X)
void TouchToggleGpioInterrupt(void)
{
	#if defined ( TOUCH_PLATFORM_QCT )

	/* Add interrupt pin change for output mode */
	gpio_direction_output(TOUCH_GPIO_INTERRUPT, 0);

	/* Add here interrupt pin goes to low */
	gpio_set_value(TOUCH_GPIO_INTERRUPT, 0);

	udelay(100);

	/* Add here interrupt pin goes to high */
	gpio_set_value(TOUCH_GPIO_INTERRUPT, 1);

	/* Add here interrupt pin change for input mode */
	gpio_direction_input(TOUCH_GPIO_INTERRUPT);

	mdelay(1);

	#elif defined ( TOUCH_PLATFORM_MTK )

	mt_set_gpio_dir(GPIO_TOUCH_INT, GPIO_DIR_OUT);

	mt_set_gpio_out(GPIO_TOUCH_INT, GPIO_OUT_ONE);

	udelay(100);

	mt_set_gpio_out(GPIO_TOUCH_INT, GPIO_OUT_ZERO);

	mt_set_gpio_dir(GPIO_TOUCH_INT, GPIO_DIR_IN);

	mdelay(1);

	#else
	#error "Platform  should be defined"
	#endif

}
#endif

int TouchRegisterIrq(TouchDriverData *pDriverData, irq_handler_t irqHandler, irq_handler_t threaded_irqHandler)
{
	TOUCH_FUNC();

	#if defined(TOUCH_PLATFORM_QCT)
	{
		int ret = 0;
		ret = request_threaded_irq(pDriverData->client->irq, irqHandler, threaded_irqHandler, TOUCH_IRQ_FLAGS, pDriverData->client->name, pDriverData);
		if( ret < 0 ) {
			TOUCH_ERR("failed at request_irq() ( error = %d )\n", ret );
			return TOUCH_FAIL;
		}
		enable_irq_wake(pDriverData->client->irq);
	}
	nIrq_num = pDriverData->client->irq;

	#elif defined(TOUCH_PLATFORM_MTK)

	mt_eint_registration(CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_TYPE, irqHandler, 1);

	#else
	#error "Platform should be defined"
	#endif

	return TOUCH_SUCCESS;
}

void TouchEnableIrq(void)
{
	TouchUnMaskIrq();

	/* TouchClearPendingIrq(); TBD */

	#if defined(TOUCH_PLATFORM_QCT)

	if (atomic_read(&touch_irq_mask) != 0) {
		atomic_set(&touch_irq_mask, 0);
		enable_irq(nIrq_num);
	}

	#elif defined(TOUCH_PLATFORM_MTK)

	mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM);

	#else
	#error "Platform should be defined"
	#endif

	TOUCH_LOG("Interrupt Enabled\n");
}

void TouchDisableIrq(void)
{
	#if defined(TOUCH_PLATFORM_QCT)

	if (atomic_read(&touch_irq_mask) == 0) {
		atomic_set(&touch_irq_mask, 1);
		disable_irq_nosync(nIrq_num);
	}

	#elif defined(TOUCH_PLATFORM_MTK)

	mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM);

	#else
	#error "Platform should be defined"
	#endif

	TouchMaskIrq();

	TOUCH_LOG("Interrupt Disabled\n");
}


int TouchReadReg(u16 addr, u8 *rxbuf, int len)
{
    int ret = 0;
    #if defined(TOUCH_I2C_USE)
       ret = Touch_I2C_Read( addr, rxbuf, len );
    #elif defined(TOUCH_SPI_USE)
       ret = Touch_SPI_Read( addr, rxbuf, len );
    #endif
    return ret;
}

int TouchWriteReg(u16 addr, u8 *txbuf, int len)
{
    int ret = 0;
    #if defined(TOUCH_I2C_USE)
         ret = Touch_I2C_Write( addr, txbuf, len );
    #elif defined(TOUCH_SPI_USE)
         ret = Touch_SPI_Write( addr, txbuf, len );
    #endif
    return ret;
}

int TouchReadByteReg(u16 addr, u8 *rxbuf)
{
    int ret = 0;

    #if defined(TOUCH_I2C_USE)
       ret = Touch_I2C_Read( addr, rxbuf, 1 );
    #elif defined(TOUCH_SPI_USE)
       ret = Touch_SPI_Read( addr, rxbuf, 1 );
    #endif

    return ret;
}

int TouchWriteByteReg(u16 addr, u8 txbuf)
{
    int ret = 0;
	u8 data = txbuf;

    #if defined(TOUCH_I2C_USE)
         ret = Touch_I2C_Write( addr, &data, 1 );
    #elif defined(TOUCH_SPI_USE)
         ret = Touch_SPI_Write( addr, &data, 1);
    #endif

    return ret;
}

#if defined(TOUCH_PLATFORM_MTK) /* TBD */
bool key_lock_status = 0;
void touch_keylock_enable(int key_lock)
{
	TOUCH_FUNC();

	if (!key_lock) {
		TouchEnableIrq();
		key_lock_status = 0;
	} else {
		TouchDisableIrq();
		key_lock_status = 1;
	}
}
EXPORT_SYMBOL(touch_keylock_enable);
#endif



/* End Of File */

