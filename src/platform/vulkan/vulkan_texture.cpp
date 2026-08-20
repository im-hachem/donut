#include "vulkan_texture.h"

namespace Donut
{
	VulkanTexture2D::VulkanTexture2D(uint32_t width, uint32_t height)
		: m_width(width), m_height(height)
	{
		m_internal_format = 0;
		m_data_format = 0;
		m_renderer_id = 0;
	}

	VulkanTexture2D::VulkanTexture2D(const std::string& path)
		: m_path(path)
	{
		m_width = 1;
		m_height = 1;
		m_internal_format = 0;
		m_data_format = 0;
		m_renderer_id = 0;
	}

	VulkanTexture2D::~VulkanTexture2D()
	{
	}

	auto VulkanTexture2D::set_data(void* data, uint32_t size) -> void
	{
	}

	auto VulkanTexture2D::bind(uint32_t slot) const -> void
	{
	}

	auto VulkanTexture2D::bind_as_image(uint32_t slot, bool read_only) const -> void
	{
	}

	// Vulkan Cubemap Implementation (Placeholder)
	VulkanCubemapTexture::VulkanCubemapTexture(uint32_t width, uint32_t height)
		: m_width(width), m_height(height)
	{
		m_internal_format = 0;
		m_data_format = 0;
		m_renderer_id = 0;
	}

	VulkanCubemapTexture::VulkanCubemapTexture(const std::string& path)
		: m_path(path)
	{
		m_width = 1024;
		m_height = 1024;
		m_internal_format = 0;
		m_data_format = 0;
		m_renderer_id = 0;
	}

	VulkanCubemapTexture::~VulkanCubemapTexture()
	{
	}

	auto VulkanCubemapTexture::set_data(void* data, uint32_t size) -> void
	{
	}

	auto VulkanCubemapTexture::bind(uint32_t slot) const -> void
	{
	}

	auto VulkanCubemapTexture::bind_as_image(uint32_t slot, bool read_only) const -> void
	{
	}
};
