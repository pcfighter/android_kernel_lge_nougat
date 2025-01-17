/*
 * Copyright LG Electronics (c) 2011
 * All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/cradle.h>
#include <linux/gpio.h>
#include <linux/switch.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#ifdef CONFIG_64BIT
#include <soc/qcom/lge/board_lge.h>
#else
#include <mach/board_lge.h>
#endif
#if defined(CONFIG_STYLUS_PEN)
#define HALL_DETECT_DELAY 200
#endif

static int pre_set_flag;
struct pm8xxx_cradle {
	struct switch_dev sdev;
	struct delayed_work pouch_work;
	struct device *dev;
	struct wake_lock wake_lock;
	const struct pm8xxx_cradle_platform_data *pdata;
	int pouch;
	spinlock_t lock;
	int state;
#if defined(CONFIG_STYLUS_PEN)
	struct switch_dev pen_sdev;
	int pen;
	int pen_state;
	struct delayed_work pen_work;
#endif
};

#if defined(CONFIG_MACH_MSM8X10_W5_MPCS_US) || defined(CONFIG_MACH_MSM8X10_W5C_VZW) || \
	defined(CONFIG_MACH_MSM8X10_W5_AIO_US) || defined(CONFIG_MACH_MSM8X10_W5C_SPR_US) || \
	defined(CONFIG_MACH_MSM8X10_W5_TRF_US) || defined(CONFIG_MACH_MSM8X10_W5C_TRF_US) || \
	defined(CONFIG_MACH_MSM8X10_W5_CCA_US) || defined(CONFIG_MACH_MSM8226_E7WIFI) || \
	defined(CONFIG_MACH_MSM8226_E8WIFI) || defined(CONFIG_MACH_MSM8926_E8LTE) || \
	defined(CONFIG_MACH_MSM8226_E9WIFI) || defined(CONFIG_MACH_MSM8226_E9WIFIN)|| \
	defined(CONFIG_MACH_MSM8926_E7LTE_VZW_US) || defined(CONFIG_MACH_MSM8926_E7LTE_ATT_US) || \
	defined(CONFIG_MACH_MSM8926_E7LTE_USC_US) || defined(CONFIG_MACH_MSM8926_B2LN_KR) || \
	defined(CONFIG_MACH_MSM8939_ALTEV2_VZW) || defined(CONFIG_MACH_MSM8939_P1B_GLOBAL_COM) || \
	defined(CONFIG_MACH_MSM8939_P1BC_SPR_US) || defined(CONFIG_MACH_MSM8916_G4STYLUS_CRK_US) || \
        defined(CONFIG_MACH_MSM8916_STYLUSC_SPR_US) || defined(CONFIG_MACH_MSM8916_G4STYLUSDS_GLOBAL_COM) || \
        defined(CONFIG_MACH_MSM8916_G4STYLUSN_GLOBAL_COM) || \
        defined(CONFIG_MACH_MSM8916_G4STYLUSN_MPCS_US) || defined(CONFIG_MACH_MSM8916_G4STYLUSN_RGS_CA) || \
        defined(CONFIG_MACH_MSM8916_G4STYLUSN_TMO_US) || defined(CONFIG_MACH_MSM8916_G4STYLUSN_VTR_CA) || \
        defined(CONFIG_MACH_MSM8916_G4STYLUSW_KT_KR) || defined(CONFIG_MACH_MSM8939_P1BSSN_SKT_KR) || \
		defined(CONFIG_MACH_MSM8939_P1BSSN_BELL_CA)  || defined(CONFIG_MACH_MSM8916_PH1_VZW) || \
		defined(CONFIG_MACH_MSM8939_PH2_GLOBAL_COM) || defined(CONFIG_MACH_MSM8916_PH1_SPR_US)|| \
		defined(CONFIG_MACH_MSM8916_PH1_GLOBAL_COM) || defined(CONFIG_MACH_MSM8916_PH1_KR) || \
        defined(CONFIG_MACH_MSM8916_K5) || defined(CONFIG_MACH_MSM8916_PH1_CRK_US)
#define POUCH_DETECT_DELAY 100
#endif

static struct workqueue_struct *cradle_wq;
static struct pm8xxx_cradle *cradle;
#if defined(CONFIG_MACH_MSM8926_X10_VZW) || defined(CONFIG_MACH_MSM8926_B2L_ATT) || \
	defined(CONFIG_MACH_MSM8926_B2LN_KR) || defined(CONFIG_MACH_MSM8939_ALTEV2_VZW) || \
	defined(CONFIG_MACH_MSM8939_P1B_GLOBAL_COM) || defined(CONFIG_MACH_MSM8939_P1BC_SPR_US) || \
	defined(CONFIG_MACH_MSM8939_P1BSSN_SKT_KR) || defined(CONFIG_MACH_MSM8939_P1BSSN_BELL_CA) || \
	defined(CONFIG_MACH_MSM8916_G4STYLUS_CRK_US) || defined(CONFIG_MACH_MSM8916_G4STYLUSW_KT_KR) || \
	defined(CONFIG_MACH_MSM8916_STYLUSC_SPR_US) || defined(CONFIG_MACH_MSM8916_G4STYLUSDS_GLOBAL_COM) || \
	defined(CONFIG_MACH_MSM8916_G4STYLUSN_GLOBAL_COM) || \
	defined(CONFIG_MACH_MSM8916_G4STYLUSN_MPCS_US) || defined(CONFIG_MACH_MSM8916_G4STYLUSN_RGS_CA) || \
	defined(CONFIG_MACH_MSM8916_G4STYLUSN_TMO_US) || defined(CONFIG_MACH_MSM8916_G4STYLUSN_VTR_CA) || \
	defined(CONFIG_MACH_MSM8916_PH1_VZW) || defined(CONFIG_MACH_MSM8939_PH2_GLOBAL_COM) || defined(CONFIG_MACH_MSM8916_PH1_SPR_US) || \
	defined(CONFIG_MACH_MSM8916_PH1_GLOBAL_COM) || defined(CONFIG_MACH_MSM8916_PH1_KR) || defined(CONFIG_MACH_MSM8916_PH1_CRK_US) || \
    defined(CONFIG_MACH_MSM8916_K5)
#if defined(CONFIG_TOUCHSCREEN_ATMEL_S540) || defined(CONFIG_TOUCHSCREEN_ATMEL_T1664) || defined(CONFIG_TOUCHSCREEN_LGE_SYNAPTICS_TD4191) || \
	defined(CONFIG_TOUCHSCREEN_MELFAS_MIT300) || defined(CONFIG_TOUCHSCREEN_MELFAS_MIT300_PH1) || defined(CONFIG_TOUCHSCREEN_UNIFIED_MELFAS_MIT300_PH1) || \
	defined(CONFIG_TOUCHSCREEN_UNIFIED_SYNAPTICS_TD4100_PH1)
static int is_smart_cover_closed = 0; /* check status of smart cover to resize quick window area */
int cradle_smart_cover_status(void)
{
	return is_smart_cover_closed;
}
#endif
#if defined(CONFIG_TOUCHSCREEN_UNIFIED_MELFAS_MIT300_PH1)
extern void MIT300_Set_BootCoverMode(int status);
#endif
#if defined(CONFIG_TOUCHSCREEN_UNIFIED_SYNAPTICS_TD4100_PH1)
extern void TD4100_Set_BootCoverMode(int status);
#endif

