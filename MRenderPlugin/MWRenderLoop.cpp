#include "MWRenderLoop.h"
#include "MWDeferredRenderer.h"
#include "MWEnvironment.h"
#include "Engine.h"

namespace Myway {

    RenderLoop::RenderLoop(DeferredRenderer * sch)
    {
        mScheme = sch;

		const DeviceProperty * dp = Engine::Instance()->GetDeviceProperty();

		_resize(dp->Width, dp->Height);
    }

    RenderLoop::~RenderLoop()
    {
    }

	void RenderLoop::_resize(int w, int h)
	{
		mTex_Color = NULL;
		mTex_LDRColor = NULL;
		mTex_HDRColor = NULL;
		mTex_Normal = NULL;
		mTex_Depth = NULL;

		mRT_Color = NULL;
		mRT_LDRColor = NULL;
		mRT_HDRColor = NULL;
		mRT_Normal = NULL;
		mRT_Depth = NULL;
		mDepthStencil = NULL;

		mTex_LDRColor = VideoBufferManager::Instance()->CreateTextureRT("Core_TX_LDRColor", w, h, FMT_A8R8G8B8);
		mTex_HDRColor = VideoBufferManager::Instance()->CreateTextureRT("Core_TX_HDRColor", w, h, FMT_A16R16G16B16F);
		mTex_Normal = VideoBufferManager::Instance()->CreateTextureRT("Core_TX_Normal", w, h, FMT_A8R8G8B8);
		//mTex_Material = VideoBufferManager::Instance()->CreateTextureRT("Core_TX_Material", -1, -1, FMT_A16R16G16B16F);
		mTex_Depth = VideoBufferManager::Instance()->CreateTextureRT("Core_TX_Depth", w, h, FMT_R16G16F);

		mRT_LDRColor = VideoBufferManager::Instance()->CreateRenderTarget("Core_RT_LDRColor", w, h, FMT_A8R8G8B8,  MSAA_NONE);
		mRT_HDRColor = VideoBufferManager::Instance()->CreateRenderTarget("Core_RT_HDRColor", w, h, FMT_A16R16G16B16F,  MSAA_NONE);
		mRT_Normal = VideoBufferManager::Instance()->CreateRenderTarget(mTex_Normal);
		//mRT_Material = VideoBufferManager::Instance()->CreateRenderTarget(mTex_Material);
		mRT_Depth = VideoBufferManager::Instance()->CreateRenderTarget(mTex_Depth);

		mDepthStencil = VideoBufferManager::Instance()->CreateDepthStencil("Core_DepthStencil", w, h, FMT_D24S8,  MSAA_NONE);

		mRT_Color = mRT_HDRColor;
		mTex_Color = mTex_HDRColor;
	}

