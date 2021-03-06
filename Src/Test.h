#ifndef __TEST_H__
#define __TEST_H__

#include "inc.h"
#include "FBXHelper.h"
#include "FileDialog.h"

class ProgressiveMeshRenderer;
class BoneRenderer;

class Test
{
public:
	Test();
	~Test();

	void OnInit(HWND hwnd, IDirect3DDevice9* device);

	void OnUpdate();

	void OnGUI();

	void OnQuit();

	void OpenFile(const char* fileName);
public:
	IDirect3DDevice9* mDevice;
	HWND mHwnd;
	int mWidth;
	int mHeight;
	D3DMATERIAL9 material;
	D3DXMATRIX mMatWorld;
	float mRotSpeed;
	DWORD mLastTime;

	ProgressiveMeshRenderer* mMeshRenderer;
	BoneRenderer* mBoneRenderer;
	FileDialog fdOpen;
	FileDialog fdSave;

	int mImGuiID;
	List<int> mDisireVtxNums;
	List<int> mMaxDisireVtxNums;
	bool mShowMesh;
	bool mShowBone;
	int mCurrentAnimIndex;
	float mCameraDistance;
	float mCameraHeight;
	float mCameraX;
	bool mLowpolyStyle;
};

#endif