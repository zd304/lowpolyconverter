#include "Test.h"
#include "ProgressiveMeshRenderer.h"
#include "BoneRenderer.h"
#include "imgui/imgui.h"

Test::Test()
{
	mLastTime = 0;
	mShowMesh = true;
	mShowBone = false;
	mRotSpeed = 0.0f;
	mCurrentAnimIndex = 0;
	mCameraDistance = 400.0f;
	mCameraHeight = 400.0f;
	mCameraX = 0.0f;
	mLowpolyStyle = false;
}

Test::~Test()
{
	mLastTime = 0;
}

void LoadTextures(IDirect3DDevice9* device)
{
	LPDIRECT3DTEXTURE9 p = NULL;
	D3DXCreateTextureFromFileEx(device, "../Image/disk.png",
		D3DX_FROM_FILE,
		D3DX_FROM_FILE,
		D3DX_DEFAULT,
		0,
		D3DFMT_A8R8G8B8,
		D3DPOOL_MANAGED,
		D3DX_FILTER_TRIANGLE,
		D3DX_FILTER_TRIANGLE,
		D3DCOLOR_RGBA(0, 0, 0, 255),
		NULL,
		NULL,
		&p);
	Global::mDiskTexID = (ImTextureID)p;
	p = NULL;
	D3DXCreateTextureFromFileEx(device, "../Image/folder.png",
		D3DX_FROM_FILE,
		D3DX_FROM_FILE,
		D3DX_DEFAULT,
		0,
		D3DFMT_A8R8G8B8,
		D3DPOOL_MANAGED,
		D3DX_FILTER_TRIANGLE,
		D3DX_FILTER_TRIANGLE,
		D3DCOLOR_RGBA(0, 0, 0, 255),
		NULL,
		NULL,
		&p);
	Global::mFolderTexID = (ImTextureID)p;
	p = NULL;
	D3DXCreateTextureFromFileEx(device, "../Image/file.png",
		D3DX_FROM_FILE,
		D3DX_FROM_FILE,
		D3DX_DEFAULT,
		0,
		D3DFMT_A8R8G8B8,
		D3DPOOL_MANAGED,
		D3DX_FILTER_TRIANGLE,
		D3DX_FILTER_TRIANGLE,
		D3DCOLOR_RGBA(0, 0, 0, 255),
		NULL,
		NULL,
		&p);
	Global::mFileTexID = (ImTextureID)p;
}

