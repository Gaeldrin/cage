#include <cage-core/geometry.h>

#include <cage-engine/graphicsError.h>
#include <cage-engine/opengl.h>
#include <cage-engine/shaderConventions.h>
#include <cage-engine/shaderProgram.h>
#include <cage-engine/uniformBuffer.h>
#include <cage-engine/frameBuffer.h>
#include <cage-engine/renderQueue.h>
#include <cage-engine/window.h>
#include <cage-engine/provisionalGraphics.h>
#include <cage-engine/screenSpaceEffects.h>

#include "../engine.h"
#include "graphics.h"

#include <map>

namespace cage
{
	namespace
	{
		struct ShadowmapBuffer
		{
		private:
			uint32 width = 0;
			uint32 height = 0;
		public:
			Holder<Texture> texture;

			void resize(uint32 w, uint32 h)
			{
				if (w == width && h == height)
					return;
				width = w;
				height = h;
				texture->bind();
				if (texture->target() == GL_TEXTURE_CUBE_MAP)
					texture->imageCube(ivec2(w, h), GL_DEPTH_COMPONENT16);
				else
					texture->image2d(ivec2(w, h), GL_DEPTH_COMPONENT24);
				CAGE_CHECK_GL_ERROR_DEBUG();
			}

			ShadowmapBuffer(uint32 target)
			{
				CAGE_ASSERT(target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_2D);
				texture = newTexture(target);
				texture->setDebugName("shadowmap");
				texture->filters(GL_LINEAR, GL_LINEAR, 16);
				texture->wraps(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
				CAGE_CHECK_GL_ERROR_DEBUG();
			}
		};

		struct GraphicsDispatchHandles
		{
			FrameBufferHandle gBufferTarget;
			FrameBufferHandle renderTarget;
			TextureHandle albedoTexture;
			TextureHandle specialTexture;
			TextureHandle normalTexture;
			TextureHandle colorTexture;
			TextureHandle intermediateTexture;
			TextureHandle velocityTexture;
			TextureHandle ambientOcclusionTexture;
			TextureHandle depthTexture;
			TextureHandle texSource;
			TextureHandle texTarget;
		};

		struct GraphicsDispatchHolders : public GraphicsDispatchHandles
		{
			Holder<Model> modelSquare, modelSphere, modelCone;
			Holder<ShaderProgram> shaderVisualizeColor, shaderVisualizeDepth, shaderVisualizeMonochromatic, shaderVisualizeVelocity;
			Holder<ShaderProgram> shaderAmbient, shaderBlit, shaderDepth, shaderGBuffer, shaderLighting, shaderTranslucent;
			Holder<ShaderProgram> shaderFont;

			Holder<RenderQueue> renderQueue;
			Holder<ProvisionalGraphics> provisionalData;

			std::vector<ShadowmapBuffer> shadowmaps2d, shadowmapsCube;
		};

		struct GraphicsDispatchImpl : public GraphicsDispatch, private GraphicsDispatchHolders
		{
		public:
			uint32 drawCalls = 0;
			uint32 drawPrimitives = 0;

		private:
			uint32 frameIndex = 0;
			CameraEffectsFlags lastCameraEffects = CameraEffectsFlags::None;
			ScreenSpaceCommonConfig gfCommonConfig; // helper to simplify initialization

			void applyShaderRoutines(const ShaderConfig *c, const Holder<ShaderProgram> &s)
			{
				renderQueue->uniform(CAGE_SHADER_UNI_ROUTINES, c->shaderRoutines);
				renderQueue->checkGlErrorDebug();
			}

			void viewportAndScissor(sint32 x, sint32 y, uint32 w, uint32 h)
			{
				renderQueue->viewport(ivec2(x, y), ivec2(w, h));
			}

			void resetAllTextures()
			{
				renderQueue->resetAllTextures();
			}

			void setTwoSided(bool twoSided)
			{
				renderQueue->culling(!twoSided);
			}

			void setDepthTest(bool depthTest, bool depthWrite)
			{
				renderQueue->depthTest(depthTest);
				renderQueue->depthWrite(depthWrite);
			}

			void useDisposableUbo(uint32 bindIndex, PointerRange<const char> data)
			{
				renderQueue->universalUniformBuffer(data, bindIndex);
			}

