/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/leds-qpnp-wled.h>
#include <linux/clk.h>

#include "mdss.h"
#include "mdss_panel.h"
#include "mdss_dsi.h"
#include "mdss_debug.h"

#ifdef CONFIG_MFD_DW8768
#include <linux/mfd/dw8768.h>
#endif

#if defined(CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
extern void lm3632_bl_en_control(int enable);
extern void lm3632_dsv_output_ctrl(int enable);
bool first_power_on = false;
#endif

#if defined(CONFIG_TOVIS_PH1SYNAP_INCELL_VIDEO_HD_PANEL)
extern int get_display_id(void);
#endif

#if defined(CONFIG_TOUCHSCREEN_UNIFIED_DRIVER_3)
#include <linux/input/unified_driver_3/lgtp_common_notify.h>
#endif

#define XO_CLK_RATE	19200000
#if defined(CONFIG_LGD_INCELL_VIDEO_WVGA_PT_PANEL)
int has_dsv_f;
/* For INCELL Knock on, When the device sleep out, DSV GPIO MUST be controled in LOW state */
/* BUT, when the device is first booting, we DON'T control DSV because of continuous_splash_enable */
/* is_first_dsv_control FLAG is for SKIPPING the DSV Control when the device first booting */
/* is_available_dsv_control FLAG is for BLOCKING the DSV GPIO Control except Display  */
int is_first_dsv_control = 1;
bool is_available_dsv_control = 0;
#endif

#ifdef CONFIG_LGE_READER_MODE
extern struct mdss_panel_data *pdata_base;
#endif

# if defined(CONFIG_LGD_INCELL_VIDEO_FWVGA_PT_PANEL)
int has_dsv_f;
extern int lm3632_dsv_ctrl(int dsv_en);
/* For INCELL Knock on, When the device sleep out, DSV GPIO MUST be controled in LOW state */
/* BUT, when the device is first booting, we DON'T control DSV because of continuous_splash_enable */
/* is_first_dsv_control FLAG is for SKIPPING the DSV Control when the device first booting */
/* is_available_dsv_control FLAG is for BLOCKING the DSV GPIO Control except Display  */
int is_first_dsv_control = 1;
bool is_available_dsv_control = 0;
int dual_panel;
#elif defined(CONFIG_LGD_INCELL_PHASE3_VIDEO_HD_PT_PANEL) || defined (CONFIG_LGD_DONGBU_INCELL_VIDEO_HD_PANEL) || defined (CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
int has_dsv_f;
extern void lm3632_dsv_fd_ctrl(void);
extern void mdss_lcd_do_lut_update(void);
#endif
#if defined(CONFIG_LGE_MODULE_DETECT)
extern int lge_dual_panel;
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
#include "lge/panel/oem_mdss_dsi_common.h"
struct lge_mdss_dsi_interface lge_mdss_dsi;
#endif

#if defined(CONFIG_LGE_DISPLAY_POWER_SEQUENCE)
int panel_power_flag;
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_AOD_SUPPORT)
extern int lge_lg4945_panel_mode_cmd_send(int mode, struct mdss_dsi_ctrl_pdata *ctrl);
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_TUNING)
static int mdss_dsi_clk_refresh(struct mdss_panel_data *pdata);
#endif
#if defined(CONFIG_JDI_INCELL_VIDEO_HD_PANEL) || defined(CONFIG_JDI_INCELL_VIDEO_FHD_PANEL)
int has_dsv_f;
int mdss_dsi_lane_config(struct mdss_panel_data *pdata, int enable)
{
	u32 tmp;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,panel_data);
	tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0xac);
	pr_info("%s+: dsi_lane_ctrl=0x%x\n", __func__, tmp);
	if (enable) {
		tmp |= DSI_LANE_CTRL_HS_MASK;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
		wmb();
	} else {
		tmp &= DSI_LANE_CTRL_LP_MASK;
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
		wmb();
	}
	pr_info("%s-: current mode=%s dsi_lane_ctrl=0x%x\n", __func__, (enable ? "hs" : "lp"), tmp);
	return 0;
}
#endif


static int mdss_dsi_pinctrl_set_state(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
					bool active);

static int mdss_dsi_regulator_init(struct platform_device *pdev)
{
	int rc = 0;

	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int i = 0;
	int j = 0;

	if (!pdev) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		pr_err("%s: invalid driver data\n", __func__);
		return -EINVAL;
	}

	for (i = 0; !rc && (i < DSI_MAX_PM); i++) {
#if defined(CONFIG_LCD_VDD_VDDIO_EXT_LDO) //EXT LDO is controlled by gpio.
		if (DSI_PANEL_PM == i)
			continue;
#endif
		rc = msm_dss_config_vreg(&pdev->dev,
			ctrl_pdata->power_data[i].vreg_config,
			ctrl_pdata->power_data[i].num_vreg, 1);
		if (rc) {
			pr_err("%s: failed to init vregs for %s\n",
				__func__, __mdss_dsi_pm_name(i));
			for (j = i-1; j >= 0; j--) {
				msm_dss_config_vreg(&pdev->dev,
				ctrl_pdata->power_data[j].vreg_config,
				ctrl_pdata->power_data[j].num_vreg, 0);
			}
		}
	}
	return rc;
}

static int mdss_dsi_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
#if !IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
	int i = 0;
#endif

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	ret = mdss_dsi_panel_reset(pdata, 0);
	if (ret) {
		pr_warn("%s: Panel reset failed. rc=%d\n", __func__, ret);
		ret = 0;
	}

	if (mdss_dsi_pinctrl_set_state(ctrl_pdata, false))
		pr_debug("reset disable: pinctrl not enabled\n");

	if (ctrl_pdata->panel_bias_vreg) {
		pr_debug("%s: Disabling panel bias vreg. ndx = %d\n",
		       __func__, ctrl_pdata->ndx);
		if (qpnp_ibb_enable(false))
			pr_err("Unable to disable bias vreg\n");
		/* Add delay recommended by panel specs */
		udelay(2000);
	}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
	if (lge_mdss_dsi.lge_msm_dss_enable_vreg)
		ret = lge_mdss_dsi.lge_msm_dss_enable_vreg(ctrl_pdata, 0);
#else
#if defined(CONFIG_LGE_MIPI_DSI_LGD_NT35521_E7II_WXGA)
	e7ii_panel_power(pdata, 0);
#endif

#if defined(CONFIG_MFD_DW8768)
	if (pdata->panel_info.shutdown_pending) {
//JDI HD panel needs min 0ms delay between vneg and vpos.
// So we don't need this power off sequence thru i2c.
// Actually when ENA/ENM is set low, 1.5 ms delay is added between vneg and vpos.
#if !defined (CONFIG_JDI_INCELL_VIDEO_HD_PANEL)
		dw8768_off_seq();
#endif
		DSV_DW8768_CTRL(pdata, 0);
	}
#endif

#if defined(CONFIG_LCD_VDD_EXT_LDO)
	if (pdata->panel_info.shutdown_pending)
		EXT_VDD_LDO_CTRL(pdata, 0);
#endif

#if defined(CONFIG_LCD_VDD_VDDIO_EXT_LDO)
	if (pdata->panel_info.shutdown_pending)
		EXT_VDD_VDDIO_LDO_CTRL(pdata, 0);
#endif


	for (i = DSI_MAX_PM - 1; i >= 0; i--) {
#if defined (CONFIG_LGD_INCELL_PHASE3_VIDEO_HD_PT_PANEL) || defined (CONFIG_LGD_DONGBU_INCELL_VIDEO_HD_PANEL) || defined(CONFIG_LCD_VDD_VDDIO_EXT_LDO) || defined (CONFIG_LGD_M2DONGBU_INCELL_VIDEO_HD_PANEL) || defined (CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
		if (DSI_PANEL_PM == i)
			continue;
#endif
		/*
		 * Core power module will be disabled when the
		 * clocks are disabled
		 */
		if (DSI_CORE_PM == i)
			continue;
#ifdef CONFIG_LGE_DISPLAY_POWER_SEQUENCE
		panel_power_flag = 1;
#endif
		ret = msm_dss_enable_vreg(
			ctrl_pdata->power_data[i].vreg_config,
			ctrl_pdata->power_data[i].num_vreg, 0);
#ifdef CONFIG_LGE_DISPLAY_POWER_SEQUENCE
		panel_power_flag = 0;
#endif
		if (ret)
			pr_err("%s: failed to disable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(i));
	}

#if defined(CONFIG_LGD_LD083_VIDEO_WUXGA_PT_PANEL)
	mdelay(150);
	pr_info("%s:(LCD_LDO) Disable", __func__);
	if (gpio_is_valid(ctrl_pdata->disp_lcd_ldo_gpio)) {
		gpio_direction_output((ctrl_pdata->disp_lcd_ldo_gpio), 0);
		//gpio_set_value((ctrl_pdata->disp_lcd_ldo_gpio), 0);
		gpio_free(ctrl_pdata->disp_lcd_ldo_gpio);
		pr_info("%s:(LCD_LDO) release GPIO ", __func__);
	}
	else
	{
		pr_info("%s: GPIO is not valid!!! (LCD_LDO) \n", __func__);
	}
#endif
#endif // CONFIG_LGE_DISPLAY_CODE_REFACTORING

end:
	return ret;
}

static int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int i = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
	if (lge_mdss_dsi.lge_msm_dss_enable_vreg)
		ret = lge_mdss_dsi.lge_msm_dss_enable_vreg(ctrl_pdata, 1);
	i = DSI_MAX_PM;
	if(ret){
		goto error;
	}
#else
#if defined(CONFIG_LGD_INCELL_VIDEO_WVGA_PT_PANEL)
	/* For INCELL Knock on, When the device sleep out, DSV GPIO MUST be controled in LOW state */
	/* BUT, when the device is first booting, we DON'T control DSV because of continuous_splash_enable */
	/* is_first_dsv_control FLAG is for SKIPPING the DSV Control when the device first booting */
	if (is_first_dsv_control == 1){
		is_first_dsv_control = 0;
	}
	else{
		/* is_available_dsv_control FLAG is for BLOCKING the DSV GPIO Control except Display  */
		/* After LCD On, DSV control is NOT available like "is_available_dsv_control = 0"  */
		is_available_dsv_control = 0;
		pr_err("%s : dsv_control is not allowed after this time. is_available_dsv_control = [%d]\n", __func__, is_available_dsv_control);
		gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
		msleep(5);
	}
#endif
#if defined(CONFIG_LGD_INCELL_VIDEO_FWVGA_PT_PANEL)
	/* For INCELL Knock on, When the device sleep out, DSV GPIO MUST be controled in LOW state */
	/* BUT, when the device is first booting, we DON'T control DSV because of continuous_splash_enable */
	/* is_first_dsv_control FLAG is for SKIPPING the DSV Control when the device first booting */
	if (is_first_dsv_control == 1){
		is_first_dsv_control = 0;
	} else {
		/* is_available_dsv_control FLAG is for BLOCKING the DSV GPIO Control except Display  */
		/* After LCD On, DSV control is NOT available like "is_available_dsv_control = 0"  */
		is_available_dsv_control = 0;
		pr_err("%s : dsv_control is not allowed after this time. is_available_dsv_control = [%d]\n", __func__, is_available_dsv_control);
		ret = lm3632_dsv_ctrl(0);
		if (ret)
			pr_err("%s: %d dsv gpio is not valid !!\n", __func__, ret);
		msleep(5);
	}
