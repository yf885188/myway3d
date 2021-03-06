#include "MForest.h"
#include "Engine.h"
#include "MWRenderEvent.h"
#include "MWEnvironment.h"
#include "MWRenderHelper.h"

namespace Myway {

	static const int MForest_VegMagic = 'MFrt';
	static const int MForest_VegVersion = 0;

	IMP_SLN (MForest);

	MForest::MForest()
		: mWindStrength(0.3f)
	{
		INIT_SLN;

		Init();
	}

	MForest::~MForest()
	{
		Shutdown();

		SHUT_SLN;
	}

	void MForest::Init()
	{
		mShaderLib = ShaderLibManager::Instance()->LoadShaderLib("Tree.ShaderLib", "Tree\\Tree.ShaderLib");
		d_assert (mShaderLib);

		mTech_VegMesh = mShaderLib->GetTechnique("GrassMesh");
		mTech_VegBillboard = mShaderLib->GetTechnique("GrassBillboasrd");
		mTech_VegX2 = mShaderLib->GetTechnique("GrassX2");

		d_assert (mTech_VegX2);

		mVegBlockRect = RectF(0, 0, 0, 0);
		mXVegBlockCount = 0;
		mZVegBlockCount = 0;

		mTech_Branch = mShaderLib->GetTechnique("Branch");
		mTech_Frond = mShaderLib->GetTechnique("Frond");
		mTech_Leaf = mShaderLib->GetTechnique("Leaf");

		d_assert (mTech_Branch && mTech_Frond && mTech_Leaf);

		mTech_BranchDepth = mShaderLib->GetTechnique("BranchDepth");
		mTech_FrondDepth = mTech_BranchDepth;
		mTech_LeafDepth = mShaderLib->GetTechnique("LeafDepth");

		d_assert (mTech_BranchDepth && mTech_FrondDepth && mTech_LeafDepth);

		mTech_BranchMirror = mShaderLib->GetTechnique("BranchMirror");
		mTech_FrondMirror = mTech_BranchMirror;
		mTech_LeafMirror = mShaderLib->GetTechnique("LeafMirror");

		d_assert (mTech_BranchMirror && mTech_FrondMirror && mTech_LeafMirror);


		CSpeedTreeRT::SetNumWindMatrices(MTreeGlobal::K_NumWindMatrix);
	}

	void MForest::Shutdown()
	{
		UnloadVeg();

		d_assert (mTreeInstances.Size() == 0);
		d_assert (mTrees.Size() == 0);
	}

	void MForest::Update()
	{
		float time = Engine::Instance()->GetTime();
		// advance wind
		CSpeedTreeRT::SetTime(time);
		_setupWindMatrix(time);

		for (int i = 0; i < mVegetationBlocks.Size(); ++i)
		{
			mVegetationBlocks[i]->_UpdateGeometry();
		}
	}

	void MForest::_setupWindMatrix(float fTimeInSecs)
	{
		// matrix computational data
		static float afMatrixTimes[MTreeGlobal::K_NumWindMatrix] = { 0.0f };
		static float afFrequencies[MTreeGlobal::K_NumWindMatrix][2] = 
		{
			{ 0.15f, 0.17f },
			{ 0.25f, 0.15f },
			{ 0.19f, 0.05f },
			{ 0.15f, 0.22f }
		};

		// compute time since last call
		static float fTimeOfLastCall = 0.0f;
		float fTimeSinceLastCall = fTimeInSecs - fTimeOfLastCall;
		fTimeOfLastCall = fTimeInSecs;

		// wind strength
		static float fOldStrength = mWindStrength;

		// increment matrix times
		for (int i = 0; i < MTreeGlobal::K_NumWindMatrix; ++i)
			afMatrixTimes[i] += fTimeSinceLastCall;

		// compute maximum branch throw
		float fBaseAngle = mWindStrength * 35.0f;

		// build rotation matrices
		for (int i = 0; i < MTreeGlobal::K_NumWindMatrix; ++i)
		{
			// adjust time to prevent "jumping"
			if (mWindStrength != 0.0f)
				afMatrixTimes[i] = (afMatrixTimes[i] * fOldStrength) / mWindStrength;

			// compute percentages for each axis
			float fBaseFreq = mWindStrength * 20.0f;
			float fXPercent = sinf(fBaseFreq * afFrequencies[i % MTreeGlobal::K_NumWindMatrix][0] * afMatrixTimes[i]);
			float fYPercent = cosf(fBaseFreq * afFrequencies[i % MTreeGlobal::K_NumWindMatrix][1] * afMatrixTimes[i]);

			// build compound rotation matrix (rotate on 'x' then on 'y')
			const float c_fDeg2Rad = 57.2957795f;
			float fSinX = sinf(fBaseAngle * fXPercent / c_fDeg2Rad);
			float fSinY = sinf(fBaseAngle * fYPercent / c_fDeg2Rad);
			float fCosX = cosf(fBaseAngle * fXPercent / c_fDeg2Rad);
			float fCosY = cosf(fBaseAngle * fYPercent / c_fDeg2Rad);

			float afMatrix[16] = { 0.0f };
			afMatrix[0] = fCosY;
			afMatrix[2] = -fSinY;
			afMatrix[4] = fSinX * fSinY;
			afMatrix[5] = fCosX;
			afMatrix[6] = fSinX * fCosY;
			afMatrix[8] = fSinY * fCosX;
			afMatrix[9] = -fSinX;
			afMatrix[10] = fCosX * fCosY;
			afMatrix[15] = 1.0f;

			Memcpy(&mWindMatrix[i], afMatrix, sizeof(Mat4));
		}

		fOldStrength = mWindStrength;
	}