			template<class T>
			void useDisposableUbo(uint32 bindIndex, const T &data)
			{
				renderQueue->universalUniformStruct<T>(data, bindIndex);
			}

			template<class T>
			void useDisposableUbo(uint32 bindIndex, const std::vector<T> &data)
			{
				renderQueue->universalUniformArray<T>(data, bindIndex);
			}

			void bindGBufferTextures()
			{
				const auto graphicsDebugScope = renderQueue->namedScope("bind gBuffer textures");
				renderQueue->bind(albedoTexture, CAGE_SHADER_TEXTURE_ALBEDO);
				renderQueue->bind(specialTexture, CAGE_SHADER_TEXTURE_SPECIAL);
				renderQueue->bind(normalTexture, CAGE_SHADER_TEXTURE_NORMAL);
				renderQueue->bind(colorTexture, CAGE_SHADER_TEXTURE_COLOR);
				renderQueue->bind(depthTexture, CAGE_SHADER_TEXTURE_DEPTH);
				renderQueue->activeTexture(4);
			}

			void bindShadowmap(sint32 shadowmap)
			{
				if (shadowmap != 0)
				{
					ShadowmapBuffer &s = shadowmap > 0 ? shadowmaps2d[shadowmap - 1] : shadowmapsCube[-shadowmap - 1];
					renderQueue->bind(s.texture, shadowmap > 0 ? CAGE_SHADER_TEXTURE_SHADOW : CAGE_SHADER_TEXTURE_SHADOW_CUBE);
				}
			}

			void renderEffect()
			{
				renderQueue->checkGlErrorDebug();
				renderQueue->colorTexture(0, texTarget);
#ifdef CAGE_DEBUG
				renderQueue->checkFrameBuffer();
#endif // CAGE_DEBUG
				renderQueue->bind(texSource);
				renderDispatch(modelSquare, 1);
				std::swap(texSource, texTarget);
			}

			void renderDispatch(const Objects *obj)
			{
				renderDispatch(obj->model, numeric_cast<uint32>(obj->uniModeles.size()));
			}

			void renderDispatch(const Holder<Model> &model, uint32 count)
			{
				renderQueue->draw(count);
			}

			void renderObject(const Objects *obj, const Holder<ShaderProgram> &shr)
			{
				renderObject(obj, shr, obj->model->flags);
			}

			void renderObject(const Objects *obj, const Holder<ShaderProgram> &shr, ModelRenderFlags flags)
			{
				applyShaderRoutines(&obj->shaderConfig, shr);
				useDisposableUbo(CAGE_SHADER_UNIBLOCK_MESHES, obj->uniModeles);
				if (!obj->uniArmatures.empty())
				{
					useDisposableUbo(CAGE_SHADER_UNIBLOCK_ARMATURES, obj->uniArmatures);
					renderQueue->uniform(CAGE_SHADER_UNI_BONESPERINSTANCE, obj->model->skeletonBones);
				}
				renderQueue->bind(obj->model);
				setTwoSided(any(flags & ModelRenderFlags::TwoSided));
				setDepthTest(any(flags & ModelRenderFlags::DepthTest), any(flags & ModelRenderFlags::DepthWrite));
				{ // bind textures
					for (uint32 i = 0; i < MaxTexturesCountPerMaterial; i++)
					{
						if (obj->textures[i])
						{
							switch (obj->textures[i]->target())
							{
							case GL_TEXTURE_2D_ARRAY:
								renderQueue->bind(obj->textures[i], CAGE_SHADER_TEXTURE_ALBEDO_ARRAY + i);
								break;
							case GL_TEXTURE_CUBE_MAP:
								renderQueue->bind(obj->textures[i], CAGE_SHADER_TEXTURE_ALBEDO_CUBE + i);
								break;
							default:
								renderQueue->bind(obj->textures[i], CAGE_SHADER_TEXTURE_ALBEDO + i);
								break;
							}
						}
					}
					renderQueue->activeTexture(0);
				}
				renderQueue->checkGlErrorDebug();
				renderDispatch(obj);
			}

			void renderOpaque(const RenderPass *pass)
			{
				const auto graphicsDebugScope = renderQueue->namedScope("opaque");
				OPTICK_EVENT("opaque");
				const Holder<ShaderProgram> &shr = pass->targetShadowmap ? shaderDepth : shaderGBuffer;
				renderQueue->bind(shr);
				for (const Holder<Objects> &o : pass->opaques)
					renderObject(o.get(), shr);
				renderQueue->checkGlErrorDebug();
			}

