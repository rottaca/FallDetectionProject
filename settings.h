#ifndef SETTINGS_H
#define SETTINGS_H

#define DAVIS_IMG_WIDHT 240
#define DAVIS_IMG_HEIGHT 180

// Time settings
// Time window in microseconds
#define TIME_WINDOW_US 100000
// Update intervals for user interface and computations
#define UPDATE_INTERVAL_COMP_US 1000
#define UPDATE_INTERVAL_UI_US 20000
// Timerange of plots
#define PLOT_TIME_RANGE_US 10000000 // 10 sec



// Lowpass filter for smoothing FPS counters
#define FPS_LOWPASS_FILTER_COEFF 0.1

// Disables tracking of multible persons
//#define ASSUME_SINGLE_PERSON

// Tracking and detection settings
#define TRACK_DELAY_KEEP_ROI_US 1000000
// Sigma for gaussian smoothing of event image
#define TRACK_BOX_DETECTOR_GAUSS_SIGMA 10
// Kernel size
#define TRACK_BOX_DETECTOR_GAUSS_KERNEL_SZ (TRACK_BOX_DETECTOR_GAUSS_SIGMA*2+1)
// Threshold for binarizing the resulting smoothed image
// Lower values expand the contour, higher values are closer to the original shape
#define TRACK_BOX_DETECTOR_THRESHOLD (255*0.25)

// Ratio between overlap of bounding boxes
// and size of old box: How high has the overlap to be
// To match the old bbox
#define TRACK_MIN_OVERLAP_RATIO 0.7
// Optional scaling factor for detected bounding boxes
#define TRACK_BOX_SCALE 1.2
// Minimum area of bouding boxes to remove noise
#define TRACK_MIN_AREA (30*30)
// Assume only n subjects in the scene and remove all smaller boxes for tracking
#define TRACK_BIGGEST_N_BOXES 3

// Statistics computations
// minimum events in boudingbox to detect noise
//#define MIN_EVENT_PER_BOX_SIZE_RATIO 0.0
#define STATS_SPEED_SMOOTHING_WINDOW_SZ 300


// Fall detector
// Coordiante system: top -> y = 0, bottom -> y == DAVIS_IMG_HEIGHT
#define FALL_DETECTOR_Y_SPEED_THRESHOLD (2.75)
#define FALL_DETECTOR_Y_CENTER_THRESHOLD_FALL (2*DAVIS_IMG_HEIGHT/3)
#define FALL_DETECTOR_Y_CENTER_THRESHOLD_UNFALL (DAVIS_IMG_HEIGHT/2)


#endif // SETTINGS_H
