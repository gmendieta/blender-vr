#include "vr_oculus.h"

#include "LibOVR/Extras/OVR_Math.h"
#include "LibOVR/OVR_CAPI_GL.h"

#include <string.h>	// memcpy

using namespace OVR;

VR_Oculus::VR_Oculus():
	initialized(false),
	mFrame(0),
	mHmd(0)							// nullptr?
{
	mErrorInfo.Result = ovrSuccess;
	memset(&mInfo, 0, sizeof(mInfo));
	memset(&mLayer, 0, sizeof(mLayer));
}

VR_Oculus::~VR_Oculus()
{
	unintialize();
}

int VR_Oculus::initialize(void * device, void * context)
{
	// We could use initParams if necessary
	ovrInitParams initParams = {};
	ovrResult result = ovr_Initialize(nullptr);
	if (OVR_FAILURE(result)) {
		ovr_GetLastErrorInfo(&mErrorInfo);
		return -1;
	}
	
	result = ovr_Create(&mHmd, &mGraphicsLuid);
	if (OVR_FAILURE(result)) {
		ovr_GetLastErrorInfo(&mErrorInfo);
		return -1;
	}
	
	mHmdDesc = ovr_GetHmdDesc(mHmd);

	// Eye Parameters
	ovrEyeRenderDesc eyeRenderDesc[2];
	eyeRenderDesc[0] = ovr_GetRenderDesc(mHmd, ovrEye_Left, mHmdDesc.DefaultEyeFov[0]);
	eyeRenderDesc[1] = ovr_GetRenderDesc(mHmd, ovrEye_Right, mHmdDesc.DefaultEyeFov[1]);

	// Fov parameters for left Eye
	mInfo.mEye[0].mProjection[0] = eyeRenderDesc[0].Fov.UpTan;
	mInfo.mEye[0].mProjection[1] = eyeRenderDesc[0].Fov.DownTan;
	mInfo.mEye[0].mProjection[2] = eyeRenderDesc[0].Fov.LeftTan;
	mInfo.mEye[0].mProjection[3] = eyeRenderDesc[0].Fov.RightTan;

	// Fov parameters for right Eye
	mInfo.mEye[1].mProjection[0] = eyeRenderDesc[1].Fov.UpTan;
	mInfo.mEye[1].mProjection[1] = eyeRenderDesc[1].Fov.DownTan;
	mInfo.mEye[1].mProjection[2] = eyeRenderDesc[1].Fov.LeftTan;
	mInfo.mEye[1].mProjection[3] = eyeRenderDesc[1].Fov.RightTan;

	// Position of Left Eye in Hmd coordinates
	mInfo.mEye[0].mPosition[0] = eyeRenderDesc[0].HmdToEyePose.Position.x;
	mInfo.mEye[0].mPosition[1] = eyeRenderDesc[0].HmdToEyePose.Position.y;
	mInfo.mEye[0].mPosition[2] = eyeRenderDesc[0].HmdToEyePose.Position.z;
	// Position of Right Eye in Hmd coordinates
	mInfo.mEye[1].mPosition[0] = eyeRenderDesc[1].HmdToEyePose.Position.x;
	mInfo.mEye[1].mPosition[1] = eyeRenderDesc[1].HmdToEyePose.Position.y;
	mInfo.mEye[1].mPosition[2] = eyeRenderDesc[1].HmdToEyePose.Position.z;

	// Create a TextureChain for each Eye
	for (int i = 0; i < 2; ++i)
	{
		// Get the recommended Texture size for each eye
		ovrSizei recommendedTextureSize = ovr_GetFovTextureSize(mHmd, ovrEyeType(i), mHmdDesc.DefaultEyeFov[i], 1);
		mInfo.mEye[i].mTextureSize[0] = recommendedTextureSize.w;
		mInfo.mEye[i].mTextureSize[1] = recommendedTextureSize.h;
		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.Width = recommendedTextureSize.w;
		desc.Height = recommendedTextureSize.h;
		desc.ArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleCount = 1;
		desc.StaticImage = false;

		ovrResult result = ovr_CreateTextureSwapChainGL(mHmd, &desc, &mInfo.mEye[i].mTextureChain);
		if (!OVR_SUCCESS(result))
		{
			return -1;
		}
		// Just if needed
		int length;
		ovr_GetTextureSwapChainLength(mHmd, mInfo.mEye[i].mTextureChain, &length);

		mLayer.ColorTexture[i] = mInfo.mEye[i].mTextureChain;
		mLayer.Viewport[i] = { {0, 0}, {recommendedTextureSize.w, recommendedTextureSize.h} };
		mLayer.Fov[i] = mHmdDesc.DefaultEyeFov[i];
	}
	mLayer.Header.Type = ovrLayerType_EyeFov;
	mLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;

	initialized = true;
	return 0;
}