			void renderLighting(const RenderPass *pass)
			{
				const auto graphicsDebugScope = renderQueue->namedScope("lighting");
				OPTICK_EVENT("lighting");
				const Holder<ShaderProgram> &shr = shaderLighting;
				renderQueue->bind(shr);
				for (const Holder<Lights> &l : pass->lights)
				{
					applyShaderRoutines(&l->shaderConfig, shr);
					Holder<Model> model;
					switch (l->lightType)
					{
					case LightTypeEnum::Directional:
						model = modelSquare.share();
						renderQueue->cullFace(false);
						break;
					case LightTypeEnum::Spot:
						model = modelCone.share();
						renderQueue->cullFace(true);
						break;
					case LightTypeEnum::Point:
						model = modelSphere.share();
						renderQueue->cullFace(true);
						break;
					default:
						CAGE_THROW_CRITICAL(Exception, "invalid light type");
					}
					bindShadowmap(l->shadowmap);
					useDisposableUbo(CAGE_SHADER_UNIBLOCK_LIGHTS, l->uniLights);
					renderQueue->bind(model);
					renderDispatch(model, numeric_cast<uint32>(l->uniLights.size()));
				}
				renderQueue->cullFace(false);
				renderQueue->checkGlErrorDebug();
			}

			void renderTranslucent(const RenderPass *pass)
			{
				const auto graphicsDebugScope = renderQueue->namedScope("translucent");
				OPTICK_EVENT("translucent");
				const Holder<ShaderProgram> &shr = shaderTranslucent;
				renderQueue->bind(shr);
				for (const Holder<Translucent> &t : pass->translucents)
				{
					{ // render ambient object
						renderQueue->blendFuncPremultipliedTransparency();
						uint32 tmp = CAGE_SHADER_ROUTINEPROC_LIGHTFORWARDBASE;
						uint32 &orig = const_cast<uint32&>(t->object.shaderConfig.shaderRoutines[CAGE_SHADER_ROUTINEUNIF_LIGHTTYPE]);
						std::swap(tmp, orig);
						renderObject(&t->object, shr);
						std::swap(tmp, orig);
					}

					if (!t->lights.empty())
					{ // render lights on the object
						renderQueue->blendFuncAdditive();
						for (const Holder<Lights> &l : t->lights)
						{
							applyShaderRoutines(&l->shaderConfig, shr);
							bindShadowmap(l->shadowmap);
							useDisposableUbo(CAGE_SHADER_UNIBLOCK_LIGHTS, l->uniLights);
							renderDispatch(t->object.model, numeric_cast<uint32>(l->uniLights.size()));
						}
					}
				}
				renderQueue->checkGlErrorDebug();
			}

			void renderTexts(const RenderPass *pass)
			{
				const auto graphicsDebugScope = renderQueue->namedScope("texts");
				OPTICK_EVENT("texts");
				for (const Holder<Texts> &t : pass->texts)
				{
					renderQueue->bind(shaderFont);
					for (const Holder<Texts::Render> &r : t->renders)
					{
						renderQueue->uniform(0, r->transform);
						renderQueue->uniform(4, r->color);
						t->font->bind(+renderQueue, modelSquare, shaderFont);
						t->font->render(+renderQueue, r->glyphs, r->format);
					}
				}
				renderQueue->checkGlErrorDebug();
			}

			void renderCameraPass(const RenderPass *pass)
			{
				const auto graphicsDebugScope = renderQueue->namedScope("camera pass");
				OPTICK_EVENT("camera pass");

				CAGE_ASSERT(pass->entityId != 0);
				renderCameraOpaque(pass);
				renderCameraEffectsOpaque(pass);
				renderCameraTransparencies(pass);
				renderCameraEffectsFinal(pass);

				// blit to final output texture
				CAGE_ASSERT(pass->targetTexture);
				if (texSource != pass->targetTexture)
				{
					texTarget = pass->targetTexture, nullptr;
					renderQueue->bind(shaderBlit);
					renderEffect();
				}
			}

