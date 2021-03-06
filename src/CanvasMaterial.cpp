#include <Core/CanvasMaterial.h>
#include <Core/ResourceManager.h>
#include <PluginAPI/Plugin.h>
#include <UI/UIHelper.h>
#include "../GodotShaders.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <string.h>
#include <fstream>
#include <string>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#define BUTTON_SPACE_LEFT -40 * Owner->GetDPI()

std::string LoadFile(const std::string& file)
{
	std::ifstream in(file);
	if (in.is_open()) {
		in.seekg(0, std::ios::beg);

		std::string content((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
		in.close();
		return content;
	}
	return "";
}

namespace gd
{
	namespace pipe
	{
		CanvasMaterial::CanvasMaterial()
		{
			Type = PipelineItemType::CanvasMaterial;
			memset(ShaderPath, 0, sizeof(char) * MAX_PATH_LENGTH);
			m_shader = 0;
			m_vw = m_vh = 1.0f;
			m_modelMat = m_projMat = glm::mat4(1.0f);
			m_uniforms.clear();
			m_glslData.BlendMode = Shader::CanvasItem::BLEND_MODE_ADD;
		}
		CanvasMaterial::~CanvasMaterial()
		{
			if (m_shader != 0)
				glDeleteShader(m_shader);
		}
		void CanvasMaterial::SetViewportSize(float w, float h)
		{
			m_vw = w;
			m_vh = h;
			m_projMat = glm::ortho(0.0f, w, h, 0.0f, 0.1f, 1000.0f);
		}
		void CanvasMaterial::Bind()
		{
			// bind shaders
			if (!m_glslData.Error && m_glslData.SCREEN_TEXTURE)
			{
				ResourceManager::Instance().Copy(((gd::GodotShaders*)Owner)->GetColorBuffer(), ((gd::GodotShaders*)Owner)->GetFBO());

				glUseProgram(m_shader);
				glActiveTexture(GL_TEXTURE0 + 1);
				glBindTexture(GL_TEXTURE_2D, ResourceManager::Instance().SCREEN_TEXTURE());
				glUniform1i(glGetUniformLocation(m_shader, "screen_texture"), 1);

				glUniform2f(m_pixelSizeLoc, 1.0f / m_vw, 1.0f/m_vh);
			}

			glUseProgram(m_shader);

			glUniformMatrix4fv(m_projMatrixLoc, 1, GL_FALSE, glm::value_ptr(m_projMat));

			if (m_glslData.TIME)
				glUniform1f(m_timeLoc, Owner->GetTime());

			glUniform1i(glGetUniformLocation(m_shader, "color_texture"), 0);

			for (const auto& uniform : m_uniforms) {
				const auto& val = uniform.second.Value;
				const auto& loc = uniform.second.Location;
				switch (uniform.second.Type) {
				case ShaderLanguage::TYPE_BOOL: glUniform1i(loc, val[0].sint); break;
				case ShaderLanguage::TYPE_BVEC2: glUniform2i(loc, val[0].sint, val[1].sint); break;
				case ShaderLanguage::TYPE_BVEC3: glUniform3i(loc, val[0].sint, val[1].sint, val[2].sint); break;
				case ShaderLanguage::TYPE_BVEC4: glUniform4i(loc, val[0].sint, val[1].sint, val[2].sint, val[3].sint); break;
				case ShaderLanguage::TYPE_INT: glUniform1i(loc, val[0].sint); break;
				case ShaderLanguage::TYPE_IVEC2: glUniform2i(loc, val[0].sint, val[1].sint); break;
				case ShaderLanguage::TYPE_IVEC3: glUniform3i(loc, val[0].sint, val[1].sint, val[2].sint); break;
				case ShaderLanguage::TYPE_IVEC4: glUniform4i(loc, val[0].sint, val[1].sint, val[2].sint, val[3].sint); break;
				case ShaderLanguage::TYPE_UINT: glUniform1ui(loc, val[0].uint); break;
				case ShaderLanguage::TYPE_UVEC2: glUniform2ui(loc, val[0].uint, val[1].uint); break;
				case ShaderLanguage::TYPE_UVEC3: glUniform3ui(loc, val[0].uint, val[1].uint, val[2].uint); break;
				case ShaderLanguage::TYPE_UVEC4: glUniform4ui(loc, val[0].uint, val[1].uint, val[2].uint, val[3].uint); break;
				case ShaderLanguage::TYPE_FLOAT: glUniform1f(loc, val[0].real); break;
				case ShaderLanguage::TYPE_VEC2: glUniform2f(loc, val[0].real, val[1].real); break;
				case ShaderLanguage::TYPE_VEC3: glUniform3f(loc, val[0].real, val[1].real, val[2].real); break;
				case ShaderLanguage::TYPE_VEC4: glUniform4f(loc, val[0].real, val[1].real, val[2].real, val[3].real); break;
				case ShaderLanguage::TYPE_MAT2: glUniformMatrix2fv(loc, 1, GL_FALSE, (float*)&val[0]); break;
				case ShaderLanguage::TYPE_MAT3: glUniformMatrix3fv(loc, 1, GL_FALSE, (float*)&val[0]); break;
				case ShaderLanguage::TYPE_MAT4: glUniformMatrix4fv(loc, 1, GL_FALSE, (float*)&val[0]); break;
				case ShaderLanguage::TYPE_SAMPLER2D:
					glActiveTexture(GL_TEXTURE0 + loc);
					glBindTexture(GL_TEXTURE_2D, val[0].uint);
					glUniform1i(glGetUniformLocation(m_shader, ("m_" + uniform.first).c_str()), loc);
					break;
				}
			}

			if (m_glslData.BlendMode == Shader::CanvasItem::BLEND_MODE_DISABLED) {
				glDisable(GL_BLEND);
			} else {
				glEnable(GL_BLEND);
				switch (m_glslData.BlendMode) {
					//-1 not handled because not blend is enabled anyway
				case Shader::CanvasItem::BLEND_MODE_MIX: {
					glBlendEquation(GL_FUNC_ADD);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				} break;
				case Shader::CanvasItem::BLEND_MODE_ADD: {
					glBlendEquation(GL_FUNC_ADD);
					glBlendFunc(GL_ONE, GL_ONE);
				} break;
				case Shader::CanvasItem::BLEND_MODE_SUB: {
					glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				} break;
				case Shader::CanvasItem::BLEND_MODE_MUL: {
					glBlendEquation(GL_FUNC_ADD);
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
				} break;
				}
			}
		}
		void CanvasMaterial::ShowProperties()
		{
			ImGui::Columns(2, "##plugin_columns");

			// TODO: this is only a temprorary fix for non-resizable columns
			static bool isColumnWidthSet = false;
			if (!isColumnWidthSet) {
				ImGui::SetColumnWidth(0, ImGui::GetWindowSize().x * 0.3f);
				isColumnWidthSet = true;
			}

			/* shader path */
			ImGui::Text("Shader:");
			ImGui::NextColumn();

			ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::InputText("##pui_vspath", ShaderPath, MAX_PATH_LENGTH);
			ImGui::PopItemFlag();
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button("...##pui_vsbtn", ImVec2(-1, 0))) {
				std::string file = "";
				bool success = UIHelper::GetOpenFileDialog(file);
				if (success) {
					char tempFile[MAX_PATH_LENGTH];
					Owner->GetRelativePath(Owner->Project, file.c_str(), tempFile);
					file = std::string(tempFile);

					strcpy(ShaderPath, file.c_str());

					Owner->ModifyProject(Owner->Project);

					if (Owner->FileExists(Owner->Project, file.c_str())) {
						Owner->ClearMessageGroup(Owner->Messages, Name);
						Compile();
					}
					else
						Owner->AddMessage(Owner->Messages, ed::plugin::MessageType::Error, Name, "Shader file doesn't exist", -1);
					((gd::GodotShaders*)Owner)->ShaderPathsUpdated = true;
				}
			}
			ImGui::NextColumn();


			ImGui::Columns(1);
		}
		void CanvasMaterial::ShowVariableEditor()
		{
			ImGui::Columns(3);
			
			static bool firstTime = true;
			if (firstTime) {
				ImGui::SetColumnWidth(0, 150.0f);
				ImGui::SetColumnWidth(1, 90.0f);
				firstTime = false;
			}
			
			ImGui::Separator();
			for (auto& u : m_uniforms) {
				ImGui::Text("%s", u.first.c_str());
				ImGui::NextColumn();

				ImGui::Text("%s", ShaderLanguage::get_datatype_name(u.second.Type).c_str());
				ImGui::NextColumn();

				if (UIHelper::ShowValueEditor(Owner, u.first, u.second))
					Owner->ModifyProject(Owner->Project);
				ImGui::NextColumn();
				ImGui::Separator();
			}

			ImGui::Columns(1);
		}

		void CanvasMaterial::SetModelMatrix(glm::mat4 mat)
		{
			m_modelMat = mat;

			if (m_glslData.SkipVertexTransform)
				glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, glm::value_ptr(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -1000.0f))));
			else
				glUniformMatrix4fv(m_modelMatrixLoc, 1, GL_FALSE, glm::value_ptr(m_modelMat));
		}
		void CanvasMaterial::Compile()
		{
			std::string vsCodeContent = ResourceManager::Instance().GetDefaultCanvasVertexShader();
			std::string psCodeContent = ResourceManager::Instance().GetDefaultCanvasPixelShader();

			std::string godotShaderContents = "";

			if (strlen(ShaderPath) != 0) {
				char outPath[MAX_PATH_LENGTH];
				Owner->GetProjectPath(Owner->Project, ShaderPath, outPath);

				godotShaderContents = LoadFile(outPath);
			}

			const char* filedata = godotShaderContents.c_str();
			int filesize = godotShaderContents.size();

			CompileFromSource(filedata, filesize);
		}
		void CanvasMaterial::CompileFromSource(const char* filedata, int filesize)
		{
			Owner->ClearMessageGroup(Owner->Messages, Name);

			std::string vsCodeContent = ResourceManager::Instance().GetDefaultCanvasVertexShader();
			std::string psCodeContent = ResourceManager::Instance().GetDefaultCanvasPixelShader();

			auto unif = m_glslData.Uniforms;

			if (filesize != 0 && filedata != nullptr) {
				gd::ShaderTranscompiler::Transcompile(filedata, m_glslData);

				if (!m_glslData.Error) {
					vsCodeContent = m_glslData.Vertex;
					psCodeContent = m_glslData.Fragment;
				} else {
					Owner->AddMessage(Owner->Messages, ed::plugin::MessageType::Error, Name, m_glslData.ErrorMessage.c_str(), m_glslData.ErrorLine);
					return;
				}
			}

			const char* vsCode = vsCodeContent.c_str();
			const char* psCode = psCodeContent.c_str();

			GLint success = 0;
			char infoLog[512];

			if (m_shader != 0)
				glDeleteShader(m_shader);

			// create vertex shader
			unsigned int canvasVS = glCreateShader(GL_VERTEX_SHADER);
			glShaderSource(canvasVS, 1, &vsCode, nullptr);
			glCompileShader(canvasVS);
			glGetShaderiv(canvasVS, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(canvasVS, 512, NULL, infoLog);
				Owner->Log("Failed to compile a GCanvasMaterial vertex shader", true, nullptr, -1);
				Owner->Log(infoLog, true, nullptr, -1);
			}

			// create pixel shader
			unsigned int canvasPS = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(canvasPS, 1, &psCode, nullptr);
			glCompileShader(canvasPS);
			glGetShaderiv(canvasPS, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(canvasPS, 512, NULL, infoLog);
				Owner->Log("Failed to compile a GCanvasMaterial pixel shader", true, nullptr, -1);
				Owner->Log(infoLog, true, nullptr, -1);
			}

			// create a shader program for gizmo
			m_shader = glCreateProgram();
			glAttachShader(m_shader, canvasVS);
			glAttachShader(m_shader, canvasPS);
			glLinkProgram(m_shader);
			glGetProgramiv(m_shader, GL_LINK_STATUS, &success);
			if (!success) {
				glGetProgramInfoLog(m_shader, 512, NULL, infoLog);
				Owner->Log("Failed to create a GCanvasMaterial shader program", true, nullptr, -1);
				Owner->Log(infoLog, true, nullptr, -1);
			}

			//glDeleteShader(canvasPS);
			//glDeleteShader(canvasVS);

			m_projMatrixLoc = glGetUniformLocation(m_shader, "projection_matrix");
			m_modelMatrixLoc = glGetUniformLocation(m_shader, "modelview_matrix");
			m_timeLoc = glGetUniformLocation(m_shader, "time");
			m_pixelSizeLoc = glGetUniformLocation(m_shader, "screen_pixel_size");

			glUniform1i(glGetUniformLocation(m_shader, "color_texture"), 0); // color_texture -> texunit: 0
			

			// user uniforms
			for (const auto& uniform : m_glslData.Uniforms) {
				Uniform* u = &m_uniforms[uniform.first];

				u->Location = glGetUniformLocation(m_shader, ("m_" + uniform.first).c_str());

				if (u->Type != uniform.second.type && u->Type != ShaderLanguage::TYPE_VOID)
					u->Value.resize(0);

				u->Type = uniform.second.type;

				bool isSampler = ShaderLanguage::is_sampler_type(u->Type);
				if (isSampler) {
					glUniform1i(u->Location, uniform.second.texture_order + 2);
					u->Location = uniform.second.texture_order + 2;
				}

				if (uniform.second.default_value.size() == 0 && u->Value.size() == 0) {
					u->Value.resize(ShaderLanguage::get_cardinality(uniform.second.type));
					for (auto& val : u->Value)
						val.sint = 0;

					if (isSampler)
						u->Value[0].uint = ResourceManager::Instance().WhiteTexture;
				}

				ShaderLanguage::DataType scalarType = ShaderLanguage::get_scalar_type(u->Type);
				bool equal = u->Value.size() == unif[uniform.first].default_value.size() && !isSampler;
				for (int i = 0; i < u->Value.size() && equal; i++) {
					if (scalarType == ShaderLanguage::DataType::TYPE_BOOL)
						equal = (u->Value[i].boolean == unif[uniform.first].default_value[i].boolean);
					else if (scalarType == ShaderLanguage::DataType::TYPE_FLOAT)
						equal = (u->Value[i].real == unif[uniform.first].default_value[i].real);
					else if (scalarType == ShaderLanguage::DataType::TYPE_INT)
						equal = (u->Value[i].sint == unif[uniform.first].default_value[i].sint);
					else if (scalarType == ShaderLanguage::DataType::TYPE_UINT)
						equal = (u->Value[i].uint == unif[uniform.first].default_value[i].uint);
				}

				if (u->Value.size() == 0 || equal)
					u->Value = uniform.second.default_value;

				memcpy(&u->HintRange[0], (void*)&uniform.second.hint_range[0], 3 * sizeof(float));
				u->HintType = uniform.second.hint;

				if (u->HintType == ShaderLanguage::ShaderNode::Uniform::HINT_NONE) {
					u->HintRange[0] = 0.0f;
					u->HintRange[1] = 0.0f;
					u->HintRange[2] = scalarType == ShaderLanguage::DataType::TYPE_FLOAT ? 0.01f : 1.0f;
				}
				else if (isSampler && u->HintType == ShaderLanguage::ShaderNode::Uniform::HINT_WHITE) {
					if (u->Value[0].uint == ResourceManager::Instance().BlackTexture)
						u->Value[0].uint = ResourceManager::Instance().WhiteTexture;
				}
				else if (isSampler && u->HintType == ShaderLanguage::ShaderNode::Uniform::HINT_BLACK) {
					if (u->Value[0].uint == ResourceManager::Instance().WhiteTexture)
						u->Value[0].uint = ResourceManager::Instance().BlackTexture;
				}
			}

			// erase non used uniforms
			std::vector<std::string> toBeErased;
			for (const auto& uniform : m_uniforms)
			{
				if (m_glslData.Uniforms.count(uniform.first) == 0)
					toBeErased.push_back(uniform.first);
			}

			for (const auto& uniform : toBeErased)
				m_uniforms.erase(uniform);
		}
	}
}