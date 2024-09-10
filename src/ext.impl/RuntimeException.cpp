module ext.RuntimeException;
ext::RuntimeException::~RuntimeException() = default;

ext::RuntimeException::RuntimeException(const std::string& str, const std::source_location& location){
	std::ostringstream ss;

	ss << str << '\n';
	ss << "at: " << location.file_name() << '\n';
	ss << "at: " << location.function_name() << '\n';
	ss << "at: " << location.column() << '\n';

	ext::print_stack_trace(ss);

	data = std::move(ss).str();

	RuntimeException::postProcess();
}

char const* ext::RuntimeException::what() const{
	return data.data();
}

void ext::RuntimeException::postProcess() const{}

ext::RuntimeException::RuntimeException(): RuntimeException("Crashed At..."){

}

ext::NullPointerException::NullPointerException(const std::string& str): RuntimeException(str){
}

ext::NullPointerException::NullPointerException(): NullPointerException("Null Pointer At..."){

}

ext::IllegalArguments::IllegalArguments(const std::string& str): RuntimeException(str){
}

ext::IllegalArguments::IllegalArguments(): IllegalArguments("Illegal Arguments At..."){

}