	void MForest::SetWindStrength(float fStrength)
	{
		for (int i = 0; i < mTrees.Size(); ++i)
		{
			mTrees[i]->_getSpeedTree()->SetWindStrength(fStrength);
		}
	}

	void MForest::UnloadVeg()
	{
		RemoveAllVegetationBlock();

		for (int i = 0; i < mVegetations.Size(); ++i)
		{
			delete mVegetations[i];
		}

		mVegetations.Clear();
	}

	void MForest::_AddVisibleVegetationBlock(MVegetationBlock * block)
	{
		mVisibleVegetationBlocks.PushBack(block);
	}

	void MForest::LoadVeg(const TString128 & source)
	{
		UnloadVeg();

		DataStreamPtr stream = ResourceManager::Instance()->OpenResource(source.c_str());

		if (stream == NULL)
			return ;

		int magic, version;

		stream->Read(&magic, sizeof(int));
		stream->Read(&version, sizeof(int));

		d_assert (magic == MForest_VegMagic);
		d_assert (version == MForest_VegVersion);
		
		int vegCount;
		stream->Read(&vegCount, sizeof(int));

		for (int i = 0; i < vegCount; ++i)
		{
			TString32 Name;
			int Type;
			TString128 MeshFile, DiffuseMap, NormalMap, SpecularMap;

			stream->Read(Name.c_str(), 32);
			stream->Read(&Type, sizeof(int));
			stream->Read(MeshFile.c_str(), 128);
			stream->Read(DiffuseMap.c_str(), 128);
			stream->Read(NormalMap.c_str(), 128);
			stream->Read(SpecularMap.c_str(), 128);

			AddVegetation(Name, (MVegetation::GeomType)Type, MeshFile, DiffuseMap, NormalMap, SpecularMap);
		}

		RectF rect;
		int xBlockCount, zBlockCount;

		stream->Read(&rect, sizeof(rect));
		stream->Read(&xBlockCount, sizeof(int));
		stream->Read(&zBlockCount, sizeof(int));

		if (xBlockCount > 0 && zBlockCount > 0)
			CreateVegetationBlocks(rect, xBlockCount, zBlockCount);

		for (int j = 0; j < zBlockCount; ++j)
		{
			for (int i = 0; i < xBlockCount; ++i)
			{
				MVegetationBlock * block = GetVegetationBlock(i, j);
				List<MVegetationBlock::Inst> & instList = block->_getInstList();

				int instCount;
				TString32 Name;
				Vec3 Position;
				float Size;

				stream->Read(&instCount, sizeof(int));

				for (int k = 0; k < instCount; ++k)
				{
					stream->Read(Name.c_str(), 32);
					stream->Read(&Position, sizeof(Vec3));
					stream->Read(&Size, sizeof(float));

					MVegetationBlock::Inst inst;

					inst.Veg = GetVegetationByName(Name);
					inst.Position = Position;
					inst.Size = Size;

					instList.PushBack(inst);
				}

				block->_notifyNeedUpdate();
			}
		}
	}

	void MForest::SaveVeg(const TString128 & source)
	{
		File file;

		file.Open(source.c_str(), OM_WRITE_BINARY);

		file.Write(&MForest_VegMagic, sizeof(int));
		file.Write(&MForest_VegVersion, sizeof(int));

		// save vegetation
		int vegCount = mVegetations.Size();
		file.Write(&vegCount, sizeof(int));

		for (int i = 0; i < mVegetations.Size(); ++i)
		{
			MVegetation * veg = mVegetations[i];
			TString32 Name = veg->Name;
			int Type = veg->Type;
			TString128 MeshFile = veg->pMesh != NULL ? veg->pMesh->GetSourceName() : "";
			TString128 DiffuseMap = veg->DiffuseMap != NULL ? veg->DiffuseMap->GetSourceName() : "";
			TString128 NormalMap = veg->NormalMap != NULL ? veg->NormalMap->GetSourceName() : "";
			TString128 SpecularMap = veg->SpecularMap != NULL ? veg->SpecularMap->GetSourceName() : "";

			file.Write(Name.c_str(), 32);
			file.Write(&Type, sizeof(int));
			file.Write(MeshFile.c_str(), 128);
			file.Write(DiffuseMap.c_str(), 128);
			file.Write(NormalMap.c_str(), 128);
			file.Write(SpecularMap.c_str(), 128);
		}

		// save block
		file.Write(&mVegBlockRect, sizeof(RectF));
		file.Write(&mXVegBlockCount, sizeof(int));
		file.Write(&mZVegBlockCount, sizeof(int));

		for (int j = 0; j < mZVegBlockCount; ++j)
		{
			for (int i = 0; i < mXVegBlockCount; ++i)
			{
				MVegetationBlock * block = GetVegetationBlock(i, j);
				int count = block->_getInstanceSize();
				List<MVegetationBlock::Inst> & insts = block->_getInstList();

				file.Write(&count, sizeof(int));

				List<MVegetationBlock::Inst>::Iterator whr = insts.Begin();
				List<MVegetationBlock::Inst>::Iterator end = insts.End();

				while (whr != end)
				{
					const TString32 & Name = whr->Veg->Name;
					const Vec3 & Position = whr->Position;
					float Size = whr->Size;

					file.Write(Name.c_str(), 32);
					file.Write(&Position, sizeof(Vec3));
					file.Write(&Size, sizeof(float));

					++whr;
				}
			}
		}
	}