#endif
static void boot_cradle_det_func(void)
{
	int state;
#if defined(CONFIG_STYLUS_PEN)
	int pen_state;
#endif
	if (cradle->pdata->hallic_pouch_detect_pin)
		cradle->pouch = !gpio_get_value(cradle->pdata->hallic_pouch_detect_pin);

	printk("%s : boot pouch === > %d \n", __func__ , cradle->pouch);

#if defined(CONFIG_STYLUS_PEN)
	if (cradle->pdata->hallic_pen_detect_pin)
		cradle->pen = !gpio_get_value(cradle->pdata->hallic_pen_detect_pin);

	printk("%s : boot pen === > %d \n", __func__ , cradle->pen);

	if (cradle->pouch == 1) {
		if (cradle->pen == 1) {
			state=SMARTCOVER_POUCH_CLOSED;
			pen_state=SMARTCOVER_PEN_IN;
		} else {
			state=SMARTCOVER_POUCH_CLOSED;
			pen_state=SMARTCOVER_PEN_OUT;
		}
	} else {
		if (cradle->pen == 1) {
			state=SMARTCOVER_POUCH_OPENED;
			pen_state=SMARTCOVER_PEN_IN;
		} else {
			state=SMARTCOVER_POUCH_OPENED;
			pen_state=SMARTCOVER_PEN_OUT;
		}
	}
	printk("%s : [Cradle] boot cradle value : pouch_state is %d, pen_state is %d\n", __func__ , state, pen_state);
	cradle->state = state;
	wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(3000));
	switch_set_state(&cradle->sdev, cradle->state);
	cradle->pen_state = pen_state;
	wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(3000));
	switch_set_state(&cradle->pen_sdev, cradle->pen_state);