			void renderCameraOpaque(const RenderPass *pass)
			{
				const auto graphicsDebugScope = renderQueue->namedScope("deferred");
				OPTICK_EVENT("deferred");

				// opaque
				renderQueue->bind(gBufferTarget);
				renderQueue->activeAttachments(31);
				renderQueue->checkFrameBuffer();
				renderQueue->checkGlErrorDebug();
				if (pass->clearFlags)
					renderQueue->clear(pass->clearFlags & GL_COLOR_BUFFER_BIT, pass->clearFlags & GL_DEPTH_BUFFER_BIT, pass->clearFlags & GL_STENCIL_BUFFER_BIT);
				renderOpaque(pass);
				setTwoSided(false);
				setDepthTest(false, false);
				renderQueue->checkGlErrorDebug();

				// lighting
				renderQueue->bind(renderTarget);
				renderQueue->clearFrameBuffer();
				renderQueue->colorTexture(0, colorTexture);
				renderQueue->activeAttachments(1);
				renderQueue->checkFrameBuffer();
				bindGBufferTextures();
				renderQueue->blending(true);
				renderQueue->blendFuncAdditive();
				renderQueue->checkGlErrorDebug();
				renderLighting(pass);
				setDepthTest(false, false);
				renderQueue->blending(false);
				renderQueue->activeTexture(CAGE_SHADER_TEXTURE_COLOR);
				renderQueue->checkGlErrorDebug();
			}

			void renderCameraEffectsOpaque(const RenderPass *pass)
			{
				const auto graphicsDebugScope2 = renderQueue->namedScope("effects opaque");
				OPTICK_EVENT("effects opaque");

				renderQueue->bind(renderTarget);
				renderQueue->depthTexture(Holder<Texture>());
				renderQueue->colorTexture(0, Holder<Texture>());
				renderQueue->activeAttachments(1);

				texSource = colorTexture;
				texTarget = intermediateTexture;
				gfCommonConfig.resolution = ivec2(pass->vpW, pass->vpH);

				// ssao
				if (any(pass->effects & CameraEffectsFlags::AmbientOcclusion))
				{
					ScreenSpaceAmbientOcclusionConfig cfg;
					(ScreenSpaceCommonConfig &)cfg = gfCommonConfig;
					(ScreenSpaceAmbientOcclusion &)cfg = pass->ssao;
					cfg.frameIndex = frameIndex;
					cfg.inDepth = depthTexture;
					cfg.inNormal = normalTexture;
					cfg.outAo = ambientOcclusionTexture;
					cfg.viewProj = pass->viewProj;
					screenSpaceAmbientOcclusion(cfg);
				}

				// ambient light
				if ((pass->uniViewport.ambientLight + pass->uniViewport.ambientDirectionalLight) != vec4())
				{
					const auto graphicsDebugScope = renderQueue->namedScope("ambient light");
					viewportAndScissor(pass->vpX, pass->vpY, pass->vpW, pass->vpH);
					renderQueue->bind(shaderAmbient);
					renderQueue->bind(modelSquare);
					if (any(pass->effects & CameraEffectsFlags::AmbientOcclusion))
					{
						renderQueue->bind(ambientOcclusionTexture, CAGE_SHADER_TEXTURE_EFFECTS);
						renderQueue->activeTexture(CAGE_SHADER_TEXTURE_COLOR);
						renderQueue->uniform(CAGE_SHADER_UNI_AMBIENTOCCLUSION, 1);
					}
					else
						renderQueue->uniform(CAGE_SHADER_UNI_AMBIENTOCCLUSION, 0);
					renderEffect();
				}

				// depth of field
				if (any(pass->effects & CameraEffectsFlags::DepthOfField))
				{
					ScreenSpaceDepthOfFieldConfig cfg;
					(ScreenSpaceCommonConfig &)cfg = gfCommonConfig;
					(ScreenSpaceDepthOfField &)cfg = pass->depthOfField;
					cfg.proj = pass->proj;
					cfg.inDepth = depthTexture;
					cfg.inColor = texSource;
					cfg.outColor = texTarget;
					screenSpaceDepthOfField(cfg);
					std::swap(texSource, texTarget);
				}

				// motion blur
				if (any(pass->effects & CameraEffectsFlags::MotionBlur))
				{
					ScreenSpaceMotionBlurConfig cfg;
					(ScreenSpaceCommonConfig &)cfg = gfCommonConfig;
					(ScreenSpaceMotionBlur &)cfg = pass->motionBlur;
					cfg.inVelocity = velocityTexture;
					cfg.inColor = texSource;
					cfg.outColor = texTarget;
					screenSpaceMotionBlur(cfg);
					std::swap(texSource, texTarget);
				}

				// eye adaptation
				if (any(pass->effects & CameraEffectsFlags::EyeAdaptation))
				{
					ScreenSpaceEyeAdaptationConfig cfg;
					(ScreenSpaceCommonConfig &)cfg = gfCommonConfig;
					(ScreenSpaceEyeAdaptation &)cfg = pass->eyeAdaptation;
					cfg.cameraId = stringizer() + pass->entityId;
					cfg.inColor = texSource;
					screenSpaceEyeAdaptationPrepare(cfg);
				}

				viewportAndScissor(pass->vpX, pass->vpY, pass->vpW, pass->vpH);
				renderQueue->bind(renderTarget);
				renderQueue->bind(texSource, CAGE_SHADER_TEXTURE_COLOR);
			}

