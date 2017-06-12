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
		if (models->mSkins.Count() > i)
		{
			FBXHelper::FbxSkinInfo* skin = models->mSkins[i];
			if (skin->size != fbxmodel->nVertexCount)
				continue;
			for (unsigned int j = 0; j < skin->size; ++j)
			{
				FBXHelper::FbxBoneWeight* bw = &(skin->weights[j]);
				pmmodel->pVB[j].skin = bw;
				if (bw->boneName.Count() == 0)
				{
					//printf("vertex(%d) has no any skin info!\n", j);
				}
			}
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

void ProgressiveMeshRenderer::Collapse(int* vtxnums, int meshCount, bool seperation)
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
		int vertexNum = buffer->v_count;
		if (seperation)
		{
			vertexNum = buffer->i_count;
		}
		ID3DXMesh* pMesh = NULL;
		HRESULT hr = D3DXCreateMeshFVF(faceNum, vertexNum, D3DXMESH_32BIT, fvf, mDevice, &pMesh);
		if (!FAILED(hr))
		{
			FocuseBoneSkin_t* skin = new FocuseBoneSkin_t();
			CustomVertex_t* pvb_t = new CustomVertex_t[vertexNum];
			for (int j = 0; j < vertexNum; ++j)
			{
				PMeshVertex_Tmp* vtcs = (PMeshVertex_Tmp*)buffer->vertices;
				PMeshVertex_Tmp* vtx = NULL;
				if (!seperation)
					vtx = &(vtcs[j]);
				else
					vtx = &(vtcs[((unsigned int*)buffer->indices)[j]]);
				CustomVertex_t& vtx_t = pvb_t[j];
				vtx_t.pos = vtx->pos;
				vtx_t.normal = vtx->normal;
				vtx_t.color = vtx->color;
				vtx_t.uv = vtx->uv;

				// 把关注点信息的蒙皮信息改为关注骨骼的蒙皮信息，方便渲染;
				if (!mIsSkinnedMesh) continue;
				FBXHelper::FbxBoneWeight* bw = (FBXHelper::FbxBoneWeight*)vtx->skin;
				if (!bw) continue;
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
			bvb.count = vertexNum;

			mBindVertexBuffer.Add(bvb);
			mFBSkin.Add(skin);

			void* ib = NULL;
			pMesh->LockIndexBuffer(0, (void**)&ib);
			if (!seperation)
				memcpy(ib, buffer->indices, buffer->i_count * buffer->i_stride);
			else
			{
				unsigned int* pIB = (unsigned int*)ib;
				for (int f = 0; f < faceNum; ++f)
				{
					for (int vi = 0; vi < 3; ++vi)
					{
						size_t index = f * 3 + vi;
						pIB[index] = index;
					}
					CustomVertex_t& cv1 = pvb_t[f * 3 + 0];
					CustomVertex_t& cv2 = pvb_t[f * 3 + 1];
					CustomVertex_t& cv3 = pvb_t[f * 3 + 2];
					D3DXVECTOR3 edge1 = cv1.pos - cv2.pos;
					D3DXVECTOR3 edge2 = cv3.pos - cv2.pos;
					D3DXVec3Normalize(&edge1, &edge1);
					D3DXVec3Normalize(&edge2, &edge2);
					D3DXVECTOR3 n;
					D3DXVec3Cross(&n, &edge2, &edge1);
					D3DXVec3Normalize(&n, &n);
					cv1.normal = n;
					cv2.normal = n;
					cv3.normal = n;
				}
			}
			pMesh->UnlockIndexBuffer();
			void* vb = NULL;
			pMesh->LockVertexBuffer(0, (void**)&vb);
			memcpy(vb, pvb_t, vertexNum * sizeof(CustomVertex_t));
			pMesh->UnlockVertexBuffer();

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
	mDevice->SetFVF(fvf);

	for (int m = 0; m < mMeshes.Count(); ++m)
	{
		FBXHelper::FbxBoneMap* bonemap = FBXHelper::GetBoneMap();

		ID3DXMesh* mesh = mMeshes[m];

		if (mIsSkinnedMesh)
		{
			CustomVertex_t* vertices = NULL;
			mesh->LockVertexBuffer(0, (void**)&vertices);

			BindVertexBuffer& bvb = mBindVertexBuffer[m];
			for (int i = 0; i < bvb.count; ++i)
			{
				vertices[i].pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
				vertices[i].normal = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
			}

			for (int i = 0; i < bonemap->mBoneList.Count(); ++i)
			{
				FBXHelper::FbxBone* bone = bonemap->mBoneList[i];

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

					D3DXVECTOR3 vn;
					D3DXVec3TransformNormal(&vn, &cv.normal, &mat);
					vn = vn * (float)weight;
					vertices[idx].normal.x += vn.x;
					vertices[idx].normal.y += vn.y;
					vertices[idx].normal.z += vn.z;
				}
			}

			mesh->UnlockVertexBuffer();
		}

		mesh->DrawSubset(0);
	}
}