#if defined(CONFIG_TOUCHSCREEN_UNIFIED_MELFAS_MIT300_PH1)
	is_smart_cover_closed = cradle->pouch;
	MIT300_Set_BootCoverMode(cradle->pouch);
#endif
#if defined(CONFIG_TOUCHSCREEN_UNIFIED_SYNAPTICS_TD4100_PH1)
	is_smart_cover_closed = cradle->pouch;
	TD4100_Set_BootCoverMode(cradle->pouch);
#endif

#else
	if(cradle->pouch == 1)
		state = SMARTCOVER_POUCH_CLOSED;
	else
		state = SMARTCOVER_POUCH_OPENED;

	printk("%s : [Cradle] boot cradle value is %d\n", __func__ , state);

	cradle->state = state;
	wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(3000));
	switch_set_state(&cradle->sdev, cradle->state);

#if defined(CONFIG_TOUCHSCREEN_LGE_SYNAPTICS_TD4191)
	is_smart_cover_closed = cradle->pouch;
#endif
#endif
}

#if defined(CONFIG_STYLUS_PEN)
static void pm8xxx_pen_work_func(struct work_struct *work)
{
	int pen_state = 0;
	unsigned long flags;

	spin_lock_irqsave(&cradle->lock, flags);

	if (cradle->pdata->hallic_pouch_detect_pin)
		cradle->pouch = !gpio_get_value(cradle->pdata->hallic_pouch_detect_pin);

	if (cradle->pdata->hallic_pen_detect_pin)
		cradle->pen = !gpio_get_value(cradle->pdata->hallic_pen_detect_pin);


	printk("%s : pouch === > %d \n", __func__ , cradle->pouch);
	printk("%s : pen === > %d \n", __func__ , cradle->pen);


	if (cradle->pen == 1) {
		pen_state = SMARTCOVER_PEN_IN;
	} else if (cradle->pen == 0) {
		pen_state = SMARTCOVER_PEN_OUT;
	} else {}

	if (cradle->pen_state != pen_state) {
		cradle->pen_state = pen_state;
		spin_unlock_irqrestore(&cradle->lock, flags);
		wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(3000));
		switch_set_state(&cradle->pen_sdev, cradle->pen_state);
		printk("%s : [Cradle] pen value is %d\n", __func__ , pen_state);
	} else {
		spin_unlock_irqrestore(&cradle->lock, flags);
		printk("%s : [Cradle] pen value is %d (no change)\n", __func__ , pen_state);
	}
}

