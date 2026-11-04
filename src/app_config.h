#ifndef ENERGY_APP_CONFIG_H
#define ENERGY_APP_CONFIG_H
/* 12V固定负载：5Ω约28.8W，10Ω约14.4W，取中间的21.6W作为分界。 */
#define MONITOR_THRESHOLD_MW 21600U
/* 从首次低功率采样开始连续计时30秒，达到时长后直接断电。 */
#define MONITOR_STANDBY_MS 30000U
/* 有效采样间隔超过500ms时关闭负载，避免沿用过期读数继续计时。 */
#define MONITOR_MAX_SAMPLE_GAP_MS 500U
/* ADC参考为MCU的3.3V电源，实物调试时可按实测值修正。 */
#define ADC_REFERENCE_UV 3300000
#define ACS712_ZERO_DEADBAND_UA 50000
#endif
