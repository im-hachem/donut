#pragma once

#include <memory>

namespace Donut
{
	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T, typename ... Args>
	constexpr auto create_scope(Args&& ... args) -> Scope<T>
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T, typename ... Args>
	constexpr auto create_ref(Args&& ... args) -> Ref<T>
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
};