#endif
	for (i = 0; i < DSI_MAX_PM; i++) {
#if defined (CONFIG_LGD_INCELL_PHASE3_VIDEO_HD_PT_PANEL) || defined (CONFIG_LGD_DONGBU_INCELL_VIDEO_HD_PANEL) || defined (CONFIG_LGD_M2DONGBU_INCELL_VIDEO_HD_PANEL) || \
	defined (CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
		if (!pdata->panel_info.cont_splash_enabled && (DSI_PANEL_PM == i)){
#if 0//defined (LGD_INCELL_PHASE3_APPLY_POWER_SEQUENCE)
		if (gpio_is_valid(ctrl_pdata->rst_gpio)) {
		    gpio_set_value((ctrl_pdata->rst_gpio), 0);
		}
		msm_dss_enable_vreg(
		    ctrl_pdata->power_data[DSI_PANEL_PM].vreg_config,
		    ctrl_pdata->power_data[DSI_PANEL_PM].num_vreg, 0);
		if (gpio_is_valid(ctrl_pdata->disp_lcd_ldo_3v0_gpio)) {
		    gpio_set_value((ctrl_pdata->disp_lcd_ldo_3v0_gpio), 0);
		}
		if (gpio_is_valid(ctrl_pdata->disp_lcd_ldo_1v8_gpio)) {
		    gpio_set_value((ctrl_pdata->disp_lcd_ldo_1v8_gpio), 0);
		}

		if (gpio_is_valid(ctrl_pdata->disp_dsv_en_gpio)) {
		    gpio_set_value((ctrl_pdata->disp_dsv_en_gpio), 0);
		}
		//lm3632_dsv_fd_ctrl();
#endif //LGD_INCELL_PHASE3_APPLY_POWER_SEQUENCE
		continue;
	}
#endif

#if defined(CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
	if (DSI_PANEL_PM == i) {
		if (first_power_on == true) {
			continue;
		} else {
			first_power_on = true;
		}
	}
#endif

	/*
	* Core power module will be enabled when the
	* clocks are enabled
	*/
	if (DSI_CORE_PM == i)
		continue;
#if defined(CONFIG_LCD_VDD_VDDIO_EXT_LDO) //EXT LDO is controlled by gpio.
	if (DSI_PANEL_PM == i)
		continue;
#endif
#ifdef CONFIG_LGE_DISPLAY_POWER_SEQUENCE
	panel_power_flag = 1;
#endif
	ret = msm_dss_enable_vreg(
		ctrl_pdata->power_data[i].vreg_config,
		ctrl_pdata->power_data[i].num_vreg, 1);
#ifdef CONFIG_LGE_DISPLAY_POWER_SEQUENCE
	panel_power_flag = 0;
#endif
	if (ret) {
		pr_err("%s: failed to enable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(i));
		goto error;
	}
	}
#endif // CONFIG_LGE_DISPLAY_CODE_REFACTORING
	if (ctrl_pdata->panel_bias_vreg) {
		pr_debug("%s: Enable panel bias vreg. ndx = %d\n",
		       __func__, ctrl_pdata->ndx);
		if (qpnp_ibb_enable(true))
			pr_err("Unable to configure bias vreg\n");
		/* Add delay recommended by panel specs */
		udelay(2000);
	}

	i--;


	/*
	 * If continuous splash screen feature is enabled, then we need to
	 * request all the GPIOs that have already been configured in the
	 * bootloader. This needs to be done irresepective of whether
	 * the lp11_init flag is set or not.
	 */
	if (pdata->panel_info.cont_splash_enabled ||
		!pdata->panel_info.mipi.lp11_init) {
		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");

		ret = mdss_dsi_panel_reset(pdata, 1);
		if (ret)
			pr_err("%s: Panel reset failed. rc=%d\n",
					__func__, ret);
	}

#if defined(CONFIG_LGD_LD083_VIDEO_WUXGA_PT_PANEL)
	//Requeset GPIO (it is free in the off)
	pr_info("%s:(LCD_LDO) Request LCD_LDO_GPIO \n", __func__);
	if (gpio_is_valid(ctrl_pdata->disp_lcd_ldo_gpio)) {
		ret = gpio_request(ctrl_pdata->disp_lcd_ldo_gpio, "disp_lcd_ldo_gpio");
		if (ret) {
			pr_err("request lcd ldo gpio failed, rc=%d\n", ret);
		}
	}

	//Requeset Backlight GPIO (it is free in the off)
	//It is already requested in probe function.(lm3697_bl.c)
	pr_info("%s:(LCD_LDO) Enable \n", __func__);
	if (gpio_is_valid(ctrl_pdata->disp_lcd_ldo_gpio)) {
		//gpio_set_value((ctrl_pdata->disp_lcd_ldo_gpio), 0);
		gpio_direction_output((ctrl_pdata->disp_lcd_ldo_gpio), 1);
	}
	else
	{
		pr_info("%s: GPIO is not valid!!! (LCD_LDO) \n", __func__);
	}
	mdelay(140);
#elif defined(CONFIG_LGD_INCELL_VIDEO_WVGA_PT_PANEL)
		gpio_direction_output((ctrl_pdata->disp_fd_gpio), 1);
		gpio_set_value((ctrl_pdata->disp_fd_gpio), 1);
		pr_info("%s: LCD IOVCC Enable", __func__);
		gpio_direction_output((ctrl_pdata->disp_iovcc_gpio), 1);
		gpio_set_value((ctrl_pdata->disp_iovcc_gpio), 1);
#endif
#if defined(CONFIG_LGE_MIPI_DSI_BYD_ILI9806E_WVGA)
		pr_info("%s: LCD IOVCC Enable", __func__);
		gpio_direction_output((ctrl_pdata->disp_iovcc_gpio), 1);
		gpio_set_value((ctrl_pdata->disp_iovcc_gpio), 1);
#endif

error:
	if (ret) {
		for (i = DSI_PANEL_MAX-1; i >= 0; i--){
#if defined(CONFIG_LCD_VDD_VDDIO_EXT_LDO) //EXT LDO is controlled by gpio.
			if (DSI_PANEL_PM == i)
				continue;
#endif
			msm_dss_enable_vreg(
				ctrl_pdata->power_data[i].vreg_config,
				ctrl_pdata->power_data[i].num_vreg, 0);
		}
	}
	return ret;
}

static int mdss_dsi_panel_power_lp(struct mdss_panel_data *pdata, int enable)
{
	/* Panel power control when entering/exiting lp mode */
	return 0;
}

static int mdss_dsi_panel_power_ctrl(struct mdss_panel_data *pdata,
	int power_state)
{
	int ret;
	struct mdss_panel_info *pinfo;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	pr_info("%s: cur_power_state=%d req_power_state=%d\n", __func__,
		pinfo->panel_power_state, power_state);

	if (pinfo->panel_power_state == power_state) {
		pr_debug("%s: no change needed\n", __func__);
		return 0;
	}
	/*
	 * If a dynamic mode switch is pending, the regulators should not
	 * be turned off or on.
	 */
	if (pdata->panel_info.dynamic_switch_pending)
		return 0;

	switch (power_state) {
	case MDSS_PANEL_POWER_OFF:
		ret = mdss_dsi_panel_power_off(pdata);
		break;
	case MDSS_PANEL_POWER_ON:
		if (mdss_dsi_is_panel_on_lp(pdata))
			ret = mdss_dsi_panel_power_lp(pdata, false);
		else
			ret = mdss_dsi_panel_power_on(pdata);
		break;
	case MDSS_PANEL_POWER_LP1:
	case MDSS_PANEL_POWER_LP2:
		ret = mdss_dsi_panel_power_lp(pdata, true);
		break;
	default:
		pr_err("%s: unknown panel power state requested (%d)\n",
			__func__, power_state);
		ret = -EINVAL;
	}

	if (!ret)
		pinfo->panel_power_state = power_state;

	return ret;
}

static void mdss_dsi_put_dt_vreg_data(struct device *dev,
	struct dss_module_power *module_power)
{
	if (!module_power) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	if (module_power->vreg_config) {
		devm_kfree(dev, module_power->vreg_config);
		module_power->vreg_config = NULL;
	}
	module_power->num_vreg = 0;
}

static int mdss_dsi_get_dt_vreg_data(struct device *dev,
	struct dss_module_power *mp, enum dsi_pm_type module)
{
	int i = 0, rc = 0;
	u32 tmp = 0;
	struct device_node *of_node = NULL, *supply_node = NULL;
	const char *pm_supply_name = NULL;
	struct device_node *supply_root_node = NULL;

	if (!dev || !mp) {
		pr_err("%s: invalid input\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	of_node = dev->of_node;

	mp->num_vreg = 0;
	pm_supply_name = __mdss_dsi_pm_supply_node_name(module);
	supply_root_node = of_get_child_by_name(of_node, pm_supply_name);
	if (!supply_root_node) {
		pr_err("no supply entry present\n");
		goto novreg;
	}

	for_each_child_of_node(supply_root_node, supply_node) {
		mp->num_vreg++;
	}

	if (mp->num_vreg == 0) {
		pr_debug("%s: no vreg\n", __func__);
		goto novreg;
	} else {
		pr_debug("%s: vreg found. count=%d\n", __func__, mp->num_vreg);
	}

	mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
		mp->num_vreg, GFP_KERNEL);
	if (!mp->vreg_config) {
		pr_err("%s: can't alloc vreg mem\n", __func__);
		rc = -ENOMEM;
		goto error;
	}

	for_each_child_of_node(supply_root_node, supply_node) {
		const char *st = NULL;
		/* vreg-name */
		rc = of_property_read_string(supply_node,
			"qcom,supply-name", &st);
		if (rc) {
			pr_err("%s: error reading name. rc=%d\n",
				__func__, rc);
			goto error;
		}
		snprintf(mp->vreg_config[i].vreg_name,
			ARRAY_SIZE((mp->vreg_config[i].vreg_name)), "%s", st);
		/* vreg-min-voltage */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-min-voltage", &tmp);
		if (rc) {
			pr_err("%s: error reading min volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].min_voltage = tmp;

		/* vreg-max-voltage */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-max-voltage", &tmp);
		if (rc) {
			pr_err("%s: error reading max volt. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].max_voltage = tmp;

		/* enable-load */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-enable-load", &tmp);
		if (rc) {
			pr_err("%s: error reading enable load. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].enable_load = tmp;

		/* disable-load */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-disable-load", &tmp);
		if (rc) {
			pr_err("%s: error reading disable load. rc=%d\n",
				__func__, rc);
			goto error;
		}
		mp->vreg_config[i].disable_load = tmp;

		/* pre-sleep */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-pre-on-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply pre sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].pre_on_sleep = tmp;
		}

		rc = of_property_read_u32(supply_node,
			"qcom,supply-pre-off-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply pre sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].pre_off_sleep = tmp;
		}

		/* post-sleep */
		rc = of_property_read_u32(supply_node,
			"qcom,supply-post-on-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply post sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].post_on_sleep = tmp;
		}

		rc = of_property_read_u32(supply_node,
			"qcom,supply-post-off-sleep", &tmp);
		if (rc) {
			pr_debug("%s: error reading supply post sleep value. rc=%d\n",
				__func__, rc);
			rc = 0;
		} else {
			mp->vreg_config[i].post_off_sleep = tmp;
		}

		pr_debug("%s: %s min=%d, max=%d, enable=%d, disable=%d, preonsleep=%d, postonsleep=%d, preoffsleep=%d, postoffsleep=%d\n",
			__func__,
			mp->vreg_config[i].vreg_name,
			mp->vreg_config[i].min_voltage,
			mp->vreg_config[i].max_voltage,
			mp->vreg_config[i].enable_load,
			mp->vreg_config[i].disable_load,
			mp->vreg_config[i].pre_on_sleep,
			mp->vreg_config[i].post_on_sleep,
			mp->vreg_config[i].pre_off_sleep,
			mp->vreg_config[i].post_off_sleep
			);
		++i;
	}

	return rc;

error:
	if (mp->vreg_config) {
		devm_kfree(dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
novreg:
	mp->num_vreg = 0;

	return rc;
}

static int mdss_dsi_get_panel_cfg(char *panel_cfg,
				struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc;
	struct mdss_panel_cfg *pan_cfg = NULL;

	if (!panel_cfg)
		return MDSS_PANEL_INTF_INVALID;

	pan_cfg = ctrl->mdss_util->panel_intf_type(MDSS_PANEL_INTF_DSI);
	if (IS_ERR(pan_cfg)) {
		return PTR_ERR(pan_cfg);
	} else if (!pan_cfg) {
		panel_cfg[0] = 0;
		return 0;
	}

	pr_debug("%s:%d: cfg:[%s]\n", __func__, __LINE__,
		 pan_cfg->arg_cfg);
	ctrl->panel_data.panel_info.is_prim_panel = true;
	rc = strlcpy(panel_cfg, pan_cfg->arg_cfg,
		     sizeof(pan_cfg->arg_cfg));
	return rc;
}

static int mdss_dsi_off(struct mdss_panel_data *pdata, int power_state)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *panel_info = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	panel_info = &ctrl_pdata->panel_data.panel_info;

	pr_debug("%s+: ctrl=%pK ndx=%d power_state=%d\n",
		__func__, ctrl_pdata, ctrl_pdata->ndx, power_state);

	if (power_state == panel_info->panel_power_state) {
		pr_debug("%s: No change in power state %d -> %d\n", __func__,
			panel_info->panel_power_state, power_state);
		goto end;
	}

	if (mdss_panel_is_power_on(power_state)) {
		pr_debug("%s: dsi_off with panel always on\n", __func__);
		goto panel_power_ctrl;
	}

	if (pdata->panel_info.type == MIPI_CMD_PANEL)
		mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);

	if (!pdata->panel_info.ulps_suspend_enabled) {
		/* disable DSI controller */
		mdss_dsi_controller_cfg(0, pdata);

		/* disable DSI phy */
		mdss_dsi_phy_disable(ctrl_pdata);
	}

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);

panel_power_ctrl:
	ret = mdss_dsi_panel_power_ctrl(pdata, power_state);
	if (ret) {
		pr_err("%s: Panel power off failed\n", __func__);
		goto end;
	}

	if (panel_info->dynamic_fps
	    && (panel_info->dfps_update == DFPS_SUSPEND_RESUME_MODE)
	    && (panel_info->new_fps != panel_info->mipi.frame_rate))
		panel_info->mipi.frame_rate = panel_info->new_fps;

end:
	pr_info("%s:-\n", __func__);

	return ret;
}

static int mdss_dsi_update_panel_config(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
				int mode)
{
	int ret = 0;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (mode == DSI_CMD_MODE) {
		pinfo->mipi.mode = DSI_CMD_MODE;
		pinfo->type = MIPI_CMD_PANEL;
		pinfo->mipi.vsync_enable = 1;
		pinfo->mipi.hw_vsync_mode = 1;
	} else {	/*video mode*/
		pinfo->mipi.mode = DSI_VIDEO_MODE;
		pinfo->type = MIPI_VIDEO_PANEL;
		pinfo->mipi.vsync_enable = 0;
		pinfo->mipi.hw_vsync_mode = 0;
	}

	ctrl_pdata->panel_mode = pinfo->mipi.mode;
	mdss_panel_get_dst_fmt(pinfo->bpp, pinfo->mipi.mode,
			pinfo->mipi.pixel_packing, &(pinfo->mipi.dst_format));
	pinfo->cont_splash_enabled = 0;

	return ret;
}

int mdss_dsi_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int cur_power_state;

#if defined(CONFIG_LGE_MIPI_DSI_LGD_NT35521_E7II_WXGA)
	u32 tmp;
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_TUNING)
	int refresh_clk_rate = 0;
#endif

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	cur_power_state = pdata->panel_info.panel_power_state;
	pr_debug("%s+: ctrl=%pK ndx=%d cur_power_state=%d\n", __func__,
		ctrl_pdata, ctrl_pdata->ndx, cur_power_state);

	pinfo = &pdata->panel_info;
	mipi = &pdata->panel_info.mipi;

	if (mdss_dsi_is_panel_on_interactive(pdata)) {
		pr_debug("%s: panel already on\n", __func__);
		goto end;
	}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
	if (lge_mdss_dsi.pre_mdss_dsi_panel_power_ctrl)
		ret = lge_mdss_dsi.pre_mdss_dsi_panel_power_ctrl(pdata, 1);
#endif // CONFIG_LGE_DISPLAY_CODE_REFACTORING

	ret = mdss_dsi_panel_power_ctrl(pdata, MDSS_PANEL_POWER_ON);
	if (ret) {
		pr_err("%s:Panel power on failed. rc=%d\n", __func__, ret);
		return ret;
	}

	if (cur_power_state != MDSS_PANEL_POWER_OFF) {
		pr_debug("%s: dsi_on from panel low power state\n", __func__);
		goto end;
	}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_TUNING)
	if (ctrl_pdata->refresh_clk_rate) {
		mdss_dsi_clk_refresh(pdata);
		refresh_clk_rate = 1;
	}
#endif
	/*
	 * Enable DSI bus clocks prior to resetting and initializing DSI
	 * Phy. Phy and ctrl setup need to be done before enabling the link
	 * clocks.
	 */
	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_BUS_CLKS, 1);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_TUNING)
	if (!pdata->panel_info.ulps_suspend_enabled || refresh_clk_rate) {
#else
	if (!pdata->panel_info.ulps_suspend_enabled) {
#endif

	/*
	 * If ULPS during suspend feature is enabled, then DSI PHY was
	 * left on during suspend. In this case, we do not need to reset/init
	 * PHY. This would have already been done when the BUS clocks are
	 * turned on. However, if cont splash is disabled, the first time DSI
	 * is powered on, phy init needs to be done unconditionally.
	 */
	    if (!pdata->panel_info.ulps_suspend_enabled || !ctrl_pdata->ulps) {
		    mdss_dsi_phy_sw_reset(ctrl_pdata);
		    mdss_dsi_phy_init(ctrl_pdata);
		    mdss_dsi_ctrl_setup(ctrl_pdata);
	    }
	}

	/* DSI link clocks need to be on prior to ctrl sw reset */
	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_LINK_CLKS, 1);
	mdss_dsi_sw_reset(ctrl_pdata, true);

#if defined(CONFIG_JDI_INCELL_VIDEO_HD_PANEL) || defined(CONFIG_JDI_INCELL_VIDEO_FHD_PANEL)
	/* set mipi lane to LP11 as LCD power on spec. */
	mdss_dsi_lane_config(pdata, 0);
#endif

#if defined(CONFIG_LGE_MIPI_DSI_LGD_NT35521_E7II_WXGA)
	e7ii_panel_power(pdata, 1);
	mipi->force_clk_lane_hs = 1;
	mdelay(5);
	tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0xac);
	tmp &= ~(1<<28);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
	wmb();
