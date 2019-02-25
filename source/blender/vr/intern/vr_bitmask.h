#ifndef vr_bitmask_h
#define vr_bitmask_h

#include <stdint.h>

typedef enum VRButtons
{
	/// A button on XBox controllers and right Touch controller. Not present on Oculus Remote.
	VR_BUTTON_A = uint64_t(1) << 0,

	/// B button on XBox controllers and right Touch controller. Not present on Oculus Remote.
	VR_BUTTON_B = uint64_t(1) << 1,

	/// X button on XBox controllers and left Touch controller. Not present on Oculus Remote.
	VR_BUTTON_X = uint64_t(1) << 2,

	/// Y button on XBox controllers and left Touch controller. Not present on Oculus Remote.
	VR_BUTTON_Y = uint64_t(1) << 3,

	/// Home button on XBox controllers. Oculus button on Touch controllers and Oculus Remote.
	VR_BUTTON_HOME = uint64_t(1) << 4,

	/// Right thumbstick on XBox controllers and Touch controllers. Not present on Oculus Remote.
	VR_BUTTON_RTHUMB = uint64_t(1) << 5,

	/// TODO Unknown
	VR_BUTTON_RTHUMB_REST = uint64_t(1) << 6,

	/// Right Index Trigger
	VR_BUTTON_RINDEX_TRIGGER = uint64_t(1) << 7,

	/// Right Handle Trigger
	VR_BUTTON_RHAND_TRIGGER = uint64_t(1) << 8,

	/// Left thumbstick on XBox controllers and Touch controllers. Not present on Oculus Remote.
	VR_BUTTON_LTHUMB = uint64_t(1) << 9,

	/// TODO Unknown
	VR_BUTTON_LTHUMB_REST = uint64_t(1) << 10,

	/// Left Index Trigger
	VR_BUTTON_LINDEX_TRIGGER = uint64_t(1) << 11,

	/// Left Handle Trigger
	VR_BUTTON_LHAND_TRIGGER = uint64_t(1) << 12,

	/// Thumbstick Swipe Left
	VR_THUMBSTICK_SWIPE_LEFT = uint64_t(1) << 13,

	/// Thumbstick Swipe Right
	VR_THUMBSTICK_SWIPE_RIGHT = uint64_t(1) << 14,

	/// Thumbstick Swipe Up
	VR_THUMBSTICK_SWIPE_UP = uint64_t(1) << 15,

	/// Thumbstick Swipe Down
	VR_THUMBSTICK_SWIPE_DOWN = uint64_t(1) << 16,

	/*
	/// Up button on XBox controllers and Oculus Remote. Not present on Touch controllers.
	VR_BUTTON_UP = uint64_t(1) << 6,

	/// Down button on XBox controllers and Oculus Remote. Not present on Touch controllers.
	VR_BUTTON_DOWN = uint64_t(1) << 7,

	/// Left button on XBox controllers and Oculus Remote. Not present on Touch controllers.
	VR_BUTTON_LEFT = uint64_t(1) << 8,

	/// Right button on XBox controllers and Oculus Remote. Not present on Touch controllers.
	VR_BUTTON_RIGHT = uint64_t(1) << 9,
	*/

};

#endif