void VR_Oculus::unintialize()
{
	if (initialized)
	{
		if (mHmd)
		{
			ovr_DestroyTextureSwapChain(mHmd, mInfo.mEye[0].mTextureChain);
			ovr_DestroyTextureSwapChain(mHmd, mInfo.mEye[1].mTextureChain);
			ovr_Destroy(mHmd);
			mHmd = 0;				// nullptr?
		}
		ovr_Shutdown();
		initialized = false;
	}
}

void VR_Oculus::beginFrame()
{
	++mFrame;

	// TODO Could this description be initialized only once??
	ovrEyeRenderDesc eyeRenderDesc[2];
	eyeRenderDesc[0] = ovr_GetRenderDesc(mHmd, ovrEye_Left, mHmdDesc.DefaultEyeFov[0]);
	eyeRenderDesc[1] = ovr_GetRenderDesc(mHmd, ovrEye_Right, mHmdDesc.DefaultEyeFov[1]);
	// Get offset from Hmd to Eyes
	ovrPosef hmdToEyeOffset[2] = { eyeRenderDesc[0].HmdToEyePose, eyeRenderDesc[1].HmdToEyePose };

	double sensorSampleTime;
	ovrPosef eyeRenderPose[2];
	ovr_GetEyePoses(mHmd, mFrame, ovrTrue, hmdToEyeOffset, eyeRenderPose, &sensorSampleTime);

	// Update Layer
	mLayer.RenderPose[0] = eyeRenderPose[0];
	mLayer.RenderPose[1] = eyeRenderPose[1];
	mLayer.SensorSampleTime = sensorSampleTime;

	// Update Eyes position and orientation
	double predictedDisplayTime = ovr_GetPredictedDisplayTime(mHmd, mFrame);
	ovrTrackingState hmdState = ovr_GetTrackingState(mHmd, predictedDisplayTime, ovrTrue);
	ovr_CalcEyePoses(hmdState.HeadPose.ThePose, hmdToEyeOffset, eyeRenderPose);


	/////////////////////////////////////
	// Head state
	/////////////////////////////////////

	mInfo.mHead.mPosition[0] = hmdState.HeadPose.ThePose.Position.x;
	mInfo.mHead.mPosition[1] = hmdState.HeadPose.ThePose.Position.y;
	mInfo.mHead.mPosition[2] = hmdState.HeadPose.ThePose.Position.z;

	mInfo.mHead.mRotation[0] = hmdState.HeadPose.ThePose.Orientation.x;
	mInfo.mHead.mRotation[1] = hmdState.HeadPose.ThePose.Orientation.y;
	mInfo.mHead.mRotation[2] = hmdState.HeadPose.ThePose.Orientation.z;
	mInfo.mHead.mRotation[3] = hmdState.HeadPose.ThePose.Orientation.w;

	/////////////////////////////////////
	// Eyes state
	/////////////////////////////////////

	// Left Eye
	mInfo.mEye[0].mPosition[0] = mLayer.RenderPose[0].Position.x;
	mInfo.mEye[0].mPosition[1] = mLayer.RenderPose[0].Position.y;
	mInfo.mEye[0].mPosition[2] = mLayer.RenderPose[0].Position.z;

	mInfo.mEye[0].mRotation[0] = mLayer.RenderPose[0].Orientation.x;
	mInfo.mEye[0].mRotation[1] = mLayer.RenderPose[0].Orientation.y;
	mInfo.mEye[0].mRotation[2] = mLayer.RenderPose[0].Orientation.z;
	mInfo.mEye[0].mRotation[3] = mLayer.RenderPose[0].Orientation.w;

	// Right Eye
	mInfo.mEye[1].mPosition[0] = mLayer.RenderPose[1].Position.x;
	mInfo.mEye[1].mPosition[1] = mLayer.RenderPose[1].Position.y;
	mInfo.mEye[1].mPosition[2] = mLayer.RenderPose[1].Position.z;

	mInfo.mEye[1].mRotation[0] = mLayer.RenderPose[1].Orientation.x;
	mInfo.mEye[1].mRotation[1] = mLayer.RenderPose[1].Orientation.y;
	mInfo.mEye[1].mRotation[2] = mLayer.RenderPose[1].Orientation.z;
	mInfo.mEye[1].mRotation[3] = mLayer.RenderPose[1].Orientation.w;

	/////////////////////////////////////
	// Controllers state
	/////////////////////////////////////

	// Just modify the availability state
	mInfo.mController[VR_SIDE_LEFT].mEnabled = false;
	mInfo.mController[VR_SIDE_RIGHT].mEnabled = false;
	
	if (hmdState.HandStatusFlags[ovrHand_Left] & ovrStatus_PositionTracked)
	{
		mInfo.mController[VR_SIDE_LEFT].mEnabled = true;
		// Position
		mInfo.mController[VR_SIDE_LEFT].mPosition[0] = hmdState.HandPoses[ovrHand_Left].ThePose.Position.x;
		mInfo.mController[VR_SIDE_LEFT].mPosition[1] = hmdState.HandPoses[ovrHand_Left].ThePose.Position.y;
		mInfo.mController[VR_SIDE_LEFT].mPosition[2] = hmdState.HandPoses[ovrHand_Left].ThePose.Position.z;
		// Orientation
		mInfo.mController[VR_SIDE_LEFT].mRotation[0] = hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.x;
		mInfo.mController[VR_SIDE_LEFT].mRotation[1] = hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.y;
		mInfo.mController[VR_SIDE_LEFT].mRotation[2] = hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.z;
		mInfo.mController[VR_SIDE_LEFT].mRotation[3] = hmdState.HandPoses[ovrHand_Left].ThePose.Orientation.w;

		ovrInputState inputState;
		ovr_GetInputState(mHmd, ovrControllerType_LTouch, &inputState);

		// Thumb Stick Movement
		ovrVector2f &thumbStickMovement = inputState.Thumbstick[ovrHand_Left];
		mInfo.mController[VR_SIDE_LEFT].mThumbstick[0] = thumbStickMovement.x;
		mInfo.mController[VR_SIDE_LEFT].mThumbstick[1] = thumbStickMovement.y;

		// Triggers
		mInfo.mController[VR_SIDE_LEFT].mIndexTrigger = inputState.IndexTrigger[ovrHand_Left];
		mInfo.mController[VR_SIDE_LEFT].mHandTrigger = inputState.HandTrigger[ovrHand_Left];

		// Buttons
		uint64_t &buttons = mInfo.mController[VR_SIDE_LEFT].mButtons;
		buttons = 0;
		if (inputState.Buttons & ovrTouch_X)
			buttons |= VR_BUTTON_X;

		if (inputState.Buttons & ovrTouch_Y)
			buttons |= VR_BUTTON_Y;

		if (inputState.Buttons & ovrTouch_LThumb)
			buttons |= VR_BUTTON_LTHUMB;

		if (inputState.Buttons & ovrTouch_LThumbRest)
			buttons |= VR_BUTTON_LTHUMB_REST;

		// Seems that does not work properly
		//if (inputState.Buttons & ovrTouch_LIndexTrigger)
		if (inputState.IndexTrigger[ovrHand_Left] > 0.8f)
			buttons |= VR_BUTTON_LINDEX_TRIGGER;

		if (inputState.HandTrigger[ovrHand_Left] > 0.8f)
			buttons |= VR_BUTTON_LHAND_TRIGGER;

		if (inputState.Buttons & ovrButton_Home)
			buttons |= VR_BUTTON_HOME;

		// Thumbstick Swipes
		if (thumbStickMovement.x > 0.8f)
			buttons |= VR_THUMBSTICK_SWIPE_RIGHT;
		else if (thumbStickMovement.x < -0.8f)
			buttons |= VR_THUMBSTICK_SWIPE_LEFT;

		if (thumbStickMovement.y > 0.8f)
			buttons |= VR_THUMBSTICK_SWIPE_UP;
		else if (thumbStickMovement.y < -0.8f)
			buttons |= VR_THUMBSTICK_SWIPE_DOWN;
	}
	if (hmdState.HandStatusFlags[ovrHand_Right] & ovrStatus_PositionTracked)
	{
		mInfo.mController[VR_SIDE_RIGHT].mEnabled = true;
		// Position
		mInfo.mController[VR_SIDE_RIGHT].mPosition[0] = hmdState.HandPoses[ovrHand_Right].ThePose.Position.x;
		mInfo.mController[VR_SIDE_RIGHT].mPosition[1] = hmdState.HandPoses[ovrHand_Right].ThePose.Position.y;
		mInfo.mController[VR_SIDE_RIGHT].mPosition[2] = hmdState.HandPoses[ovrHand_Right].ThePose.Position.z;
		// Orientation
		mInfo.mController[VR_SIDE_RIGHT].mRotation[0] = hmdState.HandPoses[ovrHand_Right].ThePose.Orientation.x;
		mInfo.mController[VR_SIDE_RIGHT].mRotation[1] = hmdState.HandPoses[ovrHand_Right].ThePose.Orientation.y;
		mInfo.mController[VR_SIDE_RIGHT].mRotation[2] = hmdState.HandPoses[ovrHand_Right].ThePose.Orientation.z;
		mInfo.mController[VR_SIDE_RIGHT].mRotation[3] = hmdState.HandPoses[ovrHand_Right].ThePose.Orientation.w;

		ovrInputState inputState;
		ovr_GetInputState(mHmd, ovrControllerType_RTouch, &inputState);

		// Thumb Stick Movement
		ovrVector2f &thumbStickMovement = inputState.Thumbstick[ovrHand_Right];
		mInfo.mController[VR_SIDE_RIGHT].mThumbstick[0] = thumbStickMovement.x;
		mInfo.mController[VR_SIDE_RIGHT].mThumbstick[1] = thumbStickMovement.y;

		// Triggers
		mInfo.mController[VR_SIDE_RIGHT].mIndexTrigger = inputState.IndexTrigger[ovrHand_Right];
		mInfo.mController[VR_SIDE_RIGHT].mHandTrigger = inputState.HandTrigger[ovrHand_Right];

		// Buttons
		uint64_t &buttons = mInfo.mController[VR_SIDE_RIGHT].mButtons;
		buttons = 0;
		if (inputState.Buttons & ovrTouch_A)
			buttons |= VR_BUTTON_A;

		if (inputState.Buttons & ovrTouch_B)
			buttons |= VR_BUTTON_B;

		if (inputState.Buttons & ovrTouch_RThumb)
			buttons |= VR_BUTTON_RTHUMB;

		if (inputState.Buttons & ovrTouch_RThumbRest)
			buttons |= VR_BUTTON_RTHUMB_REST;

		// Seems that does not work properly
		//if (inputState.Buttons & ovrTouch_LIndexTrigger)
		if (inputState.IndexTrigger[ovrHand_Right] > 0.8f)
			buttons |= VR_BUTTON_RINDEX_TRIGGER;

		if (inputState.HandTrigger[ovrHand_Right] > 0.8f)
			buttons |= VR_BUTTON_RHAND_TRIGGER;

		if (inputState.Buttons & ovrButton_Home)
			buttons |= VR_BUTTON_HOME;

		// Thumbstick Swipes
		if (thumbStickMovement.x > 0.8f)
			buttons |= VR_THUMBSTICK_SWIPE_RIGHT;
		else if (thumbStickMovement.x < -0.8f)
			buttons |= VR_THUMBSTICK_SWIPE_LEFT;

		if (thumbStickMovement.y > 0.8f)
			buttons |= VR_THUMBSTICK_SWIPE_UP;
		else if (thumbStickMovement.y < -0.8f)
			buttons |= VR_THUMBSTICK_SWIPE_DOWN;
	}
}