#endif
	/*
	 * Issue hardware reset line after enabling the DSI clocks and data
	 * data lanes for LP11 init
	 */
#if defined (CONFIG_LGD_INCELL_PHASE3_VIDEO_HD_PT_PANEL) || defined (CONFIG_LGD_DONGBU_INCELL_VIDEO_HD_PANEL) || defined (CONFIG_LGD_M2DONGBU_INCELL_VIDEO_HD_PANEL) || defined (CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
//	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, 0x0);
	if (mipi->lp11_init) {
		u32 tmp;
		tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0xac);
		tmp &= ~(1<<28);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
		wmb();
	}
#elif defined(CONFIG_JDI_INCELL_VIDEO_HD_PANEL) && defined(CONFIG_LGD_INCELL_DB7400_VIDEO_HD_DUAL_PANEL)
	if (lge_dual_panel == SECONDARY_MODULE) {
		if (mipi->lp11_init) {
			u32 tmp;
			tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0xac);
			tmp &= ~(1<<28);
			MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
			wmb();
		}	
	}
#endif
	if (mipi->lp11_init) {
		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");
		mdss_dsi_panel_reset(pdata, 1);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
		if (lge_mdss_dsi.post_mdss_dsi_panel_power_ctrl)
			ret = lge_mdss_dsi.post_mdss_dsi_panel_power_ctrl(pdata, 1);
#endif // CONFIG_LGE_DISPLAY_CODE_REFACTORING
	}

	if (mipi->init_delay)
		usleep(mipi->init_delay);

#if defined(CONFIG_LGE_MIPI_DSI_LGD_NT35521_E7II_WXGA)
	if (mipi->force_clk_lane_hs) {
		u32 tmp;

		tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0x2c);
		tmp |= (1<<28);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2c, tmp);
		wmb();
	}
#else
	if (mipi->force_clk_lane_hs) {
		u32 tmp;

		tmp = MIPI_INP((ctrl_pdata->ctrl_base) + 0xac);
		tmp |= (1<<28);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0xac, tmp);
		wmb();
	}
#endif

	if (pdata->panel_info.type == MIPI_CMD_PANEL)
		mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);

end:
	pr_info("%s: -\n", __func__);
	return 0;
}

static int mdss_dsi_pinctrl_set_state(
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	bool active)
{
	struct pinctrl_state *pin_state;
	int rc = -EFAULT;

	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.pinctrl))
		return PTR_ERR(ctrl_pdata->pin_res.pinctrl);

	pin_state = active ? ctrl_pdata->pin_res.gpio_state_active
				: ctrl_pdata->pin_res.gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pin_state)) {
		rc = pinctrl_select_state(ctrl_pdata->pin_res.pinctrl,
				pin_state);
		if (rc)
			pr_err("%s: can not set %s pins\n", __func__,
			       active ? MDSS_PINCTRL_STATE_DEFAULT
			       : MDSS_PINCTRL_STATE_SLEEP);
	} else {
		pr_err("%s: invalid '%s' pinstate\n", __func__,
		       active ? MDSS_PINCTRL_STATE_DEFAULT
		       : MDSS_PINCTRL_STATE_SLEEP);
	}
	return rc;
}

static int mdss_dsi_pinctrl_init(struct platform_device *pdev)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;

	ctrl_pdata = platform_get_drvdata(pdev);
	ctrl_pdata->pin_res.pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.pinctrl)) {
		pr_err("%s: failed to get pinctrl\n", __func__);
		return PTR_ERR(ctrl_pdata->pin_res.pinctrl);
	}

	ctrl_pdata->pin_res.gpio_state_active
		= pinctrl_lookup_state(ctrl_pdata->pin_res.pinctrl,
				MDSS_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.gpio_state_active))
		pr_warn("%s: can not get default pinstate\n", __func__);

	ctrl_pdata->pin_res.gpio_state_suspend
		= pinctrl_lookup_state(ctrl_pdata->pin_res.pinctrl,
				MDSS_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(ctrl_pdata->pin_res.gpio_state_suspend))
		pr_warn("%s: can not get sleep pinstate\n", __func__);

	return 0;
}

static int mdss_dsi_unblank(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s+: ctrl=%pK ndx=%d cur_blank_state=%d\n", __func__,
		ctrl_pdata, ctrl_pdata->ndx, pdata->panel_info.blank_state);

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);

	if (pdata->panel_info.blank_state == MDSS_PANEL_BLANK_LOW_POWER) {
		pr_debug("%s: dsi_unblank with panel always on\n", __func__);
		if (ctrl_pdata->low_power_config)
			ret = ctrl_pdata->low_power_config(pdata, false);
		goto error;
	}

	if (!(ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT)) {
		if (!pdata->panel_info.dynamic_switch_pending) {
			ret = ctrl_pdata->on(pdata);
			if (ret) {
				pr_err("%s: unable to initialize the panel\n",
							__func__);
				goto error;
			}
		}
		ctrl_pdata->ctrl_state |= CTRL_STATE_PANEL_INIT;
	}

	if ((pdata->panel_info.type == MIPI_CMD_PANEL) &&
		mipi->vsync_enable && mipi->hw_vsync_mode) {

		mdss_dsi_set_tear_on(ctrl_pdata);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_AOD_SUPPORT)
		if (pdata->panel_info.lge_pan_info.lge_panel_send_on_cmd == false){
			pr_info("[PowerMode] %s: switch u2 to u3 ready \n", __func__);
			lge_lg4945_panel_mode_cmd_send(LGE_PANEL_CMD_U3_READY, ctrl_pdata);
		}
#endif
		if (mdss_dsi_is_te_based_esd(ctrl_pdata))
			enable_irq(gpio_to_irq(ctrl_pdata->disp_te_gpio));
	}

error:
	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);
	pr_info("%s-:\n", __func__);

	return ret;
}