	void MForest::AddVegetation(const TString32 & name, MVegetation::GeomType type,
								const TString128 & mesh, const TString128 & diffueMap,
								const TString128 & normalMap, const TString128 & specularMap)
	{
		MVegetation * veg = new MVegetation;

		veg->Name = name;
		veg->Type = type;

		if (mesh != "" && type == MVegetation::GT_Mesh)
			veg->pMesh = MeshManager::Instance()->Load(mesh, mesh);

		veg->DiffuseMap = VideoBufferManager::Instance()->Load2DTexture(diffueMap, diffueMap);

		if (normalMap != "" && type == MVegetation::GT_Mesh)
			veg->NormalMap = VideoBufferManager::Instance()->Load2DTexture(normalMap, normalMap);

		if (specularMap != "" && type == MVegetation::GT_Mesh)
			veg->SpecularMap = VideoBufferManager::Instance()->Load2DTexture(specularMap, specularMap);

		mVegetations.PushBack(veg);
	}

	void MForest::RemoveVegetation(MVegetation * veg)
	{
		for (int i = 0; i < mVegetations.Size(); ++i)
		{
			if (mVegetations[i] == veg)
			{
				_OnVegRemoved(veg);

				delete veg;

				mVegetations.Erase(i);

				return ;
			}
		}

		d_assert (0);
	}

	int MForest::GetVegetationCount() const
	{
		return mVegetations.Size();
	}

	MVegetation * MForest::GetVegetation(int index)
	{
		return mVegetations[index];
	}

	MVegetation * MForest::GetVegetationByName(const TString32 & name)
	{
		for (int i = 0; i < mVegetations.Size(); ++i)
		{
			MVegetation * veg = mVegetations[i];

			if (veg->Name == name)
				return veg;
		}

		return NULL;
	}

	void MForest::OnVegetationChanged(MVegetation * veg)
	{
		for (int i = 0; i < mVegetationBlocks.Size(); ++i)
		{
			MVegetationBlock * block = mVegetationBlocks[i];
			block->_OnVegChanged(veg);
		}
	}

	void MForest::AddVegetationInst(MVegetation * veg, const Vec3 & position, float size)
	{
		for (int i = 0; i < mVegetationBlocks.Size(); ++i)
		{
			MVegetationBlock * block = mVegetationBlocks[i];
			const RectF & rc = block->GetRect();

			if (position.x >= rc.x1 && position.x <= rc.x2 &&
				position.z >= rc.y1 && position.z <= rc.y2)
			{
				block->AddVegetation(veg, position, size);
				break;
			}
		}
	}

	void MForest::RemoveVegetationInst(const RectF & rc, MVegetation * veg)
	{
		for (int i = 0; i < mVegetationBlocks.Size(); ++i)
		{
			MVegetationBlock * block = mVegetationBlocks[i];

			if (block->_getInstanceSize() == 0)
				continue;

			const RectF & rcBlock = block->GetRect();

			if (rc.x1 > rcBlock.x2)
				continue;

			if (rc.x2 < rcBlock.x1)
				continue;

			if (rc.y1 > rcBlock.y2)
				continue;

			if (rc.y2 < rcBlock.y1)
				continue;

			block->RemoveVegetation(veg, rc);
		}
	}

	void MForest::CreateVegetationBlocks(const RectF & rect, int xCount, int zCount)
	{
		d_assert (xCount > 0 && zCount > 0);

		mXVegBlockCount = xCount;
		mZVegBlockCount = zCount;
		mVegBlockRect = rect;

		float xStep = (rect.x2 - rect.x1) / xCount;
		float zStep = (rect.y2 - rect.y1) / zCount;

		for (int j = 0; j < zCount; ++j)
		{
			for (int i = 0; i < xCount; ++i)
			{
				RectF rc;

				rc.x1 = rect.x1 + xStep * i;
				rc.y1 = rect.y1 + zStep * j;
				rc.x2 = rc.x1 + xStep;
				rc.y2 = rc.y1 + zStep;

				MVegetationBlock * block = new MVegetationBlock(i, j, rc);

				mVegetationBlocks.PushBack(block);
			}
		}
	}

	MVegetationBlock * MForest::GetVegetationBlock(int x, int z)
	{
		d_assert (x < mXVegBlockCount && z < mZVegBlockCount);

		return mVegetationBlocks[z * mXVegBlockCount + x];
	}

	int MForest::GetVegetationBlockCount()
	{
		return mVegetationBlocks.Size();
	}

	void MForest::RemoveAllVegetationBlock()
	{
		for (int i = 0; i < mVegetationBlocks.Size(); ++i)
		{
			delete mVegetationBlocks[i];
		}

		mVegetationBlocks.Clear();
	}

	void MForest::_preVisibleCull()
	{
		mVisibleVegetationBlocks.Clear();
		mVisbleTreeInstances.Clear();
	}

	void MForest::_render()
	{
		Camera * cam = World::Instance()->MainCamera();
		const Mat4 & matVP = World::Instance()->MainCamera()->GetViewProjMatrix();

		float afDirection[3];
		afDirection[0] = matVP.m[0][2];
		afDirection[1] = matVP.m[1][2];
		afDirection[2] = matVP.m[2][2];

		CSpeedTreeRT::SetCamera((const float *)&cam->GetPosition(), afDirection);

		_drawMeshVeg();
		_drawBillboardVeg();
		_drawX2Veg();

		if (mVisbleTreeInstances.Size() == 0)
			return ;

		_drawBranch();
		_drawFrond();
		_drawLeaf();
	}

