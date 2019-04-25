#ifndef __VR_OP_GPENCIL_H__
#define __VR_OP_GPENCIL_H__

#include "vr_ioperator.h"

#include <vector>

#ifdef __cplusplus
extern "C"
{
#endif

struct RNG;
struct Brush;
struct bGPDspoint;

class VR_OP_GPencil : VR_IOperator
{
public:
  /// Constructor
  VR_OP_GPencil();

  /// Destructor
  ~VR_OP_GPencil();

  /// Wheter the Operator is suitable for the current State
  bool isSuitable(bContext *C, VR_Event *event) override;

  /// Invoke the operator 
  int invoke(bContext *C, VR_Event *event) override;

  /// Stop the operator
  int finish(bContext *C) override;

  /// Draw the operator
  int draw(float viewProj[4][4]) override;

private:
  float m_color[4];
  std::vector<bGPDspoint> m_points; // Stroke 3d points
  RNG *m_rng;

  Brush* getBrush(bContext *C);
  RNG* getRNG();
};

#ifdef __cplusplus
}
#endif

#endif