static int mdss_dsi_blank(struct mdss_panel_data *pdata, int power_state)
{
	int ret = 0;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi = &pdata->panel_info.mipi;

	pr_debug("%s+: ctrl=%pK ndx=%d power_state=%d\n",
		__func__, ctrl_pdata, ctrl_pdata->ndx, power_state);

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);

	if (mdss_panel_is_power_on_lp(power_state)) {
		pr_debug("%s: low power state requested\n", __func__);
		if (ctrl_pdata->low_power_config)
			ret = ctrl_pdata->low_power_config(pdata, true);
		goto error;
	}

	if (pdata->panel_info.type == MIPI_VIDEO_PANEL &&
			ctrl_pdata->off_cmds.link_state == DSI_LP_MODE) {
		mdss_dsi_sw_reset(ctrl_pdata, false);
		mdss_dsi_host_init(pdata);
	}

	mdss_dsi_op_mode_config(DSI_CMD_MODE, pdata);

	if (pdata->panel_info.dynamic_switch_pending) {
		pr_info("%s: switching to %s mode\n", __func__,
			(pdata->panel_info.mipi.mode ? "video" : "command"));
		if (pdata->panel_info.type == MIPI_CMD_PANEL) {
			ctrl_pdata->switch_mode(pdata, DSI_VIDEO_MODE);
		} else if (pdata->panel_info.type == MIPI_VIDEO_PANEL) {
			ctrl_pdata->switch_mode(pdata, DSI_CMD_MODE);
			mdss_dsi_set_tear_off(ctrl_pdata);
		}
	}

	if ((pdata->panel_info.type == MIPI_CMD_PANEL) &&
		mipi->vsync_enable && mipi->hw_vsync_mode) {
		if (mdss_dsi_is_te_based_esd(ctrl_pdata)) {
				disable_irq(gpio_to_irq(
					ctrl_pdata->disp_te_gpio));
				atomic_dec(&ctrl_pdata->te_irq_ready);
		}
		mdss_dsi_set_tear_off(ctrl_pdata);
	}

	if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
		if (!pdata->panel_info.dynamic_switch_pending) {
			ret = ctrl_pdata->off(pdata);
			if (ret) {
				pr_err("%s: Panel OFF failed\n", __func__);
				goto error;
			}
		}
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
	}
#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
	if (lge_mdss_dsi.post_mdss_dsi_blank)
		ret = lge_mdss_dsi.post_mdss_dsi_blank(pdata, power_state);
#endif
error:
	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);
#if defined (CONFIG_LGD_DONGBU_INCELL_VIDEO_HD_PANEL) || defined (CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
	mdelay(20);
#elif defined (CONFIG_LGD_INCELL_PHASE3_VIDEO_HD_PT_PANEL)
#if defined (CONFIG_LGD_INCELL_DB7400_VIDEO_HD_DUAL_PANEL)
	if(lge_dual_panel == SECONDARY_MODULE) {
		mdelay(20);
	}
#endif
#elif defined(CONFIG_JDI_INCELL_VIDEO_HD_PANEL) && defined(CONFIG_LGD_INCELL_DB7400_VIDEO_HD_DUAL_PANEL)
	if(lge_dual_panel == SECONDARY_MODULE) {
		mdelay(20);
	}
#endif
	pr_debug("%s-:End\n", __func__);
	return ret;
}

static int mdss_dsi_post_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s+: ctrl=%pK ndx=%d\n", __func__,
				ctrl_pdata, ctrl_pdata->ndx);

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);

	if (ctrl_pdata->post_panel_on)
		ctrl_pdata->post_panel_on(pdata);

	mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 0);
	pr_debug("%s-:\n", __func__);

	return 0;
}

int mdss_dsi_cont_splash_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	pr_info("%s:%d DSI on for continuous splash.\n", __func__, __LINE__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	mipi = &pdata->panel_info.mipi;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s+: ctrl=%pK ndx=%d\n", __func__,
				ctrl_pdata, ctrl_pdata->ndx);

	WARN((ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT),
		"Incorrect Ctrl state=0x%x\n", ctrl_pdata->ctrl_state);

	mdss_dsi_ctrl_setup(ctrl_pdata);
	mdss_dsi_sw_reset(ctrl_pdata, true);
	pr_debug("%s-:End\n", __func__);
	return ret;
}

static void __mdss_dsi_update_video_mode_total(struct mdss_panel_data *pdata,
		int new_fps)
{
	u32 hsync_period, vsync_period, ctrl_rev;
	u32 new_dsi_v_total, current_dsi_v_total;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s Invalid pdata\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s Invalid ctrl_pdata\n", __func__);
		return;
	}

	vsync_period =
		mdss_panel_get_vtotal(&pdata->panel_info);
	hsync_period =
		mdss_panel_get_htotal(&pdata->panel_info, true);
	current_dsi_v_total =
		MIPI_INP((ctrl_pdata->ctrl_base) + 0x2C);
	new_dsi_v_total =
		((vsync_period - 1) << 16) | (hsync_period - 1);

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
			(current_dsi_v_total | 0x8000000));
	if (new_dsi_v_total & 0x8000000) {
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
				new_dsi_v_total);
	} else {
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
				(new_dsi_v_total | 0x8000000));
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x2C,
				(new_dsi_v_total & 0x7ffffff));
	}
	ctrl_rev = MIPI_INP(ctrl_pdata->ctrl_base);
	/* Flush DSI TIMING registers for 8916/8939 */
	if (ctrl_rev == MDSS_DSI_HW_REV_103_1)
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x1e4, 0x1);
	ctrl_pdata->panel_data.panel_info.mipi.frame_rate = new_fps;

}

static void __mdss_dsi_dyn_refresh_config(
		struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int reg_data;

	reg_data = MIPI_INP((ctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_CTRL);
	reg_data &= ~BIT(12);

	pr_debug("Dynamic fps ctrl = 0x%x\n", reg_data);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_CTRL, reg_data);
}

static void __mdss_dsi_calc_dfps_delay(struct mdss_panel_data *pdata)
{
	u32 esc_clk_rate = XO_CLK_RATE;
	u32 pipe_delay, pipe_delay2 = 0, pll_delay;
	u32 hsync_period = 0;
	u32 pclk_to_esc_ratio, byte_to_esc_ratio, hr_bit_to_esc_ratio;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_dsi_phy_ctrl *pd = NULL;

	if (pdata == NULL) {
		pr_err("%s Invalid pdata\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	pinfo = &pdata->panel_info;
	pd = &(pinfo->mipi.dsi_phy_db);

	pclk_to_esc_ratio = (ctrl_pdata->pclk_rate / esc_clk_rate);
	byte_to_esc_ratio = (ctrl_pdata->byte_clk_rate / esc_clk_rate);
	hr_bit_to_esc_ratio = ((ctrl_pdata->byte_clk_rate * 4) / esc_clk_rate);

	hsync_period = mdss_panel_get_htotal(pinfo, true);
	pipe_delay = (hsync_period + 1) / pclk_to_esc_ratio;
	if (pinfo->mipi.eof_bllp_power_stop == 0)
		pipe_delay += (17 / pclk_to_esc_ratio) +
			((21 + pinfo->mipi.t_clk_pre +
			pinfo->mipi.t_clk_post) / byte_to_esc_ratio) +
			((((pd->timing[8] >> 1) + 1) +
			((pd->timing[6] >> 1) + 1) +
			((pd->timing[3] * 4) + (pd->timing[5] >> 1) + 1) +
			((pd->timing[7] >> 1) + 1) +
			((pd->timing[1] >> 1) + 1) +
			((pd->timing[4] >> 1) + 1)) / hr_bit_to_esc_ratio);

	if (pinfo->mipi.force_clk_lane_hs)
		pipe_delay2 = (6 / byte_to_esc_ratio) +
			((((pd->timing[1] >> 1) + 1) +
			((pd->timing[4] >> 1) + 1)) / hr_bit_to_esc_ratio);

	pll_delay = ((1000 * esc_clk_rate) / 1000000) * 2;

	MIPI_OUTP((ctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_PIPE_DELAY,
						pipe_delay);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_PIPE_DELAY2,
						pipe_delay2);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_PLL_DELAY,
						pll_delay);
}

static int __mdss_dsi_dfps_update_clks(struct mdss_panel_data *pdata,
		int new_fps)
{
	int rc = 0;
	u32 data;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s Invalid pdata\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s Invalid ctrl_pdata\n", __func__);
		return -EINVAL;
	}

	rc = mdss_dsi_clk_div_config
		(&ctrl_pdata->panel_data.panel_info, new_fps);
	if (rc) {
		pr_err("%s: unable to initialize the clk dividers\n",
				__func__);
		return rc;
	}

	if (pdata->panel_info.dfps_update
			== DFPS_IMMEDIATE_CLK_UPDATE_MODE) {
		__mdss_dsi_dyn_refresh_config(ctrl_pdata);
		__mdss_dsi_calc_dfps_delay(pdata);
		ctrl_pdata->pclk_rate =
			pdata->panel_info.mipi.dsi_pclk_rate;
		ctrl_pdata->byte_clk_rate =
			pdata->panel_info.clk_rate / 8;

		pr_debug("byte_rate=%i\n", ctrl_pdata->byte_clk_rate);
		pr_debug("pclk_rate=%i\n", ctrl_pdata->pclk_rate);

		if (mdss_dsi_is_ctrl_clk_slave(ctrl_pdata)) {
			pr_debug("%s DFPS already updated.\n", __func__);
			return rc;
		}

		/* add an extra reference to main clks */
		clk_prepare_enable(ctrl_pdata->pll_byte_clk);
		clk_prepare_enable(ctrl_pdata->pll_pixel_clk);

		/* change the parent to shadow clocks*/
		clk_set_parent(ctrl_pdata->mux_byte_clk,
				ctrl_pdata->shadow_byte_clk);
		clk_set_parent(ctrl_pdata->mux_pixel_clk,
				ctrl_pdata->shadow_pixel_clk);

		rc =  clk_set_rate(ctrl_pdata->byte_clk,
					ctrl_pdata->byte_clk_rate);
		if (rc) {
			pr_err("%s: dsi_byte_clk - clk_set_rate failed\n",
					__func__);
			return rc;
		}

		rc = clk_set_rate(ctrl_pdata->pixel_clk, ctrl_pdata->pclk_rate);
		if (rc) {
			pr_err("%s: dsi_pixel_clk - clk_set_rate failed\n",
				__func__);
			return rc;
		}

		mdss_dsi_en_wait4dynamic_done(ctrl_pdata);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + DSI_DYNAMIC_REFRESH_CTRL,
							0x00);

		data = MIPI_INP((ctrl_pdata->ctrl_base) + 0x0120);
		MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x120, data);
		pr_debug("pll unlock: 0x%x\n", data);
		clk_set_parent(ctrl_pdata->mux_byte_clk,
				ctrl_pdata->pll_byte_clk);
		clk_set_parent(ctrl_pdata->mux_pixel_clk,
				ctrl_pdata->pll_pixel_clk);
		clk_disable_unprepare(ctrl_pdata->pll_byte_clk);
		clk_disable_unprepare(ctrl_pdata->pll_pixel_clk);
		ctrl_pdata->panel_data.panel_info.mipi.frame_rate = new_fps;
	} else {
		ctrl_pdata->pclk_rate =
			pdata->panel_info.mipi.dsi_pclk_rate;
		ctrl_pdata->byte_clk_rate =
			pdata->panel_info.clk_rate / 8;
	}

	return rc;
}

static int mdss_dsi_dfps_config(struct mdss_panel_data *pdata, int new_fps)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_dsi_ctrl_pdata *sctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;

	pr_debug("%s+:\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	if (!ctrl_pdata->panel_data.panel_info.dynamic_fps) {
		pr_err("%s: Dynamic fps not enabled for this panel\n",
					__func__);
		return -EINVAL;
	}

	/*
	 * at split display case, DFPS registers were already programmed
	 * while programming the left ctrl(DSI0). Ignore right ctrl (DSI1)
	 * reguest.
	 */
	pinfo = &pdata->panel_info;
	if (pinfo->is_split_display) {
		if (mdss_dsi_is_right_ctrl(ctrl_pdata)) {
			pr_debug("%s DFPS already updated.\n", __func__);
			return rc;
		}
		/* left ctrl to get right ctrl */
		sctrl_pdata = mdss_dsi_get_other_ctrl(ctrl_pdata);
	}

	ctrl_pdata->dfps_status = true;
	if (sctrl_pdata)
		sctrl_pdata->dfps_status = true;

	if (new_fps !=
		ctrl_pdata->panel_data.panel_info.mipi.frame_rate) {
		if (pdata->panel_info.dfps_update
			== DFPS_IMMEDIATE_PORCH_UPDATE_MODE) {

			__mdss_dsi_update_video_mode_total(pdata, new_fps);
			if (sctrl_pdata) {
				pr_debug("%s Updating slave ctrl DFPS\n",
						__func__);
				__mdss_dsi_update_video_mode_total(
						&sctrl_pdata->panel_data,
						new_fps);
			}

		} else {
			rc = __mdss_dsi_dfps_update_clks(pdata, new_fps);
			if (!rc && sctrl_pdata) {
				pr_debug("%s Updating slave ctrl DFPS\n",
						__func__);
				rc = __mdss_dsi_dfps_update_clks(
						&sctrl_pdata->panel_data,
						new_fps);
			}
		}
	} else {
		pr_debug("%s: Panel is already at this FPS\n", __func__);
	}

	return rc;
}

