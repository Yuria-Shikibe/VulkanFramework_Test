export module Assets.Directories;

export import Core.File;

namespace Assets{
	namespace Dir{
	    export {
	        inline Core::File assets;

	        inline Core::File shader;
	        inline Core::File shader_spv;
	        inline Core::File shader_src;

	        inline Core::File texture;
	        inline Core::File font;
	        inline Core::File svg;
	        inline Core::File bundle;
	        inline Core::File sound;

	        inline Core::File data;
	        inline Core::File settings;

	        inline Core::File game;

	        inline Core::File cache;
	        inline Core::File texCache;
	    }

		void patch(const Core::File& file){
			if(!file.exist()){
				file.createDirQuiet();
			}
		}
	}

	export void loadDir();
}
