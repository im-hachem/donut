#pragma once

#include "rendering/texture.h"

namespace Donut
{
	class VulkanTexture2D
		: public Texture2D
	{
	public:
		VulkanTexture2D(uint32_t width, uint32_t height);
		VulkanTexture2D(const std::string& path);
		virtual ~VulkanTexture2D();

		virtual auto get_width() const -> uint32_t override{ return m_width; }
		virtual auto get_height() const -> uint32_t override{ return m_height; }
		virtual auto get_renderer_id() const -> uint32_t override{ return m_renderer_id; }

		virtual auto set_data(void* data, uint32_t size) -> void override;
		virtual auto bind(uint32_t slot = 0) const -> void override;
		virtual auto bind_as_image(uint32_t slot = 0, bool read_only = false) const -> void override;

		virtual bool operator==(const Texture& other) const override
		{
			return m_renderer_id == ((VulkanTexture2D&)other).m_renderer_id;
		}
	private:
		std::string m_path;
		uint32_t    m_width, m_height;
		uint32_t    m_renderer_id;
		uint32_t    m_internal_format, m_data_format;
	};

	class VulkanCubemapTexture
		: public CubemapTexture
	{
	public:
		VulkanCubemapTexture(uint32_t width, uint32_t height);
		VulkanCubemapTexture(const std::string& path);
		virtual ~VulkanCubemapTexture();

		virtual auto get_width() const -> uint32_t override{ return m_width;      }
		virtual auto get_height() const -> uint32_t override{ return m_height;     }
		virtual auto get_renderer_id() const -> uint32_t override{ return m_renderer_id; }

		virtual auto set_data(void* data, uint32_t size) -> void override;
		virtual auto bind(uint32_t slot = 0) const -> void override;
		virtual auto bind_as_image(uint32_t slot = 0, bool read_only = false) const -> void override;

		virtual bool operator==(const Texture& other) const override
		{
			return m_renderer_id == ((VulkanCubemapTexture&)other).m_renderer_id;
		}
	private:
		std::string m_path;
		uint32_t    m_width, m_height;
		uint32_t    m_renderer_id;
		uint32_t    m_internal_format, m_data_format;
	};
};