#endif
static void pm8xxx_pouch_work_func(struct work_struct *work)
{
	int state = 0;
	unsigned long flags;

	spin_lock_irqsave(&cradle->lock, flags);

	if (cradle->pdata->hallic_pouch_detect_pin)
		cradle->pouch = !gpio_get_value(cradle->pdata->hallic_pouch_detect_pin);

	printk("%s : pouch === > %d \n", __func__ , cradle->pouch);
#if defined(CONFIG_STYLUS_PEN)
	if (cradle->pdata->hallic_pen_detect_pin)
		cradle->pen = !gpio_get_value(cradle->pdata->hallic_pen_detect_pin);

	printk("%s : pen === > %d \n", __func__ , cradle->pen);
#endif

	if (cradle->pouch == 1)
		state = SMARTCOVER_POUCH_CLOSED;
	else if (cradle->pouch == 0)
		state = SMARTCOVER_POUCH_OPENED;

	if (cradle->state != state) {
		cradle->state = state;
		spin_unlock_irqrestore(&cradle->lock, flags);
		wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(3000));
		switch_set_state(&cradle->sdev, cradle->state);
		printk("%s : [Cradle] pouch value is %d\n", __func__ , state);
	}
	else {
		spin_unlock_irqrestore(&cradle->lock, flags);
		printk("%s : [Cradle] pouch value is %d (no change)\n", __func__ , state);
	}
#if defined(CONFIG_MACH_MSM8926_B2LN_KR) || defined(CONFIG_MACH_MSM8939_ALTEV2_VZW) || defined(CONFIG_MACH_MSM8939_P1B_GLOBAL_COM) || defined(CONFIG_MACH_MSM8939_P1BC_SPR_US) || defined(CONFIG_MACH_MSM8939_P1BSSN_SKT_KR) || defined(CONFIG_MACH_MSM8939_P1BSSN_BELL_CA) || defined(CONFIG_MACH_MSM8939_PH2_GLOBAL_COM)
#if defined(CONFIG_TOUCHSCREEN_ATMEL_S540) || defined(CONFIG_TOUCHSCREEN_ATMEL_T1664)
	is_smart_cover_closed = state;
#endif
#endif
}

void cradle_set_deskdock(int state)
{
	unsigned long flags;

	if (&cradle->sdev) {
		spin_lock_irqsave(&cradle->lock, flags);
		if (cradle->state != state) {
			cradle->state = state;
			spin_unlock_irqrestore(&cradle->lock, flags);
			wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(3000));
			switch_set_state(&cradle->sdev, cradle->state);
		} else {
			spin_unlock_irqrestore(&cradle->lock, flags);
		}
	} else {
		pre_set_flag = state;
	}
}

int cradle_get_deskdock(void)
{
	if (!cradle)
		return pre_set_flag;

	return cradle->state;
}

