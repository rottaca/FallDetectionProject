#ifndef SETTINGS_H
#define SETTINGS_H

//#define DAVIS_IMG_WIDHT 240
//#define DAVIS_IMG_HEIGHT 180

// Time settings
// Time window in microseconds
#define TIME_WINDOW_US 100000
// Update intervals for user interface and computations
#define UPDATE_INTERVAL_COMP_US 20000
#define UPDATE_INTERVAL_UI_US 20000
// Timerange of plots
#define PLOT_TIME_RANGE_US 10000000 // 10 sec
// Lowpass filter for smoothing FPS counters
#define FPS_LOWPASS_FILTER_COEFF 0.05



// Tracking and detection settings
// How long has a ROI with lost tracking to be kept alive
#define TRACK_DELAY_KEEP_ROI_US 1000000
// How long has a ROI with lost tracking but possible fall kept alive
#define TRACK_DELAY_KEEP_ROI_FALL_US 5000000
// Sigma for gaussian smoothing of event image
#define TRACK_BOX_DETECTOR_GAUSS_SIGMA 10
// Kernel size
#define TRACK_BOX_DETECTOR_GAUSS_KERNEL_SZ (TRACK_BOX_DETECTOR_GAUSS_SIGMA*2+1)
// Threshold for binarizing the resulting smoothed image
// Lower values expand the contour, higher values are closer to the original shape
#define TRACK_BOX_DETECTOR_THRESHOLD (255*0.05)



// Ratio between overlap of bounding boxes
// and size of old box: How high has the overlap to be
// To match the old bbox
#define TRACK_MIN_OVERLAP_RATIO 0.7
// Optional scaling factor for detected bounding boxes
#define TRACK_BOX_SCALE 1.1
// Minimum area of bouding boxes to remove noise
#define TRACK_MIN_AREA (30*30)
// Assume only N subjects in the scene and remove all smaller boxes before tracking
#define TRACK_BIGGEST_N_BOXES 5
// An object has to be at least with some parts inside the inner image region inside
// the defined boundary, otherwise the detected rectangle is ignored. This reduces false alarms
#define TRACK_IMG_BORDER_SIZE (40)

// Statistics computations
#define STATS_SPEED_SMOOTHING_WINDOW_SZ (150000/UPDATE_INTERVAL_COMP_US)

// Fall detector
// Coordiante system: top -> y = 0, bottom -> y == DAVIS_IMG_HEIGHT
#define FALL_DETECTOR_Y_SPEED_MIN_THRESHOLD (2.8)
#define FALL_DETECTOR_Y_SPEED_MAX_THRESHOLD (5)
#define FALL_DETECTOR_Y_CENTER_THRESHOLD_FALL (2*180/3)
#define FALL_DETECTOR_Y_CENTER_THRESHOLD_UNFALL (3*180/5)


#endif // SETTINGS_H
