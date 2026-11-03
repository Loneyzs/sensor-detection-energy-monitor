#ifndef ENERGY_APP_CONFIG_H
#define ENERGY_APP_CONFIG_H
/* 12V固定负载：680Ω约212mW，2kΩ约72mW，取两档中间的140mW作为分界。 */
#define MONITOR_THRESHOLD_MW 140U
/* 从首次低功率采样开始连续计时30秒，达到时长后直接断电。 */
#define MONITOR_STANDBY_MS 30000U
/* 有效采样间隔超过500ms时关闭负载，避免沿用过期读数继续计时。 */
#define MONITOR_MAX_SAMPLE_GAP_MS 500U
#endif
