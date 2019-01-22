#pragma once
#include <string>
#include <thread>
#include <fstream>
#include <map>
#include <iostream>
#include <vulkan\vulkan.hpp>
#include <shaderc\shaderc.hpp>

namespace vw
{
	
	static std::map<vk::ShaderStageFlagBits, shaderc_shader_kind >mapShaderStage =
	{
		{vk::ShaderStageFlagBits::eVertex, shaderc_shader_kind::shaderc_glsl_vertex_shader},
		{vk::ShaderStageFlagBits::eFragment, shaderc_shader_kind::shaderc_glsl_fragment_shader}
	};

	class ShaderCompiler
	{
	public:
		ShaderCompiler(vk::Device device, std::string sourcePath, shaderc_shader_kind shaderKind);
		~ShaderCompiler();
	
		vk::ShaderModule module;
		std::thread compileThread;
	private:
		void compile(std::string sourcePath, shaderc_shader_kind shaderKind);
		vk::Device deviceHandle;
	};
	
	class Shader
	{
	public:
		Shader(vk::Device device, vk::ShaderStageFlagBits shaderStage, std::string sourcePath);
		inline bool isReady() { return compiler->compileThread.joinable(); };
		void waitUntilReady();
		vk::PipelineShaderStageCreateInfo getShaderStageInfo();
	private:
		std::shared_ptr<ShaderCompiler> compiler;
		vk::PipelineShaderStageCreateInfo stageCreateInfo;
	};
	
	/*
	class Shader
	{
	public:
		Shader(vk::Device device, vk::ShaderStageFlagBits stage, std::string binaryPath) : deviceHandle(device)
		{
			stageCreateInfo.stage = stage;
			stageCreateInfo.pName = "main";

			std::ifstream shaderFile(binaryPath, std::ios::ate | std::ios::binary);
			if (!shaderFile.is_open())
				throw std::runtime_error("vwShader: shader file not found!");

			size_t fileSize = (size_t)shaderFile.tellg();
			std::vector<char> shaderCode(fileSize);

			shaderFile.seekg(0);
			shaderFile.read(shaderCode.data(), fileSize);
			shaderFile.close();

			vk::ShaderModuleCreateInfo moduleCreateInfo;
			moduleCreateInfo.codeSize = shaderCode.size();
			moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
			stageCreateInfo.module = deviceHandle.createShaderModule(moduleCreateInfo);
		}
		~Shader()
		{
			deviceHandle.destroyShaderModule(stageCreateInfo.module);
		}
		vk::PipelineShaderStageCreateInfo getShaderStageInfo()
		{
			return stageCreateInfo;
		}

	private:
		vk::Device deviceHandle;
		vk::PipelineShaderStageCreateInfo stageCreateInfo;
	};
	*/
}


