#include "ProgressiveMeshRenderer.h"
#include "FBXHelper.h"
#include "Collapse.h"
#include <algorithm>

DWORD fvf = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;

class FocusBoneWeight_t
{
public:
	List<int> index;
	List<double> weight;
};

struct FocuseBoneSkin_t
{
	typedef std::map<std::string, FocusBoneWeight_t*>::iterator IT_FBS;
	std::map<std::string, FocusBoneWeight_t*> skins;
};

struct CustomVertex_t
{
	D3DXVECTOR3 pos;
	D3DXVECTOR3 normal;
	unsigned int color;
	D3DXVECTOR2 uv;
};

ProgressiveMeshRenderer::ProgressiveMeshRenderer(IDirect3DDevice9* device)
{
	mDevice = device;
	mLastTime = timeGetTime();
	mAnimTime = 0.0f;

	FBXHelper::FbxModelList* models = FBXHelper::GetModelList();
	if (models == NULL)
	{
		printf("Error: Please Parse The FBX File First!");
		return;
	}
	mIsSkinnedMesh = false;
	if (models->mSkins.Count() > 0)
	{
		mIsSkinnedMesh = true;
	}
	for (int i = 0; i < models->mMeshes.Count(); ++i)
	{
		FBXHelper::FbxModel* fbxmodel = models->mMeshes[i];
		PMeshModel* pmmodel = new PMeshModel();

		pmmodel->nVertexCount = fbxmodel->nVertexCount;
		pmmodel->nIndexCount = fbxmodel->nIndexCount;
		pmmodel->pVB = new PMeshVertex_Tmp[fbxmodel->nVertexCount];
		pmmodel->pIB = new unsigned int[fbxmodel->nIndexCount];
		for (int j = 0; j < fbxmodel->nVertexCount; ++j)
		{
			FBXHelper::FbxMeshVertex_Tmp* vertex = &(fbxmodel->pVB[j]);
			pmmodel->pVB[j].pos = vertex->pos;
			pmmodel->pVB[j].normal = vertex->normal;
			pmmodel->pVB[j].color = vertex->color;
			pmmodel->pVB[j].uv = vertex->uv;
			pmmodel->pVB[j].skin = NULL;
		}
		for (int j = 0; j < fbxmodel->nIndexCount; ++j)
		{
			pmmodel->pIB[j] = fbxmodel->pIB[j];
		}
		if (!mIsSkinnedMesh)
		{
			continue;
		}
		FBXHelper::FbxSkinInfo* skin = models->mSkins[i];
		if (skin->size != fbxmodel->nVertexCount)
			continue;
		for (unsigned int j = 0; j < skin->size; ++j)
		{
			FBXHelper::FbxBoneWeight* bw = &(skin->weights[j]);
			pmmodel->pVB[j].skin = bw;
		}

		mModels.Add(pmmodel);
	}
}

ProgressiveMeshRenderer::~ProgressiveMeshRenderer()
{
	for (int i = 0; i < mModels.Count(); ++i)
	{
		PMeshModel* model = mModels[i];
		if (model->pVB)
			delete[] model->pVB;
		if (model->pIB)
			delete[] model->pIB;
	}
	mModels.Clear();
	Clear();
	mIsSkinnedMesh = false;
}

void ProgressiveMeshRenderer::Collapse(int* vtxnums, int meshCount)
{
	Clear();
	meshCount = std::min<int>(meshCount, mModels.Count());
	for (int i = 0; i < meshCount; ++i)
	{
		PMeshModel* pmmodel = mModels[i];
		Collapse::BeginCollapse(pmmodel->pVB, sizeof(PMeshVertex_Tmp), pmmodel->nVertexCount, 0, pmmodel->pIB, sizeof(unsigned int), pmmodel->nIndexCount);
		Collapse::DoCollapse(vtxnums ? vtxnums[i] : pmmodel->nVertexCount);
		Collapse::Buffer* buffer = Collapse::GetBuffer();
		int faceNum = buffer->i_count / 3;
		ID3DXMesh* pMesh = NULL;
		HRESULT hr = D3DXCreateMeshFVF(faceNum, buffer->v_count, D3DXMESH_32BIT, fvf, mDevice, &pMesh);
		if (!FAILED(hr))
		{
			FocuseBoneSkin_t* skin = new FocuseBoneSkin_t();
			CustomVertex_t* pvb_t = new CustomVertex_t[buffer->v_count];
			for (size_t j = 0; j < buffer->v_count; ++j)
			{
				PMeshVertex_Tmp& vtx = ((PMeshVertex_Tmp*)buffer->vertices)[j];
				CustomVertex_t& vtx_t = pvb_t[j];
				vtx_t.pos = vtx.pos;
				vtx_t.normal = vtx.normal;
				vtx_t.color = vtx.color;
				vtx_t.uv = vtx.uv;

				// 把关注点信息的蒙皮信息改为关注骨骼的蒙皮信息，方便渲染;
				if (!mIsSkinnedMesh) continue;
				FBXHelper::FbxBoneWeight* bw = (FBXHelper::FbxBoneWeight*)vtx.skin;
				for (int k = 0; k < bw->boneName.Count(); ++k)
				{
					std::string& sName = bw->boneName[k];
					FocuseBoneSkin_t::IT_FBS itfbs = skin->skins.find(sName);
					if (itfbs == skin->skins.end())
					{
						skin->skins[sName] = new FocusBoneWeight_t();
					}
					FocusBoneWeight_t* fbw = skin->skins[sName];
					fbw->index.Add(j);
					fbw->weight.Add(bw->weight[k]);
				}
			}
			BindVertexBuffer bvb;
			bvb.vb = pvb_t;
			bvb.count = buffer->v_count;

			mBindVertexBuffer.Add(bvb);
			mFBSkin.Add(skin);

			void* vb = NULL;
			pMesh->LockVertexBuffer(0, (void**)&vb);
			memcpy(vb, pvb_t, buffer->v_count * sizeof(CustomVertex_t));
			pMesh->UnlockVertexBuffer();
			void* ib = NULL;
			pMesh->LockIndexBuffer(0, (void**)&ib);
			memcpy(ib, buffer->indices, buffer->i_count * buffer->i_stride);
			pMesh->UnlockIndexBuffer();

			pvb_t = NULL;
			mMeshes.Add(pMesh);
		}
		else
		{
			printf("Error: CreateMesh Failed. Index: %d", i);
		}

		Collapse::EndCollapse();
	}
	if (mMeshes.Count() != meshCount)
	{
		Clear();
	}
}

