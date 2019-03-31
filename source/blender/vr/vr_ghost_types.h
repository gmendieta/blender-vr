
#ifndef __VR_GHOST_TYPES_H__
#define __VR_GHOST_TYPES_H__

typedef enum _VR_GHOST_TButtonMask {
	VR_GHOST_kButtonMaskLeft = 0,
	VR_GHOST_kButtonMaskMiddle,
	VR_GHOST_kButtonMaskRight,
	VR_GHOST_kButtonNumMasks
} VR_GHOST_TButtonMask;

typedef enum _VR_GHOST_TKey {
	VR_GHOST_kKeyDownArrow,
	VR_GHOST_kKeyUpArrow,
	VR_GHOST_kKeyLeftArrow,
	VR_GHOST_kKeyRightArrow
} VR_GHOST_TKey;

typedef enum _VR_GHOST_TEventType {
	VR_GHOST_kEventCursorMove,     /// Mouse move event
	VR_GHOST_kEventButtonDown,     /// Mouse button event
	VR_GHOST_kEventButtonUp,       /// Mouse button event
	//VR_GHOST_kEventWheel,          /// Mouse wheel event

	VR_GHOST_kEventKeyDown,
	VR_GHOST_kEventKeyUp,
	//VR_THOST_kEventNumTypes
} VR_GHOST_TEventType;

// This class stores VR events that will be injected into GHOST_WindowManager
struct VR_GHOST_Event
{
public:
	VR_GHOST_Event(VR_GHOST_TEventType type)
		: m_type(type)
	{
	}

	/**
	* Returns the event type.
	* \return The event type.
	*/
	VR_GHOST_TEventType getType()
	{
		return m_type;
	}

protected:
	/** Type of this event. */
	VR_GHOST_TEventType m_type;
};


struct VR_GHOST_EventCursor: public VR_GHOST_Event
{
public:
	VR_GHOST_EventCursor(VR_GHOST_TEventType type, int x, int y):
		VR_GHOST_Event(type),
		m_x(x),
		m_y(y)
	{
	}

	int getX()
	{
		return m_x;
	}

	int getY()
	{
		return m_y;
	}

protected:
	int m_x;
	int m_y;
};

struct VR_GHOST_EventButton : public VR_GHOST_Event
{
public:
	VR_GHOST_EventButton(VR_GHOST_TEventType type, VR_GHOST_TButtonMask buttonMask) :
		VR_GHOST_Event(type),
		m_buttonMask(buttonMask)
	{
	}

	VR_GHOST_TButtonMask getButtonMask() const
	{
		return m_buttonMask;
	}
protected:
	VR_GHOST_TButtonMask m_buttonMask;
};

struct VR_GHOST_EventKey : public VR_GHOST_Event
{
	VR_GHOST_EventKey(VR_GHOST_TEventType type, VR_GHOST_TKey key):
		VR_GHOST_Event(type),
		m_key(key)
	{
	}

	VR_GHOST_TKey getKey() const
	{
		return m_key;
	}
protected:
	VR_GHOST_TKey m_key;
};

#endif
