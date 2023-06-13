/**
* Copyright (c) 2023 Bosch Sensortec GmbH. All rights reserved.
*
* BSD-3-Clause
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
*    contributors may be used to endorse or promote products derived from
*    this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* @file  bmm350_forced_mode.c
*
* @brief This file contains reading of magnetometer data in forced mode
* with various combinations of ODR, average and delay.
*
*/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "bmm350.h"
#include "common.h"
#include "coines.h"

/******************************************************************************/
/*!                   Macro Definitions                                       */

#define MAG_SAMPLE_COUNT  UINT8_C(100)

/******************************************************************************/
/*!                   Static Structure Definitions                            */

/*!
 * @brief bmm350 compensated magnetometer data structure
 */
struct bmm350_mag_data
{
    /*! Compensated mag X data */
    double x;

    /*! Compensated mag Y data */
    double y;

    /*! Compensated mag Z data */
    double z;
};

/******************************************************************************/
/*!                   Static Function Declaration                             */

/*!
 *  @brief This internal API is to calculate noise level for mag data
 *
 *  @param[in] mag_temp_data            : Structure instance of bmm350_mag_temp_data.
 *  @param[in] avg_mag_data        : Structure to store average mag data.
 *
 *  @return void.
 */
static void calculate_noise(const struct bmm350_mag_temp_data *mag_temp_data, struct bmm350_mag_data avg_mag_data);

/******************************************************************************/
/*!            Functions                                                      */

