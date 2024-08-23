//
// Created by Matrix on 2024/5/10.
//

export module OS.Ctrl.Operation;

import std;
export import Core.Ctrl.Bind;
export import Core.Ctrl.Bind.Constants;
import ext.Heterogeneous;
import Assets.Bundle;

import ext.json.io;

export namespace Core::Ctrl{
	namespace InstructionEntry{
		constexpr std::string_view Unavaliable = "N/A";
		constexpr std::string_view Category = "control-bind";
		constexpr std::string_view Group = "group";
		constexpr std::string_view Name = "name";
		constexpr std::string_view Desc = "desc";
	}
	/**
	 * @brief Localized info
	 */
	struct Instruction{
		std::string name{InstructionEntry::Unavaliable};
		std::string desc{};

		[[nodiscard]] Instruction() = default;

		[[nodiscard]] Instruction(const Bundle& bundle, const std::string_view entry, const std::string_view group = "")
			: name{bundle.find({InstructionEntry::Category, group, entry, InstructionEntry::Name}, entry)},
			  desc{bundle.find({InstructionEntry::Category, group, entry, InstructionEntry::Desc}, "")}{}

		[[nodiscard]] constexpr bool hasDesc() const noexcept{
			return !desc.empty();
		}
	};

	class OperationGroup;

	class Operation{
		friend OperationGroup;
		std::size_t builtInID = 0;
		bool hide{};

	public:
		/**
		 * @brief This Should Be UNIQUE if in a same group, and SSO is better
		 */
		std::string name{};
		InputBind defaultBind{};
		InputBind customeBind{};

		std::vector<std::string> relativeOperations{};

		Instruction instruction{};
		mutable OperationGroup* group{};

		[[nodiscard]] Operation() = default;

		[[nodiscard]] Operation(const std::string& name, const InputBind& bind)
			: name{name},
			  defaultBind{bind},
			  customeBind{bind}{}

		[[nodiscard]] Operation(const std::string& name, InputBind&& bind)
			: name{name},
			  defaultBind{std::move(bind)},
			  customeBind{defaultBind}{}

		[[nodiscard]] Operation(const std::string& name, InputBind&& bind, const std::initializer_list<std::string> relative)
	: name{name},
	  defaultBind{std::move(bind)},
	  customeBind{defaultBind}, relativeOperations{relative}{}

		void loadInstruction(const Bundle& bundle, const std::string_view group = ""){
			instruction = Instruction{bundle, name, group};
		}

		void setDef(){
			customeBind = defaultBind;
		}

		constexpr bool hasCustomData() const noexcept{
			return customeBind.getFullKey() != defaultBind.getFullKey();
		}

		[[nodiscard]] bool isHidden() const{ return hide; }

		void setCustom(const int key, const int mode);

		[[nodiscard]] std::size_t getID() const{ return builtInID; }

		void updateRelativeBinds() const;

		friend bool operator==(const Operation& lhs, const Operation& rhs){
			return lhs.name == rhs.name && lhs.customeBind.getFullKey() == rhs.customeBind.getFullKey();
		}

		friend bool operator!=(const Operation& lhs, const Operation& rhs){ return !(lhs == rhs); }
	};

	class OperationGroup{
		std::string name{};

		ext::StringHashMap<Operation> binds{};

		std::unordered_map<int, unsigned> groupOccupiedKeys{};

		/**
		 * @return false if conflicted
		 */
		bool registerInfo(Operation& operation){
			const auto key = operation.customeBind.getFullKey();
			if(const auto itr = groupOccupiedKeys.find(key); itr != groupOccupiedKeys.end()){
				itr->second++;
				if(itr->second > 1)return false;
			}else{
				groupOccupiedKeys.insert_or_assign(key, 1);
			}

			operation.builtInID = binds.size();
			operation.group = this;

			return true;
		}

		void reRegisterGroup(){
			for (auto& operation : binds | std::views::values){
				operation.group = this;
			}
		}