			void renderCameraTransparencies(const RenderPass *pass)
			{
				const auto graphicsDebugScope = renderQueue->namedScope("transparencies");
				OPTICK_EVENT("transparencies");

				if (pass->translucents.empty() && pass->texts.empty())
					return;

				if (texSource != colorTexture)
				{
					renderQueue->bind(shaderBlit);
					renderEffect();
				}
				CAGE_ASSERT(texSource == colorTexture);
				CAGE_ASSERT(texTarget == intermediateTexture);

				renderQueue->colorTexture(0, colorTexture);
				renderQueue->depthTexture(depthTexture);
				renderQueue->activeAttachments(1);
				renderQueue->checkFrameBuffer();
				renderQueue->checkGlErrorDebug();

				setDepthTest(true, true);
				renderQueue->blending(true);
				renderQueue->checkGlErrorDebug();

				renderTranslucent(pass);

				setDepthTest(true, false);
				setTwoSided(true);
				renderQueue->blendFuncAlphaTransparency();
				renderQueue->checkGlErrorDebug();

				renderTexts(pass);

				setDepthTest(false, false);
				setTwoSided(false);
				renderQueue->blending(false);
				renderQueue->activeTexture(CAGE_SHADER_TEXTURE_COLOR);
				renderQueue->checkGlErrorDebug();

				renderQueue->depthTexture(Holder<Texture>());
				renderQueue->activeAttachments(1);
				renderQueue->bind(modelSquare);
				bindGBufferTextures();
			}

			void renderCameraEffectsFinal(const RenderPass *pass)
			{
				const auto graphicsDebugScope2 = renderQueue->namedScope("effects final");
				OPTICK_EVENT("effects final");

				gfCommonConfig.resolution = ivec2(pass->vpW, pass->vpH);

				// bloom
				if (any(pass->effects & CameraEffectsFlags::Bloom))
				{
					ScreenSpaceBloomConfig cfg;
					(ScreenSpaceCommonConfig &)cfg = gfCommonConfig;
					(ScreenSpaceBloom &)cfg = pass->bloom;
					cfg.inColor = texSource;
					cfg.outColor = texTarget;
					screenSpaceBloom(cfg);
					std::swap(texSource, texTarget);
				}

				// eye adaptation
				if (any(pass->effects & CameraEffectsFlags::EyeAdaptation))
				{
					ScreenSpaceEyeAdaptationConfig cfg;
					(ScreenSpaceCommonConfig &)cfg = gfCommonConfig;
					(ScreenSpaceEyeAdaptation &)cfg = pass->eyeAdaptation;
					cfg.cameraId = stringizer() + pass->entityId;
					cfg.inColor = texSource;
					cfg.outColor = texTarget;
					screenSpaceEyeAdaptationApply(cfg);
					std::swap(texSource, texTarget);
				}

				// final screen effects
				if (any(pass->effects & (CameraEffectsFlags::ToneMapping | CameraEffectsFlags::GammaCorrection)))
				{
					ScreenSpaceTonemapConfig cfg;
					(ScreenSpaceCommonConfig &)cfg = gfCommonConfig;
					(ScreenSpaceTonemap &)cfg = pass->tonemap;
					cfg.inColor = texSource;
					cfg.outColor = texTarget;
					cfg.tonemapEnabled = any(pass->effects & CameraEffectsFlags::ToneMapping);
					cfg.gamma = any(pass->effects & CameraEffectsFlags::GammaCorrection) ? pass->gamma : 1;
					screenSpaceTonemap(cfg);
					std::swap(texSource, texTarget);
				}

				// fxaa
				if (any(pass->effects & CameraEffectsFlags::AntiAliasing))
				{
					ScreenSpaceFastApproximateAntiAliasingConfig cfg;
					(ScreenSpaceCommonConfig &)cfg = gfCommonConfig;
					cfg.inColor = texSource;
					cfg.outColor = texTarget;
					screenSpaceFastApproximateAntiAliasing(cfg);
					std::swap(texSource, texTarget);
				}

				viewportAndScissor(pass->vpX, pass->vpY, pass->vpW, pass->vpH);
				renderQueue->bind(renderTarget);
				renderQueue->bind(texSource, CAGE_SHADER_TEXTURE_COLOR);
			}

