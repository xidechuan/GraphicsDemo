#include "application.h"

// uniforms
DECLARE_UNIFORMS_BEGIN(DSAppUniforms)
	ENTITY_DATA,
	ONFRAME_DATA,
	LIGHTS_DATA,
	LIGHTS_COUNT,
	DIFFUSE_MAP,
	NORMAL_MAP,
	SPECULAR_MAP,
	SKYBOX_MAP,
	DEFAULT_SAMPLER
DECLARE_UNIFORMS_END()
#define UF framework::UniformBase<DSAppUniforms>::Uniform

// entity data
#pragma pack (push, 1)
struct EntityDataRaw
{
	matrix44 modelViewProjection;
	matrix44 model;
	float specularPower;
	unsigned int materialId;
	unsigned int : 32;
	unsigned int : 32;
};
#pragma pack (pop)

// on frame data
#pragma pack (push, 1)
struct OnFrameDataRaw
{
	vector3 viewPosition;
	unsigned int lightsCount;
	unsigned int screenWidth;
	unsigned int screenHeight;
	unsigned int : 32;
	unsigned int : 32;
};
#pragma pack (pop)

// constants
const int MAX_LIGHTS_COUNT = 16;

// application
class DeferredShadingApp : public framework::Application
{
	struct Entity;
	struct EntityData;

public:
	DeferredShadingApp()
	{
		m_lightsCount = 0;
		m_pause = false;
		m_renderDebug = false;
	}

	virtual void init(const std::map<std::string, int>& params)
	{
		m_info.title = "Deferred shading (DX11)";

		auto w = params.find("w");
		auto h = params.find("h");
		if (w != params.end() && h != params.end())
		{
			m_info.windowWidth = w->second;
			m_info.windowHeight = h->second;
		}

		auto msaa = params.find("msaa");
		if (msaa != params.end())
		{
			m_info.samples = msaa->second;
		}
		else
		{
			m_info.samples = 0;
		}

		auto fullscreen = params.find("fullscreen");
		if (fullscreen != params.end())
		{
			m_info.flags.fullscreen = (fullscreen->second != 0 ? 1 : 0);
		}
		else
		{
			m_info.flags.fullscreen = 0;
		}

		//setLegend("WASD - move camera\nLeft mouse button - rotate camera\nF1 - debug info");
	}

