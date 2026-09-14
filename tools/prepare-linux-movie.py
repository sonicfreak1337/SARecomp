"""Bind the pinned movie worker to its existing FFmpeg provider on Linux.

Keep mailbox backpressure, timestamps, audio master clock, queue bounds and
public execution-domain facade. Only MF and Windows file handles are replaced.
"""
from pathlib import Path
import hashlib
import re
import sys

source, destination = map(Path, sys.argv[1:])
raw = source.read_bytes()
if hashlib.sha256(raw).hexdigest() != 'a3f723d9abb05cb41e11af9f53cd2cbc3dfb76dd6dee44f5f02d14173f6f1f16':
    raise RuntimeError('Pinned movie worker changed')
text = raw.decode()

def section(first, last):
    if text.count(first) != 1:
        raise RuntimeError('Movie extraction boundary changed: '+first)
    start=text.index(first)
    return text[start:text.index(last,start)]

provider=section('    struct ProviderSample final {','    void open_identity_locked_content()')
provider+='''    void open_linux_content() {
        try {
            content_ = std::make_unique<sonic::linux_content::SealedContent>(
                content_root_path_, path_.lexically_relative(content_root_path_), content_identity_);
            content_size_ = content_->bytes().size();
        } catch (const sonic::linux_content::Error& error) {
            fail_and_throw(error.failure == sonic::linux_content::Failure::Identity
                ? NativePortMovieFailure::ContentIdentity : NativePortMovieFailure::ContentLoad,
                error.what());
        }
    }
    static std::uint32_t provider_read_at(void* user, std::uint64_t offset,
                                         void* destination, std::uint64_t count) noexcept {
        const auto& self=*static_cast<NativePortMovieWorker*>(user);
        if (!destination || !self.content_ || offset > self.content_size_ || count > self.content_size_-offset) return 0;
        if (count) std::memcpy(destination,self.content_->bytes().data()+std::size_t(offset),std::size_t(count));
        return 1;
    }
'''
provider+=section('    [[nodiscard]] static NativePortMovieFailure\n    map_codec_failure(', '    void initialize_backend()')
provider+=section('    [[nodiscard]] bool submit_provider_audio_batch(', '    void read_pending()')
private=section('  private:\n#ifdef _WIN32\n    struct PendingSample final {', '\n    void complete_if_drained()')
text=text.replace(private,'  private:\n'+provider)

opening=section('#ifdef _WIN32\n            open_identity_locked_content();','\n            transition(NativePortMovieState::Ready);')
text=text.replace(opening,'''            open_linux_content();
            if (!config.codec_provider)
                return fail_and_throw(NativePortMovieFailure::DecoderUnavailable, "linux-codec-provider-required");
            initialize_codec_provider(*config.codec_provider);''')
text=text.replace('            update_playback_position(host_time);','            update_playback_position(host_time);\n            decode_provider_until_position();')
text=text.replace('#ifdef _WIN32\n        } catch (const NativePortMovieVideoMailboxBackpressure&)',
                  '        } catch (const NativePortMovieVideoMailboxBackpressure&)')
text=text.replace('        } catch (const PlatformError& error) {\n            transition(NativePortMovieState::Failed,\n                       map_media_foundation_failure',
                  '#ifdef _WIN32\n        } catch (const PlatformError& error) {\n            transition(NativePortMovieState::Failed,\n                       map_media_foundation_failure')

complete=section('    void complete_if_drained() {','    void bind_or_require_owner_thread()')
new_complete=complete.replace('#ifdef _WIN32\n','').replace('#endif\n','')
new_complete=new_complete.replace('''            codec_provider_ != nullptr
                ? provider_pending_.has_value() || !provider_video_queue_.empty()
                : pending_.has_value() || !video_queue_.empty();''',
                                 '            provider_pending_.has_value() || !provider_video_queue_.empty();')
text=text.replace(complete,new_complete)

reset=section('    void reset_for_open() noexcept {','    void apply_deferred_stop()')
new_reset=reset.replace('#ifdef _WIN32\n','').replace('#endif\n','')
for field in ('pending_.reset();','video_queue_.clear();','mf_video_queue_bytes_ = 0u;'):
    new_reset=new_reset.replace('        '+field+'\n','')
text=text.replace(reset,new_reset)

close=section('    void close_backend(const bool reset_state = true) noexcept {','    void transition(')
new_close=close[:close.index('#ifdef _WIN32')]
new_close+=section('        if (codec_provider_ != nullptr && codec_decoder_ != nullptr)\n','        pending_.reset();')
new_close+='''        content_.reset();content_size_=0;
        if (reset_state) state_ = NativePortMovieState::Closed;
    }

'''
text=text.replace(close,new_close)
fields=section('#ifdef _WIN32\n    HANDLE content_handle_ = nullptr;','\n};')
new_fields='    std::unique_ptr<sonic::linux_content::SealedContent> content_;\n'
new_fields+=section('    std::uint64_t content_size_ = 0u;','    ComPtr<IMFSourceReader> reader_;')
new_fields+=section('    std::uint32_t video_width_ = 0u;','    bool com_owned_ = false;').replace('DWORD','std::uint32_t')
text=text.replace(fields,new_fields)

# All remaining platform guards belong solely to Media Foundation. Do not
# pretend to be Windows at compile time or relax any codec validation.
lines=[];stack=[];active=True
for line in text.splitlines(keepends=True):
    if line.strip()=='#ifdef _WIN32': stack.append(active);active=False
    elif line.strip()=='#else' and stack: active=stack[-1] and not active
    elif line.strip()=='#endif' and stack: active=stack.pop()
    elif active: lines.append(line)
if stack: raise RuntimeError('Unbalanced movie platform guards')
text='#include "sealed_content.hpp"\n'+''.join(lines)
for obsolete in ('ComPtr<','DWORD','content_handle_','pending_.has_value() || !video_queue_','decode_until_position();'):
    if obsolete in text: raise RuntimeError('Unconverted movie platform dependency: '+obsolete)
destination.parent.mkdir(parents=True,exist_ok=True)
if not destination.exists() or destination.read_text()!=text:
    destination.write_text(text,encoding='utf-8')
print('SONIC_LINUX_MOVIE_READY codec=retained-FFmpeg content=sealed facade=retained')
