#ifndef __VR_OCULUS_H__
#define __VR_OCULUS_H__

#include "LibOVR/OVR_CAPI_GL.h"

#include "vr_types.h"

class VR_Oculus
{
public:

	typedef struct _HeadInfo
	{
		float mPosition[3];					// World space Translation
		float mRotation[4];					// World space Rotation (Quaternion)
	}HeadInfo;

	typedef struct _EyeInfo
	{
		float mPosition[3];					// World space Translation
		float mRotation[4];					// World space Rotation (Quaternion)
		float mProjection[4];				// UpTan, DownTan, LeftTan, RightTan
		int mTextureSize[2];				// Recommended Texture Size (Width, Height)
		ovrTextureSwapChain mTextureChain;	// Texture Buffer per Eye
		ovrEyeRenderDesc mRenderDesc;
	}EyeInfo;

	typedef struct _HmdInfo
	{
		HeadInfo mHead;
		EyeInfo mEye[2];
		VR_ControllerState mController[2];
	}HmdInfo;


	VR_Oculus();
	~VR_Oculus();

	int initialize(void *device, void *context);
	void unintialize();
	
	void beginFrame();
	void endFrame();

  /// Get Last error string
  void getErrorMessage(char errorMessage[512]);
	/// Recenters the Origin
	int recenterTrackingOrigin();
	/// Set the Tracking origin type. It may be Floor or Eye
	int setTrackingOrigin(VR_TrackingOrigin type);
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
  ovrErrorInfo mErrorInfo;
};


#endif