static irqreturn_t pm8xxx_pouch_irq_handler(int irq, void *handle)
{
#if !defined(CONFIG_MACH_MSM8926_X10_VZW) && !defined(CONFIG_MACH_MSM8926_B2L_ATT)
	struct pm8xxx_cradle *cradle_handle = handle;
	int v = 200;
	printk("pouch irq!!!!\n");
#if defined(CONFIG_MACH_MSM8X10_W5_MPCS_US) || defined(CONFIG_MACH_MSM8X10_W5C_VZW) || \
	defined(CONFIG_MACH_MSM8X10_W5_AIO_US) || defined(CONFIG_MACH_MSM8X10_W5C_SPR_US) || \
	defined(CONFIG_MACH_MSM8X10_W5_TRF_US) || defined(CONFIG_MACH_MSM8X10_W5C_TRF_US) || \
	defined(CONFIG_MACH_MSM8X10_W5_CCA_US) || defined(CONFIG_MACH_MSM8226_E7WIFI) || \
	defined(CONFIG_MACH_MSM8226_E8WIFI) || defined(CONFIG_MACH_MSM8926_E8LTE) || \
	defined(CONFIG_MACH_MSM8226_E9WIFI) || defined(CONFIG_MACH_MSM8226_E9WIFIN) || \
	defined(CONFIG_MACH_MSM8926_E7LTE_VZW_US) || defined(CONFIG_MACH_MSM8926_E7LTE_ATT_US) || \
	defined(CONFIG_MACH_MSM8926_E7LTE_USC_US) || defined(CONFIG_MACH_MSM8926_B2LN_KR) || \
	defined(CONFIG_MACH_MSM8939_ALTEV2_VZW) || defined(CONFIG_MACH_MSM8939_P1B_GLOBAL_COM) || \
	defined(CONFIG_MACH_MSM8939_P1BC_SPR_US) || defined(CONFIG_MACH_MSM8916_G4STYLUS_CRK_US) || \
	defined(CONFIG_MACH_MSM8916_STYLUSC_SPR_US) || defined(CONFIG_MACH_MSM8916_G4STYLUSDS_GLOBAL_COM) || \
	defined(CONFIG_MACH_MSM8916_G4STYLUSN_GLOBAL_COM) || \
	defined(CONFIG_MACH_MSM8916_G4STYLUSN_MPCS_US) || defined(CONFIG_MACH_MSM8916_G4STYLUSN_RGS_CA) || \
	defined(CONFIG_MACH_MSM8916_G4STYLUSN_TMO_US) || defined(CONFIG_MACH_MSM8916_G4STYLUSN_VTR_CA) || \
	defined(CONFIG_MACH_MSM8916_G4STYLUSW_KT_KR) || defined(CONFIG_MACH_MSM8939_P1BSSN_SKT_KR)|| \
	defined(CONFIG_MACH_MSM8939_P1BSSN_BELL_CA) || defined(CONFIG_MACH_MSM8916_PH1_VZW) || \
	defined(CONFIG_MACH_MSM8939_PH2_GLOBAL_COM) || defined(CONFIG_MACH_MSM8916_PH1_SPR_US) || \
	defined(CONFIG_MACH_MSM8916_PH1_GLOBAL_COM) || defined(CONFIG_MACH_MSM8916_PH1_KR) || \
    defined(CONFIG_MACH_MSM8916_K5) || defined(CONFIG_MACH_MSM8916_PH1_CRK_US)
#if defined(CONFIG_TOUCHSCREEN_LGE_SYNAPTICS_TD4191) || defined(CONFIG_TOUCHSCREEN_UNIFIED_MELFAS_MIT300_PH1) || \
	defined(CONFIG_TOUCHSCREEN_UNIFIED_SYNAPTICS_TD4100_PH1)
	is_smart_cover_closed = !gpio_get_value(cradle->pdata->hallic_pouch_detect_pin);
#if defined(CONFIG_MACH_MSM8916_PH1)
	v = 2;
#else
	v = 1 + 1*is_smart_cover_closed;
#endif
#else
	v = 1 + 1*(!gpio_get_value(cradle->pdata->hallic_pouch_detect_pin));
#endif
	wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(POUCH_DETECT_DELAY*v+5));
	queue_delayed_work(cradle_wq, &cradle_handle->pouch_work, msecs_to_jiffies(POUCH_DETECT_DELAY*v+5));
#else
#ifdef CONFIG_MACH_MSM8926_VFP_KR
	if(cradle->state == 1) v = 0;
#endif
	queue_delayed_work(cradle_wq, &cradle_handle->pouch_work, msecs_to_jiffies(v));
#endif
#endif
	return IRQ_HANDLED;
}

#if defined(CONFIG_STYLUS_PEN)
static irqreturn_t pm8xxx_pen_irq_handler(int irq, void *handle)
{
	struct pm8xxx_cradle *cradle_handle = handle;
	printk("pen irq!!!!\n");
	wake_lock_timeout(&cradle->wake_lock, msecs_to_jiffies(HALL_DETECT_DELAY+5));
	queue_delayed_work(cradle_wq, &cradle_handle->pen_work, msecs_to_jiffies(HALL_DETECT_DELAY));
	return IRQ_HANDLED;
}
#endif
static ssize_t
cradle_pouch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
	len = snprintf(buf, PAGE_SIZE, "pouch : %d\n", cradle->pouch);

	return len;
}