			void renderShadowPass(const RenderPass *pass)
			{
				const auto graphicsDebugScope = renderQueue->namedScope("shadow pass");
				OPTICK_EVENT("shadow pass");

				renderQueue->bind(renderTarget);
				renderQueue->clearFrameBuffer();
				renderQueue->depthTexture(pass->targetShadowmap > 0 ? shadowmaps2d[pass->targetShadowmap - 1].texture : shadowmapsCube[-pass->targetShadowmap - 1].texture);
				renderQueue->activeAttachments(0);
				renderQueue->checkFrameBuffer();
				renderQueue->colorWrite(false);
				renderQueue->clear(false, true);
				renderQueue->checkGlErrorDebug();

				const Holder<ShaderProgram> &shr = shaderDepth;
				renderQueue->bind(shr);
				for (const Holder<Objects> &o : pass->opaques)
					renderObject(o.get(), shr, o->model->flags | ModelRenderFlags::DepthWrite);
				for (const Holder<Translucent> &o : pass->translucents)
					renderObject(&o->object, shr, o->object.model->flags | ModelRenderFlags::DepthWrite);

				renderQueue->colorWrite(true);
				renderQueue->checkGlErrorDebug();
			}

			void renderPass(const RenderPass *pass)
			{
				useDisposableUbo(CAGE_SHADER_UNIBLOCK_VIEWPORT, pass->uniViewport);
				viewportAndScissor(0, 0, pass->vpW, pass->vpH);
				renderQueue->depthTest(true);
				renderQueue->scissors(true);
				renderQueue->blending(false);
				setTwoSided(false);
				setDepthTest(true, true);
				resetAllTextures();
				renderQueue->checkGlErrorDebug();

				if (pass->targetShadowmap == 0)
					renderCameraPass(pass);
				else
					renderShadowPass(pass);

				setTwoSided(false);
				setDepthTest(false, false);
				resetAllTextures();
				renderQueue->checkGlErrorDebug();
			}

			void blitToWindow(const RenderPass *pass)
			{
				const auto graphicsDebugScope = renderQueue->namedScope("blit to window");
				renderQueue->resetFrameBuffer();
				viewportAndScissor(pass->vpX, pass->vpY, pass->vpW, pass->vpH);
				setTwoSided(false);
				setDepthTest(false, false);
				renderQueue->bind(colorTexture, 0);
				renderQueue->bind(shaderVisualizeColor);
				renderQueue->uniform(0, vec2(1.0 / windowWidth, 1.0 / windowHeight));
				renderQueue->bind(modelSquare);
				renderQueue->draw();
				renderQueue->checkGlErrorDebug();
			}