	void MForest::_renderDepthForShadow(int layer)
	{
		const Shadow::CascadedMatrixs & forms = Environment::Instance()->GetShadow()->GetCascadedMatrix(layer);

		Camera * cam = World::Instance()->MainCamera();
		const Mat4 & matVP = forms.mViewProj;
		const Mat4 & matView = forms.mView;
		
		float afPosition[3];
		afPosition[0] = -forms.mView._41;
		afPosition[1] = -forms.mView._42;
		afPosition[2] = -forms.mView._43;

		/*afPosition[0] = cam->GetPosition().x;
		afPosition[1] = cam->GetPosition().y;
		afPosition[2] = cam->GetPosition().z;*/

		float afDirection[3];
		afDirection[0] = matVP.m[0][2];
		afDirection[1] = matVP.m[1][2];
		afDirection[2] = matVP.m[2][2];

		CSpeedTreeRT::SetCamera(afPosition, afDirection);

		if (mVisbleTreeInstances.Size() == 0)
			return ;

		mVisbleMask.Resize(mVisbleTreeInstances.Size());

		for (int i = 0; i < mVisbleTreeInstances.Size(); ++i)
		{
			const Aabb & bound = mVisbleTreeInstances[i]->GetWorldAabb();

			mVisbleMask[i] = Shadow::IsVisible(bound, matVP);
		}

		ShaderParam * uMatViewProj_B = mTech_BranchDepth->GetVertexShaderParamTable()->GetParam("matVP");
		ShaderParam * uMatViewProj_L = mTech_LeafDepth->GetVertexShaderParamTable()->GetParam("matVP");

		uMatViewProj_B->SetMatrix(matVP);
		uMatViewProj_L->SetMatrix(matVP);

		_drawBranchDepth();
		_drawFrondDepth();
		_drawLeafDepth();
	}

	void MForest::_renderInMirror(Camera * camera)
	{
		Camera * cam = camera;
		const Mat4 & matVP = World::Instance()->MainCamera()->GetViewProjMatrix();

		float afDirection[3];
		afDirection[0] = matVP.m[0][2];
		afDirection[1] = matVP.m[1][2];
		afDirection[2] = matVP.m[2][2];

		CSpeedTreeRT::SetCamera((const float *)&cam->GetPosition(), afDirection);

		if (mVisbleTreeInstances.Size() == 0)
			return ;

		_drawBranchMirror();
		_drawFrondMirror();
		_drawLeafMirror();
	}

	void MForest::_drawMeshVeg()
	{
	}

	void MForest::_drawBillboardVeg()
	{
	}

	void MForest::_drawX2Veg()
	{
		ShaderParam * uNormal = mTech_VegX2->GetVertexShaderParamTable()->GetParam("normal");

		Vec3 normal = -Environment::Instance()->GetEvParam()->LightDir;

		normal = normal.TransformN(World::Instance()->MainCamera()->GetViewMatrix());
		normal.NormalizeL();

		uNormal->SetUnifom(normal.x, normal.y, normal.z, 0);

		for (int i = 0; i < mVisibleVegetationBlocks.Size(); ++i)
		{
			MVegetationBlock * block = mVisibleVegetationBlocks[i];

			for (int j = 0; j < block->GetRenderOpCount(); ++j)
			{
				MVegetation * veg = block->GetRenderVegetation(j);

				if (veg->Type != MVegetation::GT_X2)
					continue;

				RenderOp * rop = block->GetRenderOp(j);

				SamplerState state;

				RenderSystem::Instance()->SetTexture(0, state, veg->DiffuseMap.c_ptr());
				RenderSystem::Instance()->Render(mTech_VegX2, rop);
			}
		}
	}


	void MForest::_OnVegRemoved(MVegetation * veg)
	{
		for (int i = 0; i < mVegetationBlocks.Size(); ++i)
		{
			mVegetationBlocks[i]->_OnVegRemoved(veg);
		}
	}


	MTreePtr MForest::LoadTree(const TString128 & source)
	{
		for (int i = 0; i < mTrees.Size(); ++i)
		{
			if (mTrees[i]->GetSourceName() == source)
				return mTrees[i];
		}

		MTree * tree = new MTree(source);

		tree ->_getSpeedTree()->SetWindStrength(mWindStrength);

		tree->Load();

		mTrees.PushBack(tree);

		return tree;
	}

	void MForest::DeleteTree(MTree * tree)
	{
		for (int i = 0; i < mTrees.Size(); ++i)
		{
			if (mTrees[i] == tree)
			{
				mTrees.Erase(i);
				delete tree;
				return ;
			}
		}

		d_assert (0);
	}

	MTreeInstance * MForest::CreateTreeInstance(const TString128 & name, const TString128 & source)
	{
		d_assert (GetTreeInstance(name) == NULL && source != "");

		MTreeInstance * inst = new MTreeInstance(name);

		MTreePtr tree = LoadTree(source);

		inst->SetTree(tree);

		mTreeInstances.PushBack(inst);

		return inst;
	}

	MTreeInstance * MForest::CreateTreeInstance(const TString128 & name)
	{
		d_assert (GetTreeInstance(name) == NULL);

		MTreeInstance * inst = new MTreeInstance(name);

		mTreeInstances.PushBack(inst);

		return inst;
	}

	MTreeInstance * MForest::GetTreeInstance(const TString128 & name)
	{
		for (int i = 0; i < mTreeInstances.Size(); ++i)
		{
			if (mTreeInstances[i]->GetName() == name)
				return mTreeInstances[i];
		}

		return NULL;
	}

	bool MForest::RenameTreeInstance(const TString128 & name, MTreeInstance * inst)
	{
		if (inst->GetName() != name && GetTreeInstance(name) == NULL)
		{
			inst->SetName(name);
			return true;
		}

		return false;
	}