static int mdss_dsi_ctl_partial_roi(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int rc = -EINVAL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (ctrl_pdata->set_col_page_addr)
		rc = ctrl_pdata->set_col_page_addr(pdata);

	return rc;
}

static int mdss_dsi_set_stream_size(struct mdss_panel_data *pdata)
{
	u32 data, idle;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo;
	struct mdss_rect *roi;
	struct panel_horizontal_idle *pidle;
	int i;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &pdata->panel_info;

	if (!pinfo->partial_update_enabled)
		return -EINVAL;

	roi = &pinfo->roi;

	/* DSI_COMMAND_MODE_MDP_STREAM_CTRL */
	data = (((roi->w * 3) + 1) << 16) |
			(pdata->panel_info.mipi.vc << 8) | DTYPE_DCS_LWRITE;
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x60, data);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x58, data);

	/* DSI_COMMAND_MODE_MDP_STREAM_TOTAL */
	data = roi->h << 16 | roi->w;
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x64, data);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x5C, data);

	/* set idle control -- dsi clk cycle */
	idle = 0;
	pidle = ctrl_pdata->line_idle;
	for (i = 0; i < ctrl_pdata->horizontal_idle_cnt; i++) {
		if (roi->w > pidle->min && roi->w <= pidle->max) {
			idle = pidle->idle;
			pr_debug("%s: ndx=%d w=%d range=%d-%d idle=%d\n",
				__func__, ctrl_pdata->ndx, roi->w,
				pidle->min, pidle->max, pidle->idle);
			break;
		}
		pidle++;
	}

	if (idle)
		idle |= BIT(12);	/* enable */

	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x194, idle);

	return 0;
}

int mdss_dsi_register_recovery_handler(struct mdss_dsi_ctrl_pdata *ctrl,
	struct mdss_intf_recovery *recovery)
{
	mutex_lock(&ctrl->mutex);
	ctrl->recovery = recovery;
	mutex_unlock(&ctrl->mutex);
	return 0;
}

static int mdss_dsi_clk_refresh(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int rc = 0;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);
	rc = mdss_dsi_clk_div_config(&pdata->panel_info,
			pdata->panel_info.mipi.frame_rate);
	if (rc) {
		pr_err("%s: unable to initialize the clk dividers\n",
								__func__);
		return rc;
	}
	ctrl_pdata->refresh_clk_rate = false;
	ctrl_pdata->pclk_rate = pdata->panel_info.mipi.dsi_pclk_rate;
	ctrl_pdata->byte_clk_rate = pdata->panel_info.clk_rate / 8;
	pr_debug("%s ctrl_pdata->byte_clk_rate=%d ctrl_pdata->pclk_rate=%d\n",
		__func__, ctrl_pdata->byte_clk_rate, ctrl_pdata->pclk_rate);
	return rc;
}

static int mdss_dsi_event_handler(struct mdss_panel_data *pdata,
				  int event, void *arg)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int power_state;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pr_debug("%s+: ctrl=%d event=%d\n", __func__, ctrl_pdata->ndx, event);

	MDSS_XLOG(event, arg, ctrl_pdata->ndx, 0x3333);

#ifdef CONFIG_LGE_READER_MODE
	if (pdata_base == NULL) {
		pr_err("%s : pdata_base =%p\n", __func__, pdata);
		pdata_base = pdata;
	}
#endif

	switch (event) {
	case MDSS_EVENT_CHECK_PARAMS:
		pr_debug("%s:Entered Case MDSS_EVENT_CHECK_PARAMS\n", __func__);
		ctrl_pdata->refresh_clk_rate = true;
		break;
	case MDSS_EVENT_LINK_READY:
#if defined(CONFIG_TOUCHSCREEN_UNIFIED_DRIVER_3)
		touch_notifier_call_chain(LCD_EVENT_TOUCH_LPWG_OFF, NULL);
#elif IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
		if (lge_mdss_dsi.lge_mdss_dsi_event_handler)
			lge_mdss_dsi.lge_mdss_dsi_event_handler(pdata, event, arg);
#endif
		rc = mdss_dsi_on(pdata);
		mdss_dsi_op_mode_config(pdata->panel_info.mipi.mode,
							pdata);
		break;
	case MDSS_EVENT_UNBLANK:
		if (ctrl_pdata->refresh_clk_rate)
			rc = mdss_dsi_clk_refresh(pdata);
		mdss_dsi_get_hw_revision(ctrl_pdata);
		if (ctrl_pdata->on_cmds.link_state == DSI_LP_MODE)
			rc = mdss_dsi_unblank(pdata);
#if defined (CONFIG_LGD_INCELL_PHASE3_VIDEO_HD_PT_PANEL)
#if defined (CONFIG_LGD_INCELL_DB7400_VIDEO_HD_DUAL_PANEL)
		if (lge_dual_panel != SECONDARY_MODULE) {
			if (ctrl_pdata->do_lut_update == true)
			{
				pr_info("%s:%d, F35 recovery, send 0x26 dcs \n",__func__, __LINE__);
				mdss_lcd_do_lut_update();
			}
		}
#else
		if (ctrl_pdata->do_lut_update == true)
		{
			pr_info("%s:%d, F35 recovery, send 0x26 dcs \n",__func__, __LINE__);
			mdss_lcd_do_lut_update();
		}
#endif
#endif
		break;
	case MDSS_EVENT_POST_PANEL_ON:
		rc = mdss_dsi_post_panel_on(pdata);
		break;
	case MDSS_EVENT_PANEL_ON:
		ctrl_pdata->ctrl_state |= CTRL_STATE_MDP_ACTIVE;
		if (ctrl_pdata->on_cmds.link_state == DSI_HS_MODE)
			rc = mdss_dsi_unblank(pdata);
		pdata->panel_info.esd_rdy = true;
		break;
	case MDSS_EVENT_BLANK:
#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
		if (lge_mdss_dsi.lge_mdss_dsi_event_handler)
			lge_mdss_dsi.lge_mdss_dsi_event_handler(pdata, event, arg);
#endif
		power_state = (int) (unsigned long) arg;
		if (ctrl_pdata->off_cmds.link_state == DSI_HS_MODE)
			rc = mdss_dsi_blank(pdata, power_state);
		break;
	case MDSS_EVENT_PANEL_OFF:
		power_state = (int) (unsigned long) arg;
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_MDP_ACTIVE;
		if (ctrl_pdata->off_cmds.link_state == DSI_LP_MODE)
			rc = mdss_dsi_blank(pdata, power_state);

		if (!(pdata->panel_info.mipi.always_on))
			rc = mdss_dsi_off(pdata, power_state);

#if defined(CONFIG_TOUCHSCREEN_UNIFIED_DRIVER_3)
		touch_notifier_call_chain(LCD_EVENT_TOUCH_LPWG_ON, NULL);
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
		if (lge_mdss_dsi.lge_mdss_dsi_event_handler)
			lge_mdss_dsi.lge_mdss_dsi_event_handler(pdata, event, arg);
#endif
		break;
	case MDSS_EVENT_CONT_SPLASH_FINISH:
		if (ctrl_pdata->off_cmds.link_state == DSI_LP_MODE)
			rc = mdss_dsi_blank(pdata, MDSS_PANEL_POWER_OFF);
		ctrl_pdata->ctrl_state &= ~CTRL_STATE_MDP_ACTIVE;
		rc = mdss_dsi_cont_splash_on(pdata);
		break;
	case MDSS_EVENT_PANEL_CLK_CTRL:
		mdss_dsi_clk_req(ctrl_pdata, (int) (unsigned long) arg);
		break;
	case MDSS_EVENT_DSI_CMDLIST_KOFF:
		mdss_dsi_cmdlist_commit(ctrl_pdata, 1);
		break;
	case MDSS_EVENT_PANEL_UPDATE_FPS:
		if (arg != NULL) {
			rc = mdss_dsi_dfps_config(pdata,
					 (int) (unsigned long) arg);
			pr_debug("%s:update fps to = %d\n",
				 __func__, (int) (unsigned long) arg);
		}
		break;
	case MDSS_EVENT_CONT_SPLASH_BEGIN:
		if (ctrl_pdata->off_cmds.link_state == DSI_HS_MODE) {
			/* Panel is Enabled in Bootloader */
			rc = mdss_dsi_blank(pdata, MDSS_PANEL_POWER_OFF);
		}
		break;
	case MDSS_EVENT_ENABLE_PARTIAL_ROI:
		rc = mdss_dsi_ctl_partial_roi(pdata);
		break;
	case MDSS_EVENT_DSI_STREAM_SIZE:
		rc = mdss_dsi_set_stream_size(pdata);
		break;
	case MDSS_EVENT_DSI_DYNAMIC_SWITCH:
		rc = mdss_dsi_update_panel_config(ctrl_pdata,
					(int)(unsigned long) arg);
		break;
	case MDSS_EVENT_REGISTER_RECOVERY_HANDLER:
		rc = mdss_dsi_register_recovery_handler(ctrl_pdata,
			(struct mdss_intf_recovery *)arg);
		break;
	case MDSS_EVENT_DSI_PANEL_STATUS:
		if (ctrl_pdata->check_status)
			rc = ctrl_pdata->check_status(ctrl_pdata);
		break;
	default:
		pr_debug("%s: unhandled event=%d\n", __func__, event);
		break;
	}
	pr_debug("%s-:event=%d, rc=%d\n", __func__, event, rc);
	return rc;
}

static struct device_node *mdss_dsi_pref_prim_panel(
		struct platform_device *pdev)
{
	struct device_node *dsi_pan_node = NULL;

	pr_debug("%s:%d: Select primary panel from dt\n",
					__func__, __LINE__);
	dsi_pan_node = of_parse_phandle(pdev->dev.of_node,
					"qcom,dsi-pref-prim-pan", 0);
	if (!dsi_pan_node)
		pr_err("%s:can't find panel phandle\n", __func__);

	return dsi_pan_node;
}

/**
 * mdss_dsi_find_panel_of_node(): find device node of dsi panel
 * @pdev: platform_device of the dsi ctrl node
 * @panel_cfg: string containing intf specific config data
 *
 * Function finds the panel device node using the interface
 * specific configuration data. This configuration data is
 * could be derived from the result of bootloader's GCDB
 * panel detection mechanism. If such config data doesn't
 * exist then this panel returns the default panel configured
 * in the device tree.
 *
 * returns pointer to panel node on success, NULL on error.
 */
static struct device_node *mdss_dsi_find_panel_of_node(
		struct platform_device *pdev, char *panel_cfg)
{
	int len, i;
	int ctrl_id = pdev->id - 1;
	char panel_name[MDSS_MAX_PANEL_LEN];
	char ctrl_id_stream[3] =  "0:";
	char *stream = NULL, *pan = NULL;
	struct device_node *dsi_pan_node = NULL, *mdss_node = NULL;

	len = strlen(panel_cfg);
	if (!len) {
		/* no panel cfg chg, parse dt */
		pr_debug("%s:%d: no cmd line cfg present\n",
			 __func__, __LINE__);
		goto end;
	} else {
		if (ctrl_id == 1)
			strlcpy(ctrl_id_stream, "1:", 3);

		stream = strnstr(panel_cfg, ctrl_id_stream, len);
		if (!stream) {
			pr_err("controller config is not present\n");
			goto end;
		}
		stream += 2;

		pan = strnchr(stream, strlen(stream), ':');
		if (!pan) {
			strlcpy(panel_name, stream, MDSS_MAX_PANEL_LEN);
		} else {
			for (i = 0; (stream + i) < pan; i++)
				panel_name[i] = *(stream + i);
			panel_name[i] = 0;
		}

		pr_debug("%s:%d:%s:%s\n", __func__, __LINE__,
			 panel_cfg, panel_name);

		mdss_node = of_parse_phandle(pdev->dev.of_node,
					     "qcom,mdss-mdp", 0);

		if (!mdss_node) {
			pr_err("%s: %d: mdss_node null\n",
			       __func__, __LINE__);
			return NULL;
		}
		dsi_pan_node = of_find_node_by_name(mdss_node,
						    panel_name);
		if (!dsi_pan_node) {
			pr_err("%s: invalid pan node, selecting prim panel\n",
			       __func__);
			pr_info("%s: Panel Name = %s\n", __func__, panel_name);
			goto end;
		}
		return dsi_pan_node;
	}
end:
	if (strcmp(panel_name, NONE_PANEL))
		dsi_pan_node = mdss_dsi_pref_prim_panel(pdev);