/* This function starts the execution of program */
int main(void)
{
    /* Status of api are returned to this variable */
    int8_t rslt;

    /* Sensor initialization configuration */
    struct bmm350_dev dev = { 0 };

    uint8_t int_ctrl, err_reg_data = 0;
    uint8_t loop = 10, set_int_ctrl;
    uint32_t time_ms = 0;

    struct bmm350_mag_temp_data mag_temp_data = { 0 };
    struct bmm350_pmu_cmd_status_0 pmu_cmd_stat_0 = { 0 };
    struct bmm350_mag_temp_data get_mag_temp_data[MAG_SAMPLE_COUNT] = { { 0 } };
    struct bmm350_mag_data mean_mag_data_1 = { 0 };
    struct bmm350_mag_data mean_mag_data_2 = { 0 };
    struct bmm350_mag_data mean_mag_data_3 = { 0 };
    struct bmm350_mag_data avg_mag_data = { 0 };

    /* Update device structure */
    rslt = bmm350_interface_init(&dev);
    bmm350_error_codes_print_result("bmm350_interface_selection", rslt);

    /* Initialize BMM350 */
    rslt = bmm350_init(&dev);
    bmm350_error_codes_print_result("bmm350_init", rslt);

    printf("Read : 0x00 : BMM350 Chip ID : 0x%X\n", dev.chip_id);

    /* Check PMU busy */
    rslt = bmm350_get_pmu_cmd_status_0(&pmu_cmd_stat_0, &dev);
    bmm350_error_codes_print_result("bmm350_get_pmu_cmd_status_0", rslt);

    printf("Expected : 0x07 : PMU cmd busy : 0x0\n");
    printf("Read : 0x07 : PMU cmd busy : 0x%X\n", pmu_cmd_stat_0.pmu_cmd_busy);

    /* Get error data */
    rslt = bmm350_get_regs(BMM350_REG_ERR_REG, &err_reg_data, 1, &dev);
    bmm350_error_codes_print_result("bmm350_get_error_reg_data", rslt);

    printf("Expected : 0x02 : Error Register : 0x0\n");
    printf("Read : 0x02 : Error Register : 0x%X\n", err_reg_data);

    /* Configure interrupt settings */
    rslt = bmm350_configure_interrupt(BMM350_PULSED,
                                      BMM350_ACTIVE_HIGH,
                                      BMM350_INTR_PUSH_PULL,
                                      BMM350_UNMAP_FROM_PIN,
                                      &dev);
    bmm350_error_codes_print_result("bmm350_configure_interrupt", rslt);

    /* Enable data ready interrupt */
    rslt = bmm350_enable_interrupt(BMM350_ENABLE_INTERRUPT, &dev);
    bmm350_error_codes_print_result("bmm350_enable_interrupt", rslt);

    /* Get interrupt settings */
    rslt = bmm350_get_regs(BMM350_REG_INT_CTRL, &int_ctrl, 1, &dev);
    bmm350_error_codes_print_result("bmm350_get_regs", rslt);

    set_int_ctrl = ((BMM350_INT_POL_ACTIVE_HIGH << 1) | (BMM350_INT_OD_PUSHPULL << 2) | BMM350_ENABLE << 7);

    printf("Expected : 0x2E : Interrupt control : 0x%X\n", set_int_ctrl);
    printf("Read : 0x2E : Interrupt control : 0x%X\n", int_ctrl);

    if (int_ctrl & BMM350_DRDY_DATA_REG_EN_MSK)
    {
        printf("Data ready enabled\r\n");
    }

    printf("Compensated Magnetometer and Temperature data in forced mode and forced mode fast\n");

    printf("\nCOMBINATION 1 :\n");
    printf("Set forced mode fast and read data with averaging between 4 samples\n");

    /* Set ODR and performance */
    rslt = bmm350_set_odr_performance(BMM350_DATA_RATE_100HZ, BMM350_AVERAGING_4, &dev);
    bmm350_error_codes_print_result("bmm350_set_odr_performance", rslt);

    /* Enable all axis */
    rslt = bmm350_enable_axes(BMM350_X_EN, BMM350_Y_EN, BMM350_Z_EN, &dev);
    bmm350_error_codes_print_result("bmm350_enable_axes", rslt);

    if (rslt == BMM350_OK)
    {
        rslt = bmm350_set_powermode(BMM350_FORCED_MODE_FAST, &dev);
        bmm350_error_codes_print_result("bmm350_set_powermode", rslt);

        printf("Timestamp(ms), Mag_X(uT), Mag_Y(uT), Mag_Z(uT), Temperature(degC)\n");

        /* Time in milliseconds */
        time_ms = coines_get_millis();

        while (loop)
        {
            rslt = bmm350_get_compensated_mag_xyz_temp_data(&mag_temp_data, &dev);
            bmm350_error_codes_print_result("bmm350_get_compensated_mag_xyz_temp_data", rslt);

            printf("%lu, %f, %f, %f, %f\n",
                   (long unsigned int)(coines_get_millis() - time_ms),
                   mag_temp_data.x,
                   mag_temp_data.y,
                   mag_temp_data.z,
                   mag_temp_data.temperature);

            loop--;
        }

        loop = 10;

        printf("\nCOMBINATION 2 :\n");
        printf("Set forced mode fast and read data with averaging between 4 samples in a loop\n");

        /* Set ODR and performance */
        rslt = bmm350_set_odr_performance(BMM350_DATA_RATE_100HZ, BMM350_AVERAGING_4, &dev);
        bmm350_error_codes_print_result("bmm350_set_odr_performance", rslt);

        printf("Timestamp(ms), Mag_X(uT), Mag_Y(uT), Mag_Z(uT), Temperature(degC)\n");

        /* Time in milliseconds */
        time_ms = coines_get_millis();

        while (loop--)
        {
            rslt = bmm350_set_powermode(BMM350_FORCED_MODE_FAST, &dev);
            bmm350_error_codes_print_result("bmm350_set_powermode", rslt);

            rslt = bmm350_get_compensated_mag_xyz_temp_data(&mag_temp_data, &dev);
            bmm350_error_codes_print_result("bmm350_get_compensated_mag_xyz_temp_data", rslt);

            printf("%lu, %f, %f, %f, %f\n",
                   (long unsigned int)(coines_get_millis() - time_ms),
                   mag_temp_data.x,
                   mag_temp_data.y,
                   mag_temp_data.z,
                   mag_temp_data.temperature);
        }

        loop = 10;

        printf("\nCOMBINATION 3 :\n");
        printf("Set forced mode and read data with no averaging between samples in a loop\n");

        /* Set ODR and performance */
        rslt = bmm350_set_odr_performance(BMM350_DATA_RATE_100HZ, BMM350_NO_AVERAGING, &dev);
        bmm350_error_codes_print_result("bmm350_set_odr_performance", rslt);

        printf("Timestamp(ms), Mag_X(uT), Mag_Y(uT), Mag_Z(uT), Temperature(degC)\n");

        /* Time in milliseconds */
        time_ms = coines_get_millis();

        while (loop--)
        {
            rslt = bmm350_set_powermode(BMM350_FORCED_MODE, &dev);
            bmm350_error_codes_print_result("bmm350_set_powermode", rslt);

            rslt = bmm350_get_compensated_mag_xyz_temp_data(&mag_temp_data, &dev);
            bmm350_error_codes_print_result("bmm350_get_compensated_mag_xyz_temp_data", rslt);

            printf("%lu, %f, %f, %f, %f\n",
                   (long unsigned int)(coines_get_millis() - time_ms),
                   mag_temp_data.x,
                   mag_temp_data.y,
                   mag_temp_data.z,
                   mag_temp_data.temperature);
        }

        printf("\nCOMBINATION 4 :\n");
        printf("Set forced mode fast and read data with averaging between 4 samples in a loop\n");

        /* Set ODR and performance */
        rslt = bmm350_set_odr_performance(BMM350_DATA_RATE_100HZ, BMM350_AVERAGING_4, &dev);
        bmm350_error_codes_print_result("bmm350_set_odr_performance", rslt);

        printf("Timestamp(ms), Mag_X(uT), Mag_Y(uT), Mag_Z(uT), Temperature(degC)\n");

        /* Time in milliseconds */
        time_ms = coines_get_millis();

        for (loop = 0; loop < MAG_SAMPLE_COUNT;)
        {
            rslt = bmm350_set_powermode(BMM350_FORCED_MODE_FAST, &dev);
            bmm350_error_codes_print_result("bmm350_set_powermode", rslt);

            rslt = bmm350_get_compensated_mag_xyz_temp_data(&get_mag_temp_data[loop], &dev);
            bmm350_error_codes_print_result("bmm350_get_compensated_mag_xyz_temp_data", rslt);

            printf("%lu, %f, %f, %f, %f\n",
                   (long unsigned int)(coines_get_millis() - time_ms),
                   get_mag_temp_data[loop].x,
                   get_mag_temp_data[loop].y,
                   get_mag_temp_data[loop].z,
                   get_mag_temp_data[loop].temperature);

            mean_mag_data_1.x += get_mag_temp_data[loop].x;
            mean_mag_data_1.y += get_mag_temp_data[loop].y;
            mean_mag_data_1.z += get_mag_temp_data[loop].z;

            loop++;
        }

        /* Taking average values to calculate percentage deviation */
        avg_mag_data.x = (double)(mean_mag_data_1.x / MAG_SAMPLE_COUNT);
        avg_mag_data.y = (double)(mean_mag_data_1.y / MAG_SAMPLE_COUNT);
        avg_mag_data.z = (double)(mean_mag_data_1.z / MAG_SAMPLE_COUNT);

        printf("***** AVERAGE MAG VALUE *****\n");
        printf("Average_Mag_X(uT), Average_Mag_Y(uT), Average_Mag_Z(uT)\n");

        printf("%lf, %lf, %lf\n", avg_mag_data.x, avg_mag_data.y, avg_mag_data.z);

        calculate_noise(get_mag_temp_data, avg_mag_data);

        printf("\nCOMBINATION 5 :\n");
        printf("Set forced mode and read data with no averaging between samples in a loop\n");

        /* Set ODR and performance */
        rslt = bmm350_set_odr_performance(BMM350_DATA_RATE_100HZ, BMM350_NO_AVERAGING, &dev);
        bmm350_error_codes_print_result("bmm350_set_odr_performance", rslt);

        printf("Timestamp(ms), Mag_X(uT), Mag_Y(uT), Mag_Z(uT), Temperature(degC)\n");

        /* Time in milliseconds */
        time_ms = coines_get_millis();

        for (loop = 0; loop < MAG_SAMPLE_COUNT;)
        {
            rslt = bmm350_set_powermode(BMM350_FORCED_MODE, &dev);
            bmm350_error_codes_print_result("bmm350_set_powermode", rslt);

            rslt = bmm350_get_compensated_mag_xyz_temp_data(&get_mag_temp_data[loop], &dev);
            bmm350_error_codes_print_result("bmm350_get_compensated_mag_xyz_temp_data", rslt);

            printf("%lu, %f, %f, %f, %f\n",
                   (long unsigned int)(coines_get_millis() - time_ms),
                   get_mag_temp_data[loop].x,
                   get_mag_temp_data[loop].y,
                   get_mag_temp_data[loop].z,
                   get_mag_temp_data[loop].temperature);

            mean_mag_data_2.x += get_mag_temp_data[loop].x;
            mean_mag_data_2.y += get_mag_temp_data[loop].y;
            mean_mag_data_2.z += get_mag_temp_data[loop].z;

            loop++;
        }

        /* Taking average values to calculate percentage deviation */
        avg_mag_data.x = (double)(mean_mag_data_2.x / MAG_SAMPLE_COUNT);
        avg_mag_data.y = (double)(mean_mag_data_2.y / MAG_SAMPLE_COUNT);
        avg_mag_data.z = (double)(mean_mag_data_2.z / MAG_SAMPLE_COUNT);

        printf("***** AVERAGE MAG VALUE *****\n");
        printf("Average_Mag_X(uT), Average_Mag_Y(uT), Average_Mag_Z(uT)\n");

        printf("%lf, %lf, %lf\n", avg_mag_data.x, avg_mag_data.y, avg_mag_data.z);

        calculate_noise(get_mag_temp_data, avg_mag_data);

        printf("\nCOMBINATION 6 :\n");
        printf("Set forced mode fast and read data with averaging between 2 samples in a loop\n");

        /* Set ODR and performance */
        rslt = bmm350_set_odr_performance(BMM350_DATA_RATE_100HZ, BMM350_AVERAGING_2, &dev);
        bmm350_error_codes_print_result("bmm350_set_odr_performance", rslt);

        printf("Timestamp(ms), Mag_X(uT), Mag_Y(uT), Mag_Z(uT), Temperature(degC)\n");

        /* Time in milliseconds */
        time_ms = coines_get_millis();

        for (loop = 0; loop < MAG_SAMPLE_COUNT;)
        {
            rslt = bmm350_set_powermode(BMM350_FORCED_MODE_FAST, &dev);
            bmm350_error_codes_print_result("bmm350_set_powermode", rslt);

            rslt = bmm350_get_compensated_mag_xyz_temp_data(&get_mag_temp_data[loop], &dev);
            bmm350_error_codes_print_result("bmm350_get_compensated_mag_xyz_temp_data", rslt);

            printf("%lu, %f, %f, %f, %f\n",
                   (long unsigned int)(coines_get_millis() - time_ms),
                   get_mag_temp_data[loop].x,
                   get_mag_temp_data[loop].y,
                   get_mag_temp_data[loop].z,
                   get_mag_temp_data[loop].temperature);

            mean_mag_data_3.x += get_mag_temp_data[loop].x;
            mean_mag_data_3.y += get_mag_temp_data[loop].y;
            mean_mag_data_3.z += get_mag_temp_data[loop].z;

            loop++;
        }

        /* Taking average values to calculate percentage deviation */
        avg_mag_data.x = (double)(mean_mag_data_3.x / MAG_SAMPLE_COUNT);
        avg_mag_data.y = (double)(mean_mag_data_3.y / MAG_SAMPLE_COUNT);
        avg_mag_data.z = (double)(mean_mag_data_3.z / MAG_SAMPLE_COUNT);

        printf("***** AVERAGE MAG VALUE *****\n");
        printf("Average_Mag_X(uT), Average_Mag_Y(uT), Average_Mag_Z(uT)\n");

        printf("%lf, %lf, %lf\n", avg_mag_data.x, avg_mag_data.y, avg_mag_data.z);

        calculate_noise(get_mag_temp_data, avg_mag_data);
    }

    bmm350_coines_deinit();

    return rslt;
}

