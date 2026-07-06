#include <MainApplication.hpp>
#include <Lang.hpp>
#include <switch.h>
#include <curl/curl.h>

int main() {
    romfsInit();
    lang::loadPreference();
    lang::load();
    socketInitializeDefault();
    // curl_global_init is not thread-safe — must be called from the main thread
    // before any secondary threads start using libcurl.
    curl_global_init(CURL_GLOBAL_ALL);

    auto renderer_opts = pu::ui::render::RendererInitOptions(SDL_INIT_EVERYTHING, pu::ui::render::RendererHardwareFlags);
    renderer_opts.UseImage(pu::ui::render::ImgAllFlags);
    renderer_opts.SetPlServiceType(PlServiceType_User);
    renderer_opts.AddDefaultAllSharedFonts();
    renderer_opts.SetInputPlayerCount(1);
    renderer_opts.AddInputNpadStyleTag(HidNpadStyleSet_NpadStandard);
    renderer_opts.AddInputNpadIdType(HidNpadIdType_Handheld);
    renderer_opts.AddInputNpadIdType(HidNpadIdType_No1);

    auto renderer = pu::ui::render::Renderer::New(renderer_opts);
    auto main = MainApplication::New(renderer);

    const auto rc = main->Load();
    if(R_FAILED(rc)) {
        diagAbortWithResult(rc);
    }

    main->Show();

    socketExit();
    romfsExit();
    return 0;
}
