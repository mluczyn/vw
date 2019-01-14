#pragma once


template<typename T>
std::vector<T> VwiEnumerate(std::function <VkResult(uint32_t*, T*)> enumerationFunction)
{
	uint32_t count;
	enumerationFunction(&count, nullptr);
	std::vector<T> result(count);
	enumerationFunction(&count, result.data());
	return result;
}

template<typename S, typename T>
std::vector<T> VwiEnumerate(std::function <void(S, uint32_t*, T*)> enumerationFunction, S firstArg)
{
	uint32_t count;
	enumerationFunction(firstArg, &count, nullptr);
	std::vector<T> result(count);
	enumerationFunction(firstArg, &count, result.data());
	return result;
}

template<typename R, typename S, typename T>
std::vector<T> VwiEnumerate(std::function <VkResult(R, S, uint32_t*, T*)> enumerationFunction, R firstArg, S secondArg)
{
	uint32_t count;
	enumerationFunction(firstArg, secondArg, &count, nullptr);
	std::vector<T> result(count);
	enumerationFunction(firstArg, secondArg, &count, result.data());
	return result;
}