	return dsi_pan_node;
}

static int mdss_dsi_ctrl_probe(struct platform_device *pdev)
{
	int rc = 0, i = 0;
	u32 index;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct device_node *dsi_pan_node = NULL;
	char panel_cfg[MDSS_MAX_PANEL_LEN];
	const char *ctrl_name;
	bool cmd_cfg_cont_splash = true;
	struct mdss_panel_cfg *pan_cfg = NULL;
	struct mdss_util_intf *util;

	util = mdss_get_util_intf();
	if (util == NULL) {
		pr_err("Failed to get mdss utility functions\n");
		return -ENODEV;
	}

	if (!util->mdp_probe_done) {
		pr_err("%s: MDP not probed yet!\n", __func__);
		return -EPROBE_DEFER;
	}

	if (!pdev->dev.of_node) {
		pr_err("DSI driver only supports device tree probe\n");
		return -ENOTSUPP;
	}

	pan_cfg = util->panel_intf_type(MDSS_PANEL_INTF_HDMI);
	if (IS_ERR(pan_cfg)) {
		return PTR_ERR(pan_cfg);
	} else if (pan_cfg) {
		pr_debug("%s: HDMI is primary\n", __func__);
		return -ENODEV;
	}

	ctrl_pdata = platform_get_drvdata(pdev);
	if (!ctrl_pdata) {
		ctrl_pdata = devm_kzalloc(&pdev->dev,
					  sizeof(struct mdss_dsi_ctrl_pdata),
					  GFP_KERNEL);
		if (!ctrl_pdata) {
			pr_err("%s: FAILED: cannot alloc dsi ctrl\n",
			       __func__);
			rc = -ENOMEM;
			goto error_no_mem;
		}
#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
		else {
			if (!ctrl_pdata->lge_pan_data && sizeof(struct lge_pan_data)) {
				ctrl_pdata->lge_pan_data = devm_kzalloc(&pdev->dev,
						sizeof(struct lge_pan_data),
						GFP_KERNEL);

				if (!ctrl_pdata->lge_pan_data) {
					pr_err("%s: FAILED: cannot alloc lge_pan_data ctrl\n",
							__func__);
					rc = -ENOMEM;
					goto error_no_mem;
				}
			}
		}
#endif // CONFIG_LGE_DISPLAY_CODE_REFACTORING
		platform_set_drvdata(pdev, ctrl_pdata);
	}
	ctrl_pdata->mdss_util = util;
	atomic_set(&ctrl_pdata->te_irq_ready, 0);

	ctrl_name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!ctrl_name)
		pr_info("%s:%d, DSI Ctrl name not specified\n",
			__func__, __LINE__);
	else
		pr_info("%s: DSI Ctrl name = %s\n",
			__func__, ctrl_name);

	rc = of_property_read_u32(pdev->dev.of_node,
				  "cell-index", &index);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: Cell-index not specified, rc=%d\n",
			__func__, rc);
		goto error_no_mem;
	}

	if (index == 0)
		pdev->id = 1;
	else
		pdev->id = 2;

	rc = of_platform_populate(pdev->dev.of_node,
				  NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: failed to add child nodes, rc=%d\n",
			__func__, rc);
		goto error_no_mem;
	}

	rc = mdss_dsi_pinctrl_init(pdev);
	if (rc)
		pr_warn("%s: failed to get pin resources\n", __func__);

	/* Parse the regulator information */
	for (i = 0; i < DSI_MAX_PM; i++) {
#if defined(CONFIG_LCD_VDD_VDDIO_EXT_LDO) //EXT LDO is controlled by gpio.
		if (DSI_PANEL_PM == i)
			continue;
#endif
		rc = mdss_dsi_get_dt_vreg_data(&pdev->dev,
			&ctrl_pdata->power_data[i], i);
		if (rc) {
			DEV_ERR("%s: '%s' get_dt_vreg_data failed.rc=%d\n",
				__func__, __mdss_dsi_pm_name(i), rc);
			goto error_vreg;
		}
	}

	/*
	 * Currently, the Bias vreg is controlled by wled driver.
	 * Once we have support from pmic driver, implement the
	 * bias vreg control using the existing vreg apis.
	 */
	ctrl_pdata->panel_bias_vreg = of_property_read_bool(
			pdev->dev.of_node, "qcom,dsi-panel-bias-vreg");

	/* DSI panels can be different between controllers */
	rc = mdss_dsi_get_panel_cfg(panel_cfg, ctrl_pdata);
	if (!rc)
		/* dsi panel cfg not present */
		pr_warn("%s:%d:dsi specific cfg not present\n",
			__func__, __LINE__);

	/* find panel device node */
	dsi_pan_node = mdss_dsi_find_panel_of_node(pdev, panel_cfg);
	if (!dsi_pan_node) {
		pr_err("%s: can't find panel node %s\n", __func__, panel_cfg);
		goto error_pan_node;
	}

	cmd_cfg_cont_splash = mdss_panel_get_boot_cfg() ? true : false;

#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
	if (index == 0) //dual_dsi
	{
		memset(&lge_mdss_dsi, 0, sizeof(lge_mdss_dsi));
		lge_mdss_dsi_seperate_panel_api_init(&lge_mdss_dsi, dsi_pan_node);
	}
#endif // CONFIG_LGE_DISPLAY_CODE_REFACTORING

	rc = mdss_dsi_panel_init(dsi_pan_node, ctrl_pdata, cmd_cfg_cont_splash);
	if (rc) {
		pr_err("%s: dsi panel init failed\n", __func__);
		goto error_pan_node;
	}

	rc = dsi_panel_device_register(dsi_pan_node, ctrl_pdata);
	if (rc) {
		pr_err("%s: dsi panel dev reg failed\n", __func__);
		goto error_pan_node;
	}
#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
	if (lge_mdss_dsi.lge_mdss_dsi_ctrl_probe)
		lge_mdss_dsi.lge_mdss_dsi_ctrl_probe(pdev);
#endif // CONFIG_LGE_DISPLAY_CODE_REFACTORING
	ctrl_pdata->cmd_clk_ln_recovery_en =
		of_property_read_bool(pdev->dev.of_node,
			"qcom,dsi-clk-ln-recovery");

	if (mdss_dsi_is_te_based_esd(ctrl_pdata)) {
		rc = devm_request_irq(&pdev->dev,
			gpio_to_irq(ctrl_pdata->disp_te_gpio),
			hw_vsync_handler, IRQF_TRIGGER_FALLING,
			"VSYNC_GPIO", ctrl_pdata);
		if (rc) {
			pr_err("TE request_irq failed.\n");
			goto error_pan_node;
		}
		disable_irq(gpio_to_irq(ctrl_pdata->disp_te_gpio));
	}
	pr_debug("%s: Dsi Ctrl->%d initialized\n", __func__, index);
	return 0;

error_pan_node:
	mdss_dsi_unregister_bl_settings(ctrl_pdata);
	of_node_put(dsi_pan_node);
	i--;
error_vreg:
	for (; i >= 0; i--){
#if defined(CONFIG_LCD_VDD_VDDIO_EXT_LDO) //EXT LDO is controlled by gpio.
		if (DSI_PANEL_PM == i)
			continue;
#endif
		mdss_dsi_put_dt_vreg_data(&pdev->dev,
			&ctrl_pdata->power_data[i]);
	}
error_no_mem:
#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
	if (ctrl_pdata && ctrl_pdata->lge_pan_data)
		devm_kfree(&pdev->dev, ctrl_pdata->lge_pan_data);
#endif // CONFIG_LGE_DISPLAY_CODE_REFACTORING
	devm_kfree(&pdev->dev, ctrl_pdata);

	return rc;
}

static int mdss_dsi_ctrl_remove(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);
	int i = 0;

	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	for (i = DSI_MAX_PM - 1; i >= 0; i--) {
#if defined(CONFIG_LCD_VDD_VDDIO_EXT_LDO) //EXT LDO is controlled by gpio.
		if (DSI_PANEL_PM == i)
			continue;
#endif
		if (msm_dss_config_vreg(&pdev->dev,
				ctrl_pdata->power_data[i].vreg_config,
				ctrl_pdata->power_data[i].num_vreg, 1) < 0)
			pr_err("%s: failed to de-init vregs for %s\n",
				__func__, __mdss_dsi_pm_name(i));
		mdss_dsi_put_dt_vreg_data(&pdev->dev,
			&ctrl_pdata->power_data[i]);
	}

	mfd = platform_get_drvdata(pdev);
	msm_dss_iounmap(&ctrl_pdata->mmss_misc_io);
	msm_dss_iounmap(&ctrl_pdata->phy_io);
	msm_dss_iounmap(&ctrl_pdata->ctrl_io);
	return 0;
}


#if defined(CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
static void mdss_dsi_ctrl_shutdown(struct platform_device *pdev)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);
	int ret = 0;

	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return;
	}

	mdelay(15);

	if (gpio_is_valid(ctrl_pdata->rst_gpio)) {
		gpio_set_value((ctrl_pdata->rst_gpio), 0);
		gpio_free(ctrl_pdata->rst_gpio);
	}
	pr_info("[LCD] LCD reset to low\n");
	mdelay(1);

	if (gpio_is_valid(ctrl_pdata->t_rst_gpio)) {
		gpio_set_value((ctrl_pdata->t_rst_gpio), 0);
		gpio_free(ctrl_pdata->t_rst_gpio);
	}
	mdelay(20);

	lm3632_dsv_output_ctrl(0);
	pr_info("[LCD] DSV to low\n");

	mdelay(1);

	if (gpio_is_valid(ctrl_pdata->disp_lcd_ldo_1v8_gpio)) {
		gpio_set_value((ctrl_pdata->disp_lcd_ldo_1v8_gpio), 0);
		gpio_free(ctrl_pdata->disp_lcd_ldo_1v8_gpio);
	}
	pr_info("[LCD] VDDIO to low\n");
	mdelay(5);

	ret = msm_dss_enable_vreg(
			ctrl_pdata->power_data[DSI_PANEL_PM].vreg_config,
			ctrl_pdata->power_data[DSI_PANEL_PM].num_vreg, 0);

	if (ret)
		pr_err("%s: failed to disable vregs for PANEL_PM\n", __func__);

	pr_info("[LCD] VPNL to low\n");
	mdelay(2);
	lm3632_bl_en_control(0);
}
#endif


struct device dsi_dev;

int mdss_dsi_retrieve_ctrl_resources(struct platform_device *pdev, int mode,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;
	u32 index;

	rc = of_property_read_u32(pdev->dev.of_node, "cell-index", &index);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: Cell-index not specified, rc=%d\n",
						__func__, rc);
		return rc;
	}

	if (index == 0) {
		if (mode != DISPLAY_1) {
			pr_err("%s:%d Panel->Ctrl mapping is wrong\n",
				       __func__, __LINE__);
			return -EPERM;
		}
	} else if (index == 1) {
		if (mode != DISPLAY_2) {
			pr_err("%s:%d Panel->Ctrl mapping is wrong\n",
				       __func__, __LINE__);
			return -EPERM;
		}
	} else {
		pr_err("%s:%d Unknown Ctrl mapped to panel\n",
			       __func__, __LINE__);
		return -EPERM;
	}

	rc = msm_dss_ioremap_byname(pdev, &ctrl->ctrl_io, "dsi_ctrl");
	if (rc) {
		pr_err("%s:%d unable to remap dsi ctrl resources\n",
			       __func__, __LINE__);
		return rc;
	}

	ctrl->ctrl_base = ctrl->ctrl_io.base;
	ctrl->reg_size = ctrl->ctrl_io.len;

	rc = msm_dss_ioremap_byname(pdev, &ctrl->phy_io, "dsi_phy");
	if (rc) {
		pr_err("%s:%d unable to remap dsi phy resources\n",
			       __func__, __LINE__);
		return rc;
	}

	pr_info("%s: ctrl_base=%pK ctrl_size=%x phy_base=%pK phy_size=%x\n",
		__func__, ctrl->ctrl_base, ctrl->reg_size, ctrl->phy_io.base,
		ctrl->phy_io.len);

	rc = msm_dss_ioremap_byname(pdev, &ctrl->mmss_misc_io,
		"mmss_misc_phys");
	if (rc) {
		pr_debug("%s:%d mmss_misc IO remap failed\n",
			__func__, __LINE__);
	}

	return 0;
}