	virtual void startup(CEGUI::DefaultWindow* root)
	{
		// camera
		m_camera.initWithPositionDirection(m_info.windowWidth, m_info.windowHeight, vector3(0, 50, -100), vector3());

		// overlays
		initOverlays(root);

		// mask buffer
		m_maskBuffer.reset(new framework::RenderTarget());
		auto maskBufferDesc = framework::RenderTarget::getDefaultDesc(m_info.windowWidth,
																	  m_info.windowHeight,
																	  DXGI_FORMAT_R32_UINT);
		maskBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		m_maskBuffer->initWithDescription(maskBufferDesc, false);
		if (!m_maskBuffer->isValid()) exit();

		// g-buffer
		unsigned int gbufferSize = (unsigned int)m_info.windowWidth * m_info.windowHeight;
		unsigned int gbufferElemSize = 12 + 12 + 4 + 4 + 4 + 4;
		m_gbuffer.reset(new framework::UnorderedAccessBuffer());
		if (!m_gbuffer->initDefaultUnorderedAccess(gbufferSize, gbufferElemSize)) exit();

		// gpu programs
		m_gbufferRendering.reset(new framework::GpuProgram());
		m_gbufferRendering->addShader("data/shaders/dx11/deferredshading/gbuffer.vsh");
		m_gbufferRendering->addShader("data/shaders/dx11/deferredshading/gbuffer.psh");
		if (!m_gbufferRendering->init()) exit();
		m_gbufferRendering->bindUniform<DSAppUniforms>(UF::ENTITY_DATA, "entityData");
		m_gbufferRendering->bindUniform<DSAppUniforms>(UF::ONFRAME_DATA, "onFrameData");
		m_gbufferRendering->bindUniform<DSAppUniforms>(UF::DIFFUSE_MAP, "diffuseMap");
		m_gbufferRendering->bindUniform<DSAppUniforms>(UF::NORMAL_MAP, "normalMap");
		m_gbufferRendering->bindUniform<DSAppUniforms>(UF::SPECULAR_MAP, "specularMap");
		m_gbufferRendering->bindUniform<DSAppUniforms>(UF::DEFAULT_SAMPLER, "defaultSampler");

		m_skyboxRendering.reset(new framework::GpuProgram());
		m_skyboxRendering->addShader("data/shaders/dx11/deferredshading/screenquad.vsh");
		m_skyboxRendering->addShader("data/shaders/dx11/deferredshading/skybox.gsh");
		m_skyboxRendering->addShader("data/shaders/dx11/deferredshading/skybox.psh");
		if (!m_skyboxRendering->init(true)) exit();
		m_skyboxRendering->bindUniform<DSAppUniforms>(UF::ENTITY_DATA, "entityData");
		m_skyboxRendering->bindUniform<DSAppUniforms>(UF::SKYBOX_MAP, "skyboxMap");
		//m_skyboxRendering->bindUniform<DSAppUniforms>(UF::ONFRAME_DATA, "onFrameData");
		m_skyboxRendering->bindUniform<DSAppUniforms>(UF::DEFAULT_SAMPLER, "defaultSampler");

		m_deferredShading.reset(new framework::GpuProgram());
		m_deferredShading->addShader("data/shaders/dx11/deferredshading/screenquad.vsh");
		m_deferredShading->addShader("data/shaders/dx11/deferredshading/screenquad.gsh");
		m_deferredShading->addShader("data/shaders/dx11/deferredshading/deferredshading.psh");
		if (!m_deferredShading->init(true)) exit();
		m_deferredShading->bindUniform<DSAppUniforms>(UF::LIGHTS_DATA, "lightsData");
		m_deferredShading->bindUniform<DSAppUniforms>(UF::ONFRAME_DATA, "onFrameData");

		// entity
		m_entity = initEntity("data/media/cube/cube.geom",
							  "data/media/cube/cube_diff.dds",
							  "data/media/cube/cube_normal.dds",
							  "data/media/textures/full_specular.dds");
		m_entity.geometry->bindToGpuProgram(m_gbufferRendering);

		m_entitiesData.resize(1);
		for (size_t i = 0; i < m_entitiesData.size(); i++)
		{
			m_entitiesData[i].model.set_translation(vector3(0,0,0));
			//m_entitiesData[i].model.set_translation(utils::Utils::random(-100.0f, 100.0f));
		}

		// skybox texture
		m_skyboxTexture.reset(new framework::Texture());
		if (!m_skyboxTexture->initWithDDS("data/media/textures/nightsky2.dds")) exit();

		// a blend state to disable color writing
		m_disableColorWriting.reset(new framework::BlendStage());
		D3D11_BLEND_DESC blendDesc = framework::BlendStage::getDisableColorWriting();
		m_disableColorWriting->initWithDescription(blendDesc);
		if (!m_disableColorWriting->isValid()) exit();

		// a depth-stencil state to disable depth writing
		m_disableDepthWriting.reset(new framework::DepthStencilStage());
		D3D11_DEPTH_STENCIL_DESC depthDesc = framework::DepthStencilStage::getDisableDepthWriting();
		m_disableDepthWriting->initWithDescription(depthDesc);
		if (!m_disableDepthWriting->isValid()) exit();

		// a depth-stencil state to disable depth test
		m_disableDepthTest.reset(new framework::DepthStencilStage());
		depthDesc = framework::DepthStencilStage::getDefault();
		depthDesc.DepthEnable = FALSE;
		m_disableDepthTest->initWithDescription(depthDesc);
		if (!m_disableDepthTest->isValid()) exit();

		// a blend state to enable alpha-blending
		m_alphaBlending.reset(new framework::BlendStage());
		blendDesc = framework::BlendStage::getAlphaBlending();
		m_alphaBlending->initWithDescription(blendDesc);
		if (!m_alphaBlending->isValid()) exit();

		// entity's data buffer
		m_entityDataBuffer.reset(new framework::UniformBuffer());
		if (!m_entityDataBuffer->initDefaultConstant<EntityDataRaw>()) exit();

		// on-frame data buffer
		m_onFrameDataBuffer.reset(new framework::UniformBuffer());
		if (!m_onFrameDataBuffer->initDefaultConstant<OnFrameDataRaw>()) exit();

		// lights
		initLights();
	}