		template <bool reset = false>
		void hideRelatived(){
			if constexpr (reset)for (auto& bind : binds | std::views::values){
				bind.hide = false;
			}

			for (auto& bind : binds | std::views::values){
				if(bind.relativeOperations.empty())continue;

				for (auto& relativeOperation : bind.relativeOperations){
					if(const auto itr = binds.find(relativeOperation); itr != binds.end()){
						itr->second.hide = true;
					}
				}
			}
		}
	public:
		Instruction instruction{};
		InputBindGroup* targetGroup{};

		[[nodiscard]] OperationGroup() = default;

		[[nodiscard]] explicit OperationGroup(const std::string& name)
			: name{name}{}

		[[nodiscard]] explicit OperationGroup(const std::string& name, const std::initializer_list<Operation> operations)
			: name{name}{
			for (auto& operation : operations){
				addKey(std::move(operation));
			}

			hideRelatived<false>();
		}

		[[nodiscard]] ext::StringHashMap<Operation>& getBinds(){ return binds; }
		[[nodiscard]] const ext::StringHashMap<Operation>& getBinds() const{ return binds; }

		OperationGroup(const OperationGroup& other)
			: name(other.name),
			  binds(other.binds),
			  groupOccupiedKeys(other.groupOccupiedKeys),
			  instruction(other.instruction),
			  targetGroup(other.targetGroup){
			reRegisterGroup();
		}

		OperationGroup(OperationGroup&& other) noexcept
			: name(std::move(other.name)),
			  binds(std::move(other.binds)),
			  groupOccupiedKeys(std::move(other.groupOccupiedKeys)),
			  instruction(std::move(other.instruction)),
			  targetGroup(other.targetGroup){
			reRegisterGroup();
		}

		OperationGroup& operator=(const OperationGroup& other){
			if(this == &other) return *this;
			name = other.name;
			binds = other.binds;
			groupOccupiedKeys = other.groupOccupiedKeys;
			instruction = other.instruction;
			targetGroup = other.targetGroup;
			reRegisterGroup();
			return *this;
		}

		OperationGroup& operator=(OperationGroup&& other) noexcept{
			if(this == &other) return *this;
			name = std::move(other.name);
			binds = std::move(other.binds);
			groupOccupiedKeys = std::move(other.groupOccupiedKeys);
			instruction = std::move(other.instruction);
			targetGroup = other.targetGroup;
			reRegisterGroup();
			return *this;
		}

		[[nodiscard]] auto getShownBinds(){
			return binds | std::views::values | std::views::filter(std::not_fn(&Operation::isHidden));
		}

		void loadInstruction(const Bundle& bundle){
			instruction.name = bundle.find({InstructionEntry::Category, InstructionEntry::Group, name, InstructionEntry::Name}, name);
			instruction.desc = bundle.find({InstructionEntry::Category, InstructionEntry::Group, name, InstructionEntry::Desc}, "");

			for (auto && bind : binds | std::ranges::views::values){
				bind.loadInstruction(bundle, name);
			}
		}

		[[nodiscard]] const std::string& getName() const noexcept{ return name; }

		void eraseCount(const int fullKey){
			if(const auto itr = groupOccupiedKeys.find(fullKey); itr != groupOccupiedKeys.end()){
				itr->second--;
			}
		}

		void addCount(const int fullKey){
			if(const auto itr = groupOccupiedKeys.find(fullKey); itr != groupOccupiedKeys.end()){
				itr->second++;
			}
		}

		bool isConfilcted(const int fullKey){
			if(const auto itr = groupOccupiedKeys.find(fullKey); itr != groupOccupiedKeys.end()){
				return itr->second > 1;
			}
			return false;
		}

		void addKey(const Operation& operation){
			const auto op = binds.insert_or_assign(operation.name, operation).first;
			registerInfo(op->second);

		}