	void MForest::DestroyInstance(MTreeInstance * tree)
	{
		for (int i = 0; i < mTreeInstances.Size(); ++i)
		{
			if (mTreeInstances[i] == tree)
			{
				delete tree;
				mTreeInstances.Erase(i);
				return ;
			}
		}

		d_assert (0);
	}

	void MForest::_AddVisibleTreeInstance(MTreeInstance * tree)
	{
		mVisbleTreeInstances.PushBack(tree);
	}

	void MForest::_drawBranch()
	{
		RenderSystem::Instance()->_BeginEvent("Draw Branch");

		ShaderParam * uWindMatrixOffset = mTech_Branch->GetVertexShaderParamTable()->GetParam("gWindMatrixOffset");
		ShaderParam * uWindMatrix = mTech_Branch->GetVertexShaderParamTable()->GetParam("gWindMatrices");
		ShaderParam * uTranslateScale = mTech_Branch->GetVertexShaderParamTable()->GetParam("gTranslateScale");
		ShaderParam * uRotationMatrix = mTech_Branch->GetVertexShaderParamTable()->GetParam("gRotationMatrix");
		ShaderParam * uDiffuseColor = mTech_Branch->GetPixelShaderParamTable()->GetParam("gDiffuseColor");

		uWindMatrix->SetMatrix(mWindMatrix, MTreeGlobal::K_NumWindMatrix);

		for (int i = 0; i < mVisbleTreeInstances.Size(); ++i)
		{
			MTreeInstance * inst = mVisbleTreeInstances[i];
			MTree * tree = inst->GetTree().c_ptr();
			Node * pNode = inst->GetAttachNode();

			Vec3 Position = pNode->GetWorldPosition();
			Quat Orientation = pNode->GetWorldOrientation();
			Vec3 Scale = pNode->GetWorldScale();

			uWindMatrixOffset->SetUnifom(inst->GetWindMatrixOffset(), 0, 0, 0);
			uTranslateScale->SetUnifom(Position.x, Position.y, Position.z, Scale.x);
			uRotationMatrix->SetMatrix(Orientation.ToMatrix());

			if (tree)
			{
				MTree::Material * mtl = tree->_getBranchMaterial();
				RenderOp * rop = tree->_getBranchRenderOp(0);

				uDiffuseColor->SetColor(inst->GetBranchDiffuse() * mtl->Diffuse);

				if (rop)
				{
					rop->xform = inst->GetAttachNode()->GetWorldMatrix();

					SamplerState state;
					RenderSystem::Instance()->SetTexture(0, state, mtl->DiffuseMap.c_ptr());

					RenderSystem::Instance()->Render(mTech_Branch, rop);
				}
			}
		}

		RenderSystem::Instance()->_EndEvent();
	}

	void MForest::_drawFrond()
	{
		RenderSystem::Instance()->_BeginEvent("Draw Frond");

		ShaderParam * uWindMatrixOffset = mTech_Frond->GetVertexShaderParamTable()->GetParam("gWindMatrixOffset");
		ShaderParam * uWindMatrix = mTech_Frond->GetVertexShaderParamTable()->GetParam("gWindMatrices");
		ShaderParam * uTranslateScale = mTech_Frond->GetVertexShaderParamTable()->GetParam("gTranslateScale");
		ShaderParam * uRotationMatrix = mTech_Frond->GetVertexShaderParamTable()->GetParam("gRotationMatrix");
		ShaderParam * uDiffuseColor = mTech_Frond->GetPixelShaderParamTable()->GetParam("gDiffuseColor");

		uWindMatrix->SetMatrix(mWindMatrix, MTreeGlobal::K_NumWindMatrix);

		for (int i = 0; i < mVisbleTreeInstances.Size(); ++i)
		{
			MTreeInstance * inst = mVisbleTreeInstances[i];
			MTree * tree = inst->GetTree().c_ptr();
			Node * pNode = inst->GetAttachNode();

			Vec3 Position = pNode->GetWorldPosition();
			Quat Orientation = pNode->GetWorldOrientation();
			Vec3 Scale = pNode->GetWorldScale();

			uWindMatrixOffset->SetUnifom(inst->GetWindMatrixOffset(), 0, 0, 0);
			uTranslateScale->SetUnifom(Position.x, Position.y, Position.z, Scale.x);
			uRotationMatrix->SetMatrix(Orientation.ToMatrix());

			if (tree)
			{
				MTree::Material * mtl = tree->_getFrondMaterial();
				RenderOp * rop = tree->_getFrondRenderOp(0);

				uDiffuseColor->SetColor(inst->GetFrondDiffuse() * mtl->Diffuse);

				if (rop != NULL)
				{
					rop->xform = inst->GetAttachNode()->GetWorldMatrix();

					SamplerState state;
					RenderSystem::Instance()->SetTexture(0, state, mtl->DiffuseMap.c_ptr());

					RenderSystem::Instance()->Render(mTech_Frond, rop);
				}
			}
		}

		RenderSystem::Instance()->_EndEvent();
	}