void Test::OnInit(HWND hwnd, IDirect3DDevice9* device)
{
	mDevice = device;
	mHwnd = hwnd;
	RECT rc;
	::GetClientRect(hwnd, &rc);
	mWidth = (int)(rc.right - rc.left);
	mHeight = (int)(rc.bottom - rc.top);

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("../Font/msyh.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesChinese());
	LoadTextures(device);

	char szPath[MAX_PATH] = { 0 };
	GetCurrentDirectory(MAX_PATH, szPath);
	std::string sCurPath = szPath;
	sCurPath = sCurPath.append("\\");

	fdOpen.ext = "fbx";
	fdOpen.dlgName = "打开模型";
	fdOpen.SetDefaultDirectory(sCurPath);
	fdSave.ext = "fbx";
	fdSave.dlgName = "保存模型";
	fdSave.SetDefaultDirectory(sCurPath);
	fdSave.mUsage = eFileDialogUsage_SaveFile;

	mLastTime = timeGetTime();

	D3DXMATRIX matView, matProj;
	D3DXMatrixPerspectiveFovLH(&matProj, D3DX_PI / 4.0f, (float)mWidth / (float)mHeight, 0.1f, 10000.0f);
	D3DXMatrixLookAtLH(&matView, &D3DXVECTOR3(mCameraX, mCameraHeight, mCameraDistance),
		&D3DXVECTOR3(0.0f, 0.0f, 0.0f), &D3DXVECTOR3(0.0f, 1.0f, 0.0f));
	D3DXMatrixScaling(&mMatWorld, -1.0f, -1.0f, -1.0f);

	device->SetTransform(D3DTS_PROJECTION, &matProj);
	device->SetTransform(D3DTS_VIEW, &matView);
	device->SetTransform(D3DTS_WORLD, &mMatWorld);
	device->SetRenderState(D3DRS_LIGHTING, TRUE);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	D3DLIGHT9 light;
	light.Ambient = D3DXCOLOR(0.7f, 0.7f, 0.7f, 1.0f);
	light.Diffuse = D3DXCOLOR(0.8f, 0.8f, 0.8f, 1.0f);
	light.Direction = D3DXVECTOR3(1.0f, -1.0f, 1.0f);
	light.Type = D3DLIGHTTYPE::D3DLIGHT_DIRECTIONAL;
	device->SetLight(0, &light);
	device->LightEnable(0, TRUE);
	material.Ambient = D3DXCOLOR(0.7f, 0.7f, 0.7f, 1.0f);
	material.Diffuse = D3DXCOLOR(1.0f, 0.1f, 1.0f, 1.0f);
	device->SetMaterial(&material);
	mRotSpeed = 0.0f;
}

void Test::OpenFile(const char* fileName)
{
	OnQuit();

	FBXHelper::BeginFBXHelper(fileName);
	FBXHelper::FbxModelList* models = FBXHelper::GetModelList();
	mDisireVtxNums.Clear();
	mMaxDisireVtxNums.Clear();
	for (int i = 0; i < models->mMeshes.Count(); ++i)
	{
		FBXHelper::FbxModel* model = models->mMeshes[i];
		mDisireVtxNums.Add(model->nVertexCount);
		mMaxDisireVtxNums.Add(model->nVertexCount);
	}

	D3DXVECTOR3 max, min;
	FBXHelper::GetBox(max, min);
	float boxSize = D3DXVec3Length(&(max - min));
	mCameraDistance = 3.0f * boxSize;
	mCameraHeight = 2.0f * boxSize;
	mCameraX = boxSize;
	D3DXMATRIX matView;
	D3DXMatrixLookAtLH(&matView, &D3DXVECTOR3(mCameraX, mCameraHeight, mCameraDistance),
		&D3DXVECTOR3(mCameraX, 0.0f, 0.0f), &D3DXVECTOR3(0.0f, 1.0f, 0.0f));
	mDevice->SetTransform(D3DTS_VIEW, &matView);

	mMeshRenderer = new ProgressiveMeshRenderer(mDevice);
	mMeshRenderer->Collapse(NULL, 1);

	mBoneRenderer = new BoneRenderer(mDevice);
	mBoneRenderer->SetBoneThick(boxSize * 0.01f);
	mBoneRenderer->BuildMesh();

	List<const char*> names;
	if (FBXHelper::GetAniamtionNames(names))
	{
		mCurrentAnimIndex = 0;
		FBXHelper::SetCurrentAnimation(names[mCurrentAnimIndex]);
	}
}

void Test::OnGUI()
{
	mImGuiID = 0;

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(256, (float)mHeight));
	ImGui::Begin(STU("转换器").c_str(), NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar);

	bool openFlag = false;
	bool saveFlag = false;

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(STU("文件").c_str()))
		{
			if (ImGui::MenuItem(STU("打开模型").c_str(), NULL))
			{
				openFlag = true;
			}
			if (ImGui::MenuItem(STU("保存模型").c_str(), (const char*)NULL, (bool*)NULL, mMeshRenderer != NULL))
			{
				//mMeshRenderer->SaveFile("test.fbx");
				saveFlag = true;
			}
			if (ImGui::MenuItem(STU("关闭模型").c_str(), NULL))
			{
				OnQuit();
			}
			ImGui::Separator();
			if (ImGui::MenuItem(STU("退出").c_str(), NULL))
			{
				PostQuitMessage(0);
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	if (openFlag)
	{
		fdOpen.Open();
		openFlag = false;
	}
	if (fdOpen.DoModal())
	{
		std::string path = fdOpen.directory;
		path += fdOpen.fileName;
		OpenFile(path.c_str());
	}
	if (saveFlag)
	{
		fdSave.Open();
		saveFlag = false;
	}
	if (fdSave.DoModal())
	{
		std::string path = fdSave.directory;
		path += fdSave.fileName;
		printf("Save: %s\n", path.c_str());
		mMeshRenderer->SaveFile(path.c_str());
	}

	if (!FBXHelper::IsWorking())
	{
		ImGui::End();
		return;
	}

	FBXHelper::FbxModelList* models = FBXHelper::GetModelList();

	if (mMeshRenderer->mIsSkinnedMesh)
	{
		if (ImGui::CollapsingHeader(STU("蒙皮模型减面").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(15.0f);
			int* collapseParam = new int[models->mMeshes.Count()];
			for (int i = 0; i < models->mMeshes.Count(); ++i)
			{
				ImGui::Text(STU("模型[%d]预期顶点数").c_str(), i + 1);

				ImGui::SameLine();
				ImGui::PushID(mImGuiID++);
				ImGui::PushItemWidth(50);
				ImGui::DragInt(STU("个").c_str(), &mDisireVtxNums[i], 1.0f, 3, mMaxDisireVtxNums[i]);
				ImGui::PopItemWidth();
				ImGui::PopID();

				collapseParam[i] = mDisireVtxNums[i];
			}

			if (ImGui::Button(STU("坍塌").c_str(), ImVec2(100, 30)))
			{
				mMeshRenderer->Collapse(collapseParam, models->mMeshes.Count(), mLowpolyStyle);
			}
			ImGui::SameLine();
			ImGui::Checkbox(STU("lowpoly风格").c_str(), &mLowpolyStyle);
			delete[] collapseParam;
			ImGui::Unindent(15.0f);
		}
	}

	if (ImGui::CollapsingHeader(STU("渲染选项").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(15.0f);
		ImGui::Text(STU("旋转速度").c_str());
		ImGui::SameLine();
		ImGui::SliderFloat("##rotSpeed", &mRotSpeed, 0.0f, 10.0f);
		ImGui::Text(STU("相机距离").c_str());
		ImGui::SameLine();
		float camDist = mCameraDistance;
		ImGui::DragFloat("##mCameraDistance", &camDist);
		ImGui::Text(STU("相机高度").c_str());
		ImGui::SameLine();
		float camHeight = mCameraHeight;
		ImGui::DragFloat("##mCameraHeight", &camHeight);
		ImGui::Text(STU("相机位置").c_str());
		ImGui::SameLine();
		float camX = mCameraX;
		ImGui::DragFloat("##mCameraX", &camX);
		if (camDist != mCameraDistance || camHeight != mCameraHeight || camX != mCameraX)
		{
			mCameraDistance = camDist;
			mCameraHeight = camHeight;
			mCameraX = camX;
			D3DXMATRIX matView;
			D3DXMatrixLookAtLH(&matView, &D3DXVECTOR3(mCameraX, mCameraHeight, mCameraDistance),
				&D3DXVECTOR3(mCameraX, 0.0f, 0.0f), &D3DXVECTOR3(0.0f, 1.0f, 0.0f));
			mDevice->SetTransform(D3DTS_VIEW, &matView);
		}

		ImGui::Checkbox(STU("显示模型").c_str(), &mShowMesh);
		ImGui::Checkbox(STU("显示骨骼").c_str(), &mShowBone);
		if (mShowBone && mBoneRenderer)
		{
			ImGui::Indent(10.0f);
			float thick = mBoneRenderer->GetBoneThick();
			ImGui::Text(STU("骨骼粗细").c_str());
			ImGui::SameLine();
			ImGui::DragFloat("##thick", &thick, 0.05f);
			mBoneRenderer->SetBoneThick(thick);

			bool bindpos = mBoneRenderer->GetShowAnimated();
			ImGui::Checkbox(STU("显示动画").c_str(), &bindpos);
			mBoneRenderer->SetShowAnimated(bindpos);

			ImGui::Unindent(10.0f);
		}
		if (mMeshRenderer->mIsSkinnedMesh)
		{
			List<const char*> names;
			if (FBXHelper::GetAniamtionNames(names))
			{
				int curAnim = mCurrentAnimIndex;
				ImGui::Text(STU("播放动画").c_str());
				ImGui::SameLine();
				ImGui::Combo("##animNames", &curAnim, &(names[0]), names.Count());
				if (curAnim != mCurrentAnimIndex)
				{
					mCurrentAnimIndex = curAnim;
					const char* name = names[curAnim];
					FBXHelper::SetCurrentAnimation(name);
				}
			}
		}
		ImGui::Unindent(15.0f);
	}

	if (ImGui::CollapsingHeader(STU("模型信息").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		char szTxt[256];
		std::string infoTxt = "";
		memset(szTxt, 0, 256);
		sprintf_s(szTxt, "模型数: %d\n", models->mMeshes.Count());
		infoTxt += szTxt;
		for (int i = 0; i < models->mMeshes.Count(); ++i)
		{
			FBXHelper::FbxModel* model = models->mMeshes[i];
			memset(szTxt, 0, 256);
			sprintf_s(szTxt, "\t模型 [%d]\n", i + 1);
			infoTxt += szTxt;
			memset(szTxt, 0, 256);
			sprintf_s(szTxt, "\t\t顶点数：%d\n", model->nVertexCount);
			infoTxt += szTxt;
			memset(szTxt, 0, 256);
			sprintf_s(szTxt, "\t\t面数：%d\n", model->nIndexCount / 3);
			infoTxt += szTxt;
		}
		memset(szTxt, 0, 256);
		sprintf_s(szTxt, "蒙皮数：%d\n", models->mSkins.Count());
		infoTxt += szTxt;

		ImGui::Indent(15.0f);
		ImGui::Text(STU(infoTxt).c_str());
		ImGui::Unindent(15.0f);
	}
	ImGui::End();
}

void Test::OnUpdate()
{
	if (!FBXHelper::IsWorking())
		return;

	DWORD curTime = timeGetTime();
	DWORD timeDelta = curTime - mLastTime;
	float dt = (float)timeDelta * 0.001f;

	D3DXMATRIX matRot;
	D3DXMatrixRotationY(&matRot, dt * mRotSpeed);
	D3DXMatrixMultiply(&mMatWorld, &mMatWorld, &matRot);
	mDevice->SetTransform(D3DTS_WORLD, &mMatWorld);

	D3DLIGHT9 light;
	mDevice->GetLight(0, &light);
	light.Direction = D3DXVECTOR3(1.0f, -1.0f, 1.0f);
	mDevice->SetLight(0, &light);

	FBXHelper::UpdateSkeleton();

	if (mShowMesh && mMeshRenderer)
		mMeshRenderer->Render();
	if (mShowBone && mBoneRenderer)
		mBoneRenderer->Render();

	mLastTime = curTime;
}

void Test::OnQuit()
{
	mDisireVtxNums.Clear();
	mMaxDisireVtxNums.Clear();
	SAFE_DELETE(mMeshRenderer);
	SAFE_DELETE(mBoneRenderer);
	FBXHelper::EndFBXHelper();
}