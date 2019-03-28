#ifndef __VR_UI_WINDOW_H__
#define __VR_UI_WINDOW_H__

#ifdef __cplusplus
extern "C"
{
#endif

struct GPUBatch;
struct GPUOffScreen;

class VR_UI_Window
{
public:
	VR_UI_Window();
	~VR_UI_Window();

	void draw(float viewProj[4][4]);

	struct GPUOffScreen* getOffscreen();

	/// Set the size of the ofscreen. Redo if necessary
	void setSize(int width, int height);

	/// Get the transformation matrix
	void getMatrix(float matrix[4][4]) const;

	/// Set the transformation matrix
	void setMatrix(float matrix[4][4]);

	/// Return the size of the window
	void getSize(int *width, int *height) const;

	/// Return the aspect of the window
	float getAspect() const;

	/// Intersect a Ray to the window
	bool intersectRay(float rayOrigin[3], float rayDir[3], float hitResult[3]) const;


protected:
	int m_width;
	int m_height;
	struct GPUBatch *m_batch;
	struct GPUOffScreen *m_offscreen;
	float m_matrix[4][4];		// Matrix in Blender space
};

#ifdef __cplusplus
}
#endif


#endif
