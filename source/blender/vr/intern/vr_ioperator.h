#ifndef __VR_IOPERATOR_H__
#define __VR_IOPERATOR_H__

#include "vr_event.h"

extern "C"
{

// Forward
struct bContext;

/* vr operator type return flags */
// This flags are linked with DNA_windowmanager_types.h operator definitions
typedef enum _VR_OPERATOR_STATE {
  VR_OPERATOR_RUNNING = (1 << 0),
  VR_OPERATOR_CANCELLED = (1 << 1),
  VR_OPERATOR_FINISHED = (1 << 2)
} VR_OPERATOR_STATE;

class VR_IOperator
{
public:

  /// Wheter the Operator is suitable for the current State
  virtual bool poll(bContext *C) = 0;

  /// Wheter the Operator overrides the cursor drawing or not
  virtual bool overridesCursor() = 0;

  /// Invoke the operator 
  virtual VR_OPERATOR_STATE invoke(bContext *C, VR_Event *event) = 0;

  /// Draw the operator
  virtual void draw(bContext *C, float viewProj[4][4]) = 0;
};

}

#endif
