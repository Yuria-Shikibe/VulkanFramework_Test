//
// Created by Matrix on 2024/8/26.
//

export module ext.TransferAdaptor;

export namespace ext{
	template <typename T>
	struct TransferAdaptor : T{
		using T::T;

		TransferAdaptor(const TransferAdaptor& other){}

		TransferAdaptor(TransferAdaptor&& other) noexcept{}

		TransferAdaptor& operator=(const TransferAdaptor& other){
			return *this;
		}

		TransferAdaptor& operator=(TransferAdaptor&& other) noexcept{
			return *this;
		}
	};
}
