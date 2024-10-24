//
// Created by Matrix on 2024/4/2.
//

export module OS.TextInputListener;

import std;

export namespace Core{
	struct TextInputListener{
		virtual ~TextInputListener() = default;

		virtual void informTextInput(unsigned int codepoint, int mods) = 0;
		virtual void informEnter(int mods){}
		virtual void informEscape(unsigned int codepoint, int mods){}

		virtual void informBackSpace(int mods) = 0;
		virtual void informDelete(int mods) = 0;

		virtual void informSelectAll(){}
		virtual void informClipboardPaste(const std::string_view str){}
		virtual std::string getClipboardCopy(){return {};}
		virtual std::string getClipboardClip(){return {};}
		//virtual void passIMEContext(<?>){}

		virtual void informDo(){}
		virtual void informUndo(){}
	};
}
