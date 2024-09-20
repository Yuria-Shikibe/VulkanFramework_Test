module Assets.Directories;


void Assets::loadDir(){
    using namespace Dir;


    assets = files.findDir("assets");
    patch(assets);

    shader = assets.subFile("shader");
    patch(shader);

    shader_spv = shader.subFile("spv");
    patch(shader_spv);

    shader_src = shader.subFile("src");
    patch(shader_src);

    texture = assets.subFile("texture");
    patch(texture);

    font = assets.subFile("fonts");
    patch(font);

    sound = assets.subFile("sounds");
    patch(sound);

    svg = assets.subFile("svg");
    patch(svg);

    bundle = assets.subFile("bundles");
    patch(bundle);


    cache = files.findDir("resource").subFile("cache");
    patch(cache);

    texCache = cache.subFile("tex");
    patch(texCache);

    game = assets.subFile("game");
    patch(game);

    data = files.findDir("resource").subFile("data");
    patch(data);

    settings = data.subFile("settings");
    patch(settings);
    //
    // screenshot = mainTree.find("screenshots"); //TODO move this to other places, it doesn't belong to assets!
    // patch(texCache);
}