	Entity initEntity(const std::string& geometry, 
					  const std::string& texture, 
					  const std::string& normalTexture, 
					  const std::string& specularTexture)
	{
		Entity ent;

		// geometry
		ent.geometry.reset(new framework::Geometry3D());
		if (!ent.geometry->init(geometry)) exit();

		// textures
		if (!texture.empty())
		{
			ent.texture.reset(new framework::Texture());
			ent.texture->initWithDDS(texture);
		}

		if (!normalTexture.empty())
		{
			ent.normalTexture.reset(new framework::Texture());
			ent.normalTexture->initWithDDS(normalTexture);
		}

		if (!specularTexture.empty())
		{
			ent.specularTexture.reset(new framework::Texture());
			ent.specularTexture->initWithDDS(specularTexture);
		}

		return std::move(ent);
	}

	void initLights()
	{
		// directional light
		framework::LightSource source;
		source.type = framework::LightType::DirectLight;
		source.position = vector3(-11, 11, -1);
		vector3 dir(1, -1, 1);
		dir.norm();
		source.orientation.set_from_axes(vector3(0, 0, 1), dir);
		source.diffuseColor = vector3(1.0f, 1.0f, 1.0f);
		source.specularColor = vector3(0.5f, 0.5f, 0.5f);
		source.ambientColor = vector3(0.3f, 0.3f, 0.3f);
		m_lightManager.addLightSource(source);

		// omni light
		framework::LightSource source2;
		source2.type = framework::LightType::OmniLight;
		source2.position = vector3(15, 15, 0);
		source2.diffuseColor = vector3(0.96f, 0.81f, 0.59f);
		source2.specularColor = vector3(0.1f, 0.1f, 0.1f);
		source2.ambientColor = vector3(0.0f, 0.0f, 0.0f);
		source2.falloff = 40.0f;
		m_lightManager.addLightSource(source2);

		// light buffer
		m_lightsBuffer.reset(new framework::UniformBuffer());
		if (!m_lightsBuffer->initDefaultStructured<framework::LightRawData>((size_t)MAX_LIGHTS_COUNT)) exit();

		m_lightsCount = std::min((int)m_lightManager.getLightSourcesCount(), MAX_LIGHTS_COUNT);
		for (unsigned int i = 0; i < m_lightsCount; i++)
		{
			m_lightsBuffer->setElement((int)i, m_lightManager.getRawLightData(i));
		}
		m_lightsBuffer->applyChanges();
	}

	void initOverlays(CEGUI::DefaultWindow* root)
	{
		CEGUI::WindowManager& winMgr = CEGUI::WindowManager::getSingleton();
		m_overlay = (CEGUI::Window*)winMgr.createWindow(getGuiFullName("/Label").c_str());
		root->addChild(m_overlay);

		m_overlay->setPosition(CEGUI::UVector2(CEGUI::UDim(1.0f, -300.0f), CEGUI::UDim(1.0f, -150.0f)));
		m_overlay->setSize(CEGUI::USize(cegui_absdim(300.0f), cegui_absdim(150.0f)));
		m_overlay->setProperty("HorzFormatting", "RightAligned");
		m_overlay->setProperty("VertFormatting", "BottomAligned");
		m_overlay->setText("");
		m_overlay->setVisible(m_renderDebug);
	}

