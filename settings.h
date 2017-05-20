#ifndef SETTINGS_H
#define SETTINGS_H

#define DAVIS_IMG_WIDHT 240
#define DAVIS_IMG_HEIGHT 180

// Time settings
// Time window in microseconds
#define TIME_WINDOW_US 30000
// Update intervals for user interface and computations
#define UPDATE_INTERVAL_COMP_US 5000
#define UPDATE_INTERVAL_UI_US 50000
// Timerange of plots
#define PLOT_TIME_RANGE_US 10000000 // 10 sec


#define FPS_LOWPASS_FILTER_COEFF 0.1

#endif // SETTINGS_H