static ssize_t
cradle_sensing_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
	len = snprintf(buf, PAGE_SIZE, "sensing(cradle state) : %d\n", cradle->state);

	return len;
}

#if defined(CONFIG_STYLUS_PEN)
static ssize_t
cradle_pen_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len;
	struct pm8xxx_cradle *cradle = dev_get_drvdata(dev);
	len = snprintf(buf, PAGE_SIZE, "pen : %d\n", cradle->pen);

	return len;
}
#endif
#ifdef CONFIG_MACH_MSM8926_VFP_KR
static ssize_t
cradle_sensing_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{

	int ret = -EINVAL, len = strcspn(buf, " ");

	if (len > 0 && buf[len] == '\0')
		len--;

	if (strncmp(buf, "1", len) == 0) {
		cradle_set_deskdock(1);
		ret = size;
	} else if (strncmp(buf, "0", len) == 0) {
		cradle_set_deskdock(0);
		ret = size;
	}

	return ret;

}
#endif

#if defined(CONFIG_STYLUS_PEN)
static struct device_attribute cradle_pen_attr   = __ATTR(pen, S_IRUGO, cradle_pen_show, NULL);
#endif
#ifdef CONFIG_MACH_MSM8926_VFP_KR
static struct device_attribute cradle_sensing_attr = __ATTR(sensing, S_IRUGO | S_IWUSR, cradle_sensing_show, cradle_sensing_store);
#else
static struct device_attribute cradle_sensing_attr = __ATTR(sensing, S_IRUGO, cradle_sensing_show, NULL);
#endif
static struct device_attribute cradle_pouch_attr   = __ATTR(pouch, S_IRUGO, cradle_pouch_show, NULL);

static ssize_t cradle_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
	case 0:
		return sprintf(buf, "UNDOCKED\n");
	case 2:
		return sprintf(buf, "CARKIT\n");
	}
	return -EINVAL;
}

#if defined(CONFIG_S5717)
static void s5717_parse_dt(struct device *dev,
		struct pm8xxx_cradle_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if ((pdata->hallic_pouch_detect_pin = of_get_named_gpio_flags(np, "hallic-pouch-irq-gpio", 0, NULL)) > 0)
		pdata->hallic_pouch_irq = gpio_to_irq(pdata->hallic_pouch_detect_pin);
	printk("[Hall IC] hallic_pouch_gpio: %d\n", pdata->hallic_pouch_detect_pin);
#if defined(CONFIG_STYLUS_PEN)
	if ((pdata->hallic_pen_detect_pin = of_get_named_gpio_flags(np, "hallic-pen-irq-gpio", 0, NULL)) > 0)
		pdata->hallic_pen_irq = gpio_to_irq(pdata->hallic_pen_detect_pin);
	printk("[Hall IC] hallic_pen_gpio: %d\n", pdata->hallic_pen_detect_pin);
#endif
	pdata->irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
}
#elif defined(CONFIG_BU52061NVX)
static void bu52061nvx_parse_dt(struct device *dev,
		struct pm8xxx_cradle_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if ((pdata->hallic_pouch_detect_pin = of_get_named_gpio_flags(np, "hallic-pouch-irq-gpio", 0, NULL)) > 0)
		pdata->hallic_pouch_irq = gpio_to_irq(pdata->hallic_pouch_detect_pin);

	printk("[Hall IC] hallic_pouch_gpio: %d\n", pdata->hallic_pouch_detect_pin);

	pdata->irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;
}
#endif

static int pm8xxx_cradle_probe(struct platform_device *pdev)
{
	int ret;
	unsigned int hall_pouch_gpio_irq = 0;
#if defined(CONFIG_STYLUS_PEN)
	unsigned int hall_pen_gpio_irq = 0;
#endif

	struct pm8xxx_cradle_platform_data *pdata;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct pm8xxx_cradle_platform_data),
				GFP_KERNEL);
		if (pdata == NULL) {
			pr_err("%s: no pdata\n", __func__);
			return -ENOMEM;
		}
		pdev->dev.platform_data = pdata;

