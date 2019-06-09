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
  bool poll(bContext *C) override;

  /// Wheter the Operator overrides the cursor drawing or not
  bool overridesCursor() override;

  /// Invoke the operator 
  VR_OPERATOR_STATE invoke(bContext *C, VR_Event *event) override;

  /// Stop the operator
  int addStroke(bContext *C);

  /// Draw the operator
  void draw(bContext *C, float viewProj[4][4]) override;

private:
  float m_color[4];
  std::vector<bGPDspoint> m_points; // Stroke 3d points
  RNG *m_rng;

  void drawStroke(bContext *C, float viewProj[4][4]);
  void drawCursor(bContext *C, float viewProj[4][4]);

  Brush* getBrush(bContext *C);
  RNG* getRNG();
};

#ifdef __cplusplus
}
#endif

#endif
