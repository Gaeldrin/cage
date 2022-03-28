#include <cage-core/config.h>
#include <cage-core/files.h>
#include <cage-core/lineReader.h>
#include <cage-core/serialization.h>

#include <cage-engine/opengl.h>
#include <cage-engine/shaderProgram.h>
#include "private.h"

#include <list>
#include <vector>

namespace cage
{
	ConfigBool shaderIntrospection("cage/graphics/shaderIntrospection", false);

	namespace
	{
		static_assert(sizeof(GLuint) == sizeof(uint32), "incompatible size of GLuint");

		struct SourceHolder
		{
			SourceHolder() = default;

			explicit SourceHolder(uint32 id) : id(id)
			{}

			SourceHolder(const SourceHolder &) = delete;

			SourceHolder(SourceHolder &&other) noexcept
			{
				std::swap(id, other.id);
			}

			SourceHolder &operator = (const SourceHolder &) = delete;

			SourceHolder &operator = (SourceHolder &&other) noexcept
			{
				std::swap(id, other.id);
				return *this;
			}

			~SourceHolder()
			{
				if (id)
					glDeleteShader(id);
				id = 0;
			}

			operator GLuint () const noexcept { return id; }

		private:
			uint32 id = 0;
		};

		class ShaderProgramImpl : public ShaderProgram
		{
		public:
			uint32 id = 0;
			std::vector<std::pair<GLuint, GLuint>> sources; // stage, id

			ShaderProgramImpl()
			{
				id = glCreateProgram();
				CAGE_CHECK_GL_ERROR_DEBUG();
			}

			~ShaderProgramImpl()
			{
				glDeleteProgram(id);
			}
		};
	}

	void ShaderProgram::setDebugName(const String &name)
	{
#ifdef CAGE_DEBUG
		debugName = name;
#endif // CAGE_DEBUG
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(impl->id);
		glObjectLabel(GL_PROGRAM, impl->id, name.length(), name.c_str());
	}

	uint32 ShaderProgram::id() const
	{
		return ((ShaderProgramImpl *)this)->id;
	}

	void ShaderProgram::bind() const
	{
		const ShaderProgramImpl *impl = (const ShaderProgramImpl *)this;
		glUseProgram(impl->id);
		CAGE_CHECK_GL_ERROR_DEBUG();
		setCurrentObject<ShaderProgram>(impl->id);
	}