void VR_Oculus::endFrame()
{
	ovr_CommitTextureSwapChain(mHmd, mInfo.mEye[0].mTextureChain);
	ovr_CommitTextureSwapChain(mHmd, mInfo.mEye[1].mTextureChain);

	ovrLayerHeader *layers = &mLayer.Header;
	ovrResult result = ovr_SubmitFrame(mHmd, mFrame, nullptr, &layers, 1);
	ovrSessionStatus sessionStatus;
	// There are lot of options here. Have a look to piLibs from IQuilez

	if (result == ovrSuccess_NotVisible)
	{
		;
	}
	else if (result == ovrError_DisplayLost)
	{
		;
	}

	ovr_GetSessionStatus(mHmd, &sessionStatus);
	if (sessionStatus.ShouldQuit)
	{
		;
	}
	if (sessionStatus.ShouldRecenter)
	{
		//ovr_ClearShouldRecenterFlag(mHmd); // Ignore the request
		ovr_RecenterTrackingOrigin(mHmd);
	}
}

int VR_Oculus::getEyeFrustumTangents(unsigned int side, float projection[4])
{
	if (!initialized)
	{
		return -1;
	}
	memcpy(projection, mInfo.mEye[side].mProjection, 4 * sizeof(float));
	return 0;
}

void VR_Oculus::getErrorMessage(char errorMessage[512])
{
	if (OVR_FAILURE(mErrorInfo.Result)) {
		strcpy(errorMessage, mErrorInfo.ErrorString);
	}
	else {
		memset(errorMessage, 0, 512 * sizeof(char));
	}
}

