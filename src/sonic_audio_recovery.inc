    void report_device(std::uint32_t code,const char* operation) const noexcept {
        ::sonic::recovery::audio_fault(code);
        std::fprintf(stderr,"SONIC_AUDIO_DEVICE operation=%s code=%u mode=%s\n",
                     operation,code,device_?"connected":"silent");
    }

    void update_silent_position() noexcept {
        const auto now=::sonic::audio_device::now();
        if(now<silent_time_){silent_time_=now;silent_fraction_=0;return;}
        const auto elapsed=now-silent_time_;silent_time_=now;
        if(state_!=NativePortAudioState::Running || played_frames_>=submitted_frames_){
            silent_fraction_=0;return;
        }
        const double frames=double(elapsed)*config_.format.sample_rate/1'000'000'000.0+silent_fraction_;
        const auto remaining=submitted_frames_-played_frames_;
        const auto advance=frames>=double(remaining)?remaining:std::uint64_t(frames);
        played_frames_+=advance;
        silent_fraction_=advance==remaining?0:frames-double(advance);
    }

    void service_device() {
        if(recovering_ || state_==NativePortAudioState::Stopped || state_==NativePortAudioState::Failed)return;
        const auto now=::sonic::audio_device::now();
        const auto resumed=::sonic::audio_device::resumes.load(std::memory_order_acquire);
        if(resumed!=seen_resume_){seen_resume_=resumed;silent_time_=now;silent_fraction_=0;}
        const auto version=::sonic::audio_device::changes.load(std::memory_order_acquire);
        if(version!=seen_change_ || (!device_&&!permanently_silent_&&now>=retry_at_)){
            if(device_){
                update_playback_position();
                // A failed cursor query has already closed the failed device.
                if(!device_){seen_change_=version;return;}
            }else update_silent_position();
            recover_endpoint(0,"default-or-device-change",true);
        }
    }

    void recover_endpoint(std::uint32_t code,const char* operation,bool reconnect) {
        const bool nested=recovering_;
        recovering_=true;
        struct End {bool& flag;bool previous;~End(){flag=previous;}} end{recovering_,nested};
        const auto old_state=state_;
        const auto old_submitted=submitted_frames_,old_buffers=submitted_buffers_;
        const auto cursor=std::min(old_submitted,std::max(played_frames_,completed_frames_));
        // Copy only still-unheard samples before releasing device-owned memory.
        // A failed write may be present without having been accepted by submit:
        // the submitted-frame bound deliberately excludes that provisional block.
        std::vector<std::vector<std::int16_t>> remaining;
        auto position=completed_frames_;
        for(const auto& pending:pending_){
            if(!pending.block)continue;
            const auto& block=*pending.block;
            const auto finish=std::min(old_submitted,position+block.frames);
            const auto first=std::max(cursor,position);
            if(first<finish)remaining.emplace_back(
                block.samples.begin()+std::size_t(first-position)*config_.format.channels,
                block.samples.begin()+std::size_t(finish-position)*config_.format.channels);
            position+=block.frames;
        }
        const auto close_error=stop_unchecked();
        // The retained SDK shutdown fences callbacks and quarantines storage
        // after a failed close. Do not repeatedly allocate drivers in that case.
        if(close_failed_)permanently_silent_=true;
        pending_.clear();pool_.clear();free_pool_indices_.clear();completion_wake_.reset();
        submitted_frames_=completed_frames_=played_frames_=cursor;
        queued_frames_=0;error_code_=0;state_=NativePortAudioState::Ready;
        if(reconnect&&!permanently_silent_)initialize_device();
        else {
            // A bounded software-clock queue keeps movies and title audio
            // progressing while no hardware is available. Retry at most 1 Hz.
            device_=nullptr;device_base_=cursor;
            silent_time_=::sonic::audio_device::now();silent_fraction_=0;
            retry_at_=silent_time_+1'000'000'000;seen_change_=::sonic::audio_device::changes.load();
        }
        // Reopening from a paused stream must never emit a brief audible tail.
        if(device_ && old_state==NativePortAudioState::Paused){
            const auto result=::sonic::audio_device::api().pause(device_);
            if(result!=MMSYSERR_NOERROR){
                static_cast<void>(stop_unchecked());if(close_failed_)permanently_silent_=true;
                pending_.clear();pool_.clear();free_pool_indices_.clear();completion_wake_.reset();
                device_=nullptr;state_=NativePortAudioState::Ready;error_code_=0;
            }
        }
        for(const auto& samples:remaining){
            // During recovery a new device can also fail. Fall back to silent
            // storage once, retaining data; never recurse through reopen loops.
            if(!submit(samples))throw std::logic_error("audio-recovery-queue-budget");
        }
        submitted_buffers_=old_buffers;
        if(submitted_frames_!=old_submitted)throw std::logic_error("audio-recovery-frame-accounting");
        state_=old_state;silent_time_=::sonic::audio_device::now();
        output_status_.set(device_?::sonic::recovery::OutputState::Connected:
            permanently_silent_?::sonic::recovery::OutputState::RestartRequired : ::sonic::recovery::OutputState::Silent);
        if(device_)::sonic::recovery::reconnections.fetch_add(1,std::memory_order_relaxed);
        report_device(code?code:close_error,operation);
    }
