#ifndef SETTINGS_H
#define SETTINGS_H

#define DAVIS_IMG_WIDHT 240
#define DAVIS_IMG_HEIGHT 180

// Time settings
// Time window in microseconds
#define TIME_WINDOW_US 30000
// Update intervals for user interface and computations
#define UPDATE_INTERVAL_COMP_US 2000
#define UPDATE_INTERVAL_UI_US 50000
// Timerange of plots
#define PLOT_TIME_RANGE_US 10000000 // 10 sec

#define MIN_EVENT_TRESHOLD 1000


// Lowpass filter for smoothing FPS counters
#define FPS_LOWPASS_FILTER_COEFF 0.1

// Uncomment to simulate event and camera data
#define SIMULATE_CAMERA_INPUT

// Tracking Settings

#define TRACK_DELAY_KEEP_ROI_US 1000000
#define TRACK_OVERLAP_RATIO 0.8
#define TRACK_BOX_DETECTOR_GAUSS_SIGMA 5
#define TRACK_BOX_DETECTOR_GAUSS_KERNEL_SZ 3
#define TRACK_BOX_DETECTOR_THRESHOLD 100
#define TRACK_BOX_SCALE 1.2
#define TRACK_MIN_AREA 100

#endif // SETTINGS_H