int VR_Oculus::recenterTrackingOrigin()
{
	if (!initialized)
	{
		return -1;
	}
	ovr_RecenterTrackingOrigin(mHmd);
	return 0;
}

int VR_Oculus::setTrackingOrigin(VR_TrackingOrigin type)
{
	if (!initialized)
	{
		return -1;
	}

	switch (type)
	{
	case VR_FLOOR_LEVEL:
		ovr_SetTrackingOriginType(mHmd, ovrTrackingOrigin_FloorLevel);
		break;
	case VR_EYE_LEVEL:
		ovr_SetTrackingOriginType(mHmd, ovrTrackingOrigin_EyeLevel);
		break;
	}
	return 0;
}


int VR_Oculus::getHmdTransform(float position[3], float rotation[4])
{
	if (!initialized)
	{
		return -1;
	}
	memcpy(position, mInfo.mHead.mPosition, 3 * sizeof(float));
	memcpy(rotation, mInfo.mHead.mRotation, 4 * sizeof(float));
	return 0;
}

int VR_Oculus::getEyeTransform(unsigned int side, float position[3], float rotation[4])
{
	if (!initialized)
	{
		return -1;
	}
	memcpy(position, mInfo.mEye[side].mPosition, 3 * sizeof(float));
	memcpy(rotation, mInfo.mEye[side].mRotation, 4 * sizeof(float));
	return 0;
}

