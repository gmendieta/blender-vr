#ifndef __VR_OCULUS_H__
#define __VR_OCULUS_H__

#include "LibOVR/OVR_CAPI_GL.h"


class VROculus
{
public:

	typedef enum TrackingOrigin
	{
		FloorLevel, EyeLevel
	};

	typedef struct HeadInfo
	{
		float mPosition[3];					// World space Translation
		float mRotation[4];					// World space Rotation (Quaternion)
	};

	typedef struct EyeInfo
	{
		float mPosition[3];					// World space Translation
		float mRotation[4];					// World space Rotation (Quaternion)
		float mProjection[4];				// UpTan, DownTan, LeftTan, RightTan
		int mTextureSize[2];				// Recommended Texture Size (Width, Height)
		ovrTextureSwapChain mTextureChain;	// Texture Buffer per Eye
		ovrEyeRenderDesc mRenderDesc;
	};

	typedef struct ControllerState
	{
		bool mEnabled;
		uint64_t mButtons;					// Buttons Pressed. See vr_bitmask.h for encodings
		float mThumbstick[2];				// ThumbStick vector [-1.0 (Left Bottom), 1.0 (Right Up)
		float mIndexTrigger;				// Index Trigger pressure [0.0, 1.0]
		float mHandTrigger;					// Hand Trigger pressure [0.0, 1.0]
	};

	typedef struct ControllerInfo
	{
		float mPosition[3];
		float mRotation[4];
		ControllerState mState;
	};

	typedef struct HmdInfo
	{
		HeadInfo mHead;
		EyeInfo mEye[2];
		ControllerInfo mController[2];
	};


	VROculus();
	~VROculus();

	int initialize(void *device, void *context);
	void unintialize();
	
	void beginFrame();
	void endFrame();

	
	/// Recenters the Origin
	int recenterTrackingOrigin();
	/// Set the Tracking origin type. It may be Floor or Eye
	int setTrackingOrigin(TrackingOrigin type);
	/// Get the Head transformation matrix
	int getHmdTransform(float position[3], float rotation[4]);
	/// Get the Frustum Tangents values for an Eye. Side may be 0 for Left eye and 1 for Right eye
	int getEyeFrustumTangents(unsigned int side, float projection[4]);
	/// Get an Eye transformation as a Position and a Quaternion. Side may be 0 for Left eye and 1 for Right eye
	int getEyeTransform(unsigned int side, float position[3], float rotation[4]);
	/// Get the Texture index for an eye
	int getEyeTextureIdx(unsigned int side, unsigned int *textureIdx);
	/// Get the Eye Texture Size
	int getEyeTextureSize(unsigned int side, int *width, int *height);
	/// Get the Controller transformation as a Position and a Quaternion
	int getControllerTransform(unsigned int side, float position[3], float rotation[4]);
	/// Get the COntroller State. The return is a type of ControllerState structure
	int getControllerState(unsigned int side, void *controllerState);

private:
	bool initialized;
	uint64_t	mFrame;
	ovrSession	mHmd;
	HmdInfo mInfo;
	ovrGraphicsLuid mGraphicsLuid;
	ovrHmdDesc	mHmdDesc;
	ovrLayerEyeFov mLayer;
};


#endif