void ProgressiveMeshRenderer::Clear()
{
	for (int i = 0; i < mMeshes.Count(); ++i)
	{
		ID3DXMesh* mesh = mMeshes[i];
		if (mesh)
			mesh->Release();
	}
	mMeshes.Clear();
	for (int i = 0; i < mFBSkin.Count(); ++i)
	{
		FocuseBoneSkin_t* skin = mFBSkin[i];
		if (skin)
		{
			FocuseBoneSkin_t::IT_FBS itfbs;
			for (itfbs = skin->skins.begin(); itfbs != skin->skins.end(); ++itfbs)
			{
				FocusBoneWeight_t* fbw = itfbs->second;
				if (fbw)
					delete fbw;
			}
			skin->skins.clear();
			delete skin;
		}
	}
	mFBSkin.Clear();
	for (int i = 0; i < mBindVertexBuffer.Count(); ++i)
	{
		BindVertexBuffer& bvb = mBindVertexBuffer[i];
		if (bvb.vb)
			delete [] bvb.vb;
	}
	mBindVertexBuffer.Clear();
}

void ProgressiveMeshRenderer::Render()
{
	DWORD curTime = timeGetTime();
	DWORD timeDelta = curTime - mLastTime;
	mLastTime = curTime;

	float dt = (float)timeDelta * 0.001f;
	mAnimTime += dt;
	if (mAnimTime > 10.0f)
		mAnimTime = 0.0f;

	mDevice->SetFVF(fvf);

	FBXHelper::FbxAnimationEvaluator* animEvaluator = FBXHelper::GetAnimationEvaluator();

	for (int m = 0; m < mMeshes.Count(); ++m)
	{
		CustomVertex_t* vertices = NULL;
		FBXHelper::FbxBoneMap* bonemap = FBXHelper::GetBoneMap();

		ID3DXMesh* mesh = mMeshes[m];

		if (mIsSkinnedMesh)
		{
			mesh->LockVertexBuffer(0, (void**)&vertices);

			BindVertexBuffer& bvb = mBindVertexBuffer[m];
			for (int i = 0; i < bvb.count; ++i)
			{
				vertices[i].pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
			}

			for (int i = 0; i < bonemap->mBoneList.Count(); ++i)
			{
				FBXHelper::FbxBone* bone = bonemap->mBoneList[i];
				bone->offset = animEvaluator->Evaluator(bone, "shot", mAnimTime);

				FocusBoneWeight_t* fbw = mFBSkin[m]->skins[bone->name];
				if (!fbw) continue;

				D3DXMATRIX mat;
				D3DXMatrixInverse(&mat, NULL, &bone->bindPose);
				D3DXMatrixMultiply(&mat, &mat, &bone->offset);

				for (int j = 0; j < fbw->index.Count(); ++j)
				{
					int idx = fbw->index[j];
					double weight = fbw->weight[j];
					CustomVertex_t& cv = ((CustomVertex_t*)bvb.vb)[idx];

					D3DXVECTOR4 vec;
					D3DXVec3Transform(&vec, &cv.pos, &mat);
					vec = vec * (float)weight;
					vertices[idx].pos.x += vec.x;
					vertices[idx].pos.y += vec.y;
					vertices[idx].pos.z += vec.z;
				}
			}

			mesh->UnlockVertexBuffer();
		}

		mesh->DrawSubset(0);
	}
}