    void RenderLoop::DoRender()
    {
        RenderSystem * render = RenderSystem::Instance();
        RenderQueue * rq = mScheme->GetRenderQueue();

        Camera * cam = World::Instance()->MainCamera();

        render->SetViewTransform(cam->GetViewMatrix());
        render->SetProjTransform(cam->GetProjMatrix());

        RenderRegister::Instance()->SetClipPlane(cam->GetNearClip(), cam->GetFarClip());

        RenderTarget * finalRT = render->GetRenderTarget(0);

		mRT_Color = mRT_LDRColor;
		mTex_Color = mTex_LDRColor;

        // ---> Bind RenderTarget
        render->SetRenderTarget(0, mRT_LDRColor.c_ptr());
        render->SetRenderTarget(1, mRT_Normal.c_ptr());
        render->SetRenderTarget(2, mRT_Material.c_ptr());
        render->SetRenderTarget(3, mRT_Depth.c_ptr());

        render->SetDepthStencil(mDepthStencil.c_ptr());

        // ---> clear buffer
        render->ClearBuffer(NULL, false, true, false, Color::Black, 1, 0);

        _clear();

		RenderEvent::OnBeginRender(NULL, NULL);
       
        // --->render terrain
        if (Environment::Instance()->GetTerrain())
            Environment::Instance()->GetTerrain()->Render();

        // --->render object
        _renderSolidObjects(true);

		RenderEvent::OnRenderSolid1(NULL, NULL);
		RenderEvent::OnRenderSolid2(NULL, NULL);
		RenderEvent::OnRenderSolid3(NULL, NULL);

		RenderEvent::OnAfterRenderSolid(NULL, NULL);

        _updateTexture();

		render->SetRenderTarget(0, mRT_HDRColor.c_ptr());
		render->SetRenderTarget(1, NULL);
		render->SetRenderTarget(2, NULL);
		render->SetRenderTarget(3, NULL);

		_LDR_To_HDR();

		mRT_Color = mRT_HDRColor;
		mTex_Color = mTex_HDRColor;

		if (Environment::Instance()->GetColorSharp())
		{
			Environment::Instance()->GetColorSharp()->Render(mTex_Color.c_ptr(), mTex_Depth.c_ptr());
			_updateColorTexture();
		}

		// sun lighting
		if (Environment::Instance()->GetShadow())
			Environment::Instance()->GetShadow()->Do(mTex_Depth.c_ptr());

		if (Environment::Instance()->GetSun())
			Environment::Instance()->GetSun()->Lighting(mTex_Color.c_ptr(), mTex_Material.c_ptr(), mTex_Normal.c_ptr(), mTex_Depth.c_ptr());

		// ---> lighting
		_doLighting();

		if (Environment::Instance()->GetSSAO())
			Environment::Instance()->GetSSAO()->Render(mTex_Depth.c_ptr(), mTex_Normal.c_ptr());

        // --->render fog
        if (Environment::Instance()->GetFog())
            Environment::Instance()->GetFog()->Render(mTex_Depth.c_ptr());

        // ---> render sky
        if (Environment::Instance()->GetSky2())
            Environment::Instance()->GetSky2()->Render();

		// ---> render star
		if (Environment::Instance()->GetStarfield())
			Environment::Instance()->GetStarfield()->Render();

        // ---> render sun
        if (!Environment::Instance()->GetGodRay() && Environment::Instance()->GetSun())
            Environment::Instance()->GetSun()->Render();

        // ---> render moons
        if (Environment::Instance()->GetMoon())
            Environment::Instance()->GetMoon()->Render();

        // ---> render cloud
        if (Environment::Instance()->GetCloud())
            Environment::Instance()->GetCloud()->Render();

        // ---> render godray for under water
        if (Environment::Instance()->GetGodRay() && WaterManager::Instance()->IsUnderWater())
            Environment::Instance()->GetGodRay()->Render(mTex_Depth.c_ptr());

		if (Environment::Instance()->GetWaterManager())
		{
			// ---> render caustics
			/*if (WaterManager::Instance()->IsUnderWater())
				WaterManager::Instance()->RenderUnderCaustics(mTex_Depth.c_ptr(), mTex_Color.c_ptr());*/

			_updateColorTexture();

			if (WaterManager::Instance()->IsUnderWater())
				WaterManager::Instance()->RenderUnderFog(mTex_Depth.c_ptr(), mTex_Color.c_ptr());

			if (WaterManager::Instance()->IsUnderWater())
				WaterManager::Instance()->RenderUnderBubble();

			// ---> render water
			WaterManager::Instance()->Render(mTex_Depth.c_ptr(), mTex_Color.c_ptr());

			_updateColorTexture();

			//
			if (WaterManager::Instance()->IsUnderWater())
				WaterManager::Instance()->RenderUnderNoise(mTex_Color.c_ptr());

			if (WaterManager::Instance()->IsUnderWater())
				WaterManager::Instance()->RenderUnderGodRay();

			_updateColorTexture();
		}

		// ---> render godray for up water
		if (Environment::Instance()->GetGodRay() && !WaterManager::Instance()->IsUnderWater())
		{
			Environment::Instance()->GetGodRay()->Render(mTex_Depth.c_ptr());
		}

		// ---> render object
		_renderSolidObjects(false);

		// ---> soft leaf
		if (Environment::Instance()->GetSoftLeaf())
		{
			_updateColorTexture();
			Environment::Instance()->GetSoftLeaf()->Do(mTex_Color.c_ptr(), mTex_Depth.c_ptr());
		}

		RenderEvent::OnAfterDefferedShading(NULL, NULL);

		// ---> render forward objects

		// ---> render trans objects
		_randerTransObjects();

		RenderEvent::OnAfterRenderTrans(NULL, NULL);

		// ---> hdr
		_updateColorTexture();

		if (Environment::Instance()->GetHDR())
			Environment::Instance()->GetHDR()->Render(mTex_Color.c_ptr());

		RenderEvent::OnAfterRender(NULL, NULL);

		RenderEvent::OnDebugRender(NULL, NULL);

		_updateColorTexture();

		_frush(finalRT);

		render->SetDepthStencil(NULL);

		RenderEvent::OnRenderGUI1();
		RenderEvent::OnRenderGUI2();
		RenderEvent::OnRenderGUI3();
    }