/*!
 *  @brief This internal API is to calculate noise level for mag data.
 */
static void calculate_noise(const struct bmm350_mag_temp_data *mag_temp_data, struct bmm350_mag_data avg_mag_data)
{
    double variance_x = 0, variance_y = 0, variance_z = 0;
    double noise_level_x, noise_level_y, noise_level_z;
    uint8_t index = 0;

    for (index = 0; index < MAG_SAMPLE_COUNT; index++)
    {
        variance_x += ((mag_temp_data[index].x - avg_mag_data.x) * (mag_temp_data[index].x - avg_mag_data.x));

        variance_y += ((mag_temp_data[index].y - avg_mag_data.y) * (mag_temp_data[index].y - avg_mag_data.y));

        variance_z += ((mag_temp_data[index].z - avg_mag_data.z) * (mag_temp_data[index].z - avg_mag_data.z));
    }

    variance_x /= MAG_SAMPLE_COUNT;
    noise_level_x = sqrt(variance_x) * 1000;

    variance_y /= MAG_SAMPLE_COUNT;
    noise_level_y = sqrt(variance_y) * 1000;

    variance_z /= MAG_SAMPLE_COUNT;
    noise_level_z = sqrt(variance_z) * 1000;

    printf("\nNoise level x (nTrms), Noise level y (nTrms), Noise level z (nTrms)\n");

    printf("%lf, %lf, %lf\n", noise_level_x, noise_level_y, noise_level_z);
}
