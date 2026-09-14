// Included inside katana::runtime. Only the audio execution-domain owner
// touches this endpoint. The SDL callback merely wakes that owner.
class NativePortAudioEndpointCore final {
    struct Block {
        std::uint64_t first=0,end=0;
        std::vector<std::int16_t> samples;
    };
    NativePortAudioConfig config_;
    std::thread::id owner_=std::this_thread::get_id();
    NativePortAudioExecutionDomain* domain_;
    ::sonic::audio_device::Watch watch_;
    ::sonic::recovery::OutputStatus output_status_;
    SDL_AudioStream* stream_=nullptr;
    std::deque<Block> pending_;
    NativePortAudioState state_=NativePortAudioState::Ready;
    std::uint64_t submitted_buffers_=0,submitted_frames_=0,completed_frames_=0,played_frames_=0,queries_=0;
    std::uint64_t base_=0,handed_=0,pull_time_=0,clock_=0,retry_at_=0,seen_change_=0,seen_resume_=0;
    std::uint64_t latency_frames_=0;
    double fraction_=0;
    void require_owner_thread() const {
        if(std::this_thread::get_id()!=owner_)throw std::logic_error("native-port-audio-thread-violation");
    }
    static std::uint64_t awake() noexcept {return ::sonic::audio_device::working_time();}
    static void SDLCALL wake(void* owner,SDL_AudioStream*,int,int) noexcept {
        static_cast<NativePortAudioEndpointCore*>(owner)->domain_->request_consumer_service();
    }
    void close_device() noexcept {
        if(!stream_)return;
        // Setting a null callback takes SDL's stream lock and fences every
        // callback before the domain pointer / endpoint can be destroyed.
        SDL_SetAudioStreamGetCallback(stream_,nullptr,nullptr);
        SDL_DestroyAudioStream(stream_);stream_=nullptr;
    }
    void silent(const char* operation) noexcept {
        std::fprintf(stderr,"SONIC_AUDIO_DEVICE operation=%s backend=SDL3 mode=silent detail=%s\n",operation,SDL_GetError());
        ::sonic::recovery::audio_fault(1);
        close_device();clock_=awake();fraction_=0;retry_at_=clock_+1'000'000'000;
        output_status_.set(::sonic::recovery::OutputState::Silent);
    }
    bool latency() noexcept {
        SDL_AudioSpec format{};int frames=0;
        if(!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream_),&format,&frames) || format.freq<=0 || frames<1)return false;
        latency_frames_=(std::uint64_t(frames)*config_.format.sample_rate+format.freq-1)/format.freq;
        return true;
    }
    bool open_device() {
        retry_at_=awake()+1'000'000'000;
        if(!watch_.ensure())return false;
        const SDL_AudioSpec format{SDL_AUDIO_S16,int(config_.format.channels),int(config_.format.sample_rate)};
        stream_=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&format,wake,this);
        if(!stream_)return false;
        // SDL opens paused. Retain unheard PCM across device removal and never
        // briefly play a paused stream while opening the replacement endpoint.
        for(const auto& block:pending_){
            const auto first=std::max(played_frames_,block.first);
            if(first>=block.end)continue;
            const auto offset=std::size_t(first-block.first)*config_.format.channels;
            if(!SDL_PutAudioStreamData(stream_,block.samples.data()+offset,int((block.samples.size()-offset)*sizeof(std::int16_t)))){
                close_device();return false;
            }
        }
        if(!latency()){close_device();return false;}
        base_=handed_=played_frames_;pull_time_=clock_=awake();fraction_=0;
        if(state_==NativePortAudioState::Running&&!SDL_ResumeAudioStreamDevice(stream_)){close_device();return false;}
        output_status_.set(::sonic::recovery::OutputState::Connected);
        return true;
    }
    void update_position() {
        const auto now=awake();
        const auto elapsed=now>=clock_?now-clock_:0;clock_=now;
        if(state_!=NativePortAudioState::Running||played_frames_>=submitted_frames_){fraction_=0;return;}
        if(stream_){
            const int queued=SDL_GetAudioStreamQueued(stream_);
            if(queued<0){silent("queue-position");return;}
            const auto remaining=(std::uint64_t(queued)+config_.format.channels*2-1)/(config_.format.channels*2);
            const auto handed=submitted_frames_-std::min(remaining,submitted_frames_-base_);
            if(handed>handed_){handed_=handed;pull_time_=now;}
            // SDL exposes consumed input, not a DAC sample counter. Keep one
            // device period behind that boundary, then advance its bounded
            // tail on the awake clock. This also drains the final movie block;
            // a permanent latency subtraction would leave it stuck forever.
            const auto tail=std::min(handed_-base_,latency_frames_);
            const auto since=now>=pull_time_?now-pull_time_:0;
            const auto advance=std::min(tail,std::uint64_t(double(since)*config_.format.sample_rate/1e9));
            played_frames_=std::max(played_frames_,handed_-tail+advance);
        }else{
            const double frames=double(elapsed)*config_.format.sample_rate/1e9+fraction_;
            const auto remaining=submitted_frames_-played_frames_;
            const auto advance=frames>=double(remaining)?remaining:std::uint64_t(frames);
            played_frames_+=advance;fraction_=advance==remaining?0:frames-double(advance);
        }
        played_frames_=std::min(played_frames_,submitted_frames_);
    }
    void service_device() {
        if(state_==NativePortAudioState::Stopped||state_==NativePortAudioState::Failed)return;
        const auto now=awake();
        const auto resumed=::sonic::audio_device::resumes.load(std::memory_order_acquire);
        if(resumed!=seen_resume_){seen_resume_=resumed;clock_=pull_time_=now;fraction_=0;}
        const auto changed=::sonic::audio_device::changes.load(std::memory_order_acquire);
        if(changed!=seen_change_){
            seen_change_=changed;
            // SDL's default logical device follows output changes itself.
            if(stream_&&!latency())silent("device-change");
            if(!stream_)retry_at_=now;
        }
        if(!stream_&&now>=retry_at_){
            update_position();
            if(open_device())::sonic::recovery::reconnections.fetch_add(1,std::memory_order_relaxed);
        }
    }