#if defined(CONFIG_S5717)
		s5717_parse_dt(&pdev->dev, pdata);
#elif defined(CONFIG_BU52061NVX)
		bu52061nvx_parse_dt(&pdev->dev, pdata);
#endif
	} else {
		pdata = pdev->dev.platform_data;
	}
	if (!pdata) {
		pr_err("%s: no pdata\n", __func__);
		return -ENOMEM;
	}

	cradle = kzalloc(sizeof(*cradle), GFP_KERNEL);
	if (!cradle)
		return -ENOMEM;

	cradle->pdata	= pdata;
#ifdef CONFIG_MACH_MSM8926_VFP_KR
	cradle->sdev.name = "folderstatus";
#else
	cradle->sdev.name = "smartcover";
#endif
	cradle->sdev.print_name = cradle_print_name;
	cradle->pouch = 0;
#if defined(CONFIG_STYLUS_PEN)
	cradle->pen = 0;
	cradle->pen_sdev.name = "pen_state";
	cradle->pen_sdev.print_name = cradle_print_name;
#endif

	spin_lock_init(&cradle->lock);

	ret = switch_dev_register(&cradle->sdev);
	if (ret < 0)
		goto err_switch_dev_register;
#if defined(CONFIG_STYLUS_PEN)
	ret = switch_dev_register(&cradle->pen_sdev);
	if (ret < 0)
		goto err_switch_dev_register;
#endif
	if (pre_set_flag) {
		cradle_set_deskdock(pre_set_flag);
		cradle->state = pre_set_flag;
	}
	wake_lock_init(&cradle->wake_lock, WAKE_LOCK_SUSPEND, "hall_ic_wakeups");

	INIT_DELAYED_WORK(&cradle->pouch_work, pm8xxx_pouch_work_func);
#if defined(CONFIG_STYLUS_PEN)
	INIT_DELAYED_WORK(&cradle->pen_work, pm8xxx_pen_work_func);
#endif

	printk("%s : init cradle\n", __func__);

	/* initialize irq of gpio_hall */
	if (cradle->pdata->hallic_pouch_detect_pin > 0) {
		hall_pouch_gpio_irq = gpio_to_irq(cradle->pdata->hallic_pouch_detect_pin);
		printk("%s : hall_pouch_gpio_irq = [%d]\n", __func__, hall_pouch_gpio_irq);
		if (hall_pouch_gpio_irq < 0) {
			printk("Failed : GPIO TO IRQ \n");
			ret = hall_pouch_gpio_irq;
			goto err_request_irq;
		}

		ret = request_irq(hall_pouch_gpio_irq, pm8xxx_pouch_irq_handler, pdata->irq_flags, HALL_IC_DEV_NAME, cradle);
		if (ret > 0) {
			printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n", __func__, hall_pouch_gpio_irq, ret);
			goto err_request_irq;
		}

		if (enable_irq_wake(hall_pouch_gpio_irq) == 0)
			printk("%s :enable_irq_wake Enable(1)\n",__func__);
		else
			printk("%s :enable_irq_wake failed(1)\n",__func__);
	}

#if defined(CONFIG_STYLUS_PEN)
	if (cradle->pdata->hallic_pen_detect_pin > 0) {
		hall_pen_gpio_irq = gpio_to_irq(cradle->pdata->hallic_pen_detect_pin);
		printk("%s : hall_pen_gpio_irq = [%d]\n", __func__, hall_pen_gpio_irq);
		if (hall_pen_gpio_irq < 0) {
			printk("Failed : GPIO TO IRQ \n");
			ret = hall_pen_gpio_irq;
			goto err_request_irq;
		}
		ret = request_irq(hall_pen_gpio_irq, pm8xxx_pen_irq_handler, pdata->irq_flags, HALL_IC_DEV_NAME, cradle);
		if (ret > 0) {
			printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n", __func__, hall_pen_gpio_irq, ret);
			goto err_request_irq;
		}

		if (enable_irq_wake(hall_pen_gpio_irq) == 0)
			printk("%s :enable_irq_wake Enable(2)\n",__func__);
		else
			printk("%s :enable_irq_wake failed(2)\n",__func__);
	}
