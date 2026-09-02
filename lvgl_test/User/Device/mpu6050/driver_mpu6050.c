#include "driver_mpu6050.h"
#include "bsp_iic.h"
#include "inv_mpu.h"
#include "inv_mpu_dmp_motion_driver.h"
#include "math.h"
#include "debug_func.h"
static signed char gyro_orientation[9] = {-1, 0, 0,
                                          0, -1, 0,
                                          0, 0, 1};

/*-------------------------------------------------------------------*/
/* These next two functions converts the orientation matrix (see
 * gyro_orientation) to a scalar representation for use by the DMP.
 * NOTE: These functions are borrowed from Invensense's MPL.
 */
static unsigned short inv_row_2_scale(const signed char *row) {
    unsigned short b;

    if (row[0] > 0)
        b = 0;
    else if (row[0] < 0)
        b = 4;
    else if (row[1] > 0)
        b = 1;
    else if (row[1] < 0)
        b = 5;
    else if (row[2] > 0)
        b = 2;
    else if (row[2] < 0)
        b = 6;
    else
        b = 7; // error
    return b;
}

/*add by miaowlabs*/
unsigned short inv_orientation_matrix_to_scalar(
    const signed char *mtx) {
    unsigned short scalar;

    /*
       XYZ  010_001_000 Identity Matrix
       XZY  001_010_000
       YXZ  010_000_001
       YZX  000_010_001
       ZXY  001_000_010
       ZYX  000_001_010
     */

    scalar = inv_row_2_scale(mtx);
    scalar |= inv_row_2_scale(mtx + 3) << 3;
    scalar |= inv_row_2_scale(mtx + 6) << 6;

    return scalar;
}

/**
 * @brief MPU6050 自检和零偏校准
 * @retval 无
 */
void run_self_test(void) {
    long gyro[3], accel[3];
    int result = mpu_run_self_test(gyro, accel);

    if (result & 0x1) {
        float sens;
        mpu_get_gyro_sens(&sens);
        gyro[0] = (long)(gyro[0] * sens);
        gyro[1] = (long)(gyro[1] * sens);
        gyro[2] = (long)(gyro[2] * sens);
        dmp_set_gyro_bias(gyro);
        RTT_PRINTF("Gyro bias set.\n");
    } else {
        RTT_PRINTF("Gyro self-test failed!\n");
    }

    if (result & 0x2) {
        unsigned short accel_sens;
        mpu_get_accel_sens(&accel_sens);
        accel[0] *= accel_sens;
        accel[1] *= accel_sens;
        accel[2] *= accel_sens;
        dmp_set_accel_bias(accel);
        RTT_PRINTF("Accel bias set.\n");
    } else {
        RTT_PRINTF("Accel self-test failed!\n");
    }

    if ((result & 0x3) == 0) {
        RTT_PRINTF("Both self-tests failed! Check hardware.\n");
        // 可考虑使用备用偏置（从非易失存储器加载）
    }
}

/**
 * @brief MPU6050 初始化
 * @retval SYS_OK: 初始化成功
 */
SYS_StatusTypeDef MPU6050_Init(void) {
    int result = 0;
    result = mpu_init(NULL);
    if (!result) {
        // RTT_PRINTF("mpu initialization complete......\n "); // mpu initialization complete
        ;

        if (!mpu_set_sensors(INV_XYZ_GYRO | INV_XYZ_ACCEL)) // mpu_set_sensor
                                                            // RTT_PRINTF("mpu_set_sensor complete ......\n");
            ;
        else // RTT_PRINTF("mpu_set_sensor come across error ......\n");
            ;

        if (!mpu_configure_fifo(INV_XYZ_GYRO | INV_XYZ_ACCEL)) // mpu_configure_fifo
                                                               // RTT_PRINTF("mpu_configure_fifo complete ......\n");
            ;
        else // RTT_PRINTF("mpu_configure_fifo come across error ......\n");
            ;

        if (!mpu_set_sample_rate(DEFAULT_MPU_HZ)) // mpu_set_sample_rate
                                                  //  RTT_PRINTF("mpu_set_sample_rate complete ......\n");
            ;
        else // RTT_PRINTF("mpu_set_sample_rate error ......\n");
            ;

        if (!dmp_load_motion_driver_firmware()) // dmp_load_motion_driver_firmvare
                                                // RTT_PRINTF("dmp_load_motion_driver_firmware complete ......\n");
            ;
        else // RTT_PRINTF("dmp_load_motion_driver_firmware come across error ......\n");
            ;

        if (!dmp_set_orientation(inv_orientation_matrix_to_scalar(gyro_orientation))) // dmp_set_orientation
                                                                                      // RTT_PRINTF("dmp_set_orientation complete ......\n");
            ;
        else // RTT_PRINTF("dmp_set_orientation come across error ......\n");
            ;

        if (!dmp_enable_feature(DMP_FEATURE_6X_LP_QUAT | DMP_FEATURE_TAP | DMP_FEATURE_ANDROID_ORIENT | DMP_FEATURE_SEND_RAW_ACCEL | DMP_FEATURE_SEND_CAL_GYRO | DMP_FEATURE_GYRO_CAL)) // dmp_enable_feature
                                                                                                                                                                                        // RTT_PRINTF("dmp_enable_feature complete ......\n");
            ;
        else // RTT_PRINTF("dmp_enable_feature come across error ......\n");
            ;

        if (!dmp_set_fifo_rate(DEFAULT_MPU_HZ)) // dmp_set_fifo_rate
                                                // RTT_PRINTF("dmp_set_fifo_rate complete ......\n");
            ;
        else
            ////printf("dmp_set_fifo_rate come across error ......\n");
            ;

        run_self_test(); // ×Ô¼ì

        if (!mpu_set_dmp_state(1))
            ////printf("mpu_set_dmp_state complete ......\n");
            ;
        else
            ////printf("mpu_set_dmp_state come across error ......\n");
            ;
    } else {
        // GPIO_ResetBits(GPIOC, GPIO_Pin_13); // MPU6050状态指示灯，亮起为不正常
        // while (1);
        return SYS_ERROR;
    }
    return SYS_OK;
}

/**
 * @brief MPU6050 获取角度
 * @param pitch: 横向角度
 * @param roll: 纵向角度
 * @param yaw: 偏航角度
 * @retval SYS_OK: 获取成功
 * @retval SYS_ERROR: 获取失败
 */
SYS_StatusTypeDef MPU6050_getAngle(float *pitch, float *roll, float *yaw) {
    float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
    unsigned long sensor_timestamp;
    short gyro[3], accel[3], sensors;
    unsigned char more;
    long quat[4];
    dmp_read_fifo(gyro, accel, quat, &sensor_timestamp, &sensors, &more);

    // attitude_update(0.005f, accX, accY, accZ, gyroX, gyroY, gyroZ); // 0.001s为时间间隔
    if (sensors & INV_WXYZ_QUAT) {
        q0 = quat[0] / q30;
        q1 = quat[1] / q30;
        q2 = quat[2] / q30;
        q3 = quat[3] / q30;

        *roll = asin(-2 * q1 * q3 + 2 * q0 * q2) * 57.3;                                     // pitch
        *pitch = atan2(2 * q2 * q3 + 2 * q0 * q1, -2 * q1 * q1 - 2 * q2 * q2 + 1) * 57.3;    // roll
        *yaw = atan2(2 * (q1 * q2 + q0 * q3), q0 * q0 + q1 * q1 - q2 * q2 - q3 * q3) * 57.3; // yaw
        return SYS_OK;
    }
    return SYS_ERROR;
}
