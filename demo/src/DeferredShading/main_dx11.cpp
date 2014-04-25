#include "application.h"

// uniforms
DECLARE_UNIFORMS_BEGIN(DSAppUniforms)
	SPATIAL_DATA,
	LIGHTS_DATA,
	LIGHTS_COUNT,
	DIFFUSE_MAP,
	NORMAL_MAP,
	SPECULAR_MAP,
	SKYBOX_MAP,
	DEFAULT_SAMPLER
DECLARE_UNIFORMS_END()
#define UF framework::UniformBase<DSAppUniforms>::Uniform

// spatial data
#pragma pack (push, 1)
struct SpatialData
{
	matrix44 modelViewProjection;
	matrix44 model;
	vector3 viewDirection;
	unsigned int lightsCount;
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

		// gpu programs
		m_opaqueRendering.reset(new framework::GpuProgram());
		m_opaqueRendering->addShader("data/shaders/dx11/oit/opaque.vsh");
		m_opaqueRendering->addShader("data/shaders/dx11/oit/opaque.psh");
		if (!m_opaqueRendering->init()) exit();
		m_opaqueRendering->bindUniform<DSAppUniforms>(UF::SPATIAL_DATA, "spatialData");
		m_opaqueRendering->bindUniform<DSAppUniforms>(UF::LIGHTS_DATA, "lightsData");
		m_opaqueRendering->bindUniform<DSAppUniforms>(UF::DIFFUSE_MAP, "diffuseMap");
		m_opaqueRendering->bindUniform<DSAppUniforms>(UF::NORMAL_MAP, "normalMap");
		m_opaqueRendering->bindUniform<DSAppUniforms>(UF::SPECULAR_MAP, "specularMap");
		m_opaqueRendering->bindUniform<DSAppUniforms>(UF::DEFAULT_SAMPLER, "defaultSampler");

		m_skyboxRendering.reset(new framework::GpuProgram());
		m_skyboxRendering->addShader("data/shaders/dx11/oit/screenquad.vsh");
		m_skyboxRendering->addShader("data/shaders/dx11/oit/skybox.gsh");
		m_skyboxRendering->addShader("data/shaders/dx11/oit/skybox.psh");
		if (!m_skyboxRendering->init(true)) exit();
		m_skyboxRendering->bindUniform<DSAppUniforms>(UF::SPATIAL_DATA, "spatialData");
		m_skyboxRendering->bindUniform<DSAppUniforms>(UF::SKYBOX_MAP, "skyboxMap");
		m_skyboxRendering->bindUniform<DSAppUniforms>(UF::DEFAULT_SAMPLER, "defaultSampler");

		// entity
		m_entity = initEntity("data/media/cube/cube.geom",
							  "data/media/cube/cube_diff.dds",
							  "data/media/cube/cube_normal.dds",
							  "");
		m_entity.geometry->bindToGpuProgram(m_opaqueRendering);

		m_entitiesData.resize(25);
		for (size_t i = 0; i < m_entitiesData.size(); i++)
		{
			m_entitiesData[i].model.set_translation(utils::Utils::random(-100.0f, 100.0f));
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

		// a rasterizer to render without culling
		m_cullingOff.reset(new framework::RasterizerStage());
		D3D11_RASTERIZER_DESC rasterizerDesc = defaultRasterizer()->getDesc();
		rasterizerDesc.CullMode = D3D11_CULL_NONE;
		m_cullingOff->initWithDescription(rasterizerDesc);
		if (!m_cullingOff->isValid()) exit();
		auto viewports = defaultRasterizer()->getViewports();
		m_cullingOff->getViewports().reserve(viewports.size());
		m_cullingOff->getViewports() = viewports;

		// a blend state to enable alpha-blending
		m_alphaBlending.reset(new framework::BlendStage());
		blendDesc = framework::BlendStage::getAlphaBlending();
		for (int i = 0; i < 8; i++)
		{
			blendDesc.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
			blendDesc.RenderTarget[i].DestBlend = D3D11_BLEND_SRC_ALPHA;
		}
		m_alphaBlending->initWithDescription(blendDesc);
		if (!m_alphaBlending->isValid()) exit();

		// spatial info buffer
		m_spatialBuffer.reset(new framework::UniformBuffer());
		if (!m_spatialBuffer->initDefaultConstant<SpatialData>()) exit();

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
		ent.geometry->bindToGpuProgram(m_opaqueRendering);

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
		source.position = vector3(0, 15, 0);
		vector3 dir(1, -1, 1);
		dir.norm();
		source.orientation.set_from_axes(vector3(0, 0, 1), dir);
		source.diffuseColor = vector3(0.7f, 0.7f, 0.7f);
		source.specularColor = vector3(0.3f, 0.3f, 0.3f);
		source.ambientColor = vector3(0.1f, 0.1f, 0.1f);
		m_lightManager.addLightSource(source);

		// directional light 2
		framework::LightSource source2;
		source2.type = framework::LightType::DirectLight;
		source2.position = vector3(15, 15, 0);
		dir = vector3(0, -1, 1);
		dir.norm();
		source2.diffuseColor = vector3(0.96f, 0.81f, 0.59f) * 0.5f;
		source2.specularColor = vector3(0.1f, 0.1f, 0.1f);
		source2.ambientColor = vector3(0.0f, 0.0f, 0.0f);
		source2.orientation.set_from_axes(vector3(0, 0, 1), dir);
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
		m_overlay->setText("Fragments buffer usage = 0%\nLost fragments = 0");
		m_overlay->setVisible(m_renderDebug);
	}