#endif
	printk("%s : pdata->irq_flags = [%d]\n", __func__,(int)pdata->irq_flags);

	printk("%s :boot_cradle_det_func START\n",__func__);
	boot_cradle_det_func();

	ret = device_create_file(&pdev->dev, &cradle_sensing_attr);
	if (ret)
		goto err_request_irq;

	if (cradle->pdata->hallic_pouch_detect_pin > 0) {
		ret = device_create_file(&pdev->dev, &cradle_pouch_attr);
		if (ret)
			goto err_request_irq;
	}

#if defined(CONFIG_STYLUS_PEN)
	if (cradle->pdata->hallic_pen_detect_pin > 0) {
		ret = device_create_file(&pdev->dev, &cradle_pen_attr);
		if (ret)
			goto err_request_irq;
	}
#endif
	platform_set_drvdata(pdev, cradle);
	return 0;

err_request_irq:
	if (hall_pouch_gpio_irq)
		free_irq(hall_pouch_gpio_irq, 0);
#if defined(CONFIG_STYLUS_PEN)
	if (hall_pen_gpio_irq)
		free_irq(hall_pen_gpio_irq, 0);
#endif

err_switch_dev_register:
	switch_dev_unregister(&cradle->sdev);
	kfree(cradle);
	return ret;
}

static int pm8xxx_cradle_remove(struct platform_device *pdev)
{
	struct pm8xxx_cradle *cradle = platform_get_drvdata(pdev);
	cancel_delayed_work_sync(&cradle->pouch_work);
#if defined(CONFIG_STYLUS_PEN)
	cancel_delayed_work_sync(&cradle->pen_work);
#endif
	switch_dev_unregister(&cradle->sdev);
	platform_set_drvdata(pdev, NULL);
	kfree(cradle);

	return 0;
}

static int pm8xxx_cradle_suspend(struct device *dev)
{
	return 0;
}

static int pm8xxx_cradle_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops pm8xxx_cradle_pm_ops = {
	.suspend = pm8xxx_cradle_suspend,
	.resume = pm8xxx_cradle_resume,
};

#ifdef CONFIG_OF
static struct of_device_id pm8xxx_match_table[] = {
	{ .compatible = "seiko,hall-s5712", },
	{ .compatible = "seiko,hall-s5717", },
	{ .compatible = "rohm,hall-bu52061nvx", },
	{ },
};
#endif

static struct platform_driver pm8xxx_cradle_driver = {
	.probe		= pm8xxx_cradle_probe,
	.remove		= pm8xxx_cradle_remove,
	.driver		= {
		.name    = HALL_IC_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = pm8xxx_match_table,
#endif
#ifdef CONFIG_PM
		.pm	= &pm8xxx_cradle_pm_ops,
#endif
	},
};

static int __init pm8xxx_cradle_init(void)
{
	cradle_wq = create_singlethread_workqueue("cradle_wq");
	printk(KERN_INFO "cradle init \n");
	if (!cradle_wq)
		return -ENOMEM;
	return platform_driver_register(&pm8xxx_cradle_driver);
}
module_init(pm8xxx_cradle_init);

static void __exit pm8xxx_cradle_exit(void)
{
	if (cradle_wq)
		destroy_workqueue(cradle_wq);
	platform_driver_unregister(&pm8xxx_cradle_driver);
}
module_exit(pm8xxx_cradle_exit);

MODULE_ALIAS("platform:" HALL_IC_DEV_NAME);
MODULE_AUTHOR("LG Electronics Inc.");
MODULE_DESCRIPTION("pm8xxx cradle driver");
MODULE_LICENSE("GPL");
