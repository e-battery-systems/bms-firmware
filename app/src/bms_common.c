/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bms/bms.h>
#include <bms/bms_common.h>

#include "helper.h"

#include <assert.h>
#include <stdio.h>

LOG_MODULE_REGISTER(bms, CONFIG_LOG_DEFAULT_LEVEL);

extern const struct device *bms_ic;

#define OCV_LFP \
    3.392F, 3.314F, 3.309F, 3.308F, 3.304F, 3.296F, 3.283F, 3.275F, 3.271F, 3.268F, 3.265F, \
        3.264F, 3.262F, 3.252F, 3.240F, 3.226F, 3.213F, 3.190F, 3.177F, 3.132F, 2.833F

#define OCV_NMC \
    4.198F, 4.135F, 4.089F, 4.056F, 4.026F, 3.993F, 3.962F, 3.924F, 3.883F, 3.858F, 3.838F, \
        3.819F, 3.803F, 3.787F, 3.764F, 3.745F, 3.726F, 3.702F, 3.684F, 3.588F, 2.800F

BUILD_ASSERT(ARRAY_SIZE(((struct bms_context *)0)->ocv_points) == OCV_POINTS,
             "ocv_points array in bms_context is too small");
BUILD_ASSERT(ARRAY_SIZE(((struct bms_context *)0)->soc_points) == OCV_POINTS,
             "soc_points array in bms_context is too small");

void bms_init_config(struct bms_context *bms, enum bms_cell_type type, float nominal_capacity_Ah)
{
    bms->nominal_capacity_Ah = nominal_capacity_Ah;

    bms->chg_enable = true;
    bms->dis_enable = true;

    bms->ic_conf.bal_idle_delay = 1800;         // default: 30 minutes
    bms->ic_conf.bal_idle_current = 0.1F;       // A
    bms->ic_conf.bal_cell_voltage_diff = 0.01F; // 10 mV

#ifdef CONFIG_BMS_IC_CURRENT_MONITORING
    /* 1C should be safe for all batteries */
    bms->ic_conf.dis_oc_limit = bms->nominal_capacity_Ah;
    bms->ic_conf.chg_oc_limit = bms->nominal_capacity_Ah;

    bms->ic_conf.dis_oc_delay_ms = 320;
    bms->ic_conf.chg_oc_delay_ms = 320;

    bms->ic_conf.dis_sc_limit = bms->ic_conf.dis_oc_limit * 2;
    bms->ic_conf.dis_sc_delay_us = 200;
#endif /* CONFIG_BMS_IC_CURRENT_MONITORING */

    bms->ic_conf.dis_ut_limit = -20;
    bms->ic_conf.dis_ot_limit = 45;
    bms->ic_conf.chg_ut_limit = 0;
    bms->ic_conf.chg_ot_limit = 45;
    bms->ic_conf.temp_limit_hyst = 5;

    bms->ic_conf.cell_ov_delay_ms = 2000;
    bms->ic_conf.cell_uv_delay_ms = 2000;

    memset(bms->ocv_points, 0, sizeof(bms->ocv_points));

    switch (type) {
        case CELL_TYPE_LFP:
            bms->ic_conf.cell_ov_limit = 3.80F;
            bms->ic_conf.cell_chg_voltage_limit = 3.55F;
            bms->ic_conf.cell_ov_reset = 3.40F;
            bms->ic_conf.bal_cell_voltage_min = 3.30F;
            bms->ic_conf.cell_uv_reset = 3.10F;
            bms->ic_conf.cell_dis_voltage_limit = 2.80F;
            /*
             * most cells survive even 2.0V, but we should keep some margin for further
             * self-discharge
             */
            bms->ic_conf.cell_uv_limit = 2.50F;
            {
                float ocv_points[OCV_POINTS] = { OCV_LFP };
                memcpy(bms->ocv_points, ocv_points, sizeof(bms->ocv_points));
            }
            break;
        case CELL_TYPE_NMC:
            bms->ic_conf.cell_ov_limit = 4.25F;
            bms->ic_conf.cell_chg_voltage_limit = 4.20F;
            bms->ic_conf.cell_ov_reset = 4.05F;
            bms->ic_conf.bal_cell_voltage_min = 3.80F;
            bms->ic_conf.cell_uv_reset = 3.50F;
            bms->ic_conf.cell_dis_voltage_limit = 3.20F;
            bms->ic_conf.cell_uv_limit = 3.00F;
            {
                float ocv_points[OCV_POINTS] = { OCV_NMC };
                memcpy(bms->ocv_points, ocv_points, sizeof(bms->ocv_points));
            }
            break;
        case CELL_TYPE_LTO:
            bms->ic_conf.cell_ov_limit = 2.85F;
            bms->ic_conf.cell_chg_voltage_limit = 2.80F;
            bms->ic_conf.cell_ov_reset = 2.70F;
            bms->ic_conf.bal_cell_voltage_min = 2.50F;
            bms->ic_conf.cell_uv_reset = 2.10F;
            bms->ic_conf.cell_dis_voltage_limit = 2.00F;
            bms->ic_conf.cell_uv_limit = 1.90F;
            // ToDo: Use typical OCV curve for LTO cells
            break;
        case CELL_TYPE_CUSTOM:
            break;
    }

    /* trigger alert for all possible errors by default */
    bms->ic_conf.alert_mask = BMS_ERR_ALL;
}

