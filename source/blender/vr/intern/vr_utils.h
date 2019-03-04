#ifndef vr_utils_h
#define vr_utils_h

/// Get a Projection matrix from Frustum tangents, Near and Far
void getGLProjectionMatrix(float t, float b, float l, float r, float n, float f, float m[4][4])
{
	// Standard Right Handed OpenGL projection Matrix
	// http://www.songho.ca/opengl/gl_projectionmatrix.html
	 // Math for Games (Lengyel)

	m[0][0] = 2.0f * n / (r - l);
	m[0][1] = 0.0f;
	m[0][2] = 0.0f;
	m[0][3] = 0.0f;

	m[1][0] = 0.0f;
	m[1][1] = 2.0f * n / (t - b);
	m[1][2] = 0.0f;
	m[1][3] = 0.0f;

	m[2][0] = (r + l) / (r - l);
	m[2][1] = (t + b) / (t - b);
	m[2][2] = -(f + n) / (f - n);
	m[2][3] = -1.0f;

	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = -2.0f * f * n / (f - n);
	m[3][3] = 0.0f;
}

/// Get a View matrix from Quaternion and Position
static void getGLViewMatrix(float q[4], float p[3], float mat[4][4])
{
	// The different methods to convert a Quaternion to a Rotation matrix. We are using first one
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm

	float x2 = q[0] * q[0];
	float y2 = q[1] * q[1];
	float z2 = q[2] * q[2];
	float xy = q[0] * q[1];
	float xz = q[0] * q[2];
	float yz = q[1] * q[2];
	float wx = q[3] * q[0];
	float wy = q[3] * q[1];
	float wz = q[3] * q[2];

	mat[0][0] = 1.0f - 2.f * (y2 + z2);
	mat[1][0] = 2.0f * (xy - wz);
	mat[2][0] = 2.0f * (xz + wy);

	mat[0][1] = 2.0f * (xy + wz);
	mat[1][1] = 1.0f - 2.f * (x2 + z2);
	mat[2][1] = 2.0f * (yz - wx);

	mat[0][2] = 2.0f * (xz - wy);
	mat[1][2] = 2.0f * (yz + wx);
	mat[2][2] = 1.0f - 2.0f * (x2 + y2);

	// Translation
	mat[3][0] = p[0];
	mat[3][1] = p[1];
	mat[3][2] = p[2];

	// Remaining
	mat[0][3] = 0.0f;
	mat[1][3] = 0.0f;
	mat[2][3] = 0.0f;
	mat[3][3] = 1.0f;
}

static void vr_view_matrix_build(float q[4], float p[3], float mat[4][4])
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
	float wx = q[3] * q[0];
	float wy = q[3] * q[1];
	float wz = q[3] * q[2];

	mat[0][0] = 1.0f - 2.0f * (y2 + z2);
	mat[1][0] = 2.0f * (xy - wz);
	mat[2][0] = 2.0f * (xz + wy);

	mat[0][1] = -2.0f * (xz - wy);
	mat[1][1] = -2.0f * (yz + wx);
	mat[2][1] = 2.0f * (x2 + y2) - 1.0f;

	mat[0][2] = 2.0f * (xy + wz);
	mat[1][2] = 1.0f - 2.0f * (x2 + z2);
	mat[2][2] = 2.0f * (yz - wx);

	// Translation
	mat[3][0] = p[0];
	mat[3][1] = -p[2];
	mat[3][2] = p[1];

	// Remaining
	mat[0][3] = 0.0f;
	mat[1][3] = 0.0f;
	mat[2][3] = 0.0f;
	mat[3][3] = 1.0f;
}

#endif