	void MForest::_drawLeaf()
	{
		RenderSystem::Instance()->_BeginEvent("Draw Leaf");

		ShaderParam * uWindMatrixOffset = mTech_Leaf->GetVertexShaderParamTable()->GetParam("gWindMatrixOffset");
		ShaderParam * uWindMatrix = mTech_Leaf->GetVertexShaderParamTable()->GetParam("gWindMatrices");

		ShaderParam * uTranslateScale = mTech_Leaf->GetVertexShaderParamTable()->GetParam("gTranslateScale");
		ShaderParam * uRotationMatrix = mTech_Leaf->GetVertexShaderParamTable()->GetParam("gRotationMatrix");

		ShaderParam * uBillboardTable = mTech_Leaf->GetVertexShaderParamTable()->GetParam("gBillboardTable");

		ShaderParam * uDiffuseColor = mTech_Leaf->GetPixelShaderParamTable()->GetParam("gDiffuseColor");

		uWindMatrix->SetMatrix(mWindMatrix, MTreeGlobal::K_NumWindMatrix);

		for (int i = 0; i < mVisbleTreeInstances.Size(); ++i)
		{
			MTreeInstance * inst = mVisbleTreeInstances[i];
			MTree * tree = inst->GetTree().c_ptr();
			Node * pNode = inst->GetAttachNode();

			Vec3 Position = pNode->GetWorldPosition();
			Quat Orientation = pNode->GetWorldOrientation();
			Vec3 Scale = pNode->GetWorldScale();

			uWindMatrixOffset->SetUnifom(inst->GetWindMatrixOffset(), 0, 0, 0);
			uTranslateScale->SetUnifom(Position.x, Position.y, Position.z, Scale.x);
			uRotationMatrix->SetMatrix(Orientation.ToMatrix());

			if (tree)
			{
				MTree::Material * mtl = tree->_getLeafMaterial();
				RenderOp * rop = tree->_getLeafRenderOp(0);

				uDiffuseColor->SetColor(inst->GetLeafDiffuse() * mtl->Diffuse);

				if (rop)
				{
					tree->SetupLeafBillboard(uBillboardTable);

					rop->xform = inst->GetAttachNode()->GetWorldMatrix();

					SamplerState state;
					RenderSystem::Instance()->SetTexture(0, state, mtl->DiffuseMap.c_ptr());

					RenderSystem::Instance()->Render(mTech_Leaf, rop);
				}
				
			}
		}

		RenderSystem::Instance()->_EndEvent();
	}

	void MForest::_drawBranchDepth()
	{
		RenderSystem::Instance()->_BeginEvent("Draw Branch Depth");

		ShaderParam * uWindMatrixOffset = mTech_BranchDepth->GetVertexShaderParamTable()->GetParam("gWindMatrixOffset");
		ShaderParam * uWindMatrix = mTech_BranchDepth->GetVertexShaderParamTable()->GetParam("gWindMatrices");
		ShaderParam * uTranslateScale = mTech_BranchDepth->GetVertexShaderParamTable()->GetParam("gTranslateScale");
		ShaderParam * uRotationMatrix = mTech_BranchDepth->GetVertexShaderParamTable()->GetParam("gRotationMatrix");

		uWindMatrix->SetMatrix(mWindMatrix, MTreeGlobal::K_NumWindMatrix);

		for (int i = 0; i < mVisbleTreeInstances.Size(); ++i)
		{
			if (!mVisbleMask[i])
				continue;

			MTreeInstance * inst = mVisbleTreeInstances[i];
			MTree * tree = inst->GetTree().c_ptr();
			Node * pNode = inst->GetAttachNode();

			Vec3 Position = pNode->GetWorldPosition();
			Quat Orientation = pNode->GetWorldOrientation();
			Vec3 Scale = pNode->GetWorldScale();

			uWindMatrixOffset->SetUnifom(inst->GetWindMatrixOffset(), 0, 0, 0);
			uTranslateScale->SetUnifom(Position.x, Position.y, Position.z, Scale.x);
			uRotationMatrix->SetMatrix(Orientation.ToMatrix());

			if (tree)
			{
				MTree::Material * mtl = tree->_getBranchMaterial();
				RenderOp * rop = tree->_getBranchRenderOp(0);

				if (rop)
				{
					rop->xform = inst->GetAttachNode()->GetWorldMatrix();

					SamplerState state;
					RenderSystem::Instance()->SetTexture(0, state, mtl->DiffuseMap.c_ptr());

					RenderSystem::Instance()->Render(mTech_BranchDepth, rop);
				}
			}
		}

		RenderSystem::Instance()->_EndEvent();
	}

	void MForest::_drawFrondDepth()
	{
		RenderSystem::Instance()->_BeginEvent("Draw Frond Depth");

		ShaderParam * uWindMatrixOffset = mTech_FrondDepth->GetVertexShaderParamTable()->GetParam("gWindMatrixOffset");
		ShaderParam * uWindMatrix = mTech_FrondDepth->GetVertexShaderParamTable()->GetParam("gWindMatrices");
		ShaderParam * uTranslateScale = mTech_FrondDepth->GetVertexShaderParamTable()->GetParam("gTranslateScale");
		ShaderParam * uRotationMatrix = mTech_FrondDepth->GetVertexShaderParamTable()->GetParam("gRotationMatrix");

		uWindMatrix->SetMatrix(mWindMatrix, MTreeGlobal::K_NumWindMatrix);

		for (int i = 0; i < mVisbleTreeInstances.Size(); ++i)
		{
			if (!mVisbleMask[i])
				continue;

			MTreeInstance * inst = mVisbleTreeInstances[i];
			MTree * tree = inst->GetTree().c_ptr();
			Node * pNode = inst->GetAttachNode();

			Vec3 Position = pNode->GetWorldPosition();
			Quat Orientation = pNode->GetWorldOrientation();
			Vec3 Scale = pNode->GetWorldScale();

			uWindMatrixOffset->SetUnifom(inst->GetWindMatrixOffset(), 0, 0, 0);
			uTranslateScale->SetUnifom(Position.x, Position.y, Position.z, Scale.x);
			uRotationMatrix->SetMatrix(Orientation.ToMatrix());

			if (tree)
			{
				MTree::Material * mtl = tree->_getFrondMaterial();
				RenderOp * rop = tree->_getFrondRenderOp(0);

				if (rop != NULL)
				{
					rop->xform = inst->GetAttachNode()->GetWorldMatrix();

					SamplerState state;
					RenderSystem::Instance()->SetTexture(0, state, mtl->DiffuseMap.c_ptr());

					RenderSystem::Instance()->Render(mTech_FrondDepth, rop);
				}
			}
		}

		RenderSystem::Instance()->_EndEvent();
	}