__weak void bms_state_machine(struct bms_context *bms)
{
    switch (bms->state) {
        case BMS_STATE_OFF:
            if (bms_dis_allowed(bms)) {
                bms_ic_set_switches(bms_ic, BMS_SWITCH_DIS, true);
                bms->state = BMS_STATE_DIS;
                LOG_INF("OFF -> DIS (error flags: 0x%08x)", bms->ic_data.error_flags);
            }
            else if (bms_chg_allowed(bms)) {
                bms_ic_set_switches(bms_ic, BMS_SWITCH_CHG, true);
                bms->state = BMS_STATE_CHG;
                LOG_INF("OFF -> CHG (error flags: 0x%08x)", bms->ic_data.error_flags);
            }
            break;
        case BMS_STATE_CHG:
            if (!bms_chg_allowed(bms)) {
                bms_ic_set_switches(bms_ic, BMS_SWITCH_CHG, false);
                /* DIS switch may be on on because of ideal diode control */
                bms_ic_set_switches(bms_ic, BMS_SWITCH_DIS, false);
                bms->state = BMS_STATE_OFF;
                LOG_INF("CHG -> OFF (error flags: 0x%08x)", bms->ic_data.error_flags);
            }
            else if (bms_dis_allowed(bms)) {
                bms_ic_set_switches(bms_ic, BMS_SWITCH_DIS, true);
                bms->state = BMS_STATE_NORMAL;
                LOG_INF("CHG -> NORMAL (error flags: 0x%08x)", bms->ic_data.error_flags);
            }
#ifndef CONFIG_BMS_IC_BQ769X2 /* bq769x2 has built-in ideal diode control */
            else {
                /* ideal diode control for discharge MOSFET (with hysteresis) */
                if (bms->ic_data.current > 0.5F) {
                    bms_ic_set_switches(bms_ic, BMS_SWITCH_DIS, true);
                }
                else if (bms->ic_data.current < 0.1F) {
                    bms_ic_set_switches(bms_ic, BMS_SWITCH_DIS, false);
                }
            }
#endif
            break;
        case BMS_STATE_DIS:
            if (!bms_dis_allowed(bms)) {
                bms_ic_set_switches(bms_ic, BMS_SWITCH_DIS, false);
                /* CHG_FET may be on because of ideal diode control */
                bms_ic_set_switches(bms_ic, BMS_SWITCH_CHG, false);
                bms->state = BMS_STATE_OFF;
                LOG_INF("DIS -> OFF (error flags: 0x%08x)", bms->ic_data.error_flags);
            }
            else if (bms_chg_allowed(bms)) {
                bms_ic_set_switches(bms_ic, BMS_SWITCH_CHG, true);
                bms->state = BMS_STATE_NORMAL;
                LOG_INF("DIS -> NORMAL (error flags: 0x%08x)", bms->ic_data.error_flags);
            }
#ifndef CONFIG_BMS_IC_BQ769X2 /* bq769x2 has built-in ideal diode control */
            else {
                /* ideal diode control for charge MOSFET (with hysteresis) */
                if (bms->ic_data.current < -0.5F) {
                    bms_ic_set_switches(bms_ic, BMS_SWITCH_CHG, true);
                }
                else if (bms->ic_data.current > -0.1F) {
                    bms_ic_set_switches(bms_ic, BMS_SWITCH_CHG, false);
                }
            }
#endif
            break;
        case BMS_STATE_NORMAL:
            if (!bms_dis_allowed(bms)) {
                bms_ic_set_switches(bms_ic, BMS_SWITCH_DIS, false);
                bms->state = BMS_STATE_CHG;
                LOG_INF("NORMAL -> CHG (error flags: 0x%08x)", bms->ic_data.error_flags);
            }
            else if (!bms_chg_allowed(bms)) {
                bms_ic_set_switches(bms_ic, BMS_SWITCH_CHG, false);
                bms->state = BMS_STATE_DIS;
                LOG_INF("NORMAL -> DIS (error flags: 0x%08x)", bms->ic_data.error_flags);
            }
            break;
        case BMS_STATE_SHUTDOWN:
            /* do nothing and wait until shutdown is completed */
            break;
    }
}

bool bms_chg_error(uint32_t error_flags)
{
    return error_flags
           & (BMS_ERR_CELL_OVERVOLTAGE | BMS_ERR_CHG_OVERCURRENT | BMS_ERR_OPEN_WIRE
              | BMS_ERR_CHG_UNDERTEMP | BMS_ERR_CHG_OVERTEMP | BMS_ERR_INT_OVERTEMP
              | BMS_ERR_CELL_FAILURE | BMS_ERR_CHG_OFF);
}

bool bms_dis_error(uint32_t error_flags)
{
    return error_flags
           & (BMS_ERR_CELL_UNDERVOLTAGE | BMS_ERR_SHORT_CIRCUIT | BMS_ERR_DIS_OVERCURRENT
              | BMS_ERR_OPEN_WIRE | BMS_ERR_DIS_UNDERTEMP | BMS_ERR_DIS_OVERTEMP
              | BMS_ERR_INT_OVERTEMP | BMS_ERR_CELL_FAILURE | BMS_ERR_DIS_OFF);
}

bool bms_chg_allowed(struct bms_context *bms)
{
    return !bms_chg_error(bms->ic_data.error_flags & ~BMS_ERR_CHG_OFF) && !bms->full
           && bms->chg_enable;
}

bool bms_dis_allowed(struct bms_context *bms)
{
    return !bms_dis_error(bms->ic_data.error_flags & ~BMS_ERR_DIS_OFF) && !bms->empty
           && bms->dis_enable;
}