	void ShaderProgram::uniform(uint32 name, const sint32 &u)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform1i(name, u);
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, const uint32 &u)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform1ui(name, u);
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, const Vec2i &u)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform2iv(name, 1, &const_cast<Vec2i&>(u)[0]);
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, const Vec3i &u)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform3iv(name, 1, &const_cast<Vec3i&>(u)[0]);
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, const Vec4i &u)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform4iv(name, 1, &const_cast<Vec4i&>(u)[0]);
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, const Real &u)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform1f(name, u.value);
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, const Vec2 &u)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform2fv(name, 1, &const_cast<Vec2&>(u)[0].value);
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, const Vec3 &u)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform3fv(name, 1, &const_cast<Vec3&>(u)[0].value);
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, const Vec4 &u)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform4fv(name, 1, &const_cast<Vec4&>(u)[0].value);
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, const Quat &u)
	{
		uniform(name, Vec4(u[0], u[1], u[2], u[3]));
	}

	void ShaderProgram::uniform(uint32 name, const Mat3 &u)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniformMatrix3fv(name, 1, GL_FALSE, &const_cast<Mat3&>(u)[0].value);
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, const Mat4 &u)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniformMatrix4fv(name, 1, GL_FALSE, &const_cast<Mat4&>(u)[0].value);
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const sint32> values)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform1iv(name, numeric_cast<uint32>(values.size()), values.data());
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const uint32> values)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform1uiv(name, numeric_cast<uint32>(values.size()), values.data());
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const Vec2i> values)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform2iv(name, numeric_cast<uint32>(values.size()), (sint32 *)values.data());
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const Vec3i> values)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform3iv(name, numeric_cast<uint32>(values.size()), (sint32 *)values.data());
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const Vec4i> values)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform4iv(name, numeric_cast<uint32>(values.size()), (sint32 *)values.data());
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const Real> values)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform1fv(name, numeric_cast<uint32>(values.size()), (float*)values.data());
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const Vec2> values)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform2fv(name, numeric_cast<uint32>(values.size()), (float*)values.data());
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const Vec3> values)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform3fv(name, numeric_cast<uint32>(values.size()), (float*)values.data());
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const Vec4> values)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniform4fv(name, numeric_cast<uint32>(values.size()), (float*)values.data());
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const Quat> values)
	{
		uniform(name, bufferCast<const Vec4>(values));
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const Mat3> values)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniformMatrix3fv(name, numeric_cast<uint32>(values.size()), GL_FALSE, (float*)values.data());
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	void ShaderProgram::uniform(uint32 name, PointerRange<const Mat4> values)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);
		glUniformMatrix4fv(name, numeric_cast<uint32>(values.size()), GL_FALSE, (float*)values.data());
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	namespace
	{
		const String glTypeToName(uint32 type)
		{
			switch (type)
			{
			case GL_FLOAT: return "float";
			case GL_FLOAT_VEC2: return "vec2";
			case GL_FLOAT_VEC3: return "vec3";
			case GL_FLOAT_VEC4: return "vec4";
			case GL_DOUBLE: return "double";
			case GL_DOUBLE_VEC2: return "dvec2";
			case GL_DOUBLE_VEC3: return "dvec3";
			case GL_DOUBLE_VEC4: return "dvec4";
			case GL_INT: return "int";
			case GL_INT_VEC2: return "ivec2";
			case GL_INT_VEC3: return "ivec3";
			case GL_INT_VEC4: return "ivec4";
			case GL_UNSIGNED_INT: return "unsigned int";
			case GL_UNSIGNED_INT_VEC2: return "uvec2";
			case GL_UNSIGNED_INT_VEC3: return "uvec3";
			case GL_UNSIGNED_INT_VEC4: return "uvec4";
			case GL_BOOL: return "bool";
			case GL_BOOL_VEC2: return "bvec2";
			case GL_BOOL_VEC3: return "bvec3";
			case GL_BOOL_VEC4: return "bvec4";
			case GL_FLOAT_MAT2: return "mat2";
			case GL_FLOAT_MAT3: return "mat3";
			case GL_FLOAT_MAT4: return "mat4";
			case GL_FLOAT_MAT2x3: return "mat2x3";
			case GL_FLOAT_MAT2x4: return "mat2x4";
			case GL_FLOAT_MAT3x2: return "mat3x2";
			case GL_FLOAT_MAT3x4: return "mat3x4";
			case GL_FLOAT_MAT4x2: return "mat4x2";
			case GL_FLOAT_MAT4x3: return "mat4x3";
			case GL_DOUBLE_MAT2: return "dmat2";
			case GL_DOUBLE_MAT3: return "dmat3";
			case GL_DOUBLE_MAT4: return "dmat4";
			case GL_DOUBLE_MAT2x3: return "dmat2x3";
			case GL_DOUBLE_MAT2x4: return "dmat2x4";
			case GL_DOUBLE_MAT3x2: return "dmat3x2";
			case GL_DOUBLE_MAT3x4: return "dmat3x4";
			case GL_DOUBLE_MAT4x2: return "dmat4x2";
			case GL_DOUBLE_MAT4x3: return "dmat4x3";
			case GL_SAMPLER_1D: return "sampler1D";
			case GL_SAMPLER_2D: return "sampler2D";
			case GL_SAMPLER_3D: return "sampler3D";
			case GL_SAMPLER_CUBE: return "samplerCube";
			case GL_SAMPLER_1D_SHADOW: return "sampler1DShadow";
			case GL_SAMPLER_2D_SHADOW: return "sampler2DShadow";
			case GL_SAMPLER_1D_ARRAY: return "sampler1DArray";
			case GL_SAMPLER_2D_ARRAY: return "sampler2DArray";
			case GL_SAMPLER_1D_ARRAY_SHADOW: return "sampler1DArrayShadow";
			case GL_SAMPLER_2D_ARRAY_SHADOW: return "sampler2DArrayShadow";
			case GL_SAMPLER_2D_MULTISAMPLE: return "sampler2DMS";
			case GL_SAMPLER_2D_MULTISAMPLE_ARRAY: return "sampler2DMSArray";
			case GL_SAMPLER_CUBE_SHADOW: return "samplerCubeShadow";
			case GL_SAMPLER_BUFFER: return "samplerBuffer";
			case GL_SAMPLER_2D_RECT: return "sampler2DRect";
			case GL_SAMPLER_2D_RECT_SHADOW: return "sampler2DRectShadow";
			case GL_INT_SAMPLER_1D: return "isampler1D";
			case GL_INT_SAMPLER_2D: return "isampler2D";
			case GL_INT_SAMPLER_3D: return "isampler3D";
			case GL_INT_SAMPLER_CUBE: return "isamplerCube";
			case GL_INT_SAMPLER_1D_ARRAY: return "isampler1DArray";
			case GL_INT_SAMPLER_2D_ARRAY: return "isampler2DArray";
			case GL_INT_SAMPLER_2D_MULTISAMPLE: return "isampler2DMS";
			case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: return "isampler2DMSArray";
			case GL_INT_SAMPLER_BUFFER: return "isamplerBuffer";
			case GL_INT_SAMPLER_2D_RECT: return "isampler2DRect";
			case GL_UNSIGNED_INT_SAMPLER_1D: return "usampler1D";
			case GL_UNSIGNED_INT_SAMPLER_2D: return "usampler2D";
			case GL_UNSIGNED_INT_SAMPLER_3D: return "usampler3D";
			case GL_UNSIGNED_INT_SAMPLER_CUBE: return "usamplerCube";
			case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY: return "usampler2DArray";
			case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY: return "usampler2DArray";
			case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE: return "usampler2DMS";
			case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: return "usampler2DMSArray";
			case GL_UNSIGNED_INT_SAMPLER_BUFFER: return "usamplerBuffer";
			case GL_UNSIGNED_INT_SAMPLER_2D_RECT: return "usampler2DRect";
			default: return "unknown";
			}
		}

		static constexpr uint32 shaderLogBufferSize = 1024 * 8;
	}

	void ShaderProgram::source(uint32 stage, PointerRange<const char> buffer)
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;

		String stageName;
		switch (stage)
		{
		case GL_VERTEX_SHADER: stageName = "vertex"; break;
		case GL_TESS_CONTROL_SHADER: stageName = "tess_control"; break;
		case GL_TESS_EVALUATION_SHADER: stageName = "tess_evaluation"; break;
		case GL_GEOMETRY_SHADER: stageName = "geometry"; break;
		case GL_FRAGMENT_SHADER: stageName = "fragment"; break;
		case GL_COMPUTE_SHADER: stageName = "compute"; break;
		default:
			CAGE_THROW_CRITICAL(Exception, "invalid shader stage");
		}

		if (shaderIntrospection)
		{
			Holder<File> f = writeFile(Stringizer() + "shaderIntrospection/" + impl->id + "/" + stageName + ".glsl");
			f->write(buffer);
			f->close();
		}

		SourceHolder shader = SourceHolder(glCreateShader(stage));
		CAGE_CHECK_GL_ERROR_DEBUG();

		GLint len = numeric_cast<GLint>(buffer.size());
		const GLchar *tmp = (const GLchar *)buffer.data();
		glShaderSource(shader, 1, &tmp, &len);
		CAGE_CHECK_GL_ERROR_DEBUG();

		glCompileShader(shader);
		CAGE_CHECK_GL_ERROR_DEBUG();

		len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		CAGE_CHECK_GL_ERROR_DEBUG();
		if (len > 5)
		{
			char buf[shaderLogBufferSize];
			glGetShaderInfoLog(shader, shaderLogBufferSize - 1, &len, buf);
			CAGE_CHECK_GL_ERROR_DEBUG();
			if (shaderIntrospection)
			{
				Holder<File> f = newFile(Stringizer() + "shaderIntrospection/" + impl->id + "/" + stageName + "_compile.log", FileMode(false, true));
				f->write({ buf, buf + len });
			}
#ifdef CAGE_DEBUG
			CAGE_LOG(SeverityEnum::Note, "shader", Stringizer() + "shader name: " + debugName);
#endif // CAGE_DEBUG
			CAGE_LOG(SeverityEnum::Warning, "shader", Stringizer() + "shader compilation log (id: " + impl->id + ", stage: " + stageName + "):");
			Holder<LineReader> lrb = newLineReader({ buf, buf + len });
			for (String line; lrb->readLine(line);)
				CAGE_LOG_CONTINUE(SeverityEnum::Warning, "shader", line);
		}

		len = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &len);
		CAGE_CHECK_GL_ERROR_DEBUG();
		if (len != GL_TRUE)
		{
#ifdef CAGE_DEBUG
			CAGE_LOG(SeverityEnum::Note, "shader", Stringizer() + "shader name: " + debugName);
#endif // CAGE_DEBUG
			CAGE_LOG(SeverityEnum::Note, "shader", Stringizer() + "shader stage: " + stageName);
			CAGE_THROW_ERROR(GraphicsError, "shader compilation failed", len);
		}

		glAttachShader(impl->id, shader); // the shader source can be deleted after this
		CAGE_CHECK_GL_ERROR_DEBUG();

		impl->sources.push_back({ stage, (GLuint)shader });
	}

	void ShaderProgram::relink()
	{
		ShaderProgramImpl *impl = (ShaderProgramImpl *)this;
		CAGE_ASSERT(!impl->sources.empty());

		struct sourcesClearer
		{
			ShaderProgramImpl *impl;
			sourcesClearer(ShaderProgramImpl *impl) : impl(impl) {}
			~sourcesClearer() { impl->sources.clear(); }
		} sourcesClearerInstance(impl);

		glLinkProgram(impl->id);
		CAGE_CHECK_GL_ERROR_DEBUG();

		for (const auto &it : impl->sources)
			glDetachShader(impl->id, it.second);
		CAGE_CHECK_GL_ERROR_DEBUG();

		GLint len = 0;
		glGetProgramiv(impl->id, GL_INFO_LOG_LENGTH, &len);
		CAGE_CHECK_GL_ERROR_DEBUG();
		if (len > 5)
		{
			char buf[shaderLogBufferSize];
			glGetProgramInfoLog(impl->id, shaderLogBufferSize - 1, &len, buf);
			CAGE_CHECK_GL_ERROR_DEBUG();
			if (shaderIntrospection)
			{
				Holder<File> f = newFile(Stringizer() + "shaderIntrospection/" + impl->id + "/linking.log", FileMode(false, true));
				f->write({ buf, buf + len });
			}
#ifdef CAGE_DEBUG
			CAGE_LOG(SeverityEnum::Note, "shader", Stringizer() + "shader name: " + debugName);
#endif // CAGE_DEBUG
			CAGE_LOG(SeverityEnum::Warning, "shader", Stringizer() + "shader linking log (id: " + impl->id + "):");
			Holder<LineReader> lrb = newLineReader({ buf, buf + len });
			for (String line; lrb->readLine(line);)
				CAGE_LOG_CONTINUE(SeverityEnum::Warning, "shader", line);
		}

		len = 0;
		glGetProgramiv(impl->id, GL_LINK_STATUS, &len);
		CAGE_CHECK_GL_ERROR_DEBUG();
		if (len != GL_TRUE)
		{
#ifdef CAGE_DEBUG
			CAGE_LOG(SeverityEnum::Note, "shader", Stringizer() + "shader name: " + debugName);
#endif // CAGE_DEBUG
			CAGE_THROW_ERROR(GraphicsError, "shader linking failed", len);
		}

		if (shaderIntrospection)
		{
			{ // uniforms
				FileMode fm(false, true);
				fm.textual = true;
				Holder<File> f = newFile(Stringizer() + "shaderIntrospection/" + impl->id + "/uniforms.txt", fm);
				f->writeLine("<location> <type> <name>");
				GLint numUniforms = 0;
				glGetProgramInterfaceiv(impl->id, GL_UNIFORM, GL_ACTIVE_RESOURCES, &numUniforms);
				for (GLint unif = 0; unif < numUniforms; unif++)
				{
					const GLenum props[] = { GL_BLOCK_INDEX, GL_LOCATION, GL_TYPE, GL_ARRAY_SIZE };
					GLint values[4];
					glGetProgramResourceiv(impl->id, GL_UNIFORM, unif, 4, props, 4, nullptr, values);
					if (values[0] != m)
						continue; // skip variables that are inside any block
					GLchar name[100];
					GLint nameLen = 0;
					glGetProgramResourceName(impl->id, GL_UNIFORM, unif, 99, &nameLen, name);
					f->writeLine(Stringizer() + values[1] + "\t" + glTypeToName(values[2]) + (values[3] > 1 ? Stringizer() + "[" + values[3] + "]" : Stringizer()) + " " + String({ name, name + nameLen }));
				}
				CAGE_CHECK_GL_ERROR_DEBUG();
			}

			{ // blocks
				FileMode fm(false, true);
				fm.textual = true;
				Holder<File> f = newFile(Stringizer() + "shaderIntrospection/" + impl->id + "/blocks.txt", fm);
				f->writeLine("<binding> <name> <variables> <size>");
				GLint numBlocks = 0;
				glGetProgramInterfaceiv(impl->id, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &numBlocks);
				for (GLint block = 0; block < numBlocks; block++)
				{
					const GLenum props[] = { GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE, GL_NUM_ACTIVE_VARIABLES };
					GLint values[3];
					glGetProgramResourceiv(impl->id, GL_UNIFORM_BLOCK, block, 3, props, 3, nullptr, values);
					GLchar name[100];
					GLint nameLen = 0;
					glGetProgramResourceName(impl->id, GL_UNIFORM_BLOCK, block, 99, &nameLen, name);
					f->writeLine(Stringizer() + values[0] + "\t" + String({ name, name + nameLen }) + " " + values[2] + " " + values[1]);
				}
				CAGE_CHECK_GL_ERROR_DEBUG();
			}

			{ // subroutines
				FileMode fm(false, true);
				fm.textual = true;
				Holder<File> f = newFile(Stringizer() + "shaderIntrospection/" + impl->id + "/routines.txt", fm);
				f->writeLine("<stage> <location> <name> <compatibles>");
				const GLint stages[] = { GL_VERTEX_SUBROUTINE_UNIFORM, GL_TESS_CONTROL_SUBROUTINE_UNIFORM, GL_TESS_EVALUATION_SUBROUTINE_UNIFORM, GL_GEOMETRY_SUBROUTINE_UNIFORM, GL_FRAGMENT_SUBROUTINE_UNIFORM, GL_COMPUTE_SUBROUTINE_UNIFORM };
				const GLint stages2[] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER };
				for (GLint stageIndex = 0; stageIndex < 6; stageIndex++)
				{
					GLint stage = stages[stageIndex];
					GLint stage2 = stages2[stageIndex];
					GLint numRoutines = 0;
					glGetProgramInterfaceiv(impl->id, stage, GL_ACTIVE_RESOURCES, &numRoutines);
					for (GLint routine = 0; routine < numRoutines; routine++)
					{
						const GLenum props[] = { GL_LOCATION, GL_NUM_COMPATIBLE_SUBROUTINES };
						GLint values[2];
						glGetProgramResourceiv(impl->id, stage, routine, 2, props, 2, nullptr, values);
						GLchar name[100];
						GLint nameLen = 0;
						glGetProgramResourceName(impl->id, stage, routine, 99, &nameLen, name);
						f->writeLine(Stringizer() + stageIndex + "\t" + values[0] + "\t" + String({ name, name + nameLen }) + " " + values[1]);
						// enumerate compatible subroutines
						const GLenum props2[] = { GL_COMPATIBLE_SUBROUTINES };
						GLint values2[100];
						GLint numCompatibles = m;
						glGetProgramResourceiv(impl->id, stage, routine, 1, props2, 99, &numCompatibles, values2);
						for (GLint compat = 0; compat < numCompatibles; compat++)
						{
							glGetActiveSubroutineName(impl->id, stage2, values2[compat], 99, &nameLen, name);
							f->writeLine(Stringizer() + "\t\t\t" + values2[compat] + "\t" + String({ name, name + nameLen }));
						}
					}
				}
				CAGE_CHECK_GL_ERROR_DEBUG();
			}

			{ // inouts
				FileMode fm(false, true);
				fm.textual = true;
				Holder<File> f = newFile(Stringizer() + "shaderIntrospection/" + impl->id + "/inout.txt", fm);
				f->writeLine("<inout> <location> <index> <type> <name>");
				const GLint inouts[] = { GL_PROGRAM_INPUT, GL_PROGRAM_OUTPUT };
				const String inoutsNames[] = { "in", "out" };
				for (GLint stageIndex = 0; stageIndex < 2; stageIndex++)
				{
					GLint stage = inouts[stageIndex];
					GLint numVars = 0;
					glGetProgramInterfaceiv(impl->id, stage, GL_ACTIVE_RESOURCES, &numVars);
					for (GLint var = 0; var < numVars; var++)
					{
						const GLenum props[] = { GL_LOCATION, GL_TYPE, GL_ARRAY_SIZE, GL_LOCATION_INDEX };
						GLint values[4];
						values[3] = m;
						glGetProgramResourceiv(impl->id, stage, var, 3 + stageIndex, props, 4, nullptr, values);
						GLchar name[100];
						GLint nameLen = 0;
						glGetProgramResourceName(impl->id, stage, var, 99, &nameLen, name);
						f->writeLine(Stringizer() + inoutsNames[stageIndex] + "\t" + values[0] + "\t" + values[3] + "\t" + glTypeToName(values[1]) + (values[2] > 1 ? Stringizer() + "[" + values[2] + "]" : Stringizer()) + " " + String({ name, name + nameLen }));
					}
				}
				CAGE_CHECK_GL_ERROR_DEBUG();
			}
		}
	}

	void ShaderProgram::validate() const
	{
		const ShaderProgramImpl *impl = (const ShaderProgramImpl *)this;
		CAGE_ASSERT(graphicsPrivat::getCurrentObject<ShaderProgram>() == impl->id);

		glValidateProgram(impl->id);
		CAGE_CHECK_GL_ERROR_DEBUG();

		GLint len;
		glGetProgramiv(impl->id, GL_INFO_LOG_LENGTH, &len);
		CAGE_CHECK_GL_ERROR_DEBUG();
		if (len > 5)
		{
			char buf[shaderLogBufferSize];
			glGetProgramInfoLog(impl->id, shaderLogBufferSize - 1, &len, buf);
			CAGE_CHECK_GL_ERROR_DEBUG();
			if (shaderIntrospection)
			{
				Holder<File> f = newFile(Stringizer() + "shaderIntrospection/" + impl->id + "/validation.log", FileMode(false, true));
				f->write({ buf, buf + len });
			}
#ifdef CAGE_DEBUG
			CAGE_LOG(SeverityEnum::Note, "shader", Stringizer() + "shader name: " + debugName);
#endif // CAGE_DEBUG
			CAGE_LOG(SeverityEnum::Warning, "shader", Stringizer() + "shader validation log (id: " + impl->id + "):");
			Holder<LineReader> lrb = newLineReader({ buf, buf + len });
			for (String line; lrb->readLine(line);)
				CAGE_LOG_CONTINUE(SeverityEnum::Warning, "shader", line);
		}

		glGetProgramiv(impl->id, GL_VALIDATE_STATUS, &len);
		CAGE_CHECK_GL_ERROR_DEBUG();
		if (len != GL_TRUE)
			CAGE_THROW_ERROR(GraphicsError, "shader validation failed", len);
	}

	Holder<ShaderProgram> newShaderProgram()
	{
		return systemMemory().createImpl<ShaderProgram, ShaderProgramImpl>();
	}
}
