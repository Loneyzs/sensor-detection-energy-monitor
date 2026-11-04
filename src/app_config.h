#ifndef ENERGY_APP_CONFIG_H
#define ENERGY_APP_CONFIG_H
/* 12V、0.1Ω分流：5Ω负载约27.7W，10Ω约14.1W，以21W作为分界。 */
#define MONITOR_THRESHOLD_MW 21000U
/* 从首次低功率采样开始连续计时30秒，达到时长后直接断电。 */
#define MONITOR_STANDBY_MS 30000U
/* 有效采样间隔超过500ms时关闭负载，避免沿用过期读数继续计时。 */
#define MONITOR_MAX_SAMPLE_GAP_MS 500U
#endif