			TextureHandle prepareTexture(const RenderPass *pass, const char *name, bool enabled, uint32 internalFormat, uint32 downscale = 1, bool mipmaps = false)
			{
				if (!enabled)
					return {};
				RenderQueue *q = +renderQueue;
				TextureHandle t = provisionalData->texture(name);
				q->bind(t, 0);
				q->filters(mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR, GL_LINEAR, 0);
				q->wraps(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
				q->image2d(max(ivec2(pass->vpW, pass->vpH) / downscale, 1u), internalFormat);
				if (mipmaps)
					q->generateMipmaps();
				return t;
			}

			void prepareGBuffer(const RenderPass *pass)
			{
				const auto graphicsDebugScope = renderQueue->namedScope("prepare gBuffer");

				albedoTexture = prepareTexture(pass, "albedoTexture", true, GL_RGB8);
				specialTexture = prepareTexture(pass, "specialTexture", true, GL_RG8);
				normalTexture = prepareTexture(pass, "normalTexture", true, GL_RGB16F);
				colorTexture = prepareTexture(pass, "colorTexture", true, GL_RGB16F);
				velocityTexture = prepareTexture(pass, "velocityTexture", any(pass->effects & CameraEffectsFlags::MotionBlur), GL_RG16F);
				depthTexture = prepareTexture(pass, "depthTexture", true, GL_DEPTH_COMPONENT32);
				intermediateTexture = prepareTexture(pass, "intermediateTexture", true, GL_RGB16F);
				ambientOcclusionTexture = prepareTexture(pass, "ambientOcclusionTexture", any(pass->effects & CameraEffectsFlags::AmbientOcclusion), GL_R8, CAGE_SHADER_SSAO_DOWNSCALE);
				CAGE_CHECK_GL_ERROR_DEBUG();

				RenderQueue *q = +renderQueue;
				gBufferTarget = provisionalData->frameBufferDraw("gBuffer");
				q->bind(gBufferTarget);
				q->colorTexture(CAGE_SHADER_ATTRIB_OUT_ALBEDO, albedoTexture);
				q->colorTexture(CAGE_SHADER_ATTRIB_OUT_SPECIAL, specialTexture);
				q->colorTexture(CAGE_SHADER_ATTRIB_OUT_NORMAL, normalTexture);
				q->colorTexture(CAGE_SHADER_ATTRIB_OUT_COLOR, colorTexture);
				q->colorTexture(CAGE_SHADER_ATTRIB_OUT_VELOCITY, any(pass->effects & CameraEffectsFlags::MotionBlur) ? velocityTexture : TextureHandle());
				q->depthTexture(depthTexture);
				q->resetFrameBuffer();
				CAGE_CHECK_GL_ERROR_DEBUG();
			}

		public:
			explicit GraphicsDispatchImpl(const EngineCreateConfig &config)
			{}

			void initialize()
			{
				shadowmaps2d.reserve(4);
				shadowmapsCube.reserve(32);

				gBufferTarget = newFrameBufferDraw();
				renderTarget = newFrameBufferDraw();
				CAGE_CHECK_GL_ERROR_DEBUG();

				renderQueue = newRenderQueue();
				provisionalData = newProvisionalGraphics();
			}

			void finalize()
			{
				*(GraphicsDispatchHolders*)this = GraphicsDispatchHolders();
			}

			void tick()
			{
				GraphicsDebugScope graphicsDebugScope("engine tick");

				drawCalls = 0;
				drawPrimitives = 0;

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				CAGE_CHECK_GL_ERROR_DEBUG();

				if (windowWidth == 0 || windowHeight == 0)
					return;

				AssetManager *ass = engineAssets();
				if (!ass->get<AssetSchemeIndexPack, AssetPack>(HashString("cage/cage.pack")))
					return;

				modelSquare = ass->get<AssetSchemeIndexModel, Model>(HashString("cage/model/square.obj"));
				modelSphere = ass->get<AssetSchemeIndexModel, Model>(HashString("cage/model/sphere.obj"));
				modelCone = ass->get<AssetSchemeIndexModel, Model>(HashString("cage/model/cone.obj"));
				shaderVisualizeColor = ass->get<AssetSchemeIndexShaderProgram, ShaderProgram>(HashString("cage/shader/engine/visualize/color.glsl"));
				shaderVisualizeDepth = ass->get<AssetSchemeIndexShaderProgram, ShaderProgram>(HashString("cage/shader/engine/visualize/depth.glsl"));
				shaderVisualizeMonochromatic = ass->get<AssetSchemeIndexShaderProgram, ShaderProgram>(HashString("cage/shader/engine/visualize/monochromatic.glsl"));
				shaderVisualizeVelocity = ass->get<AssetSchemeIndexShaderProgram, ShaderProgram>(HashString("cage/shader/engine/visualize/velocity.glsl"));
				shaderAmbient = ass->get<AssetSchemeIndexShaderProgram, ShaderProgram>(HashString("cage/shader/engine/ambient.glsl"));
				shaderBlit = ass->get<AssetSchemeIndexShaderProgram, ShaderProgram>(HashString("cage/shader/engine/blit.glsl"));
				shaderDepth = ass->get<AssetSchemeIndexShaderProgram, ShaderProgram>(HashString("cage/shader/engine/depth.glsl"));
				shaderGBuffer = ass->get<AssetSchemeIndexShaderProgram, ShaderProgram>(HashString("cage/shader/engine/gBuffer.glsl"));
				shaderLighting = ass->get<AssetSchemeIndexShaderProgram, ShaderProgram>(HashString("cage/shader/engine/lighting.glsl"));
				shaderTranslucent = ass->get<AssetSchemeIndexShaderProgram, ShaderProgram>(HashString("cage/shader/engine/translucent.glsl"));
				shaderFont = ass->get<AssetSchemeIndexShaderProgram, ShaderProgram>(HashString("cage/shader/gui/font.glsl"));

				if (!shaderBlit)
					return;

				renderQueue->resetQueue();
				*(GraphicsDispatchHandles *)this = {};
				provisionalData->reset();

				if (renderPasses.empty())
					return;

				gfCommonConfig.assets = engineAssets();
				gfCommonConfig.provisionals = +provisionalData;
				gfCommonConfig.queue = +renderQueue;

				renderTarget = provisionalData->frameBufferDraw("renderTarget");

				// render all passes
				for (const Holder<RenderPass> &pass : renderPasses)
				{
					if (pass->targetShadowmap > 0)
					{ // 2d shadowmap
						uint32 idx = pass->targetShadowmap - 1;
						while (shadowmaps2d.size() <= idx)
							shadowmaps2d.push_back(ShadowmapBuffer(GL_TEXTURE_2D));
						ShadowmapBuffer &s = shadowmaps2d[idx];
						s.resize(pass->shadowmapResolution, pass->shadowmapResolution);
						renderPass(+pass);
						continue;
					}
					else if (pass->targetShadowmap < 0)
					{ // cube shadowmap
						uint32 idx = -pass->targetShadowmap - 1;
						while (shadowmapsCube.size() <= idx)
							shadowmapsCube.push_back(ShadowmapBuffer(GL_TEXTURE_CUBE_MAP));
						ShadowmapBuffer &s = shadowmapsCube[idx];
						s.resize(pass->shadowmapResolution, pass->shadowmapResolution);
						renderPass(+pass);
						continue;
					}
					else
					{ // regular scene render
						CAGE_ASSERT(pass->targetShadowmap == 0);
						prepareGBuffer(+pass);
						if (pass->targetTexture)
						{ // render to texture
							renderPass(+pass);
						}
						else
						{ // render to screen
							pass->targetTexture = colorTexture;
							renderPass(+pass);
							blitToWindow(+pass);
						}
					}
				}

				{
					OPTICK_EVENT("dispatch render queue");
					renderQueue->dispatch();
				}

				{ // check gl errors (even in release, but do not halt the game)
					try
					{
						checkGlError();
					}
					catch (const GraphicsError &)
					{
					}
				}

				frameIndex++;
				drawCalls = renderQueue->drawsCount();
				drawPrimitives = renderQueue->primitivesCount();
			}

			void swap()
			{
				CAGE_CHECK_GL_ERROR_DEBUG();
				engineWindow()->swapBuffers();
				glFinish(); // this is where the engine should be waiting for the gpu
			}
		};
	}

	GraphicsDispatch *graphicsDispatch;

	void graphicsDispatchCreate(const EngineCreateConfig &config)
	{
		graphicsDispatch = systemArena().createObject<GraphicsDispatchImpl>(config);
	}

	void graphicsDispatchDestroy()
	{
		systemArena().destroy<GraphicsDispatchImpl>(graphicsDispatch);
		graphicsDispatch = nullptr;
	}

	void graphicsDispatchInitialize()
	{
		((GraphicsDispatchImpl*)graphicsDispatch)->initialize();
	}

	void graphicsDispatchFinalize()
	{
		((GraphicsDispatchImpl*)graphicsDispatch)->finalize();
	}

	void graphicsDispatchTick(uint32 &drawCalls, uint32 &drawPrimitives)
	{
		GraphicsDispatchImpl *impl = (GraphicsDispatchImpl*)graphicsDispatch;
		impl->tick();
		drawCalls = impl->drawCalls;
		drawPrimitives = impl->drawPrimitives;
	}

	void graphicsDispatchSwap()
	{
		((GraphicsDispatchImpl*)graphicsDispatch)->swap();
	}
}
