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
		ShaderCompiler(vk::Device device, std::string sourcePath, shaderc_shader_kind shaderKind) : deviceHandle(device)
		{
			compileThread = std::thread(&ShaderCompiler::compile, this, sourcePath, shaderKind);
		}
		~ShaderCompiler()
		{
			if(compileThread.joinable())
				compileThread.join();
			deviceHandle.destroyShaderModule(module);
		}
	
		vk::ShaderModule module;
		std::thread compileThread;
	private:
		void compile(std::string sourcePath, shaderc_shader_kind shaderKind)
		{
			bool precompiled = sourcePath.substr(sourcePath.length() - 4, 4) == ".spv";

			std::ifstream shaderFile(sourcePath, std::ios::ate | (precompiled ? std::ios::binary : 0));
			if (!shaderFile.is_open())
				throw std::runtime_error("vwShader: shader file not found!");

			size_t fileSize = (size_t)shaderFile.tellg();
			std::vector<char> shaderCode(fileSize);

			shaderFile.seekg(0);
			shaderFile.read(shaderCode.data(), fileSize);
			shaderFile.close();

			vk::ShaderModuleCreateInfo moduleCreateInfo;
			if (precompiled)
			{
				moduleCreateInfo.codeSize = shaderCode.size();
				moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
			}
			else
			{
			shaderc::Compiler compiler;
			shaderc::CompileOptions options;
			options.SetOptimizationLevel(shaderc_optimization_level::shaderc_optimization_level_size);

			auto result = compiler.CompileGlslToSpv(shaderCode.data(), shaderKind, "shader", options);
			if (result.GetCompilationStatus() != shaderc_compilation_status::shaderc_compilation_status_success)
				std::cerr << result.GetErrorMessage();

			std::vector<uint32_t> shaderBinary(result.begin(), result.end());
			moduleCreateInfo.codeSize = shaderBinary.size() * 4;
			moduleCreateInfo.pCode = shaderBinary.data();
			}
			module = deviceHandle.createShaderModule(moduleCreateInfo);
		}
		vk::Device deviceHandle;
	};
	/*
	class Shader
	{
	public:
		Shader(vk::Device device, vk::ShaderStageFlagBits shaderStage, std::string sourcePath) 
		{
			stageCreateInfo.stage = shaderStage;
			stageCreateInfo.pName = "main";
			compiler = std::shared_ptr<ShaderCompiler>(new ShaderCompiler(device, sourcePath, mapShaderStage[shaderStage]));
		}
		bool isReady() { return compiler->compileThread.joinable(); };
		void waitUntilReady()
		{
			if(compiler->compileThread.joinable())
				compiler->compileThread.join();
		}
		vk::PipelineShaderStageCreateInfo getShaderStageInfo()
		{
			waitUntilReady();
			stageCreateInfo.module = compiler->module;
			return stageCreateInfo;
		}
	private:
		std::shared_ptr<ShaderCompiler> compiler;
		vk::PipelineShaderStageCreateInfo stageCreateInfo;
	};
	*/
	
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
}


