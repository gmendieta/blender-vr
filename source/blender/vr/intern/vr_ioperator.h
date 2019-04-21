#ifndef __VR_ITOOL_H__
#define __VR_ITOOL_H__

#include "vr_types.h"

extern "C"
{

// Forward
struct bContext;

class VR_IOperator
{
public:

  /// Wheter the Operator is suitable for the current State
  virtual bool isSuitable(bContext *C) = 0;

  /// Invoke the operator 
  virtual int invoke(bContext *C, VR_Event *event) = 0;

  /// Stop the operator
  virtual int finish(bContext *C) = 0;

  /// Draw the operator
  virtual int draw(float viewProj[4][4]) = 0;
};

}

#endif