		void addKey(Operation&& operation){
			registerInfo(operation);
			binds.insert_or_assign(operation.name, operation);
		}

		void setDef(){
			for (auto & bind : binds | std::views::values){
				bind.setDef();
			}
		}

		bool applyToTarget(){
		    //TODO throw
			if(!targetGroup)return false;

			// new (targetGroup) InputBindGroup;
			targetGroup->clearAllBinds();

			for (const auto & bind : binds | std::views::values){
				targetGroup->registerBind(InputBind{bind.customeBind});
			}

			return true;
		}

		bool isEquivalentTo(const Ctrl::OperationGroup& other) const{
			return binds == other.binds;
			// using PairTy = decltype(binds)::value_type;
			// using ValTy = PairTy::second_type;

			// return std::ranges::equal(binds, other.binds, [](const ValTy& l, const ValTy& r) constexpr {
			// 	return l.customeBind == r.customeBind;
			// }, &PairTy::second, &PairTy::second);
		}
	};

	void Operation::setCustom(const int key, const int mode){
		group->eraseCount(customeBind.getFullKey());
		customeBind.setKey(key);
		customeBind.setMode(mode);
		customeBind.setIgnoreMode(defaultBind.isIgnoreMode() && mode == Ctrl::Mode::None);
		group->addCount(customeBind.getFullKey());

		updateRelativeBinds();
	}

	void Operation::updateRelativeBinds() const{
		if(!group)return;

		ext::StringHashMap<Operation>& bind = group->getBinds();

		for (const auto& relativeOperation : relativeOperations){
			if(auto itr = bind.find(relativeOperation); itr != bind.end()){
				itr->second.setCustom(customeBind.getKey(), customeBind.mode());
			}
		}
	}
}

export
template<>
struct ::std::equal_to<Core::Ctrl::Operation>{
	bool operator()(const Core::Ctrl::Operation& l, const Core::Ctrl::Operation& r) const noexcept{
		return l.name == r.name;
	}
};

export
template<>
struct ::std::equal_to<Core::Ctrl::OperationGroup>{
	bool operator()(const Core::Ctrl::OperationGroup& l, const Core::Ctrl::OperationGroup& r) const noexcept{
		return l.getName() == r.getName();
	}
};

export
template <>
	struct ext::json::JsonSerializator<Core::Ctrl::Operation>{
	static void write(ext::json::JsonValue& jsonValue, const Core::Ctrl::Operation& data){
		jsonValue.asObject();
		jsonValue.append("full", static_cast<ext::json::Integer>(data.customeBind.getFullKey()));
		jsonValue.append("ignore", data.customeBind.isIgnoreMode());

	}

	static void read(const ext::json::JsonValue& jsonValue, Core::Ctrl::Operation& data){
		auto& map = jsonValue.asObject();
		if(const auto val = map.tryFind("full")){
			auto [k, a, m] = Core::Ctrl::getSeperatedKey(val->as<int>());
			data.setCustom(k, m);
		}

		if(const auto val = map.tryFind("ignore")){
			data.customeBind.setIgnoreMode(val->as<bool>());
		}
	}
};

export
template <>
	struct ext::json::JsonSerializator<Core::Ctrl::OperationGroup>{
	using UmapIO = JsonSrlContBase_string_map<Core::Ctrl::Operation, true>;

	static void write(ext::json::JsonValue& jsonValue, const Core::Ctrl::OperationGroup& data){
		ext::json::JsonValue bindsData{};

		UmapIO::write(bindsData, data.getBinds());

		jsonValue.asObject();
		jsonValue.append("binds", bindsData);
	}

	static void read(const ext::json::JsonValue& jsonValue, Core::Ctrl::OperationGroup& data){
		const ext::json::JsonValue* bindsData = jsonValue.asObject().tryFind("binds");

		if(bindsData)UmapIO::read(*bindsData, data.getBinds());
	}
};

