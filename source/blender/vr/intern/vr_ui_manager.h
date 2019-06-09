
#ifndef __VR_UI_MANAGER_H__
#define __VR_UI_MANAGER_H__

#include <deque>

#include "vr_types.h"
#include "vr_event.h"
#include "vr_ui_window.h"

// Operators
class VR_IOperator;
class VR_OP_GPencil;

struct VR_GHOST_Event;

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;
struct wmWindow;
struct ARegion;
struct GPUOffScreen;

class VR_UI_Manager
{
	// Hit data for UI
	typedef struct _VR_UI_HitResult
	{
		bool m_hit;
    float m_rayOrigin[3];   // Ray origin
    float m_rayDir[3];      // Ray direction
		float m_uv[2];
		float m_dist;
		VR_UI_Window *m_window;

		void clear() {
			m_hit = false;
      m_rayOrigin[0] = m_rayOrigin[1] = m_rayOrigin[2] = 0.0f;
      m_rayDir[0] = m_rayDir[2] = 0.0f; m_rayDir[1] = 1.0f;
			m_dist = 0.0f;
			m_uv[0] = m_uv[1] = 0.0f;
			m_window = NULL;
		}
	} VR_UI_HitResult;

	typedef enum _VR_UI_State {
		VR_UI_State_kIdle = 0,
		VR_UI_State_kNavigate,
		VR_UI_State_kMenu,
    VR_UI_State_kOperator,
	} VR_UI_State;

  typedef enum _VR_UI_Visibility {
    VR_UI_Visibility_kVisible = 0,
    VR_UI_Visibility_kHidden,
  } VR_UI_Visibility;

public:
	VR_UI_Manager();
	~VR_UI_Manager();

	/// Set the current state of a controller
	void setControllerState(unsigned int side, const VR_ControllerState &controllerState);

	/// Set the current Head matrix
	void setHeadMatrix(const float matrix[4][4]);

	/// Set the current Eye matrix
	void setEyeMatrix(unsigned int side, const float matrix[4][4]);

	/// Set the Blender built View matrix
	void setViewMatrix(unsigned int side, const float matrix[4][4]);

	/// Set the Blender built Projection matrix
	void setProjectionMatrix(unsigned int side, const float matrix[4][4]);

	/// Process the controller states
	void processUserInput(bContext *C);

	/// Draw GUI before Blender drawing
	void doPreDraw(bContext *C, unsigned int side);

	/// Draw GUI after Blender drawing
	void doPostDraw(bContext *C, unsigned int side);

	/// Get the current navigation matrix
	void getNavMatrix(float matrix[4][4], bool scaled);

	/// Get the current navigation scale
	float getNavScale();

  /// Get a Controller Matrix
  void getControllerMatrix(VR_Side side, float matrix[4][4]);

	/// Store Blender window
	void setBlenderWindow(struct wmWindow *bWindow);

	/// Store Blender ARegion
	void setBlenderARegion(struct ARegion *bARegion);

	/// Update internal textures
	void updateUiTextures();

	/// Returns the first Ghost event
	struct VR_GHOST_Event* popGhostEvent();

	/// Delete Ghost events
	void clearGhostEvents();

private:

	const wmWindow *m_bWindow;
	const ARegion *m_bARegion;
	VR_UI_Window *m_mainMenu;

	VR_UI_State m_state;
  VR_UI_Visibility m_uiVisibility;

	float m_bProjectionMatrix[VR_SIDES_MAX][4][4];		// Blender built Projection matrix
	float m_bViewMatrix[VR_SIDES_MAX][4][4];				  // Blender built View matrix

	float m_viewMatrix[4][4];							          	// Cache view matrix
	float m_viewProjectionMatrix[4][4];   						// Cache view projection matrix

	float m_eyeMatrix[VR_SIDES_MAX][4][4];					  // Eye matrices
	float m_headMatrix[4][4];								          // Current Head matrix
	float m_headInvMatrix[4][4];							        // Current Head inverse matrix

	float m_touchPrevMatrices[VR_SIDES_MAX][4][4];		// Touch controller start matrices
	float m_touchMatrices[VR_SIDES_MAX][4][4];				// Touch controller matrices
	float m_navScale;										              // Navigation scale
	float m_navMatrix[4][4];								          // Accumulated navigation matrix
	float m_navInvMatrix[4][4];								        // Accumulated inverse matrix
	float m_navScaledMatrix[4][4];							      // Accumulated navigation matrix scaled
	float m_navScaledInvMatrix[4][4];						      // Accumulated navigation inverse matrix scaled

	float m_menuPrevMatrix[4][4];							        // Previous Menu matrix for Menu moving

	// GHOST Events
	std::deque<VR_GHOST_Event*> m_events;
	std::deque<VR_GHOST_Event*> m_handledEvents;

	VR_ControllerState m_currentState[VR_SIDES_MAX];		// Current state of controllers
	VR_ControllerState m_previousState[VR_SIDES_MAX];		// Previous state of controllers

	VR_UI_HitResult m_hitResult[VR_SIDES_MAX];					// Hit States

  VR_Event m_event, m_prevEvent;                      // Event and Previous Event
  VR_DrawData m_drawData;                             // Draw data used by Operators
  VR_IOperator *m_currentOp;                          // Invoking operator

  VR_OP_GPencil *m_gpencilOp;

	/// Returns the primary side
	VR_Side getPrimarySide();

	/// Returns the secondary side
	VR_Side getSecondarySide();

	/// Get current Touch controller coordinates in Screen coordinates
	void getTouchScreenCoordinates(unsigned int side, float coords[2]);

  /// Process Menu visibility
  void processMenuVisibility();

	/// Process Menu Ray hits
	void processMenuRayHits();

	/// Process Menu matrix
	void processMenuMatrix();

	/// Process Ghost events over the Menus
	void processMenuGhostEvents();

	/// Process navigation matrix
	void processNavMatrix();

  /// Process VR events. Events will be saved as internal variables
  void processVREvents();

  /// Get the Suitable operator for the current context
  VR_IOperator* getSuitableOperator(bContext *C);

  /// Process VR tools
  void processOperators(bContext *C, VR_Event *event);

	/// Process VR Ghost events
	void processGhostEvents(bContext *C);

	/// Compute a ray from a touch current state
	void computeTouchControllerRay(unsigned int side, VR_Space space, float rayOrigin[3], float rayDir[3]);

	/// Draw Touch controllers in Navigation Space
	void drawTouchControllers();

	/// Draw a Ray in Navigation Space
	void drawRay(float rayOrigin[3], float rayDir[3], float rayLen, float rayColor[4]);

	/// Draw graphical user interface
	void drawUserInterface();

  /// Draw Operators
  void drawOperators(bContext *C);

	/// Ghost Events
	void pushGhostEvent(VR_GHOST_Event *event);
};

#ifdef __cplusplus
}
#endif

#endif // __VR_UI_MANAGER_H__

