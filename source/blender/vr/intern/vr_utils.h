#ifndef vr_utils_h
#define vr_utils_h

#include "BLI_math_matrix.h"

/// Build a matrix from a Oculus quaternion and vector. Oculus Space
static void vr_oculus_oculus_matrix_build(const float q[4], const float p[3], float matrix[4][4])
{
	// The different methods to convert a Quaternion to a Rotation matrix. We are using first one
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
	// README Note the change of axis Y and Z and the change of sign of Z axis

	float x2 = q[0] * q[0];
	float y2 = q[1] * q[1];
	float z2 = q[2] * q[2];
	float xy = q[0] * q[1];
	float xz = q[0] * q[2];
	float yz = q[1] * q[2];
	float xw = q[3] * q[0];
	float yw = q[3] * q[1];
	float zw = q[3] * q[2];

	// X axis
	matrix[0][0] = 1.0f - 2.0f * y2 - 2.0f * z2;
	matrix[0][1] = 2.0f * xy + 2.0 * zw;
	matrix[0][2] = 2.0f * xz -  2.0f * yw;

	// Y axis
	matrix[1][0] = 2.0f * xy - 2.0f * zw;
	matrix[1][1] = 1.0f - 2.0f * x2 - 2.0f * z2;
	matrix[1][2] = 2.0f * yz + 2.0f * xw;

	// Z axis
	matrix[2][0] = 2.0f * xz +  2.0f * yw;
	matrix[2][1] = 2.0f * yz - 2.0f * xw;
	matrix[2][2] = 1.0f - 2.0f * x2 - 2.0f * y2;

	// Translation
	matrix[3][0] = p[0];
	matrix[3][1] = p[1];
	matrix[3][2] = p[2];

	// Remaining
	matrix[0][3] = 0.0f;
	matrix[1][3] = 0.0f;
	matrix[2][3] = 0.0f;
	matrix[3][3] = 1.0f;
}

/// Transforms a Oculus Pose to a Blender matrix
static void vr_oculus_blender_matrix_build(const float q[4], const float p[3], float matrix[4][4])
{
	// The different methods to convert a Quaternion to a Rotation matrix. We are using first one
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
	// Basically we swap 2nd and 3rd column, 2nd and 3rd row, and negate 3rd column and 3rd row

	float x2 = q[0] * q[0];
	float y2 = q[1] * q[1];
	float z2 = q[2] * q[2];
	float xy = q[0] * q[1];
	float xz = q[0] * q[2];
	float yz = q[1] * q[2];
	float xw = q[3] * q[0];
	float yw = q[3] * q[1];
	float zw = q[3] * q[2];

	// X axis
	matrix[0][0] = 1.0f - 2.0f * y2 - 2.0f * z2;
	matrix[0][1] = -(2.0f * xz - 2.0f * yw);
	matrix[0][2] = 2.0f * xy + 2.0 * zw;

	// Y axis
	matrix[1][0] = 2.0f * xy - 2.0f * zw;
	matrix[1][1] = -(2.0f * yz + 2.0f * xw);
	matrix[1][2] = 1.0f - 2.0f * x2 - 2.0f * z2;

	// Z axis
	matrix[2][0] = 2.0f * xz + 2.0f * yw;
	matrix[2][1] = -(1.0f - 2.0f * x2 - 2.0f * y2);
	matrix[2][2] = 2.0f * yz - 2.0f * xw;

	// Translation
	matrix[3][0] = p[0];
	matrix[3][1] = -p[2];
	matrix[3][2] = p[1];

	// Remaining
	matrix[0][3] = 0;
	matrix[1][3] = 0;
	matrix[2][3] = 0;
	matrix[3][3] = 1;
}


#endif