int VR_Oculus::getEyeTextureIdx(unsigned int side, unsigned int *textureIdx)
{
	if (!initialized)
	{
		return -1;
	}
	int textureChainIdx;
	ovr_GetTextureSwapChainCurrentIndex(mHmd, mInfo.mEye[side].mTextureChain, &textureChainIdx);
	ovr_GetTextureSwapChainBufferGL(mHmd, mInfo.mEye[side].mTextureChain, textureChainIdx, textureIdx);
	return 0;
}

int VR_Oculus::getEyeTextureSize(unsigned int side, int *width, int *height)
{
	if (!initialized)
	{
		return -1;
	}
	*width = mInfo.mEye[side].mTextureSize[0];
	*height = mInfo.mEye[side].mTextureSize[1];
	return 0;
}

int VR_Oculus::getControllerTransform(unsigned int side, float position[3], float rotation[4])
{
	if (!initialized)
	{
		return -1;
	}
	memcpy(position, mInfo.mController[side].mPosition, 3 * sizeof(float));
	memcpy(rotation, mInfo.mController[side].mRotation, 4 * sizeof(float));
	return 0;
}

int VR_Oculus::getControllerState(unsigned int side, void * controllerState)
{
	if (!initialized)
	{
		return -1;
	}
	memcpy(controllerState, &mInfo.mController[side], sizeof(VR_ControllerState));
	return 0;
}