	virtual void render(double elapsedTime)
	{
		m_camera.update(elapsedTime);
		update(elapsedTime);

		useDefaultRenderTarget();

		// set up on-frame data
		OnFrameDataRaw onFrameData;
		onFrameData.viewPosition = m_camera.getPosition();
		onFrameData.lightsCount = m_lightsCount;
		onFrameData.screenWidth = m_info.windowWidth;
		onFrameData.screenHeight = m_info.windowHeight;
		m_onFrameDataBuffer->setData(onFrameData);
		m_onFrameDataBuffer->applyChanges();

		// clear only mask buffer
		framework::UnorderedAccessibleBatch clearbatch;
		clearbatch.add(m_maskBuffer);
		getPipeline().clearUnorderedAccessBatch(clearbatch);

		// set render target and mask/g- buffers
		framework::UnorderedAccessibleBatch batch;
		batch.add(m_maskBuffer);
		batch.add(m_gbuffer);
		getPipeline().setRenderTarget(defaultRenderTarget(), batch);

		// render skybox
		renderSkybox();

		// render to g-buffer
		if (m_gbufferRendering->use())
		{
			m_disableColorWriting->apply();
			for (size_t i = 0; i < m_entitiesData.size(); i++)
			{
				renderEntity(m_entity, m_entitiesData[i]);
			}
			m_disableColorWriting->cancel();
		}

		// deferred shading pass
		if (m_deferredShading->use())
		{
			m_deferredShading->setUniform<DSAppUniforms>(UF::LIGHTS_DATA, m_lightsBuffer);
			m_deferredShading->setUniform<DSAppUniforms>(UF::ONFRAME_DATA, m_onFrameDataBuffer);

			m_disableDepthTest->apply();
			m_alphaBlending->apply();
			getPipeline().drawPoints(1);
			m_disableDepthTest->cancel();
			m_alphaBlending->cancel();
		}

		// debug rendering
		renderDebug();
	}

	void renderSkybox()
	{
		if (m_skyboxRendering->use())
		{
			m_disableDepthTest->apply();

			EntityDataRaw entityDataRaw;
			matrix44 model;
			model.set_translation(m_camera.getPosition());
			entityDataRaw.modelViewProjection = model * m_camera.getView() * m_camera.getProjection();
			m_entityDataBuffer->setData(entityDataRaw);
			m_entityDataBuffer->applyChanges();

			m_skyboxRendering->setUniform<DSAppUniforms>(UF::SKYBOX_MAP, m_skyboxTexture);
			m_skyboxRendering->setUniform<DSAppUniforms>(UF::DEFAULT_SAMPLER, anisotropicSampler());
			m_skyboxRendering->setUniform<DSAppUniforms>(UF::ENTITY_DATA, m_entityDataBuffer);
			//m_skyboxRendering->setUniform<DSAppUniforms>(UF::ONFRAME_DATA, m_onFrameDataBuffer);

			getPipeline().drawPoints(1);

			m_disableDepthTest->cancel();
		}
	}

	void renderEntity(const Entity& entity, const EntityData& entityData)
	{
		EntityDataRaw entityDataRaw;
		entityDataRaw.modelViewProjection = entityData.mvp;
		entityDataRaw.model = entityData.model;
		entityDataRaw.specularPower = entityData.specularPower;
		entityDataRaw.materialId = entityData.materialId;
		m_entityDataBuffer->setData(entityDataRaw);
		m_entityDataBuffer->applyChanges();

		m_gbufferRendering->setUniform<DSAppUniforms>(UF::ONFRAME_DATA, m_onFrameDataBuffer);
		m_gbufferRendering->setUniform<DSAppUniforms>(UF::ENTITY_DATA, m_entityDataBuffer);
		m_gbufferRendering->setUniform<DSAppUniforms>(UF::DIFFUSE_MAP, entity.texture);
		m_gbufferRendering->setUniform<DSAppUniforms>(UF::NORMAL_MAP, entity.normalTexture);
		m_gbufferRendering->setUniform<DSAppUniforms>(UF::SPECULAR_MAP, entity.specularTexture);
		m_gbufferRendering->setUniform<DSAppUniforms>(UF::DEFAULT_SAMPLER, anisotropicSampler());

		entity.geometry->renderAllMeshes();
	}