	void MForest::_drawLeafDepth()
	{
		RenderSystem::Instance()->_BeginEvent("Draw Leaf Depth");

		ShaderParam * uWindMatrixOffset = mTech_LeafDepth->GetVertexShaderParamTable()->GetParam("gWindMatrixOffset");
		ShaderParam * uWindMatrix = mTech_LeafDepth->GetVertexShaderParamTable()->GetParam("gWindMatrices");

		ShaderParam * uTranslateScale = mTech_LeafDepth->GetVertexShaderParamTable()->GetParam("gTranslateScale");
		ShaderParam * uRotationMatrix = mTech_LeafDepth->GetVertexShaderParamTable()->GetParam("gRotationMatrix");

		ShaderParam * uBillboardTable = mTech_LeafDepth->GetVertexShaderParamTable()->GetParam("gBillboardTable");

		uWindMatrix->SetMatrix(mWindMatrix, MTreeGlobal::K_NumWindMatrix);

		for (int i = 0; i < mVisbleTreeInstances.Size(); ++i)
		{
			if (!mVisbleMask[i])
				continue;

			MTreeInstance * inst = mVisbleTreeInstances[i];
			MTree * tree = inst->GetTree().c_ptr();
			Node * pNode = inst->GetAttachNode();

			Vec3 Position = pNode->GetWorldPosition();
			Quat Orientation = pNode->GetWorldOrientation();
			Vec3 Scale = pNode->GetWorldScale();

			uWindMatrixOffset->SetUnifom(inst->GetWindMatrixOffset(), 0, 0, 0);
			uTranslateScale->SetUnifom(Position.x, Position.y, Position.z, Scale.x);
			uRotationMatrix->SetMatrix(Orientation.ToMatrix());

			if (tree)
			{
				RenderOp * rop = tree->_getLeafRenderOp(0);

				if (rop)
				{
					MTree::Material * mtl = tree->_getLeafMaterial();

					tree->SetupLeafBillboard(uBillboardTable);

					rop->xform = inst->GetAttachNode()->GetWorldMatrix();

					SamplerState state;
					RenderSystem::Instance()->SetTexture(0, state, mtl->DiffuseMap.c_ptr());

					RenderSystem::Instance()->Render(mTech_LeafDepth, rop);
				}
			}
		}

		RenderSystem::Instance()->_EndEvent();
	}

	void MForest::_drawBranchMirror()
	{
		RenderSystem::Instance()->_BeginEvent("Draw Branch Mirror");

		ShaderParam * uWindMatrixOffset = mTech_BranchMirror->GetVertexShaderParamTable()->GetParam("gWindMatrixOffset");
		ShaderParam * uWindMatrix = mTech_BranchMirror->GetVertexShaderParamTable()->GetParam("gWindMatrices");
		ShaderParam * uTranslateScale = mTech_BranchMirror->GetVertexShaderParamTable()->GetParam("gTranslateScale");
		ShaderParam * uRotationMatrix = mTech_BranchMirror->GetVertexShaderParamTable()->GetParam("gRotationMatrix");

		ShaderParam * uDiffuseColor = mTech_BranchMirror->GetPixelShaderParamTable()->GetParam("gDiffuseColor");

		uDiffuseColor->SetColor(RenderRegister::Instance()->GetMirrorColor());

		uWindMatrix->SetMatrix(mWindMatrix, MTreeGlobal::K_NumWindMatrix);

		for (int i = 0; i < mVisbleTreeInstances.Size(); ++i)
		{
			MTreeInstance * inst = mVisbleTreeInstances[i];
			MTree * tree = inst->GetTree().c_ptr();
			Node * pNode = inst->GetAttachNode();

			Vec3 Position = pNode->GetWorldPosition();
			Quat Orientation = pNode->GetWorldOrientation();
			Vec3 Scale = pNode->GetWorldScale();

			uWindMatrixOffset->SetUnifom(inst->GetWindMatrixOffset(), 0, 0, 0);
			uTranslateScale->SetUnifom(Position.x, Position.y, Position.z, Scale.x);
			uRotationMatrix->SetMatrix(Orientation.ToMatrix());

			if (tree)
			{
				MTree::Material * mtl = tree->_getBranchMaterial();
				RenderOp * rop = tree->_getBranchRenderOp(0);

				if (rop)
				{
					rop->xform = inst->GetAttachNode()->GetWorldMatrix();

					SamplerState state;
					RenderSystem::Instance()->SetTexture(0, state, mtl->DiffuseMap.c_ptr());

					RenderSystem::Instance()->Render(mTech_BranchMirror, rop);
				}
			}
		}

		RenderSystem::Instance()->_EndEvent();
	}

