// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Read-only, fail-closed control of the ORIGINAL Akai S3000XL SAMPLE/REC UI.
// This sends real panel button pulses only after the live firmware LCD is
// positively identified. No guest memory writes or shortcut samples.
// The Akai's own audio A/D/DMA and sample allocation remain authoritative.
#include <cstdint>
#include <cstddef>
#include <string_view>

namespace akai::capture {
enum class Button: unsigned char {
    None, Sample, RecF2, ArmF8, GoF7, Name, Enter,
    Digit0, Digit1, Digit2, Digit3, Digit4, Digit5, Digit6, Digit7, Digit8, Digit9
};
enum class Command: unsigned char { None, Press, Release };
enum class Stage: int {
    Idle=0, WaitWav=1, PressSample=2, WaitSetup=3,
    PressRec=4, WaitNewSample=5, PressArm=6, WaitArmed=7,
    PressGo=8, WaitRecording=9, Capturing=10,
    WaitStop=11, RecordedSample=12,
    PressName=13, PressNumericMode=14, TypeName=15,
    ConfirmName=16, WaitNamed=17,
    NeedNewSample=20, NoReadableLcd=21, OverwriteRisk=22,
    TimedOut=23, Busy=24, Cancelled=25,
    SourceUnavailable=26, SampleTruncated=27
};
struct Observe {
    // Exactly the CURRENT displayed LCD text, never stale unmapped RAM.
    std::string_view lcd{};
    bool readable=false;
    bool wavReady=false;
    bool wavComplete=false;
    bool wavSourcePresent=true; // accepted engine asset, not only idle wavReady
    bool firmwareReady=false;
    bool wadcsKnown=false;
    std::uint8_t wadcs=0;
};
struct Decision {
    Command command=Command::None;
    Button button=Button::None;
    Stage stage=Stage::Idle;
};
constexpr bool contains(std::string_view text,std::string_view wanted) noexcept {
    if (wanted.empty() || text.size() < wanted.size()) return false;
    for(std::size_t i=0;i<=text.size()-wanted.size();++i) {
        bool match=true;
        for(std::size_t k=0;k<wanted.size();++k) {
            char a=text[i+k],b=wanted[k];
            if (a>='a'&&a<='z')a=char(a-('a'-'A'));
            if (b>='a'&&b<='z')b=char(b-('a'-'A'));
            if(a!=b){match=false;break;}
        }
        if(match)return true;
    }
    return false;
}
// S3000XL V2 operator manual p.121: a unique name is marked "* NEW *"
// on the RECORD page. The preexisting candidate "- NEW -" is retained
// for compatibility but is never accepted outside an actual RECORD page.
inline bool hasUniqueRecordName(std::string_view lcd) noexcept {
    return contains(lcd,"RECORD") &&
        (contains(lcd,"* NEW *") || contains(lcd,"- NEW -"));
}
// These two text-mode headers are present in the owner's ORIGINAL V2.0
// firmware; never type into a generic readable LCD page. In particular,
// receiving an unrelated RECORD page while waiting for NAME must not make
// numeric keys modify unrelated native sampler parameters.
inline bool lettersEditorVisible(std::string_view lcd) noexcept {
    return contains(lcd,"LETTERS") && contains(lcd,"NAME FOR NUMBERS");
}
inline bool numbersEditorVisible(std::string_view lcd) noexcept {
    return contains(lcd,"NUMBERS") && contains(lcd,"NAME FOR LETTERS");
}
inline bool overwriteWarning(std::string_view text) noexcept {
    return contains(text,"OVERWRITE") || contains(text,"EXISTS!")
        || contains(text,"REPLACE EXISTING") || contains(text,"DELETE");
}
constexpr bool isRunningStage(Stage s) noexcept {
    return (s>=Stage::WaitWav && s<=Stage::WaitStop)
        || (s>=Stage::PressName && s<=Stage::WaitNamed);
}
constexpr Button numericButton(char digit) noexcept {
    switch(digit){
    case '0':return Button::Digit0;case '1':return Button::Digit1;
    case '2':return Button::Digit2;case '3':return Button::Digit3;
    case '4':return Button::Digit4;case '5':return Button::Digit5;
    case '6':return Button::Digit6;case '7':return Button::Digit7;
    case '8':return Button::Digit8;case '9':return Button::Digit9;
    default:return Button::None;
    }
}
class Controller {
public:
    bool start(std::uint64_t ms) noexcept {
        if (isRunning()) return false;
        if(nextName_>=999999999u)return false; // never re-use a known generated name
        nameSequence_=++nextName_;
        nameIndex_=0;
        nameRetries_=0;
        stage_=Stage::WaitWav; enteredMs_=ms; held_=Button::None;
        return true;
    }
    Decision cancel(std::uint64_t ms) noexcept {
        const auto released=held_;
        held_=Button::None; enteredMs_=ms; stage_=Stage::Cancelled;
        return {released==Button::None?Command::None:Command::Release,
                released,stage_};
    }
    bool isRunning() const noexcept {
        return isRunningStage(stage_);
    }
    Stage stage()const noexcept{return stage_;}
    // Called only by MAME owner thread with emulated milliseconds.
    // Strict timeout plus live LCD and WADCS feedback; never confirm overwrite.
    Decision step(const Observe& o,std::uint64_t ms) noexcept {
        auto result=[&](Command cmd=Command::None,Button b=Button::None) {
            return Decision{cmd,b,stage_};
        };
        if(!isRunning())return result();
        if(!o.firmwareReady)return finish(Stage::TimedOut);
        if (o.readable && overwriteWarning(o.lcd))
            return finish(Stage::OverwriteRisk);
        // Before GO, a replaced/unloaded WAV must abort; never record
        // silence just because the UI still displays an old filename.
        if ((stage_==Stage::PressSample || stage_==Stage::WaitSetup ||
             stage_==Stage::PressRec || stage_==Stage::WaitNewSample ||
             stage_==Stage::PressArm || stage_==Stage::WaitArmed ||
             (stage_>=Stage::PressName && stage_<=Stage::WaitNamed)) &&
             !o.wavReady)
            return finish(Stage::SourceUnavailable);
        if(ms>=enteredMs_ && ms-enteredMs_>timeoutForStage())
            return finish(stage_==Stage::WaitNewSample?Stage::NeedNewSample
                        :(!o.readable && (stage_==Stage::WaitSetup ||
                           stage_==Stage::WaitNewSample))?Stage::NoReadableLcd
                        :Stage::TimedOut);
        // Release a held button only after a stable >=120 ms key scan interval.
        if(held_!=Button::None) {
            if(ms>=pulseStartMs_ && ms-pulseStartMs_>=120) {
                auto b=held_;held_=Button::None;
                switch(stage_) {
                case Stage::PressSample: transition(Stage::WaitSetup,ms);break;
                case Stage::PressRec: transition(Stage::WaitNewSample,ms);break;
                case Stage::PressArm: transition(Stage::WaitArmed,ms);break;
                case Stage::PressGo: transition(Stage::WaitRecording,ms);break;
                case Stage::PressName: transition(Stage::PressNumericMode,ms);break;
                case Stage::PressNumericMode: transition(Stage::TypeName,ms);break;
                case Stage::TypeName:
                    if(++nameIndex_>=12)transition(Stage::ConfirmName,ms);
                    break;
                case Stage::ConfirmName: transition(Stage::WaitNamed,ms);break;
                default:break;
                }
                return result(Command::Release,b);
            }
            return result();
        }
        switch(stage_) {
            case Stage::WaitWav:
                if(o.wavReady && o.readable){
                    // If already on record setup, don't press SAMPLE again.
                    if(contains(o.lcd,"RECORD SET-UP")) {
                        transition(Stage::PressRec,ms);
                        return press(Button::RecF2,ms);
                    }
                    transition(Stage::PressSample,ms);
                    return press(Button::Sample,ms);
                }
                break;
            case Stage::WaitSetup:
                if(o.readable && contains(o.lcd,"RECORD SET-UP")) {
                    transition(Stage::PressRec,ms);
                    return press(Button::RecF2,ms);
                }
                break;
            case Stage::WaitNewSample:
                if(o.readable && hasUniqueRecordName(o.lcd)) {
                    // Firmware explicitly says this name does not already exist.
                    transition(Stage::PressArm,ms);
                    return press(Button::ArmF8,ms);
                }
                // Fresh S3000XL boots with existing factory waveforms:
                // the original REC page does NOT offer "* NEW *" by default.
                // Enter the original panel naming dialog, do not touch guest RAM.
                if(o.readable && contains(o.lcd,"RECORD") && ms>=enteredMs_
                   && ms-enteredMs_>=400) {
                    transition(Stage::PressName,ms);
                    return press(Button::Name,ms);
                }
                break;
            case Stage::PressNumericMode:
                if(o.readable && lettersEditorVisible(o.lcd))
                    return press(Button::Name,ms);
                break;
            case Stage::TypeName:
                if(o.readable && numbersEditorVisible(o.lcd))
                    return press(numericButton(generatedDigit(nameIndex_)),ms);
                break;
            case Stage::ConfirmName:
                if(o.readable && numbersEditorVisible(o.lcd))
                    return press(Button::Enter,ms);
                break;
            case Stage::WaitNamed:
                if(o.readable && hasUniqueRecordName(o.lcd)) {
                    transition(Stage::PressArm,ms);
                    return press(Button::ArmF8,ms);
                }
                if(o.readable && contains(o.lcd,"RECORD") && ms>=enteredMs_
                   && ms-enteredMs_>=700) {
                    if(++nameRetries_>16 || nextName_>=999999999u)
                        return finish(Stage::NeedNewSample);
                    nameSequence_=++nextName_;
                    nameIndex_=0;
                    transition(Stage::PressName,ms);
                    return press(Button::Name,ms);
                }
                break;
            case Stage::WaitArmed:
                // Upstream measured A2 -> 82 on ARM; bit3 must remain clear.
                if(o.readable && contains(o.lcd,"WAITING FOR START")
                   && o.wadcsKnown && (o.wadcs&0x08)==0
                   && (o.wadcs&0x20)==0) {
                    transition(Stage::PressGo,ms);
                    return press(Button::GoF7,ms);
                }
                break;
            case Stage::WaitRecording:
                if(o.wadcsKnown && (o.wadcs&0x08)!=0)
                    transition(Stage::Capturing,ms);
                break;
            case Stage::Capturing:
                if(!o.wavSourcePresent) return finish(Stage::SourceUnavailable);
                // The Akai's own REC TIME may be shorter than the WAV.
                // A native WADCS STOP before the source is exhausted is
                // a truncated sample, never a successful import.
                if(o.wadcsKnown && (o.wadcs&0x08)==0 && !o.wavComplete)
                    return finish(Stage::SampleTruncated);
                if(o.wavComplete)transition(Stage::WaitStop,ms);
                break;
            case Stage::WaitStop:
                if(!o.wavSourcePresent) return finish(Stage::SourceUnavailable);
                if(o.wadcsKnown && (o.wadcs&0x08)==0)
                    return finish(Stage::RecordedSample);
                break;
            default:break;
        }
        return result();
    }
private:
    Stage stage_=Stage::Idle;
    Button held_=Button::None;
    std::uint64_t enteredMs_=0;
    std::uint64_t pulseStartMs_=0;
    std::uint32_t nextName_=0;
    std::uint32_t nameSequence_=0;
    unsigned nameIndex_=0, nameRetries_=0;
    // Exactly 12 digits: prefix "125" + nine-digit serial (1..999999999).
    // The Akai naming dialog is limited to 12 characters. Fully overwrite
    // all positions, including any existing waveform name suffix.
    constexpr char generatedDigit(unsigned index)const noexcept{
        if(index==0)return '1';
        if(index==1)return '2';
        if(index==2)return '5';
        if(index>=12)return '0';
        std::uint32_t factor=1;
        for(unsigned n=0;n<11-index;++n)factor*=10u;
        return char('0'+((nameSequence_/factor)%10u));
    }
    void transition(Stage next,std::uint64_t ms)noexcept{
        stage_=next;enteredMs_=ms;
    }
    std::uint64_t timeoutForStage()const noexcept{
        if(stage_==Stage::WaitStop)return 120000; // firmware's duration controls stop
        if(stage_==Stage::Capturing)return 120000;
        if(stage_==Stage::WaitWav)return 30000;
        return 8000;
    }
    Decision press(Button button,std::uint64_t ms)noexcept{
        held_=button;pulseStartMs_=ms;
        return {Command::Press,button,stage_};
    }
    Decision finish(Stage result)noexcept{
        // Cannot silently leave a hardware button held.
        const Button release=held_;
        held_=Button::None;stage_=result;
        return {release==Button::None?Command::None:Command::Release,release,result};
    }
};
} // namespace akai::capture