static int mdss_dsi_irq_init(struct device *dev, int irq_no,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret;

	ret = devm_request_irq(dev, irq_no, mdss_dsi_isr,
				IRQF_DISABLED, "DSI", ctrl);
	if (ret) {
		pr_err("msm_dsi_irq_init request_irq() failed!\n");
		return ret;
	}

	disable_irq(irq_no);
	ctrl->dsi_hw->irq_info = kzalloc(sizeof(struct irq_info), GFP_KERNEL);
	if (!ctrl->dsi_hw->irq_info) {
		pr_err("no mem to save irq info: kzalloc fail\n");
		return -ENOMEM;
	}
	ctrl->dsi_hw->irq_info->irq = irq_no;
	ctrl->dsi_hw->irq_info->irq_ena = false;

	return ret;
}

int dsi_panel_device_register(struct device_node *pan_node,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mipi_panel_info *mipi;
	int rc, i, len;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);
	struct device_node *dsi_ctrl_np = NULL;
	struct platform_device *ctrl_pdev = NULL;
	const char *data;
	struct resource *res;

	mipi  = &(pinfo->mipi);

	pinfo->type =
		((mipi->mode == DSI_VIDEO_MODE)
			? MIPI_VIDEO_PANEL : MIPI_CMD_PANEL);

	rc = mdss_dsi_clk_div_config(pinfo, mipi->frame_rate);
	if (rc) {
		pr_err("%s: unable to initialize the clk dividers\n", __func__);
		return rc;
	}

	dsi_ctrl_np = of_parse_phandle(pan_node,
				"qcom,mdss-dsi-panel-controller", 0);
	if (!dsi_ctrl_np) {
		pr_err("%s: Dsi controller node not initialized\n", __func__);
		return -EPROBE_DEFER;
	}

	ctrl_pdev = of_find_device_by_node(dsi_ctrl_np);

	rc = mdss_dsi_regulator_init(ctrl_pdev);
	if (rc) {
		pr_err("%s: failed to init regulator, rc=%d\n",
						__func__, rc);
		return rc;
	}

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-strength-ctrl", &len);
	if ((!data) || (len != 2)) {
		pr_err("%s:%d, Unable to read Phy Strength ctrl settings\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	pinfo->mipi.dsi_phy_db.strength[0] = data[0];
	pinfo->mipi.dsi_phy_db.strength[1] = data[1];

	pinfo->mipi.dsi_phy_db.reg_ldo_mode = of_property_read_bool(
		ctrl_pdev->dev.of_node, "qcom,regulator-ldo-mode");

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-regulator-settings", &len);
	if ((!data) || (len != 7)) {
		pr_err("%s:%d, Unable to read Phy regulator settings\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++) {
		pinfo->mipi.dsi_phy_db.regulator[i]
			= data[i];
	}

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-bist-ctrl", &len);
	if ((!data) || (len != 6)) {
		pr_err("%s:%d, Unable to read Phy Bist Ctrl settings\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++) {
		pinfo->mipi.dsi_phy_db.bistctrl[i]
			= data[i];
	}

	data = of_get_property(ctrl_pdev->dev.of_node,
		"qcom,platform-lane-config", &len);
	if ((!data) || (len != 45)) {
		pr_err("%s:%d, Unable to read Phy lane configure settings\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	for (i = 0; i < len; i++) {
		pinfo->mipi.dsi_phy_db.lanecfg[i] =
			data[i];
	}

	rc = of_property_read_u32(ctrl_pdev->dev.of_node,
			"qcom,mmss-ulp-clamp-ctrl-offset",
			&ctrl_pdata->ulps_clamp_ctrl_off);
	if (!rc) {
		rc = of_property_read_u32(ctrl_pdev->dev.of_node,
				"qcom,mmss-phyreset-ctrl-offset",
				&ctrl_pdata->ulps_phyrst_ctrl_off);
	}
	if (rc && pinfo->ulps_feature_enabled) {
		pr_err("%s: dsi ulps clamp register settings missing\n",
				__func__);
		return -EINVAL;
	}

	ctrl_pdata->cmd_sync_wait_broadcast = of_property_read_bool(
		pan_node, "qcom,cmd-sync-wait-broadcast");

	ctrl_pdata->cmd_sync_wait_trigger = of_property_read_bool(
		pan_node, "qcom,cmd-sync-wait-trigger");

	pr_debug("%s: cmd_sync_wait_enable=%d trigger=%d\n", __func__,
				ctrl_pdata->cmd_sync_wait_broadcast,
				ctrl_pdata->cmd_sync_wait_trigger);

	pinfo->panel_max_fps = mdss_panel_get_framerate(pinfo);
	pinfo->panel_max_vtotal = mdss_panel_get_vtotal(pinfo);

	/*
	 * If disp_en_gpio has been set previously (disp_en_gpio > 0)
	 *  while parsing the panel node, then do not override it
	 */
	if (ctrl_pdata->disp_en_gpio <= 0) {
		ctrl_pdata->disp_en_gpio = of_get_named_gpio(
			ctrl_pdev->dev.of_node,
			"qcom,platform-enable-gpio", 0);

		if (!gpio_is_valid(ctrl_pdata->disp_en_gpio))
			pr_err("%s:%d, Disp_en gpio not specified\n",
					__func__, __LINE__);
	}
#if defined (CONFIG_LGD_INCELL_PHASE3_VIDEO_HD_PT_PANEL)|| defined (CONFIG_LGD_DONGBU_INCELL_VIDEO_HD_PANEL) || defined (CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
	ctrl_pdata->disp_lcd_ldo_1v8_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-lcd-ldo-1v8-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_lcd_ldo_1v8_gpio))
		pr_err("%s:%d, platform-lcd-ldo-1v8-gpio gpio not specified\n",
						__func__, __LINE__);
	rc = gpio_request(ctrl_pdata->disp_lcd_ldo_1v8_gpio, "disp_lcd_ldo_1v8_gpio");
#if defined (CONFIG_LGD_DONGBU_INCELL_VIDEO_HD_PANEL)
	ctrl_pdata->disp_dsv_p_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-dsv_p_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dsv_p_gpio))
		pr_err("%s:%d, platform-disp-en-p gpio not specified\n",
					__func__, __LINE__);
	ctrl_pdata->disp_dsv_n_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-dsv_n_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dsv_n_gpio))
		pr_err("%s:%d, platform-disp-en-n gpio not specified\n",
					__func__, __LINE__);
#elif defined (CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
	ctrl_pdata->t_rst_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
            "qcom,platform-t_reset-gpio", 0);
    if (!gpio_is_valid(ctrl_pdata->t_rst_gpio))
        pr_err("%s:%d, touch reset gpio not specified\n",
               __func__, __LINE__);
	ctrl_pdata->disp_dsv_p_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-dsv_p_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dsv_p_gpio))
		pr_err("%s:%d, platform-disp-en-p gpio not specified\n",
					__func__, __LINE__);
	ctrl_pdata->disp_dsv_n_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-dsv_n_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dsv_n_gpio))
		pr_err("%s:%d, platform-disp-en-n gpio not specified\n",
					__func__, __LINE__);
#else
	ctrl_pdata->disp_lcd_ldo_3v0_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-lcd-ldo-3v0-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_lcd_ldo_3v0_gpio))
		pr_err("%s:%d, platform-lcd-ldo-3v0-gpio gpio not specified\n",
						__func__, __LINE__);
	ctrl_pdata->disp_dsv_en_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-dsv-en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dsv_en_gpio))
		pr_err("%s:%d, platform-disp-en-gpio gpio not specified\n",
						__func__, __LINE__);
#endif
#endif
#if defined (CONFIG_LGD_M2DONGBU_INCELL_VIDEO_HD_PANEL)
	ctrl_pdata->disp_dsv_p_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-dsv_p_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dsv_p_gpio))
		pr_err("%s:%d, platform-disp-en-p gpio not specified\n",
					__func__, __LINE__);
	ctrl_pdata->disp_dsv_n_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-dsv_n_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dsv_n_gpio))
		pr_err("%s:%d, platform-disp-en-n gpio not specified\n",
					__func__, __LINE__);
#endif
#if defined(CONFIG_LGD_INCELL_VIDEO_FWVGA_PT_PANEL)
	dual_panel = of_property_read_bool(pan_node,
			"lge,dual-panel");
	if(dual_panel)
		pr_info("[mdss] dual panel is detected\n");
	else
		pr_info("[mdss] original panel is detected\n");
#endif

#if defined(CONFIG_LGD_INCELL_VIDEO_WVGA_PT_PANEL)
	ctrl_pdata->disp_fd_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-fd-gpio", 0);
#endif
	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio))
		pr_err("%s:%d, Disp_en gpio not specified\n",
						__func__, __LINE__);
#if defined(CONFIG_LGD_INCELL_VIDEO_WVGA_PT_PANEL)
	if (!gpio_is_valid(ctrl_pdata->disp_fd_gpio))
		pr_err("%s:%d, Disp_fd gpio not specified\n",
						__func__, __LINE__);
	ctrl_pdata->disp_iovcc_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-iovcc-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_iovcc_gpio))
		pr_err("%s:%d, Disp_iovcc gpio not specified\n",
						__func__, __LINE__);
#endif
#if defined(CONFIG_LGE_MIPI_DSI_BYD_ILI9806E_WVGA)
	ctrl_pdata->disp_iovcc_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-iovcc-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_iovcc_gpio))
		pr_err("%s:%d, Disp_iovcc gpio not specified\n",
						__func__, __LINE__);
#endif

#if defined(CONFIG_LCD_LGE_IOVCC_EXT_LDO)
	ctrl_pdata->disp_iovcc_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-iovcc-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_iovcc_gpio))
		pr_err("%s:%d, Disp_iovcc gpio not specified\n",
						__func__, __LINE__);
#endif

#if defined(CONFIG_LGE_MIPI_DSI_LGD_NT35521_E7II_WXGA)
#if defined(CONFIG_BACKLIGHT_LM3632) || defined(CONFIG_LGD_INCELL_PHASE3_VIDEO_HD_PT_PANEL)
//nothing
#else
	ctrl_pdata->disp_dsv_p_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			"qcom,platform-dsv_p_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dsv_p_gpio))
		pr_err("%s: dsv_p_en-gpio gpio not specified\n", __func__);
	rc = gpio_request(ctrl_pdata->disp_dsv_p_gpio, "disp_dsv_p_gpio");
	if (rc)
		pr_err("%s: dsv_p_en-gpio gpio request\n", __func__);

	ctrl_pdata->disp_dsv_n_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			"qcom,platform-dsv_n_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dsv_n_gpio))
		pr_err("%s: disp_dsv_n_gpio gpio not specified\n", __func__);
	rc = gpio_request(ctrl_pdata->disp_dsv_n_gpio, "disp_dsv_n_gpio");
	if (rc)
		pr_err("%s: dsv_n_en-gpio gpio request\n", __func__);
#endif
#endif

#if defined(CONFIG_MFD_DW8768)
	ctrl_pdata->disp_dsv_p_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			"qcom,platform-dsv_p_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dsv_p_gpio))
		pr_err("%s: dsv_p_en-gpio gpio not specified\n", __func__);
	rc = gpio_request(ctrl_pdata->disp_dsv_p_gpio, "disp_dsv_p_gpio");
	if (rc)
		pr_err("%s: dsv_p_en-gpio gpio request\n", __func__);

	ctrl_pdata->disp_dsv_n_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			"qcom,platform-dsv_n_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_dsv_n_gpio))
		pr_err("%s: disp_dsv_n_gpio gpio not specified\n", __func__);
	rc = gpio_request(ctrl_pdata->disp_dsv_n_gpio, "disp_dsv_n_gpio");
	if (rc)
		pr_err("%s: dsv_n_en-gpio gpio request\n", __func__);