	virtual void render(double elapsedTime)
	{
		m_camera.update(elapsedTime);
		update(elapsedTime);

		useDefaultRenderTarget();

		// render skybox
		m_disableDepthTest->apply();
		if (m_skyboxRendering->use())
		{
			SpatialData spatialData;
			matrix44 model;
			model.set_translation(m_camera.getPosition());
			spatialData.modelViewProjection = model * m_camera.getView() * m_camera.getProjection();
			spatialData.viewDirection = m_camera.getOrientation().z_direction();
			m_spatialBuffer->setData(spatialData);
			m_spatialBuffer->applyChanges();

			m_skyboxRendering->setUniform<DSAppUniforms>(UF::SKYBOX_MAP, m_skyboxTexture);
			m_skyboxRendering->setUniform<DSAppUniforms>(UF::DEFAULT_SAMPLER, anisotropicSampler());
			m_skyboxRendering->setUniform<DSAppUniforms>(UF::SPATIAL_DATA, m_spatialBuffer);

			getPipeline().drawPoints(1);
		}
		m_disableDepthTest->cancel();

		// render opaque objects
		if (m_opaqueRendering->use())
		{
			for (size_t i = 0; i < m_entitiesData.size(); i++)
			{
				renderEntity(m_entity, m_entitiesData[i]);
			}
		}

		// debug rendering
		renderDebug();
	}

	void renderEntity(const Entity& entity, const EntityData& entityData)
	{
		SpatialData spatialData;
		spatialData.modelViewProjection = entityData.mvp;
		spatialData.model = entityData.model;
		spatialData.viewDirection = m_camera.getOrientation().z_direction();
		spatialData.lightsCount = m_lightsCount;
		m_spatialBuffer->setData(spatialData);
		m_spatialBuffer->applyChanges();

		m_opaqueRendering->setUniform<DSAppUniforms>(UF::SPATIAL_DATA, m_spatialBuffer);
		m_opaqueRendering->setUniform<DSAppUniforms>(UF::LIGHTS_DATA, m_lightsBuffer);
		m_opaqueRendering->setUniform<DSAppUniforms>(UF::DIFFUSE_MAP, entity.texture);
		m_opaqueRendering->setUniform<DSAppUniforms>(UF::NORMAL_MAP, entity.normalTexture);
		m_opaqueRendering->setUniform<DSAppUniforms>(UF::SPECULAR_MAP, entity.specularTexture);
		m_opaqueRendering->setUniform<DSAppUniforms>(UF::DEFAULT_SAMPLER, anisotropicSampler());

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
	// gpu program to render opaque geometry
	std::shared_ptr<framework::GpuProgram> m_opaqueRendering;

	std::shared_ptr<framework::GpuProgram> m_skyboxRendering;

	std::shared_ptr<framework::BlendStage> m_disableColorWriting;
	std::shared_ptr<framework::DepthStencilStage> m_disableDepthWriting;
	std::shared_ptr<framework::RasterizerStage> m_cullingOff;
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
	};

	// opaque entity
	Entity m_entity;
	std::vector<EntityData> m_entitiesData;

	std::shared_ptr<framework::UniformBuffer> m_spatialBuffer;
	std::shared_ptr<framework::UniformBuffer> m_lightsBuffer;
	unsigned int m_lightsCount;

	std::shared_ptr<framework::Texture> m_skyboxTexture;

	framework::FreeCamera m_camera;

	bool m_pause;
	bool m_renderDebug;

	CEGUI::Window* m_overlay;
};

DECLARE_MAIN(DeferredShadingApp);