public:
    NativePortAudioEndpointCore(const NativePortAudioConfig& config,NativePortAudioExecutionDomain& domain)
        :config_(config),domain_(&domain){
        validate_config(config_);clock_=awake();
        seen_change_=::sonic::audio_device::changes.load();seen_resume_=::sonic::audio_device::resumes.load();
        if(!open_device())silent("open");
    }
    ~NativePortAudioEndpointCore(){stop_unchecked();}
    const NativePortAudioFormat& format() const noexcept {return config_.format;}
    bool submit(std::span<const std::int16_t> samples){
        require_owner_thread();
        if(state_==NativePortAudioState::Stopped||state_==NativePortAudioState::Failed)throw std::logic_error("native-port-audio-not-running");
        if(state_==NativePortAudioState::Paused)throw std::logic_error("native-port-audio-paused");
        if(samples.empty()||samples.size()%config_.format.channels)throw std::invalid_argument("native-port-audio-frame-layout");
        const auto frames=samples.size()/config_.format.channels;
        if(frames>config_.maximum_queued_frames)throw std::out_of_range("native-port-audio-buffer-size");
        poll();
        if(submitted_frames_-completed_frames_>config_.maximum_queued_frames-frames)return false;
        Block block{submitted_frames_,submitted_frames_+frames,{samples.begin(),samples.end()}};
        if(background_test_mode_requested())std::fill(block.samples.begin(),block.samples.end(),0);
        pending_.push_back(std::move(block));
        if(stream_&&!SDL_PutAudioStreamData(stream_,pending_.back().samples.data(),int(samples.size_bytes())))silent("submit");
        submitted_frames_+=frames;saturating_add(submitted_buffers_,1);
        const bool starting=state_!=NativePortAudioState::Running;
        state_=NativePortAudioState::Running;
        if(starting){
            clock_=pull_time_=awake();
            if(stream_&&!SDL_ResumeAudioStreamDevice(stream_))silent("resume-first-buffer");
        }
        return true;
    }
    void poll(){
        require_owner_thread();service_device();update_position();
        while(!pending_.empty()&&pending_.front().end<=played_frames_){
            completed_frames_=pending_.front().end;pending_.pop_front();
        }
    }
    void refresh_playback_position(){require_owner_thread();saturating_add(queries_,1);poll();}
    void pause(){
        require_owner_thread();
        if(state_==NativePortAudioState::Stopped||state_==NativePortAudioState::Failed||state_==NativePortAudioState::Paused)return;
        poll();if(stream_&&!SDL_PauseAudioStreamDevice(stream_))silent("pause");
        state_=NativePortAudioState::Paused;clock_=awake();
    }
    void resume(){
        require_owner_thread();
        if(state_==NativePortAudioState::Stopped||state_==NativePortAudioState::Failed)throw std::logic_error("native-port-audio-stopped");
        if(state_!=NativePortAudioState::Paused)return;
        service_device();clock_=pull_time_=awake();fraction_=0;
        state_=NativePortAudioState::Running;
        if(stream_&&!SDL_ResumeAudioStreamDevice(stream_))silent("resume");
    }
    void stop(){require_owner_thread();stop_unchecked();}
    std::uint32_t stop_unchecked() noexcept {
        close_device();pending_.clear();state_=NativePortAudioState::Stopped;
        output_status_.set(::sonic::recovery::OutputState::Idle);return 0;
    }
    NativePortAudioSnapshot snapshot() const noexcept {
        return {state_,submitted_buffers_,submitted_frames_,completed_frames_,
            state_==NativePortAudioState::Stopped?0:submitted_frames_-completed_frames_,played_frames_,queries_,0};
    }
    std::uint64_t playback_position_frames() const noexcept {return played_frames_;}
};