    void RenderLoop::_updateTexture()
    {
        mRT_Color->Stretch(mTex_Color.c_ptr());
        //mRT_Normal->Stretch(mTex_Normal.c_ptr());
        //mRT_Material->Stretch(mTex_Material.c_ptr());
        //mRT_Depth->Stretch(mTex_Depth.c_ptr());
    }

	void RenderLoop::_LDR_To_HDR()
	{
		Technique * frushTech = mScheme->GetMainShaderProvider()->GetFrushTech();

		SamplerState state;
		state.Filter = TEXF_POINT;
		state.Address = TEXA_CLAMP;

		RenderSystem::Instance()->SetTexture(0, state, mTex_LDRColor.c_ptr());
		RenderHelper::Instance()->DrawScreenQuad(BM_OPATICY, frushTech);

		mRT_HDRColor->Stretch(mTex_HDRColor.c_ptr());
	}

    void RenderLoop::_updateColorTexture()
    {
        mRT_Color->Stretch(mTex_Color.c_ptr());
    }

    void RenderLoop::_clear()
    {
        Technique * clearTech = mScheme->GetMainShaderProvider()->GetClearTech();
        RenderHelper::Instance()->DrawScreenQuad(BM_OPATICY, clearTech);
    }

    void RenderLoop::_frush(RenderTarget * finalRT)
	{
		RenderSystem * render = RenderSystem::Instance();

		if (render->GetSMAAType() != eSmaaType::SMAA_NONE)
		{
			render->DoSMAA(mRT_Color.c_ptr(), mTex_Color.c_ptr());
			_updateColorTexture();
		}

		render->SetRenderTarget(0, finalRT);
		render->SetRenderTarget(1, NULL);
		render->SetRenderTarget(2, NULL);
		render->SetRenderTarget(3, NULL);


		/*if (dp->SmaaType != SMAA_NONE)
		{
		}
		else*/
		{
			Technique * frushTech = mScheme->GetMainShaderProvider()->GetFrushTech();

			SamplerState state;
			state.Filter = TEXF_POINT;
			state.Address = TEXA_CLAMP;

			RenderSystem::Instance()->SetTexture(0, state, mTex_Color.c_ptr());
			RenderHelper::Instance()->DrawScreenQuad(BM_OPATICY, frushTech);
		}
    }

	void RenderLoop::_renderSolidObjects(bool deffered)
	{
		RenderSystem * render = RenderSystem::Instance();
		RenderQueue * rq = mScheme->GetRenderQueue();

		render->_BeginEvent("RenderSolidObjects");

		const Array<Renderer *> & objs = rq->GetSolidRender();

		for (int i = 0; i < objs.Size(); ++i)
		{
			Renderer * rd = objs[i];

			if (rd->IsUsingDefferedShading() == deffered)
			{
				bool skined = (rd->GetBlendMatrix(NULL) > 0);
				Technique * tech = rd->GetTechnique(eRenderTechType::RTT_Base);

				if (!tech)
					tech = mScheme->GetMainShaderProvider()->GetTechnique(eRenderTechType::RTT_Base, skined);

				render->Render(tech, rd);
			}
		}

		render->_EndEvent();
	}

