export module ext.RuntimeException;

import ext.stack_trace;
import std;

export namespace ext{
	//all deprecated

	class RuntimeException : public std::exception{
	public:
		~RuntimeException() override;

		std::string data{};

		explicit RuntimeException(const std::string& str, const std::source_location& location = std::source_location::current());

		[[nodiscard]] char const* what() const override;

		virtual void postProcess() const;;

		RuntimeException();
	};

	class [[deprecated]] NullPointerException final : public RuntimeException{
	public:
		[[nodiscard]] explicit NullPointerException(const std::string& str);

		[[nodiscard]] NullPointerException();
	};

	class [[deprecated]] IllegalArguments final : public RuntimeException {
	public:
		[[nodiscard]] explicit IllegalArguments(const std::string& str);

		[[nodiscard]] IllegalArguments();
	};

	class [[deprecated]] ArrayIndexOutOfBound final : public RuntimeException {
	public:
		[[nodiscard]] explicit ArrayIndexOutOfBound(const std::string& str)
			: RuntimeException(str) {
		}

		[[nodiscard]] ArrayIndexOutOfBound(const size_t index, const size_t bound) : RuntimeException("Array Index Out Of Bound! : [" + std::to_string(index) + "] Out of " + std::to_string(bound)) {

		}
	};
}