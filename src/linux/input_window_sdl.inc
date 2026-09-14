namespace {
// Keep the established INI key identities compatible with Windows. SDL still
// supplies the actual keyboard-layout label and physical event on Linux.
constexpr std::pair<SDL_Keycode,unsigned> named_keys[]{
    {SDLK_BACKSPACE,0x08},{SDLK_TAB,0x09},{SDLK_RETURN,0x0d},{SDLK_LSHIFT,0x10},{SDLK_RSHIFT,0x10},
    {SDLK_LCTRL,0x11},{SDLK_RCTRL,0x11},{SDLK_LALT,0x12},{SDLK_RALT,0x12},{SDLK_PAUSE,0x13},
    {SDLK_CAPSLOCK,0x14},{SDLK_ESCAPE,0x1b},{SDLK_SPACE,0x20},{SDLK_PAGEUP,0x21},{SDLK_PAGEDOWN,0x22},
    {SDLK_END,0x23},{SDLK_HOME,0x24},{SDLK_LEFT,0x25},{SDLK_UP,0x26},{SDLK_RIGHT,0x27},{SDLK_DOWN,0x28},
    {SDLK_PRINTSCREEN,0x2c},{SDLK_INSERT,0x2d},{SDLK_DELETE,0x2e},{SDLK_LGUI,0x5b},{SDLK_RGUI,0x5c},
    {SDLK_APPLICATION,0x5d},{SDLK_KP_0,0x60},{SDLK_KP_1,0x61},{SDLK_KP_2,0x62},{SDLK_KP_3,0x63},
    {SDLK_KP_4,0x64},{SDLK_KP_5,0x65},{SDLK_KP_6,0x66},{SDLK_KP_7,0x67},{SDLK_KP_8,0x68},
    {SDLK_KP_9,0x69},{SDLK_KP_MULTIPLY,0x6a},{SDLK_KP_PLUS,0x6b},{SDLK_KP_MINUS,0x6d},
    {SDLK_KP_PERIOD,0x6e},{SDLK_KP_DIVIDE,0x6f},{SDLK_NUMLOCKCLEAR,0x90},{SDLK_SCROLLLOCK,0x91},
    {SDLK_KP_ENTER,0x0d}};
constexpr std::pair<SDL_Scancode,unsigned> punctuation_keys[]{
    {SDL_SCANCODE_SEMICOLON,0xba},{SDL_SCANCODE_EQUALS,0xbb},{SDL_SCANCODE_COMMA,0xbc},
    {SDL_SCANCODE_MINUS,0xbd},{SDL_SCANCODE_PERIOD,0xbe},{SDL_SCANCODE_SLASH,0xbf},
    {SDL_SCANCODE_GRAVE,0xc0},{SDL_SCANCODE_LEFTBRACKET,0xdb},{SDL_SCANCODE_BACKSLASH,0xdc},
    {SDL_SCANCODE_RIGHTBRACKET,0xdd},{SDL_SCANCODE_APOSTROPHE,0xde},{SDL_SCANCODE_NONUSBACKSLASH,0xe2}};
unsigned virtual_key(const SDL_KeyboardEvent& event) {
    const auto key=event.key;
    if(key>=SDLK_A&&key<=SDLK_Z)return unsigned(key-SDLK_A+'A');
    if(key>=SDLK_0&&key<=SDLK_9)return unsigned(key);
    if(key>=SDLK_F1&&key<=SDLK_F12)return unsigned(key-SDLK_F1+0x70);
    if(key>=SDLK_F13&&key<=SDLK_F24)return unsigned(key-SDLK_F13+0x7c);
    for(const auto& [source,target]:named_keys)if(key==source)return target;
    for(const auto& [source,target]:punctuation_keys)if(event.scancode==source)return target;
    return 0;
}
std::wstring sdl_key_label(unsigned key) {
    SDL_Keycode code=0;
    if(key>='A'&&key<='Z')code=SDL_Keycode(key-'A'+SDLK_A);
    else if(key>='0'&&key<='9')code=SDL_Keycode(key);
    else if(key>=0x70&&key<=0x7b)code=SDL_Keycode(key-0x70+SDLK_F1);
    else if(key>=0x7c&&key<=0x87)code=SDL_Keycode(key-0x7c+SDLK_F13);
    else for(const auto& [source,target]:named_keys)if(key==target){code=source;break;}
    if(!code)for(const auto& [source,target]:punctuation_keys)if(key==target){code=SDL_GetKeyFromScancode(source,SDL_KMOD_NONE,false);break;}
    if(!code)return {};
    std::string_view name=SDL_GetKeyName(code);std::wstring result;
    for(std::size_t i=0;i<name.size();){
        unsigned cp=static_cast<unsigned char>(name[i++]),count=0;
        if(cp>=0xf0){cp&=7;count=3;}else if(cp>=0xe0){cp&=15;count=2;}else if(cp>=0xc0){cp&=31;count=1;}
        for(unsigned n=0;n<count;++n){if(i==name.size()||(static_cast<unsigned char>(name[i])&0xc0)!=0x80){cp=0xfffd;break;}cp=(cp<<6)|(static_cast<unsigned char>(name[i++])&63);}
        result+=wchar_t(cp>0x10ffff?0xfffd:cp);
    }
    return result;
}
void dimensions(SDL_Window* handle) {
    int w=0,h=0;SDL_GetWindowSizeInPixels(handle,&w,&h);
    os.client_width=unsigned(std::max(w,0));os.client_height=unsigned(std::max(h,0));
}
}
void window_created(void* handle) {
    auto* h=static_cast<SDL_Window*>(handle);window=h;
    std::lock_guard guard(lock);os={};
    if(h){os.focused=(SDL_GetWindowFlags(h)&SDL_WINDOW_INPUT_FOCUS)!=0;dimensions(h);}
    else {camera_active=false;look_dx=0;look_dy=0;}
}
void set_modal(bool value) noexcept {modal=value;if(value){camera_active=false;look_dx=0;look_dy=0;}}
void set_camera_active(bool value) noexcept {camera_active=value;}
bool window_message(void*,unsigned,std::uintptr_t,std::intptr_t) noexcept {return false;}
void window_update(void* handle) noexcept {
    auto* h=static_cast<SDL_Window*>(handle);if(!h)return;
    try {
        std::lock_guard guard(lock);dimensions(h);
        const bool relative=!hidden()&&os.focused&&camera_active.load()&&!modal.load()&&presentation::settings().mouse_camera;
        if(SDL_GetWindowRelativeMouseMode(h)!=relative)SDL_SetWindowRelativeMouseMode(h,relative);
    }catch(...){}
}
void window_event(void* handle,const SDL_Event& event) noexcept {
    auto* h=static_cast<SDL_Window*>(handle);if(!h)return;
    try {
        std::lock_guard guard(lock);
        switch(event.type){
        case SDL_EVENT_WINDOW_FOCUS_GAINED:os.focused=true;dimensions(h);break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:os={};camera_active=false;look_dx=0;look_dy=0;break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:dimensions(h);break;
        case SDL_EVENT_KEY_DOWN:case SDL_EVENT_KEY_UP:{
            if(const auto key=virtual_key(event.key);key&&key<os.keys.size())os.keys[key]=event.type==SDL_EVENT_KEY_DOWN;
            os.keys[0x10]=(event.key.mod&SDL_KMOD_SHIFT)!=0;
            os.keys[0x11]=(event.key.mod&SDL_KMOD_CTRL)!=0;
            os.keys[0x12]=(event.key.mod&SDL_KMOD_ALT)!=0;
            last_device=GlyphStyle::Keyboard;break;
        }
        case SDL_EVENT_MOUSE_MOTION:{
            int w=0,hgt=0;SDL_GetWindowSize(h,&w,&hgt);
            os.cursor_x=int(event.motion.x*os.client_width/std::max(w,1));
            os.cursor_y=int(event.motion.y*os.client_height/std::max(hgt,1));
            if(os.focused&&camera_active.load()&&!modal.load()&&presentation::settings().mouse_camera){
                os.mouse_dx+=event.motion.xrel;os.mouse_dy+=event.motion.yrel;last_device=GlyphStyle::Keyboard;
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:case SDL_EVENT_MOUSE_BUTTON_UP:{
            unsigned button=event.button.button;
            if(button==SDL_BUTTON_MIDDLE)button=3;else if(button==SDL_BUTTON_RIGHT)button=2;
            if(button<os.mouse.size())os.mouse[button]=event.type==SDL_EVENT_MOUSE_BUTTON_DOWN;
            last_device=GlyphStyle::Keyboard;break;
        }
        case SDL_EVENT_MOUSE_WHEEL:os.wheel+=int(event.wheel.y)*(event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1:1);break;
        default:break;
        }
    }catch(...){}
}