	void renderDebug()
	{
		if (!m_renderDebug) return;

		//static char buf[100];
		//sprintf(buf, "", );
		//m_overlay->setText(buf);

		matrix44 vp = m_camera.getView() * m_camera.getProjection();
		renderAxes(vp);
		m_lightManager.renderDebugVisualization(vp);
	}

	virtual void onKeyButton(int key, int scancode, bool pressed)
	{
		if (key == CEGUI::Key::Space && pressed)
		{
			m_pause = !m_pause;
			return;
		}
		if (key == CEGUI::Key::F1 && pressed)
		{
			m_renderDebug = !m_renderDebug;
			m_overlay->setVisible(m_renderDebug);
			return;
		}
		m_camera.onKeyButton(key, scancode, pressed);
	}

	virtual void onMouseButton(double xpos, double ypos, int button, bool pressed)
	{
		m_camera.onMouseButton(xpos, ypos, button, pressed);
	}

	virtual void onMouseMove(double xpos, double ypos)
	{
		m_camera.onMouseMove(xpos, ypos);
	}

	void update(double elapsedTime)
	{
		matrix44 vp = m_camera.getView() * m_camera.getProjection();
		for (size_t i = 0; i < m_entitiesData.size(); i++)
		{
			m_entitiesData[i].mvp = m_entitiesData[i].model * vp;
		}
	}

private:
	std::shared_ptr<framework::RenderTarget> m_maskBuffer;
	std::shared_ptr<framework::UnorderedAccessBuffer> m_gbuffer;

	// gpu program to render in g-buffer
	std::shared_ptr<framework::GpuProgram> m_gbufferRendering;
	// gpu program to render skybox
	std::shared_ptr<framework::GpuProgram> m_skyboxRendering;
	//
	std::shared_ptr<framework::GpuProgram> m_deferredShading;

	std::shared_ptr<framework::BlendStage> m_disableColorWriting;
	std::shared_ptr<framework::DepthStencilStage> m_disableDepthWriting;
	std::shared_ptr<framework::DepthStencilStage> m_disableDepthTest;
	std::shared_ptr<framework::BlendStage> m_alphaBlending;

	// entity
	struct Entity
	{
		std::shared_ptr<framework::Geometry3D> geometry;
		std::shared_ptr<framework::Texture> texture;
		std::shared_ptr<framework::Texture> normalTexture;
		std::shared_ptr<framework::Texture> specularTexture;
	};

	struct EntityData
	{
		matrix44 model;
		matrix44 mvp;
		float specularPower;
		unsigned int materialId;

		EntityData() : specularPower(30.0f), materialId(1){}
	};

	// opaque entity
	Entity m_entity;
	std::vector<EntityData> m_entitiesData;

	std::shared_ptr<framework::UniformBuffer> m_entityDataBuffer;
	std::shared_ptr<framework::UniformBuffer> m_onFrameDataBuffer;
	std::shared_ptr<framework::UniformBuffer> m_lightsBuffer;
	unsigned int m_lightsCount;

	std::shared_ptr<framework::Texture> m_skyboxTexture;

	framework::FreeCamera m_camera;

	bool m_pause;
	bool m_renderDebug;

	CEGUI::Window* m_overlay;
};

DECLARE_MAIN(DeferredShadingApp);