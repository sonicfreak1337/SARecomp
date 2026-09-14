#pragma once
#include "sonic_presentation.hpp"
#include "katana/runtime/native_port_graphics.hpp"
#include "katana/runtime/native_port_platform.hpp"
namespace sonic::restart {
// Called before any guest starts. A live parent owns rollback until explicit
// confirmation; a dead parent's transaction is recovered on the next launch.
void recover(const std::filesystem::path& executable);
bool confirm_display(katana::runtime::NativePortDesktopHost&,katana::runtime::NativePortPlatformServices&);
// Called only after the entire old host and save provider have been destroyed.
int launch(const std::filesystem::path& executable,const presentation::Settings& next,int language,
           const presentation::Settings* edited_from=nullptr);
}