	void MForest::_drawFrondMirror()
	{
		RenderSystem::Instance()->_BeginEvent("Draw Frond Mirror");

		ShaderParam * uWindMatrixOffset = mTech_FrondMirror->GetVertexShaderParamTable()->GetParam("gWindMatrixOffset");
		ShaderParam * uWindMatrix = mTech_FrondMirror->GetVertexShaderParamTable()->GetParam("gWindMatrices");
		ShaderParam * uTranslateScale = mTech_FrondMirror->GetVertexShaderParamTable()->GetParam("gTranslateScale");
		ShaderParam * uRotationMatrix = mTech_FrondMirror->GetVertexShaderParamTable()->GetParam("gRotationMatrix");

		uWindMatrix->SetMatrix(mWindMatrix, MTreeGlobal::K_NumWindMatrix);

		for (int i = 0; i < mVisbleTreeInstances.Size(); ++i)
		{
			MTreeInstance * inst = mVisbleTreeInstances[i];
			MTree * tree = inst->GetTree().c_ptr();
			Node * pNode = inst->GetAttachNode();

			Vec3 Position = pNode->GetWorldPosition();
			Quat Orientation = pNode->GetWorldOrientation();
			Vec3 Scale = pNode->GetWorldScale();

			uWindMatrixOffset->SetUnifom(inst->GetWindMatrixOffset(), 0, 0, 0);
			uTranslateScale->SetUnifom(Position.x, Position.y, Position.z, Scale.x);
			uRotationMatrix->SetMatrix(Orientation.ToMatrix());

			if (tree)
			{
				MTree::Material * mtl = tree->_getFrondMaterial();
				RenderOp * rop = tree->_getFrondRenderOp(0);

				if (rop != NULL)
				{
					rop->xform = inst->GetAttachNode()->GetWorldMatrix();

					SamplerState state;
					RenderSystem::Instance()->SetTexture(0, state, mtl->DiffuseMap.c_ptr());

					RenderSystem::Instance()->Render(mTech_FrondMirror, rop);
				}
			}
		}

		RenderSystem::Instance()->_EndEvent();
	}

	void MForest::_drawLeafMirror()
	{
		RenderSystem::Instance()->_BeginEvent("Draw Leaf Mirror");

		ShaderParam * uWindMatrixOffset = mTech_LeafMirror->GetVertexShaderParamTable()->GetParam("gWindMatrixOffset");
		ShaderParam * uWindMatrix = mTech_LeafMirror->GetVertexShaderParamTable()->GetParam("gWindMatrices");

		ShaderParam * uTranslateScale = mTech_LeafMirror->GetVertexShaderParamTable()->GetParam("gTranslateScale");
		ShaderParam * uRotationMatrix = mTech_LeafMirror->GetVertexShaderParamTable()->GetParam("gRotationMatrix");

		ShaderParam * uBillboardTable = mTech_LeafMirror->GetVertexShaderParamTable()->GetParam("gBillboardTable");

		uWindMatrix->SetMatrix(mWindMatrix, MTreeGlobal::K_NumWindMatrix);

		ShaderParam * uDiffuseColor = mTech_LeafMirror->GetPixelShaderParamTable()->GetParam("gDiffuseColor");

		uDiffuseColor->SetColor(RenderRegister::Instance()->GetMirrorColor());

		for (int i = 0; i < mVisbleTreeInstances.Size(); ++i)
		{
			MTreeInstance * inst = mVisbleTreeInstances[i];
			MTree * tree = inst->GetTree().c_ptr();
			Node * pNode = inst->GetAttachNode();

			Vec3 Position = pNode->GetWorldPosition();
			Quat Orientation = pNode->GetWorldOrientation();
			Vec3 Scale = pNode->GetWorldScale();

			uWindMatrixOffset->SetUnifom(inst->GetWindMatrixOffset(), 0, 0, 0);
			uTranslateScale->SetUnifom(Position.x, Position.y, Position.z, Scale.x);
			uRotationMatrix->SetMatrix(Orientation.ToMatrix());

			if (tree)
			{
				RenderOp * rop = tree->_getLeafRenderOp(0);

				if (rop)
				{
					MTree::Material * mtl = tree->_getLeafMaterial();

					tree->SetupLeafBillboard(uBillboardTable);

					rop->xform = inst->GetAttachNode()->GetWorldMatrix();

					SamplerState state;
					RenderSystem::Instance()->SetTexture(0, state, mtl->DiffuseMap.c_ptr());

					RenderSystem::Instance()->Render(mTech_LeafMirror, rop);
				}
			}
		}

		RenderSystem::Instance()->_EndEvent();
	}














	MForestListener gForestListener;

	MForestListener::MForestListener()
		: OnInit(RenderEvent::OnEngineInit, this, &MForestListener::_init)
		, OnShutdown(RenderEvent::OnEngineShutdown, this, &MForestListener::_shutdown)
		, OnUpdate(RenderEvent::OnPostUpdateScene, this, &MForestListener::_update)
		, OnRender(RenderEvent::OnAfterRenderSolid, this, &MForestListener::_render)
		, OnPreVisibleCull(RenderEvent::OnPreVisibleCull, this, &MForestListener::_preVisibleCull)
		, OnRenderDepth(Shadow::OnRenderDepth, this, &MForestListener::_renderDepth)
		, OnRenderInMirror(RenderEvent::OnMirrorRenderSolid1, this, &MForestListener::_renderInMirror)
	{
	}

	MForestListener::~MForestListener()
	{
	}

	void MForestListener::_init(Event * sender)
	{
		mForest = new MForest();
	}

	void MForestListener::_shutdown(Event * sender)
	{
		delete mForest;
	}

	void MForestListener::_update(Event * sender)
	{
		mForest->Update();
	}

	void MForestListener::_render(Event * sender)
	{
		mForest->_render();
	}

	void MForestListener::_preVisibleCull(Event * sender)
	{
		mForest->_preVisibleCull();
	}

	void MForestListener::_renderDepth(Event * sender)
	{
		int layer = *(int *)sender->GetParam(1);
		mForest->_renderDepthForShadow(layer);
	}

	void MForestListener::_renderInMirror(Event * sender)
	{
		Camera * cam = (Camera *) sender->GetParam(0);
		mForest->_renderInMirror(cam);
	}

}