	void RenderLoop::_randerTransObjects()
	{
		RenderSystem * render = RenderSystem::Instance();
		RenderQueue * rq = mScheme->GetRenderQueue();

		render->_BeginEvent("RenderTransObjects");

		rq->SortTransparency(World::Instance()->MainCamera());

		const Array<Renderer *> & objs = rq->GetTransRender();

		for (int i = 0; i < objs.Size(); ++i)
		{
			Renderer * rd = objs[i];

			bool skined = (rd->GetBlendMatrix(NULL) > 0);
			Technique * tech = rd->GetTechnique(eRenderTechType::RTT_Base);

			if (!tech)
				tech = mScheme->GetMainShaderProvider()->GetTechnique(eRenderTechType::RTT_Base, skined);

			render->Render(tech, rd);
		}

		render->_EndEvent();
	}

	void RenderLoop::_doLighting()
	{
		VisibleCullResult * cullResult = mScheme->GetCullResult();

		List<Light *>::Iterator whr = cullResult->lights.Begin();
		List<Light *>::Iterator end = cullResult->lights.End();

		Technique * tPointLight = mScheme->GetMainShaderProvider()->GetTech_PointLight();

		ShaderParam * uCornerLeftTop = tPointLight->GetPixelShaderParamTable()->GetParam("gCornerLeftTop");
		ShaderParam * uCornerRightDir = tPointLight->GetPixelShaderParamTable()->GetParam("gCornerRightDir");
		ShaderParam * uCornerDownDir = tPointLight->GetPixelShaderParamTable()->GetParam("gCornerDownDir");
		ShaderParam * uLightPos = tPointLight->GetPixelShaderParamTable()->GetParam("gLightPos");
		ShaderParam * uLightParam = tPointLight->GetPixelShaderParamTable()->GetParam("gLightParam");
		ShaderParam * uDiffuse = tPointLight->GetPixelShaderParamTable()->GetParam("gDiffuse");
		ShaderParam * uSpecular = tPointLight->GetPixelShaderParamTable()->GetParam("gSpecular");

		const Vec3 * corner = World::Instance()->MainCamera()->GetCorner();

		Vec3 cornerLeftTop = corner[4];
		Vec3 cornerRightDir = corner[5] - corner[4];
		Vec3 cornerDownDir = corner[6] - corner[4];

		uCornerLeftTop->SetUnifom(cornerLeftTop.x, cornerLeftTop.y, cornerLeftTop.z, 0);
		uCornerRightDir->SetUnifom(cornerRightDir.x, cornerRightDir.y, cornerRightDir.z, 0);
		uCornerDownDir->SetUnifom(cornerDownDir.x, cornerDownDir.y, cornerDownDir.z, 0);

		while (whr != end)
		{
			Light * light = *whr;

			if (light->GetType() == LT_POINT)
			{
				Vec3 pos = light->GetPosition();
				const float range = light->GetRange();
				const Color4 & diffuse = light->GetDiffsue();
				const Color4 & specular = light->GetSpecular();

				pos *= World::Instance()->MainCamera()->GetViewMatrix();

				uLightPos->SetUnifom(pos.x, pos.y, pos.z, 0);
				uLightParam->SetUnifom(1 / range, 0, 0, 0);
				uDiffuse->SetColor(diffuse);

				SamplerState state;
				state.Address = TEXA_CLAMP;
				state.Filter = TEXF_POINT;
				RenderSystem::Instance()->SetTexture(0, state, mTex_Color.c_ptr());
				RenderSystem::Instance()->SetTexture(1, state, mTex_Normal.c_ptr());
				RenderSystem::Instance()->SetTexture(2, state, mTex_Depth.c_ptr());

				RenderHelper::Instance()->DrawScreenQuad(BM_ADD, tPointLight);
			}

			++whr;
		}
	}












}