#endif

#if defined(CONFIG_LCD_VDD_VDDIO_EXT_LDO)
	ctrl_pdata->disp_ext_vdd_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			"qcom,platform-vdd_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_ext_vdd_gpio))
		pr_err("%s: ext_VDD_gpio not specified\n", __func__);
	rc = gpio_request(ctrl_pdata->disp_ext_vdd_gpio, "disp_ext_vdd_gpio");
	if (rc)
		pr_err("%s: ext_VDD_gpio request\n", __func__);
	ctrl_pdata->disp_ext_vddio_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			"qcom,platform-vddio_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_ext_vddio_gpio))
		pr_err("%s: ext_VDDIO_gpio not specified\n", __func__);
	rc = gpio_request(ctrl_pdata->disp_ext_vddio_gpio, "disp_ext_vddio_gpio");
	if (rc)
		pr_err("%s: ext_VDDIO_gpio request\n", __func__);
#endif

#if defined(CONFIG_LCD_VDD_EXT_LDO)
	ctrl_pdata->disp_ext_vdd_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			"qcom,platform-vdd_en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_ext_vdd_gpio))
		pr_err("%s: ext_VDD_gpio not specified\n", __func__);
	rc = gpio_request(ctrl_pdata->disp_ext_vdd_gpio, "disp_ext_vdd_gpio");
	if (rc)
		pr_err("%s: ext_VDD_gpio request\n", __func__);
#endif

#if defined (CONFIG_LGD_LD083_VIDEO_WUXGA_PT_PANEL)
	ctrl_pdata->disp_lcd_ldo_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			"qcom,platform-lcd-ldo-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->disp_lcd_ldo_gpio))
			pr_err("%s:%d, disp_lcd_ldo_gpio gpio not specified\n",
							__func__, __LINE__);
#endif
	ctrl_pdata->disp_te_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-te-gpio", 0);

	if (!gpio_is_valid(ctrl_pdata->disp_te_gpio))
		pr_err("%s:%d, TE gpio not specified\n",
						__func__, __LINE__);

	ctrl_pdata->bklt_en_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
		"qcom,platform-bklight-en-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->bklt_en_gpio))
		pr_info("%s: bklt_en gpio not specified\n", __func__);

	ctrl_pdata->rst_gpio = of_get_named_gpio(ctrl_pdev->dev.of_node,
			 "qcom,platform-reset-gpio", 0);
	if (!gpio_is_valid(ctrl_pdata->rst_gpio))
		pr_err("%s:%d, reset gpio not specified\n",
						__func__, __LINE__);

#if defined(CONFIG_JDI_INCELL_VIDEO_HD_PANEL) || defined(CONFIG_JDI_INCELL_VIDEO_FHD_PANEL) || defined (CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
	rc = gpio_request(ctrl_pdata->rst_gpio, "rst_gpio");
	if(rc)
		pr_err("%s: rst-gpio gpio request\n", __func__);
#endif
#if defined(CONFIG_LGD_INCELL_VIDEO_WVGA_PT_PANEL) || defined(CONFIG_LGD_INCELL_VIDEO_FWVGA_PT_PANEL) || defined(CONFIG_LGE_MIPI_DSI_BYD_ILI9806E_WVGA) || defined(CONFIG_LGE_MIPI_DSI_LGD_NT35521_E7II_WXGA)
	ctrl_pdata->ldo_mode = of_property_read_bool(ctrl_pdev->dev.of_node, "qcom,ldo_mode");
#endif

	if (pinfo->mode_gpio_state != MODE_GPIO_NOT_VALID) {

		ctrl_pdata->mode_gpio = of_get_named_gpio(
					ctrl_pdev->dev.of_node,
					"qcom,platform-mode-gpio", 0);
		if (!gpio_is_valid(ctrl_pdata->mode_gpio))
			pr_info("%s:%d, mode gpio not specified\n",
							__func__, __LINE__);
	} else {
		ctrl_pdata->mode_gpio = -EINVAL;
	}
#if IS_ENABLED(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
	if (lge_mdss_dsi.lge_dsi_panel_device_register)
		lge_mdss_dsi.lge_dsi_panel_device_register(pan_node, ctrl_pdata);
#endif // CONFIG_LGE_DISPLAY_CODE_REFACTORING
	ctrl_pdata->timing_db_mode = of_property_read_bool(
		ctrl_pdev->dev.of_node, "qcom,timing-db-mode");

	if (mdss_dsi_clk_init(ctrl_pdev, ctrl_pdata)) {
		pr_err("%s: unable to initialize Dsi ctrl clks\n", __func__);
		return -EPERM;
	}

	rc = mdss_dsi_pll_1_clk_init(ctrl_pdev, ctrl_pdata);
	if (rc)
		pr_err("PLL 1 Clock's did not register\n");

	if (pinfo->dynamic_fps &&
			pinfo->dfps_update == DFPS_IMMEDIATE_CLK_UPDATE_MODE) {
		if (mdss_dsi_shadow_clk_init(ctrl_pdev, ctrl_pdata)) {
			pr_err("unable to initialize shadow ctrl clks\n");
			return -EPERM;
		}
	}

	if (mdss_dsi_retrieve_ctrl_resources(ctrl_pdev,
					     pinfo->pdest,
					     ctrl_pdata)) {
		pr_err("%s: unable to get Dsi controller res\n", __func__);
		return -EPERM;
	}

	ctrl_pdata->panel_data.event_handler = mdss_dsi_event_handler;

	if (ctrl_pdata->status_mode == ESD_REG ||
			ctrl_pdata->status_mode == ESD_REG_NT35596)
		ctrl_pdata->check_status = mdss_dsi_reg_status_check;
	else if (ctrl_pdata->status_mode == ESD_BTA)
		ctrl_pdata->check_status = mdss_dsi_bta_status_check;

	if (ctrl_pdata->status_mode == ESD_MAX) {
		pr_err("%s: Using default BTA for ESD check\n", __func__);
		ctrl_pdata->check_status = mdss_dsi_bta_status_check;
	}
	if (ctrl_pdata->bklt_ctrl == BL_PWM)
		mdss_dsi_panel_pwm_cfg(ctrl_pdata);

	mdss_dsi_ctrl_init(&ctrl_pdev->dev, ctrl_pdata);

	ctrl_pdata->dsi_irq_line = of_property_read_bool(
				ctrl_pdev->dev.of_node, "qcom,dsi-irq-line");

	if (ctrl_pdata->dsi_irq_line) {
		/* DSI has it's own irq line */
		res = platform_get_resource(ctrl_pdev, IORESOURCE_IRQ, 0);
		if (!res || res->start == 0) {
			pr_err("%s:%d unable to get the MDSS irq resources\n",
							__func__, __LINE__);
			return -ENODEV;
		}
		rc = mdss_dsi_irq_init(&ctrl_pdev->dev, res->start, ctrl_pdata);
		if (rc) {
			dev_err(&ctrl_pdev->dev, "%s: failed to init irq\n",
							__func__);
			return rc;
		}
	}

	ctrl_pdata->pclk_rate = mipi->dsi_pclk_rate;
	ctrl_pdata->byte_clk_rate = pinfo->clk_rate / 8;
	pr_debug("%s: pclk=%d, bclk=%d\n", __func__,
			ctrl_pdata->pclk_rate, ctrl_pdata->byte_clk_rate);

	ctrl_pdata->ctrl_state = CTRL_STATE_UNKNOWN;
#if defined(CONFIG_LGD_INCELL_VIDEO_WVGA_PT_PANEL) || defined(CONFIG_LGD_INCELL_VIDEO_FWVGA_PT_PANEL) \
	|| defined(CONFIG_JDI_INCELL_VIDEO_HD_PANEL) || defined(CONFIG_LGD_INCELL_PHASE3_VIDEO_HD_PT_PANEL) \
	|| defined(CONFIG_JDI_INCELL_VIDEO_FHD_PANEL) || defined (CONFIG_LGD_DONGBU_INCELL_VIDEO_HD_PANEL) || defined (CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
	has_dsv_f = of_property_read_bool(pan_node,
			"lge,has-dsv");
#endif

	/*
	 * If ULPS during suspend is enabled, add an extra vote for the
	 * DSI CTRL power module. This keeps the regulator always enabled.
	 * This is needed for the DSI PHY to maintain ULPS state during
	 * suspend also.
	 */
	if (pinfo->ulps_suspend_enabled) {
		rc = msm_dss_enable_vreg(
			ctrl_pdata->power_data[DSI_CTRL_PM].vreg_config,
			ctrl_pdata->power_data[DSI_CTRL_PM].num_vreg, 1);
		if (rc) {
			pr_err("%s: failed to enable vregs for DSI_CTRL_PM\n",
				__func__);
			return rc;
		}
	}

	if (pinfo->cont_splash_enabled) {
		rc = mdss_dsi_panel_power_ctrl(&(ctrl_pdata->panel_data),
			MDSS_PANEL_POWER_ON);
		if (rc) {
			pr_err("%s: Panel power on failed\n", __func__);
			return rc;
		}
		if (ctrl_pdata->bklt_ctrl == BL_PWM)
			ctrl_pdata->pwm_enabled = 1;
		pinfo->blank_state = MDSS_PANEL_BLANK_UNBLANK;
		mdss_dsi_clk_ctrl(ctrl_pdata, DSI_ALL_CLKS, 1);
		ctrl_pdata->ctrl_state |=
			(CTRL_STATE_PANEL_INIT | CTRL_STATE_MDP_ACTIVE);
	} else {
		pinfo->panel_power_state = MDSS_PANEL_POWER_OFF;
	}

	rc = mdss_register_panel(ctrl_pdev, &(ctrl_pdata->panel_data));
	if (rc) {
		pr_err("%s: unable to register MIPI DSI panel\n", __func__);
		return rc;
	}

	if (pinfo->pdest == DISPLAY_1) {
		mdss_debug_register_io("dsi0_ctrl", &ctrl_pdata->ctrl_io);
		mdss_debug_register_io("dsi0_phy", &ctrl_pdata->phy_io);
		ctrl_pdata->ndx = 0;
	} else {
		mdss_debug_register_io("dsi1_ctrl", &ctrl_pdata->ctrl_io);
		mdss_debug_register_io("dsi1_phy", &ctrl_pdata->phy_io);
		ctrl_pdata->ndx = 1;
	}

	panel_debug_register_base("panel",
		ctrl_pdata->ctrl_base, ctrl_pdata->reg_size);

	pr_debug("%s: Panel data initialized\n", __func__);
	return 0;
}

static const struct of_device_id mdss_dsi_ctrl_dt_match[] = {
	{.compatible = "qcom,mdss-dsi-ctrl"},
	{}
};
MODULE_DEVICE_TABLE(of, mdss_dsi_ctrl_dt_match);

static struct platform_driver mdss_dsi_ctrl_driver = {
	.probe = mdss_dsi_ctrl_probe,
	.remove = mdss_dsi_ctrl_remove,
#if defined(CONFIG_LGD_PH1DONGBU_INCELL_VIDEO_HD_PANEL)
	.shutdown = mdss_dsi_ctrl_shutdown,
#elif defined(CONFIG_LGE_DISPLAY_CODE_REFACTORING)
	.shutdown = mdss_dsi_ctrl_shutdown,
#else
	.shutdown = NULL,
#endif
	.driver = {
		.name = "mdss_dsi_ctrl",
		.of_match_table = mdss_dsi_ctrl_dt_match,
	},
};

static int mdss_dsi_register_driver(void)
{
	return platform_driver_register(&mdss_dsi_ctrl_driver);
}

static int __init mdss_dsi_driver_init(void)
{
	int ret;

	ret = mdss_dsi_register_driver();
	if (ret) {
		pr_err("mdss_dsi_register_driver() failed!\n");
		return ret;
	}

	return ret;
}
module_init(mdss_dsi_driver_init);

static void __exit mdss_dsi_driver_cleanup(void)
{
	platform_driver_unregister(&mdss_dsi_ctrl_driver);
}
module_exit(mdss_dsi_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DSI controller driver");
MODULE_AUTHOR("Chandan Uddaraju <chandanu@codeaurora.org>");
