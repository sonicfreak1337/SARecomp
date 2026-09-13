#include "sonic_matrix_inverse.hpp"
#include "sonic_fpu_body.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <array>
#include <bit>
#include <cstring>
namespace sonic::matrix_inverse {
namespace {
using namespace katana::runtime;
// Exact PAL words, including literal islands. SHA is in the header.
constexpr std::array<std::uint16_t,1026> inverse_words{
    0x2FE6u,0xD30Cu,0x4F22u,0x430Bu,0x6E43u,0xF18Du,0xF104u,0x8B28u,0x2EE8u,0xC709u,0x64E3u,0xF408u,
    0x8F10u,0xF54Cu,0xF3FDu,0xF14Cu,0xF34Cu,0xF54Cu,0xF74Cu,0xF94Cu,0xFB4Cu,0xFD4Cu,0xFF4Cu,0xF3FDu,
    0xA3E2u,0xE000u,0xF32Cu,0x8C64u,0xC99Eu,0x7F7Fu,0x7440u,0xF44Bu,0xF44Bu,0xF44Bu,0xF44Bu,0xF44Bu,
    0xF44Bu,0xF44Bu,0xF44Bu,0xF44Bu,0xF44Bu,0xF44Bu,0xF44Bu,0xF44Bu,0xF44Bu,0xF44Bu,0xF44Bu,0xA3CBu,
    0xE000u,0xF19Du,0xF103u,0xFFCBu,0xFFDBu,0x2EE8u,0xF11Du,0x8D02u,0xFFEBu,0xA1BAu,0x0009u,0xF3FDu,
    0xF09Cu,0xF2BCu,0xF4DCu,0xF6FCu,0xF85Cu,0xFA7Cu,0xF3FDu,0xF01Cu,0xF42Cu,0xF252u,0xF472u,0xF532u,
    0xF162u,0xF072u,0xF362u,0xF69Cu,0xF7ACu,0xF642u,0xF752u,0xFA02u,0xF670u,0xF932u,0xF7BCu,0xFB12u,
    0xF722u,0xF6B0u,0xF671u,0xF6A1u,0xF691u,0xF3FDu,0xF81Cu,0xFA3Cu,0xF3FDu,0xF89Cu,0xF5A2u,0xF842u,
    0xF1B2u,0xF0A2u,0xF2B2u,0xF932u,0xF580u,0xF150u,0xF121u,0xF101u,0xF191u,0xF06Cu,0xF14Du,0xF3FDu,
    0xF69Cu,0xF8BCu,0xFADCu,0xFCFCu,0xF3FDu,0xF9A2u,0xFC62u,0xF29Cu,0xF8A2u,0xF5CCu,0xF6D2u,0xF78Cu,
    0xF3FDu,0xF85Cu,0xFA7Cu,0xFC7Cu,0xF3FDu,0xF98Cu,0xFC22u,0xF942u,0xFD52u,0xFB72u,0xFA62u,0xF832u,
    0xF9C0u,0xF9D0u,0xF9B1u,0xF9A1u,0xF981u,0xF3FDu,0xFA1Cu,0xFC3Cu,0xF3FDu,0xF94Du,0xF4A2u,0xF2C2u,
    0xF5D2u,0xF6C2u,0xF7D2u,0xF3A2u,0xF420u,0xF450u,0xF34Du,0xF471u,0xF29Cu,0xF461u,0xF340u,0xF40Du,
    0xF042u,0xF142u,0xF242u,0xF342u,0xFF0Bu,0xFF1Bu,0xFF2Bu,0xFF3Bu,0xF3FDu,0xF09Cu,0xF2BCu,0xF4DCu,
    0xF6FCu,0xF85Cu,0xFA7Cu,0xF3FDu,0xF21Cu,0xF65Cu,0xF172u,0xF702u,0xF052u,0xF532u,0xF342u,0xF242u,
    0xF47Cu,0xFCBCu,0xF68Cu,0xF79Cu,0xF612u,0xF732u,0xFC02u,0xF670u,0xFB22u,0xF6C0u,0xF942u,0xF6B1u,
    0xF852u,0xF691u,0xF681u,0xF3FDu,0xF81Cu,0xFA3Cu,0xF3FDu,0xF182u,0xFABCu,0xF392u,0xFA02u,0xF130u,
    0xFB22u,0xF1A0u,0xF492u,0xF1B1u,0xF582u,0xF141u,0xF151u,0xF36Cu,0xF14Du,0xF3FDu,0xF49Cu,0xF6BCu,
    0xF8DCu,0xFAFCu,0xF3FDu,0xF5A2u,0xF862u,0xF4A2u,0xF78Cu,0xF692u,0xF3FDu,0xF85Cu,0xFA7Cu,0xF3FDu,
    0xFB8Cu,0xFC9Cu,0xFDACu,0xFB52u,0xFC72u,0xFD02u,0xFCB0u,0xFA22u,0xFCD0u,0xF942u,0xFCA1u,0xF862u,
    0xFC91u,0xFC81u,0xFC4Du,0xF3FDu,0xF81Cu,0xFA3Cu,0xF3FDu,0xF582u,0xF792u,0xF0A2u,0xF570u,0xF2A2u,
    0xF050u,0xF492u,0xF021u,0xF682u,0xF041u,0xF061u,0xF80Du,0xF382u,0xF182u,0xFC82u,0xF082u,0xFF3Bu,
    0xFF1Bu,0xFFCBu,0xFF0Bu,0xF3FDu,0xF01Cu,0xF23Cu,0xF45Cu,0xF67Cu,0xF3FDu,0xF01Cu,0xF47Cu,0xF062u,
    0xF422u,0xF172u,0xF252u,0xF532u,0xF362u,0xF3FDu,0xF6DCu,0xF8FCu,0xF3FDu,0xF69Cu,0xFA7Cu,0xFB8Cu,
    0xF602u,0xF742u,0xF852u,0xF670u,0xFA32u,0xF680u,0xF922u,0xF6A1u,0xFB12u,0xF691u,0xF6B1u,0xF3FDu,
    0xF89Cu,0xFABCu,0xF3FDu,0xF89Cu,0xFC9Cu,0xF0B2u,0xF842u,0xF5A2u,0xF080u,0xF932u,0xF050u,0xF2B2u,
    0xF091u,0xF1A2u,0xF021u,0xF011u,0xF24Cu,0xF16Cu,0xF04Du,0xF3FDu,0xF41Cu,0xF63Cu,0xF85Cu,0xFA7Cu,
    0xF3FDu,0xF782u,0xF682u,0xFA42u,0xF4B2u,0xF5ACu,0xF3FDu,0xF8DCu,0xFAFCu,0xFCFCu,0xF3FDu,0xF98Cu,
    0xFB52u,0xF822u,0xFA72u,0xF8B0u,0xF932u,0xF8A0u,0xFD62u,0xF891u,0xFC42u,0xF8D1u,0xF8C1u,0xF3FDu,
    0xFA9Cu,0xFCBCu,0xF3FDu,0xF84Du,0xF5D2u,0xF2A2u,0xF7C2u,0xF250u,0xF3A2u,0xF270u,0xF6D2u,0xF231u,
    0xF4C2u,0xF261u,0xF38Cu,0xF241u,0xF40Du,0xF142u,0xF042u,0xF342u,0xF242u,0xFF1Bu,0xFF0Bu,0xFF3Bu,
    0xFF2Bu,0xF3FDu,0xF01Cu,0xF23Cu,0xF45Cu,0xF67Cu,0xF81Cu,0xF3FDu,0xF23Cu,0xF872u,0xF052u,0xF172u,
    0xF242u,0xF352u,0xF492u,0xF58Cu,0xF3FDu,0xF6DCu,0xF8FCu,0xFADCu,0xFCFCu,0xF3FDu,0xF902u,0xF612u,
    0xF722u,0xF690u,0xFA32u,0xF670u,0xFD42u,0xF6A1u,0xFB52u,0xF6D1u,0xF6B1u,0xF3FDu,0xF89Cu,0xFABCu,
    0xF3FDu,0xFABCu,0xF182u,0xFA02u,0xF292u,0xF1A0u,0xF832u,0xF120u,0xFB42u,0xF181u,0xF592u,0xF1B1u,
    0xF34Cu,0xF151u,0xF26Cu,0xF14Du,0xF3FDu,0xF41Cu,0xF63Cu,0xF85Cu,0xFA7Cu,0xF3FDu,0xF5A2u,0xF862u,
    0xF4A2u,0xF692u,0xF78Cu,0xF3FDu,0xF8DCu,0xFAFCu,0xFCDCu,0xF3FDu,0xFBACu,0xF852u,0xFA02u,0xF972u,
    0xF8A0u,0xFC62u,0xF890u,0xFB32u,0xF8C1u,0xFD42u,0xF8B1u,0xF8D1u,0xF84Du,0xF3FDu,0xFA9Cu,0xFCBCu,
    0xF3FDu,0xF0C2u,0xF5A2u,0xF7B2u,0xF050u,0xF6A2u,0xF070u,0xF3C2u,0xF061u,0xF4B2u,0xF031u,0xF041u,
    0xF40Du,0xF31Cu,0xF90Cu,0xF242u,0xF342u,0xF842u,0xF942u,0xF3FDu,0xFB2Cu,0xFF8Cu,0xF3FDu,0xF1F9u,
    0xF0F9u,0xF3F9u,0xF2F9u,0xF5F9u,0xF4F9u,0xF7F9u,0xF6F9u,0xF9F9u,0xF8F9u,0xFBF9u,0xFAF9u,0xF3FDu,
    0xF70Cu,0xF32Cu,0xFD4Cu,0xF96Cu,0xF58Cu,0xF1ACu,0xF3FDu,0xA204u,0xE001u,0x63E3u,0x62E3u,0x7320u,
    0x7210u,0xF039u,0xF139u,0xF239u,0xF339u,0xF439u,0xF539u,0xF639u,0xF739u,0xF829u,0xF929u,0xFA29u,
    0xFB29u,0xF01Cu,0xF42Cu,0xF252u,0xF472u,0xF532u,0xF162u,0xF072u,0xF362u,0xF69Cu,0xF7ACu,0xF642u,
    0xF752u,0xFA02u,0xF670u,0xF932u,0xF7BCu,0xFB12u,0xF722u,0xF6B0u,0xF671u,0x65E3u,0xF6A1u,0xF691u,
    0xF859u,0xF959u,0xFA59u,0xFB59u,0xF89Cu,0xF5A2u,0xF842u,0xF1B2u,0xF580u,0xF0A2u,0xF150u,0xF2B2u,
    0xF932u,0xF121u,0x67E3u,0xF101u,0x7710u,0x66E3u,0xF191u,0xF06Cu,0x7620u,0xF14Du,0xF669u,0xF769u,
    0xF869u,0xF969u,0xFA69u,0xFB69u,0xFC69u,0xFD69u,0xF9A2u,0xFC62u,0xF29Cu,0xF8A2u,0xF5CCu,0xF6D2u,
    0xF78Cu,0xF879u,0xF979u,0xFA78u,0xFC79u,0xFB78u,0xFD78u,0xF98Cu,0xFC22u,0xF942u,0xFD52u,0xFB72u,
    0xFA62u,0xF832u,0xF9C0u,0xF9D0u,0xF9B1u,0x66E3u,0xF9A1u,0xF981u,0xFA69u,0xFB69u,0xFC69u,0xFD69u,
    0xF94Du,0xF4A2u,0xF2C2u,0xF5D2u,0xF6C2u,0xF7D2u,0xF3A2u,0xF420u,0xF450u,0xF34Du,0xF471u,0xF29Cu,
    0xF461u,0xF340u,0xF40Du,0x65E3u,0xF042u,0x66E3u,0xF142u,0x7520u,0xF242u,0x7610u,0xF342u,0xFF0Bu,
    0xFF1Bu,0xFF2Bu,0xFF3Bu,0xF059u,0xF159u,0xF259u,0xF359u,0xF459u,0xF559u,0xF659u,0xF759u,0xF869u,
    0xF969u,0xFA69u,0xFB69u,0xF21Cu,0xF65Cu,0xF172u,0xF702u,0xF052u,0xF532u,0xF342u,0xF242u,0xF47Cu,
    0xFCBCu,0xF68Cu,0xF79Cu,0xF612u,0xF732u,0xFC02u,0xF670u,0xFB22u,0xF6C0u,0xF942u,0xF6B1u,0xF852u,
    0xF691u,0x65E3u,0xF681u,0xF859u,0xF959u,0xFA59u,0xFB59u,0xF182u,0xFABCu,0xF392u,0xFA02u,0xF130u,
    0xFB22u,0xF1A0u,0xF492u,0xF1B1u,0xF582u,0xF141u,0x65E3u,0xF151u,0x7520u,0xF36Cu,0x66E3u,0xF14Du,
    0x7610u,0xF459u,0xF559u,0xF659u,0xF759u,0xF859u,0xF959u,0xFA59u,0xFB59u,0xF5A2u,0xF862u,0xF4A2u,
    0xF78Cu,0xF692u,0xF869u,0xF969u,0xFA69u,0xFB69u,0xFB8Cu,0xFC9Cu,0xFDACu,0xFB52u,0xFC72u,0xFD02u,
    0xFCB0u,0xFA22u,0xFCD0u,0xF942u,0xFCA1u,0xF862u,0xFC91u,0x65E3u,0xFC81u,0xFC4Du,0xF859u,0xF959u,
    0xFA59u,0xFB59u,0xF582u,0xF792u,0xF0A2u,0xF570u,0xF2A2u,0xF050u,0xF492u,0xF021u,0xF682u,0xF041u,
    0xF061u,0xF80Du,0x65E3u,0xF382u,0xF182u,0xFC82u,0xF082u,0xFF3Bu,0xFF1Bu,0xFFCBu,0xFF0Bu,0xF059u,
    0xF159u,0xF259u,0xF359u,0xF459u,0xF559u,0xF659u,0xF759u,0xF01Cu,0xF47Cu,0xF062u,0xF422u,0xF172u,
    0x7510u,0xF252u,0xF532u,0xF362u,0xF659u,0xF759u,0xF859u,0xF959u,0xF69Cu,0xFA7Cu,0xFB8Cu,0xF602u,
    0xF742u,0xF852u,0xF670u,0xFA32u,0xF680u,0xF922u,0xF6A1u,0xFB12u,0xF691u,0x65E3u,0xF6B1u,0x7520u,
    0xF859u,0xF959u,0xFA59u,0xFB59u,0xF89Cu,0xFC9Cu,0xF0B2u,0xF842u,0xF5A2u,0xF080u,0xF932u,0xF050u,
    0xF2B2u,0xF091u,0xF1A2u,0xF021u,0xF011u,0x65E3u,0xF24Cu,0xF16Cu,0xF04Du,0xF459u,0xF559u,0xF659u,
    0xF759u,0xF859u,0xF959u,0xFA59u,0xFB59u,0xF782u,0x7510u,0xF682u,0xFA42u,0xF4B2u,0xF5ACu,0xF859u,
    0xF959u,0xFA58u,0xFC59u,0xFB58u,0xFD58u,0xF98Cu,0xFB52u,0xF822u,0xFA72u,0xF8B0u,0xF932u,0xF8A0u,
    0xFD62u,0xF891u,0xFC42u,0xF8D1u,0x65E3u,0xF8C1u,0x7520u,0xFA59u,0xFB59u,0xFC59u,0xFD59u,0xF84Du,
    0xF5D2u,0xF2A2u,0xF7C2u,0xF250u,0xF3A2u,0xF270u,0xF6D2u,0xF231u,0xF4C2u,0xF261u,0xF38Cu,0xF241u,
    0xF40Du,0xF142u,0x65E3u,0xF042u,0xF342u,0xF242u,0xFF1Bu,0xFF0Bu,0xFF3Bu,0xFF2Bu,0xF858u,0xF059u,
    0xF958u,0xF159u,0xF259u,0xF359u,0xF459u,0xF559u,0xF659u,0xF759u,0xF23Cu,0xF872u,0xF052u,0xF172u,
    0xF242u,0x7510u,0xF352u,0xF492u,0xF58Cu,0xF658u,0xFA59u,0xF758u,0xFB59u,0xF858u,0xFC59u,0xF958u,
    0xFD59u,0xF902u,0xF612u,0xF722u,0xF690u,0xFA32u,0xF670u,0xFD42u,0xF6A1u,0xFB52u,0xF6D1u,0x75E0u,
    0xF6B1u,0xF859u,0xF959u,0xFA59u,0xFB59u,0xFABCu,0xF182u,0x65E3u,0xFA02u,0xF292u,0xF1A0u,0xF832u,
    0xF120u,0xFB42u,0xF181u,0xF592u,0xF1B1u,0xF34Cu,0xF151u,0xF26Cu,0xF14Du,0xF459u,0xF559u,0xF659u,
    0xF759u,0xF859u,0xF959u,0xFA59u,0xFB59u,0xF5A2u,0x7510u,0xF862u,0xF4A2u,0xF692u,0xF78Cu,0xF858u,
    0xFC59u,0xF958u,0xFD59u,0xFA59u,0xFB58u,0xFBACu,0xF852u,0xFA02u,0xF972u,0xF8A0u,0xFC62u,0xF890u,
    0xFB32u,0xF8C1u,0xFD42u,0xF8B1u,0x65E3u,0xF8D1u,0x7520u,0xF84Du,0xFA59u,0xFB59u,0xFC59u,0xFD59u,
    0xF0C2u,0xF5A2u,0xF7B2u,0xF050u,0xF6A2u,0xF070u,0xF3C2u,0xF061u,0xF4B2u,0xF031u,0x65E3u,0xF041u,
    0xF40Du,0xF31Cu,0xF90Cu,0xF242u,0xE02Cu,0xF342u,0x66E3u,0xF842u,0x7640u,0xF942u,0xF537u,0xF69Bu,
    0xE028u,0xF68Bu,0xF527u,0xF1F9u,0x65E3u,0xF0F9u,0x66E3u,0xF3F9u,0x7638u,0xF2F9u,0x7528u,0xF5F9u,
    0xF4F9u,0xF7F9u,0xF6F9u,0xF9F9u,0xF8F9u,0xFBF9u,0xFAF9u,0x67E3u,0xF57Bu,0xF56Bu,0xF51Bu,0xF50Bu,
    0xF59Bu,0xF58Bu,0xF53Bu,0xF52Bu,0xF5BBu,0xF5ABu,0xF65Bu,0xF64Bu,0xD003u,0xFEF9u,0xFDF9u,0xFCF9u,
    0x4F26u,0x000Bu,0x6EF6u,0x0000u,0x0001u,0x0000u,
};
// Exact PAL words, including literal islands. SHA is in the header.
constexpr std::array<std::uint16_t,172> determinant_words{
    0x2448u,0x8B51u,0xF3FDu,0xF09Cu,0xF2BCu,0xF4DCu,0xF6FCu,0xF89Cu,0xFABCu,0xF3FDu,0xFA72u,0xF362u,
    0xFA31u,0xF962u,0xF252u,0xF921u,0xFB42u,0xF072u,0xFB01u,0xF852u,0xF142u,0xF811u,0xF3FDu,0xF09Cu,
    0xF2BCu,0xF4DCu,0xF6FCu,0xF3FDu,0xF532u,0xF602u,0xF712u,0xF422u,0xF05Cu,0xF16Cu,0xF27Cu,0xF34Cu,
    0xF521u,0xF631u,0xF701u,0xF411u,0xF3FDu,0xF05Cu,0xF25Cu,0xF3FDu,0xF722u,0xF2A2u,0xF092u,0xFA32u,
    0xF1B2u,0xF342u,0xF710u,0xF300u,0xF3FDu,0xF07Cu,0xF3FDu,0xF10Cu,0xFB12u,0xF502u,0xF182u,0xF5A0u,
    0xF2B0u,0xF310u,0xF3FDu,0xF07Cu,0xF3FDu,0xF01Cu,0xF902u,0xF612u,0xF082u,0xF590u,0xF260u,0xF070u,
    0xF3FDu,0xF61Cu,0xF83Cu,0xF3FDu,0xF562u,0xF272u,0xF082u,0xF392u,0xF521u,0xF031u,0x000Bu,0xF050u,
    0x6143u,0x7120u,0xF019u,0xF119u,0xF219u,0xF319u,0xF419u,0xF519u,0xF619u,0xF719u,0xF80Cu,0xF91Cu,
    0xFA2Cu,0xFB3Cu,0xFA72u,0xF362u,0xFA31u,0xF962u,0xF252u,0xF921u,0xFB42u,0xF072u,0xFB01u,0xF852u,
    0xF142u,0xF811u,0x71E0u,0xF019u,0xF119u,0xF219u,0xF319u,0xF419u,0xF519u,0xF619u,0xF719u,0xF532u,
    0xF602u,0xF712u,0xF422u,0xF05Cu,0xF16Cu,0xF27Cu,0xF34Cu,0xF521u,0xF631u,0xF701u,0xF411u,0x71D0u,
    0xF019u,0xF119u,0xF20Cu,0xF31Cu,0xF722u,0xF2A2u,0xF092u,0xFA32u,0xF1B2u,0xF342u,0xF710u,0xF300u,
    0xF019u,0xF10Cu,0xFB12u,0xF502u,0xF182u,0xF5A0u,0xF2B0u,0xF310u,0xF018u,0xF10Cu,0xF902u,0xF612u,
    0xF082u,0xF590u,0xF260u,0xF070u,0xF649u,0xF749u,0xF849u,0xF949u,0xF562u,0xF272u,0xF082u,0xF392u,
    0xF521u,0xF031u,0x000Bu,0xF050u,
};
struct Range { std::uint32_t address, size; };
bool overlaps(Range a, Range b) noexcept {
    const auto x=a.address&0x1FFFFFFFu, y=b.address&0x1FFFFFFFu;
    return x<std::uint64_t(y)+b.size && y<std::uint64_t(x)+a.size;
}
bool admitted(const DirectLinearMemoryGuard& g, bool allow_p0, Range r, unsigned alignment=4u) noexcept {
    const auto p=r.address&0x1FFFFFFFu;
    const bool segment=(r.address&0xC0000000u)==0x80000000u ||
        (allow_p0 && r.address>=0x0C000000u && r.address<0x0D000000u);
    return segment && (r.address&(alignment-1u))==0u &&
        r.size && p>=0x0C000000u && p<0x0D000000u && r.size<=0x0D000000u-p &&
        g && g.physical_base==0x0C000000u && g.physical_span>=0x01000000u &&
        g.backing_mask==0x00FFFFFFu;
}
} // namespace

bool try_execute(katana::runtime::CpuState& cpu,
                 const katana::runtime::NativePortImmutableWriteGuard* immutable_guard) {
    using namespace katana::runtime;
    static_assert(std::endian::native==std::endian::little);
    const bool inverse=cpu.pc==inverse_entry;
    const auto fpscr=cpu.read_fpscr();
    if ((!inverse && cpu.pc!=determinant_entry) || !immutable_guard || immutable_guard->write_detected() ||
        !cpu.privileged_mode_inline() || cpu.trap_pending || cpu.sleeping || (cpu.sr&sr_fd_mask) ||
        (fpscr&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) ||
        !(fpscr&fpscr_dn_mask) || (fpscr&fpscr_rounding_mode_mask)>1u) return false;
    auto& memory=cpu.memory;
    if (memory.watchpoint_count() || memory.has_trace_handler() || memory.has_guest_memory_access_sink() ||
        memory.has_mmio_trace_handler() || !memory.guest_write_observer_allows_prevalidated_linear_writes()) return false;
    const auto guard=memory.direct_linear_memory_guard(false);
    const bool allow_p0=(cpu.mmucr&1u)==0u &&
        (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu);
    const Range inverse_code{inverse_entry,inverse_size},determinant_code{determinant_entry,determinant_size};
    const auto code_matches=[&](Range code,const auto& words) {
        if (!admitted(guard,allow_p0,code)) return false;
        return std::memcmp(guard.read_bytes+(code.address&0xFFFFFFu),words.data(),code.size)==0;
    };
    if (!code_matches(determinant_code,determinant_words) ||
        (inverse && !code_matches(inverse_code,inverse_words))) return false;
    const Range matrix{cpu.r[4],64u},stack{cpu.r[15]-68u,68u};
    if (matrix.address && !admitted(guard,allow_p0,matrix)) return false;
    if (inverse) {
        const auto writable=[&](Range range) {
            return admitted(guard,allow_p0,range) &&
                !immutable_guard->tracks_address(range.address&0x1FFFFFFFu,range.size) &&
                !overlaps(range,inverse_code) && !overlaps(range,determinant_code) &&
                memory.is_writable_linear_range(range.address&0x1FFFFFFFu,range.size,false);
        };
        // Both non-singular branches peak at 68 bytes: 2 integer saves,
        // 3 preserved FRs and 12 temporary FRs. Determinant never touches SP.
        if (!writable(stack) || (matrix.address && (!writable(matrix) || overlaps(stack,matrix)))) return false;
    }
    sonic::fpu_body::NontrappingSingleBody fp(cpu);
    if (!fp.admitted()) return false;
    // No fallback after admission. Static whole-body translations, with one
    // native internal call and no original dispatch or runtime opcode decoder.
    const auto load32=[&](std::uint32_t address) {
        std::uint32_t value=0;
        (void)direct_linear_guard_read_u32(guard,(address&0x1FFFFFFFu)|0x80000000u,value);
        return value;
    };
    const auto store32=[&](std::uint32_t pc,std::uint32_t address,std::uint32_t value,CodeWriteSource source) {
        if (!memory.try_write_direct_linear_u32(address&0x1FFFFFFFu,value,source))
            guest_write_u32_at(cpu,GuestInstructionOrigin{pc,pc,true},address,value,source);
    };
    auto& r=cpu.r;auto& fr=cpu.fr;auto& xf=cpu.xf;
    const auto determinant=[&] {
        cpu.t=(r[4]&r[4])==0u; // 8C64F32C tst r4,r4
        if(!cpu.t) goto L_8C64F3D4; // 8C64F32E bf
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F330 fschg 
        fr[0]=xf[8];fr[1]=xf[9]; // 8C64F332 fmov fr9,fr0
        fr[2]=xf[10];fr[3]=xf[11]; // 8C64F334 fmov fr11,fr2
        fr[4]=xf[12];fr[5]=xf[13]; // 8C64F336 fmov fr13,fr4
        fr[6]=xf[14];fr[7]=xf[15]; // 8C64F338 fmov fr15,fr6
        fr[8]=xf[8];fr[9]=xf[9]; // 8C64F33A fmov fr9,fr8
        fr[10]=xf[10];fr[11]=xf[11]; // 8C64F33C fmov fr11,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F33E fschg 
        fp.binary<FpuBinaryOperation::Multiply,7u,10u>(); // 8C64F340 fmul fr7,fr10
        fp.binary<FpuBinaryOperation::Multiply,6u,3u>(); // 8C64F342 fmul fr6,fr3
        fp.binary<FpuBinaryOperation::Subtract,3u,10u>(); // 8C64F344 fsub fr3,fr10
        fp.binary<FpuBinaryOperation::Multiply,6u,9u>(); // 8C64F346 fmul fr6,fr9
        fp.binary<FpuBinaryOperation::Multiply,5u,2u>(); // 8C64F348 fmul fr5,fr2
        fp.binary<FpuBinaryOperation::Subtract,2u,9u>(); // 8C64F34A fsub fr2,fr9
        fp.binary<FpuBinaryOperation::Multiply,4u,11u>(); // 8C64F34C fmul fr4,fr11
        fp.binary<FpuBinaryOperation::Multiply,7u,0u>(); // 8C64F34E fmul fr7,fr0
        fp.binary<FpuBinaryOperation::Subtract,0u,11u>(); // 8C64F350 fsub fr0,fr11
        fp.binary<FpuBinaryOperation::Multiply,5u,8u>(); // 8C64F352 fmul fr5,fr8
        fp.binary<FpuBinaryOperation::Multiply,4u,1u>(); // 8C64F354 fmul fr4,fr1
        fp.binary<FpuBinaryOperation::Subtract,1u,8u>(); // 8C64F356 fsub fr1,fr8
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F358 fschg 
        fr[0]=xf[8];fr[1]=xf[9]; // 8C64F35A fmov fr9,fr0
        fr[2]=xf[10];fr[3]=xf[11]; // 8C64F35C fmov fr11,fr2
        fr[4]=xf[12];fr[5]=xf[13]; // 8C64F35E fmov fr13,fr4
        fr[6]=xf[14];fr[7]=xf[15]; // 8C64F360 fmov fr15,fr6
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F362 fschg 
        fp.binary<FpuBinaryOperation::Multiply,3u,5u>(); // 8C64F364 fmul fr3,fr5
        fp.binary<FpuBinaryOperation::Multiply,0u,6u>(); // 8C64F366 fmul fr0,fr6
        fp.binary<FpuBinaryOperation::Multiply,1u,7u>(); // 8C64F368 fmul fr1,fr7
        fp.binary<FpuBinaryOperation::Multiply,2u,4u>(); // 8C64F36A fmul fr2,fr4
        fr[0]=fr[5]; // 8C64F36C fmov fr5,fr0
        fr[1]=fr[6]; // 8C64F36E fmov fr6,fr1
        fr[2]=fr[7]; // 8C64F370 fmov fr7,fr2
        fr[3]=fr[4]; // 8C64F372 fmov fr4,fr3
        fp.binary<FpuBinaryOperation::Subtract,2u,5u>(); // 8C64F374 fsub fr2,fr5
        fp.binary<FpuBinaryOperation::Subtract,3u,6u>(); // 8C64F376 fsub fr3,fr6
        fp.binary<FpuBinaryOperation::Subtract,0u,7u>(); // 8C64F378 fsub fr0,fr7
        fp.binary<FpuBinaryOperation::Subtract,1u,4u>(); // 8C64F37A fsub fr1,fr4
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F37C fschg 
        fr[0]=xf[4];fr[1]=xf[5]; // 8C64F37E fmov fr5,fr0
        fr[2]=xf[4];fr[3]=xf[5]; // 8C64F380 fmov fr5,fr2
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F382 fschg 
        fp.binary<FpuBinaryOperation::Multiply,2u,7u>(); // 8C64F384 fmul fr2,fr7
        fp.binary<FpuBinaryOperation::Multiply,10u,2u>(); // 8C64F386 fmul fr10,fr2
        fp.binary<FpuBinaryOperation::Multiply,9u,0u>(); // 8C64F388 fmul fr9,fr0
        fp.binary<FpuBinaryOperation::Multiply,3u,10u>(); // 8C64F38A fmul fr3,fr10
        fp.binary<FpuBinaryOperation::Multiply,11u,1u>(); // 8C64F38C fmul fr11,fr1
        fp.binary<FpuBinaryOperation::Multiply,4u,3u>(); // 8C64F38E fmul fr4,fr3
        fp.binary<FpuBinaryOperation::Add,1u,7u>(); // 8C64F390 fadd fr1,fr7
        fp.binary<FpuBinaryOperation::Add,0u,3u>(); // 8C64F392 fadd fr0,fr3
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F394 fschg 
        fr[0]=xf[6];fr[1]=xf[7]; // 8C64F396 fmov fr7,fr0
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F398 fschg 
        fr[1]=fr[0]; // 8C64F39A fmov fr0,fr1
        fp.binary<FpuBinaryOperation::Multiply,1u,11u>(); // 8C64F39C fmul fr1,fr11
        fp.binary<FpuBinaryOperation::Multiply,0u,5u>(); // 8C64F39E fmul fr0,fr5
        fp.binary<FpuBinaryOperation::Multiply,8u,1u>(); // 8C64F3A0 fmul fr8,fr1
        fp.binary<FpuBinaryOperation::Add,10u,5u>(); // 8C64F3A2 fadd fr10,fr5
        fp.binary<FpuBinaryOperation::Add,11u,2u>(); // 8C64F3A4 fadd fr11,fr2
        fp.binary<FpuBinaryOperation::Add,1u,3u>(); // 8C64F3A6 fadd fr1,fr3
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F3A8 fschg 
        fr[0]=xf[6];fr[1]=xf[7]; // 8C64F3AA fmov fr7,fr0
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F3AC fschg 
        fr[0]=fr[1]; // 8C64F3AE fmov fr1,fr0
        fp.binary<FpuBinaryOperation::Multiply,0u,9u>(); // 8C64F3B0 fmul fr0,fr9
        fp.binary<FpuBinaryOperation::Multiply,1u,6u>(); // 8C64F3B2 fmul fr1,fr6
        fp.binary<FpuBinaryOperation::Multiply,8u,0u>(); // 8C64F3B4 fmul fr8,fr0
        fp.binary<FpuBinaryOperation::Add,9u,5u>(); // 8C64F3B6 fadd fr9,fr5
        fp.binary<FpuBinaryOperation::Add,6u,2u>(); // 8C64F3B8 fadd fr6,fr2
        fp.binary<FpuBinaryOperation::Add,7u,0u>(); // 8C64F3BA fadd fr7,fr0
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F3BC fschg 
        fr[6]=xf[0];fr[7]=xf[1]; // 8C64F3BE fmov fr1,fr6
        fr[8]=xf[2];fr[9]=xf[3]; // 8C64F3C0 fmov fr3,fr8
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C64F3C2 fschg 
        fp.binary<FpuBinaryOperation::Multiply,6u,5u>(); // 8C64F3C4 fmul fr6,fr5
        fp.binary<FpuBinaryOperation::Multiply,7u,2u>(); // 8C64F3C6 fmul fr7,fr2
        fp.binary<FpuBinaryOperation::Multiply,8u,0u>(); // 8C64F3C8 fmul fr8,fr0
        fp.binary<FpuBinaryOperation::Multiply,9u,3u>(); // 8C64F3CA fmul fr9,fr3
        fp.binary<FpuBinaryOperation::Subtract,2u,5u>(); // 8C64F3CC fsub fr2,fr5
        fp.binary<FpuBinaryOperation::Subtract,3u,0u>(); // 8C64F3CE fsub fr3,fr0
        {const auto target=cpu.pr;
        fp.binary<FpuBinaryOperation::Add,5u,0u>(); // 8C64F3D2 fadd fr5,fr0
        cpu.pc=target;return;}
L_8C64F3D4:
        r[1]=r[4]; // 8C64F3D4 mov r4,r1
        r[1]+=0x00000020u; // 8C64F3D6 add #32,r1
        fr[0]=load32(r[1]);r[1]+=4u; // 8C64F3D8 fmov @r1+,fr0
        fr[1]=load32(r[1]);r[1]+=4u; // 8C64F3DA fmov @r1+,fr1
        fr[2]=load32(r[1]);r[1]+=4u; // 8C64F3DC fmov @r1+,fr2
        fr[3]=load32(r[1]);r[1]+=4u; // 8C64F3DE fmov @r1+,fr3
        fr[4]=load32(r[1]);r[1]+=4u; // 8C64F3E0 fmov @r1+,fr4
        fr[5]=load32(r[1]);r[1]+=4u; // 8C64F3E2 fmov @r1+,fr5
        fr[6]=load32(r[1]);r[1]+=4u; // 8C64F3E4 fmov @r1+,fr6
        fr[7]=load32(r[1]);r[1]+=4u; // 8C64F3E6 fmov @r1+,fr7
        fr[8]=fr[0]; // 8C64F3E8 fmov fr0,fr8
        fr[9]=fr[1]; // 8C64F3EA fmov fr1,fr9
        fr[10]=fr[2]; // 8C64F3EC fmov fr2,fr10
        fr[11]=fr[3]; // 8C64F3EE fmov fr3,fr11
        fp.binary<FpuBinaryOperation::Multiply,7u,10u>(); // 8C64F3F0 fmul fr7,fr10
        fp.binary<FpuBinaryOperation::Multiply,6u,3u>(); // 8C64F3F2 fmul fr6,fr3
        fp.binary<FpuBinaryOperation::Subtract,3u,10u>(); // 8C64F3F4 fsub fr3,fr10
        fp.binary<FpuBinaryOperation::Multiply,6u,9u>(); // 8C64F3F6 fmul fr6,fr9
        fp.binary<FpuBinaryOperation::Multiply,5u,2u>(); // 8C64F3F8 fmul fr5,fr2
        fp.binary<FpuBinaryOperation::Subtract,2u,9u>(); // 8C64F3FA fsub fr2,fr9
        fp.binary<FpuBinaryOperation::Multiply,4u,11u>(); // 8C64F3FC fmul fr4,fr11
        fp.binary<FpuBinaryOperation::Multiply,7u,0u>(); // 8C64F3FE fmul fr7,fr0
        fp.binary<FpuBinaryOperation::Subtract,0u,11u>(); // 8C64F400 fsub fr0,fr11
        fp.binary<FpuBinaryOperation::Multiply,5u,8u>(); // 8C64F402 fmul fr5,fr8
        fp.binary<FpuBinaryOperation::Multiply,4u,1u>(); // 8C64F404 fmul fr4,fr1
        fp.binary<FpuBinaryOperation::Subtract,1u,8u>(); // 8C64F406 fsub fr1,fr8
        r[1]+=0xFFFFFFE0u; // 8C64F408 add #-32,r1
        fr[0]=load32(r[1]);r[1]+=4u; // 8C64F40A fmov @r1+,fr0
        fr[1]=load32(r[1]);r[1]+=4u; // 8C64F40C fmov @r1+,fr1
        fr[2]=load32(r[1]);r[1]+=4u; // 8C64F40E fmov @r1+,fr2
        fr[3]=load32(r[1]);r[1]+=4u; // 8C64F410 fmov @r1+,fr3
        fr[4]=load32(r[1]);r[1]+=4u; // 8C64F412 fmov @r1+,fr4
        fr[5]=load32(r[1]);r[1]+=4u; // 8C64F414 fmov @r1+,fr5
        fr[6]=load32(r[1]);r[1]+=4u; // 8C64F416 fmov @r1+,fr6
        fr[7]=load32(r[1]);r[1]+=4u; // 8C64F418 fmov @r1+,fr7
        fp.binary<FpuBinaryOperation::Multiply,3u,5u>(); // 8C64F41A fmul fr3,fr5
        fp.binary<FpuBinaryOperation::Multiply,0u,6u>(); // 8C64F41C fmul fr0,fr6
        fp.binary<FpuBinaryOperation::Multiply,1u,7u>(); // 8C64F41E fmul fr1,fr7
        fp.binary<FpuBinaryOperation::Multiply,2u,4u>(); // 8C64F420 fmul fr2,fr4
        fr[0]=fr[5]; // 8C64F422 fmov fr5,fr0
        fr[1]=fr[6]; // 8C64F424 fmov fr6,fr1
        fr[2]=fr[7]; // 8C64F426 fmov fr7,fr2
        fr[3]=fr[4]; // 8C64F428 fmov fr4,fr3
        fp.binary<FpuBinaryOperation::Subtract,2u,5u>(); // 8C64F42A fsub fr2,fr5
        fp.binary<FpuBinaryOperation::Subtract,3u,6u>(); // 8C64F42C fsub fr3,fr6
        fp.binary<FpuBinaryOperation::Subtract,0u,7u>(); // 8C64F42E fsub fr0,fr7
        fp.binary<FpuBinaryOperation::Subtract,1u,4u>(); // 8C64F430 fsub fr1,fr4
        r[1]+=0xFFFFFFD0u; // 8C64F432 add #-48,r1
        fr[0]=load32(r[1]);r[1]+=4u; // 8C64F434 fmov @r1+,fr0
        fr[1]=load32(r[1]);r[1]+=4u; // 8C64F436 fmov @r1+,fr1
        fr[2]=fr[0]; // 8C64F438 fmov fr0,fr2
        fr[3]=fr[1]; // 8C64F43A fmov fr1,fr3
        fp.binary<FpuBinaryOperation::Multiply,2u,7u>(); // 8C64F43C fmul fr2,fr7
        fp.binary<FpuBinaryOperation::Multiply,10u,2u>(); // 8C64F43E fmul fr10,fr2
        fp.binary<FpuBinaryOperation::Multiply,9u,0u>(); // 8C64F440 fmul fr9,fr0
        fp.binary<FpuBinaryOperation::Multiply,3u,10u>(); // 8C64F442 fmul fr3,fr10
        fp.binary<FpuBinaryOperation::Multiply,11u,1u>(); // 8C64F444 fmul fr11,fr1
        fp.binary<FpuBinaryOperation::Multiply,4u,3u>(); // 8C64F446 fmul fr4,fr3
        fp.binary<FpuBinaryOperation::Add,1u,7u>(); // 8C64F448 fadd fr1,fr7
        fp.binary<FpuBinaryOperation::Add,0u,3u>(); // 8C64F44A fadd fr0,fr3
        fr[0]=load32(r[1]);r[1]+=4u; // 8C64F44C fmov @r1+,fr0
        fr[1]=fr[0]; // 8C64F44E fmov fr0,fr1
        fp.binary<FpuBinaryOperation::Multiply,1u,11u>(); // 8C64F450 fmul fr1,fr11
        fp.binary<FpuBinaryOperation::Multiply,0u,5u>(); // 8C64F452 fmul fr0,fr5
        fp.binary<FpuBinaryOperation::Multiply,8u,1u>(); // 8C64F454 fmul fr8,fr1
        fp.binary<FpuBinaryOperation::Add,10u,5u>(); // 8C64F456 fadd fr10,fr5
        fp.binary<FpuBinaryOperation::Add,11u,2u>(); // 8C64F458 fadd fr11,fr2
        fp.binary<FpuBinaryOperation::Add,1u,3u>(); // 8C64F45A fadd fr1,fr3
        fr[0]=load32(r[1]); // 8C64F45C fmov @r1,fr0
        fr[1]=fr[0]; // 8C64F45E fmov fr0,fr1
        fp.binary<FpuBinaryOperation::Multiply,0u,9u>(); // 8C64F460 fmul fr0,fr9
        fp.binary<FpuBinaryOperation::Multiply,1u,6u>(); // 8C64F462 fmul fr1,fr6
        fp.binary<FpuBinaryOperation::Multiply,8u,0u>(); // 8C64F464 fmul fr8,fr0
        fp.binary<FpuBinaryOperation::Add,9u,5u>(); // 8C64F466 fadd fr9,fr5
        fp.binary<FpuBinaryOperation::Add,6u,2u>(); // 8C64F468 fadd fr6,fr2
        fp.binary<FpuBinaryOperation::Add,7u,0u>(); // 8C64F46A fadd fr7,fr0
        fr[6]=load32(r[4]);r[4]+=4u; // 8C64F46C fmov @r4+,fr6
        fr[7]=load32(r[4]);r[4]+=4u; // 8C64F46E fmov @r4+,fr7
        fr[8]=load32(r[4]);r[4]+=4u; // 8C64F470 fmov @r4+,fr8
        fr[9]=load32(r[4]);r[4]+=4u; // 8C64F472 fmov @r4+,fr9
        fp.binary<FpuBinaryOperation::Multiply,6u,5u>(); // 8C64F474 fmul fr6,fr5
        fp.binary<FpuBinaryOperation::Multiply,7u,2u>(); // 8C64F476 fmul fr7,fr2
        fp.binary<FpuBinaryOperation::Multiply,8u,0u>(); // 8C64F478 fmul fr8,fr0
        fp.binary<FpuBinaryOperation::Multiply,9u,3u>(); // 8C64F47A fmul fr9,fr3
        fp.binary<FpuBinaryOperation::Subtract,2u,5u>(); // 8C64F47C fsub fr2,fr5
        fp.binary<FpuBinaryOperation::Subtract,3u,0u>(); // 8C64F47E fsub fr3,fr0
        {const auto target=cpu.pr;
        fp.binary<FpuBinaryOperation::Add,5u,0u>(); // 8C64F482 fadd fr5,fr0
        cpu.pc=target;return;}
    };
    if (!inverse) {determinant();return true;}
    const auto invert=[&] {
        store32(0x8C638FF0u,r[15]-4u,r[14],CodeWriteSource::Cpu);r[15]-=4u; // 8C638FF0 mov.l r14,@-r15
        r[3]=load32(0x8C639024u); // 8C638FF2 mov.l 0x8c639024,r3
        store32(0x8C638FF4u,r[15]-4u,cpu.pr,CodeWriteSource::Cpu);r[15]-=4u; // 8C638FF4 sts.l pr,@-r15
        cpu.pr=0x8C638FFAu; // 8C638FF6 internal JSR
        r[14]=r[4]; // 8C638FF8 mov r4,r14
        cpu.pc=determinant_entry;determinant();
        fr[1]=0u; // 8C638FFA fldi0 fr1
        fp.compare_equal<0u,1u>(); // 8C638FFC fcmp/eq fr0,fr1
        if(!cpu.t) goto L_8C639052; // 8C638FFE bf
        cpu.t=(r[14]&r[14])==0u; // 8C639000 tst r14,r14
        r[0]=0x8C639028u; // 8C639002 mova 0x8c639028,r0
        r[4]=r[14]; // 8C639004 mov r14,r4
        fr[4]=load32(r[0]); // 8C639006 fmov @r0,fr4
        {const bool taken=!cpu.t; // 8C639008 bf/s
        fr[5]=fr[4]; // 8C63900A fmov fr4,fr5
        if(taken) goto L_8C63902C;}
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C63900C fschg 
        xf[0]=fr[4];xf[1]=fr[5]; // 8C63900E fmov fr4,fr1
        xf[2]=fr[4];xf[3]=fr[5]; // 8C639010 fmov fr4,fr3
        xf[4]=fr[4];xf[5]=fr[5]; // 8C639012 fmov fr4,fr5
        xf[6]=fr[4];xf[7]=fr[5]; // 8C639014 fmov fr4,fr7
        xf[8]=fr[4];xf[9]=fr[5]; // 8C639016 fmov fr4,fr9
        xf[10]=fr[4];xf[11]=fr[5]; // 8C639018 fmov fr4,fr11
        xf[12]=fr[4];xf[13]=fr[5]; // 8C63901A fmov fr4,fr13
        xf[14]=fr[4];xf[15]=fr[5]; // 8C63901C fmov fr4,fr15
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C63901E fschg 
        r[0]=0x00000000u; // 8C639022 mov #0,r0
        goto L_8C6397E8; // 8C639020 bra
L_8C63902C:
        r[4]+=0x00000040u; // 8C63902C add #64,r4
        store32(0x8C63902Eu,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C63902E fmov fr4,@-r4
        store32(0x8C639030u,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C639030 fmov fr4,@-r4
        store32(0x8C639032u,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C639032 fmov fr4,@-r4
        store32(0x8C639034u,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C639034 fmov fr4,@-r4
        store32(0x8C639036u,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C639036 fmov fr4,@-r4
        store32(0x8C639038u,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C639038 fmov fr4,@-r4
        store32(0x8C63903Au,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C63903A fmov fr4,@-r4
        store32(0x8C63903Cu,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C63903C fmov fr4,@-r4
        store32(0x8C63903Eu,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C63903E fmov fr4,@-r4
        store32(0x8C639040u,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C639040 fmov fr4,@-r4
        store32(0x8C639042u,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C639042 fmov fr4,@-r4
        store32(0x8C639044u,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C639044 fmov fr4,@-r4
        store32(0x8C639046u,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C639046 fmov fr4,@-r4
        store32(0x8C639048u,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C639048 fmov fr4,@-r4
        store32(0x8C63904Au,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C63904A fmov fr4,@-r4
        store32(0x8C63904Cu,r[4]-4u,fr[4],CodeWriteSource::Fpu);r[4]-=4u; // 8C63904C fmov fr4,@-r4
        r[0]=0x00000000u; // 8C639050 mov #0,r0
        goto L_8C6397E8; // 8C63904E bra
L_8C639052:
        fr[1]=0x3F800000u; // 8C639052 fldi1 fr1
        fp.binary<FpuBinaryOperation::Divide,0u,1u>(); // 8C639054 fdiv fr0,fr1
        store32(0x8C639056u,r[15]-4u,fr[12],CodeWriteSource::Fpu);r[15]-=4u; // 8C639056 fmov fr12,@-r15
        store32(0x8C639058u,r[15]-4u,fr[13],CodeWriteSource::Fpu);r[15]-=4u; // 8C639058 fmov fr13,@-r15
        cpu.t=(r[14]&r[14])==0u; // 8C63905A tst r14,r14
        cpu.fpul=fr[1]; // 8C63905C flds fr1,fpul
        {const bool taken=cpu.t; // 8C63905E bt/s
        store32(0x8C639060u,r[15]-4u,fr[14],CodeWriteSource::Fpu);r[15]-=4u; // 8C639060 fmov fr14,@-r15
        if(taken) goto L_8C639066;}
         // 8C639064 nop 
        goto L_8C6393DA; // 8C639062 bra
L_8C639066:
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639066 fschg 
        fr[0]=xf[8];fr[1]=xf[9]; // 8C639068 fmov fr9,fr0
        fr[2]=xf[10];fr[3]=xf[11]; // 8C63906A fmov fr11,fr2
        fr[4]=xf[12];fr[5]=xf[13]; // 8C63906C fmov fr13,fr4
        fr[6]=xf[14];fr[7]=xf[15]; // 8C63906E fmov fr15,fr6
        fr[8]=xf[4];fr[9]=xf[5]; // 8C639070 fmov fr5,fr8
        fr[10]=xf[6];fr[11]=xf[7]; // 8C639072 fmov fr7,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639074 fschg 
        fr[0]=fr[1]; // 8C639076 fmov fr1,fr0
        fr[4]=fr[2]; // 8C639078 fmov fr2,fr4
        fp.binary<FpuBinaryOperation::Multiply,5u,2u>(); // 8C63907A fmul fr5,fr2
        fp.binary<FpuBinaryOperation::Multiply,7u,4u>(); // 8C63907C fmul fr7,fr4
        fp.binary<FpuBinaryOperation::Multiply,3u,5u>(); // 8C63907E fmul fr3,fr5
        fp.binary<FpuBinaryOperation::Multiply,6u,1u>(); // 8C639080 fmul fr6,fr1
        fp.binary<FpuBinaryOperation::Multiply,7u,0u>(); // 8C639082 fmul fr7,fr0
        fp.binary<FpuBinaryOperation::Multiply,6u,3u>(); // 8C639084 fmul fr6,fr3
        fr[6]=fr[9]; // 8C639086 fmov fr9,fr6
        fr[7]=fr[10]; // 8C639088 fmov fr10,fr7
        fp.binary<FpuBinaryOperation::Multiply,4u,6u>(); // 8C63908A fmul fr4,fr6
        fp.binary<FpuBinaryOperation::Multiply,5u,7u>(); // 8C63908C fmul fr5,fr7
        fp.binary<FpuBinaryOperation::Multiply,0u,10u>(); // 8C63908E fmul fr0,fr10
        fp.binary<FpuBinaryOperation::Add,7u,6u>(); // 8C639090 fadd fr7,fr6
        fp.binary<FpuBinaryOperation::Multiply,3u,9u>(); // 8C639092 fmul fr3,fr9
        fr[7]=fr[11]; // 8C639094 fmov fr11,fr7
        fp.binary<FpuBinaryOperation::Multiply,1u,11u>(); // 8C639096 fmul fr1,fr11
        fp.binary<FpuBinaryOperation::Multiply,2u,7u>(); // 8C639098 fmul fr2,fr7
        fp.binary<FpuBinaryOperation::Add,11u,6u>(); // 8C63909A fadd fr11,fr6
        fp.binary<FpuBinaryOperation::Subtract,7u,6u>(); // 8C63909C fsub fr7,fr6
        fp.binary<FpuBinaryOperation::Subtract,10u,6u>(); // 8C63909E fsub fr10,fr6
        fp.binary<FpuBinaryOperation::Subtract,9u,6u>(); // 8C6390A0 fsub fr9,fr6
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6390A2 fschg 
        fr[8]=xf[0];fr[9]=xf[1]; // 8C6390A4 fmov fr1,fr8
        fr[10]=xf[2];fr[11]=xf[3]; // 8C6390A6 fmov fr3,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6390A8 fschg 
        fr[8]=fr[9]; // 8C6390AA fmov fr9,fr8
        fp.binary<FpuBinaryOperation::Multiply,10u,5u>(); // 8C6390AC fmul fr10,fr5
        fp.binary<FpuBinaryOperation::Multiply,4u,8u>(); // 8C6390AE fmul fr4,fr8
        fp.binary<FpuBinaryOperation::Multiply,11u,1u>(); // 8C6390B0 fmul fr11,fr1
        fp.binary<FpuBinaryOperation::Multiply,10u,0u>(); // 8C6390B2 fmul fr10,fr0
        fp.binary<FpuBinaryOperation::Multiply,11u,2u>(); // 8C6390B4 fmul fr11,fr2
        fp.binary<FpuBinaryOperation::Multiply,3u,9u>(); // 8C6390B6 fmul fr3,fr9
        fp.binary<FpuBinaryOperation::Add,8u,5u>(); // 8C6390B8 fadd fr8,fr5
        fp.binary<FpuBinaryOperation::Add,5u,1u>(); // 8C6390BA fadd fr5,fr1
        fp.binary<FpuBinaryOperation::Subtract,2u,1u>(); // 8C6390BC fsub fr2,fr1
        fp.binary<FpuBinaryOperation::Subtract,0u,1u>(); // 8C6390BE fsub fr0,fr1
        fp.binary<FpuBinaryOperation::Subtract,9u,1u>(); // 8C6390C0 fsub fr9,fr1
        fr[0]=fr[6]; // 8C6390C2 fmov fr6,fr0
        fr[1]^=0x80000000u; // 8C6390C4 fneg fr1
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6390C6 fschg 
        fr[6]=xf[8];fr[7]=xf[9]; // 8C6390C8 fmov fr9,fr6
        fr[8]=xf[10];fr[9]=xf[11]; // 8C6390CA fmov fr11,fr8
        fr[10]=xf[12];fr[11]=xf[13]; // 8C6390CC fmov fr13,fr10
        fr[12]=xf[14];fr[13]=xf[15]; // 8C6390CE fmov fr15,fr12
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6390D0 fschg 
        fp.binary<FpuBinaryOperation::Multiply,10u,9u>(); // 8C6390D2 fmul fr10,fr9
        fp.binary<FpuBinaryOperation::Multiply,6u,12u>(); // 8C6390D4 fmul fr6,fr12
        fr[2]=fr[9]; // 8C6390D6 fmov fr9,fr2
        fp.binary<FpuBinaryOperation::Multiply,10u,8u>(); // 8C6390D8 fmul fr10,fr8
        fr[5]=fr[12]; // 8C6390DA fmov fr12,fr5
        fp.binary<FpuBinaryOperation::Multiply,13u,6u>(); // 8C6390DC fmul fr13,fr6
        fr[7]=fr[8]; // 8C6390DE fmov fr8,fr7
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6390E0 fschg 
        fr[8]=xf[4];fr[9]=xf[5]; // 8C6390E2 fmov fr5,fr8
        fr[10]=xf[6];fr[11]=xf[7]; // 8C6390E4 fmov fr7,fr10
        fr[12]=xf[6];fr[13]=xf[7]; // 8C6390E6 fmov fr7,fr12
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6390E8 fschg 
        fr[9]=fr[8]; // 8C6390EA fmov fr8,fr9
        fp.binary<FpuBinaryOperation::Multiply,2u,12u>(); // 8C6390EC fmul fr2,fr12
        fp.binary<FpuBinaryOperation::Multiply,4u,9u>(); // 8C6390EE fmul fr4,fr9
        fp.binary<FpuBinaryOperation::Multiply,5u,13u>(); // 8C6390F0 fmul fr5,fr13
        fp.binary<FpuBinaryOperation::Multiply,7u,11u>(); // 8C6390F2 fmul fr7,fr11
        fp.binary<FpuBinaryOperation::Multiply,6u,10u>(); // 8C6390F4 fmul fr6,fr10
        fp.binary<FpuBinaryOperation::Multiply,3u,8u>(); // 8C6390F6 fmul fr3,fr8
        fp.binary<FpuBinaryOperation::Add,12u,9u>(); // 8C6390F8 fadd fr12,fr9
        fp.binary<FpuBinaryOperation::Add,13u,9u>(); // 8C6390FA fadd fr13,fr9
        fp.binary<FpuBinaryOperation::Subtract,11u,9u>(); // 8C6390FC fsub fr11,fr9
        fp.binary<FpuBinaryOperation::Subtract,10u,9u>(); // 8C6390FE fsub fr10,fr9
        fp.binary<FpuBinaryOperation::Subtract,8u,9u>(); // 8C639100 fsub fr8,fr9
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639102 fschg 
        fr[10]=xf[0];fr[11]=xf[1]; // 8C639104 fmov fr1,fr10
        fr[12]=xf[2];fr[13]=xf[3]; // 8C639106 fmov fr3,fr12
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639108 fschg 
        fr[9]^=0x80000000u; // 8C63910A fneg fr9
        fp.binary<FpuBinaryOperation::Multiply,10u,4u>(); // 8C63910C fmul fr10,fr4
        fp.binary<FpuBinaryOperation::Multiply,12u,2u>(); // 8C63910E fmul fr12,fr2
        fp.binary<FpuBinaryOperation::Multiply,13u,5u>(); // 8C639110 fmul fr13,fr5
        fp.binary<FpuBinaryOperation::Multiply,12u,6u>(); // 8C639112 fmul fr12,fr6
        fp.binary<FpuBinaryOperation::Multiply,13u,7u>(); // 8C639114 fmul fr13,fr7
        fp.binary<FpuBinaryOperation::Multiply,10u,3u>(); // 8C639116 fmul fr10,fr3
        fp.binary<FpuBinaryOperation::Add,2u,4u>(); // 8C639118 fadd fr2,fr4
        fp.binary<FpuBinaryOperation::Add,5u,4u>(); // 8C63911A fadd fr5,fr4
        fr[3]^=0x80000000u; // 8C63911C fneg fr3
        fp.binary<FpuBinaryOperation::Subtract,7u,4u>(); // 8C63911E fsub fr7,fr4
        fr[2]=fr[9]; // 8C639120 fmov fr9,fr2
        fp.binary<FpuBinaryOperation::Subtract,6u,4u>(); // 8C639122 fsub fr6,fr4
        fp.binary<FpuBinaryOperation::Add,4u,3u>(); // 8C639124 fadd fr4,fr3
        fr[4]=cpu.fpul; // 8C639126 fsts fpul,fr4
        fp.binary<FpuBinaryOperation::Multiply,4u,0u>(); // 8C639128 fmul fr4,fr0
        fp.binary<FpuBinaryOperation::Multiply,4u,1u>(); // 8C63912A fmul fr4,fr1
        fp.binary<FpuBinaryOperation::Multiply,4u,2u>(); // 8C63912C fmul fr4,fr2
        fp.binary<FpuBinaryOperation::Multiply,4u,3u>(); // 8C63912E fmul fr4,fr3
        store32(0x8C639130u,r[15]-4u,fr[0],CodeWriteSource::Fpu);r[15]-=4u; // 8C639130 fmov fr0,@-r15
        store32(0x8C639132u,r[15]-4u,fr[1],CodeWriteSource::Fpu);r[15]-=4u; // 8C639132 fmov fr1,@-r15
        store32(0x8C639134u,r[15]-4u,fr[2],CodeWriteSource::Fpu);r[15]-=4u; // 8C639134 fmov fr2,@-r15
        store32(0x8C639136u,r[15]-4u,fr[3],CodeWriteSource::Fpu);r[15]-=4u; // 8C639136 fmov fr3,@-r15
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639138 fschg 
        fr[0]=xf[8];fr[1]=xf[9]; // 8C63913A fmov fr9,fr0
        fr[2]=xf[10];fr[3]=xf[11]; // 8C63913C fmov fr11,fr2
        fr[4]=xf[12];fr[5]=xf[13]; // 8C63913E fmov fr13,fr4
        fr[6]=xf[14];fr[7]=xf[15]; // 8C639140 fmov fr15,fr6
        fr[8]=xf[4];fr[9]=xf[5]; // 8C639142 fmov fr5,fr8
        fr[10]=xf[6];fr[11]=xf[7]; // 8C639144 fmov fr7,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639146 fschg 
        fr[2]=fr[1]; // 8C639148 fmov fr1,fr2
        fr[6]=fr[5]; // 8C63914A fmov fr5,fr6
        fp.binary<FpuBinaryOperation::Multiply,7u,1u>(); // 8C63914C fmul fr7,fr1
        fp.binary<FpuBinaryOperation::Multiply,0u,7u>(); // 8C63914E fmul fr0,fr7
        fp.binary<FpuBinaryOperation::Multiply,5u,0u>(); // 8C639150 fmul fr5,fr0
        fp.binary<FpuBinaryOperation::Multiply,3u,5u>(); // 8C639152 fmul fr3,fr5
        fp.binary<FpuBinaryOperation::Multiply,4u,3u>(); // 8C639154 fmul fr4,fr3
        fp.binary<FpuBinaryOperation::Multiply,4u,2u>(); // 8C639156 fmul fr4,fr2
        fr[4]=fr[7]; // 8C639158 fmov fr7,fr4
        fr[12]=fr[11]; // 8C63915A fmov fr11,fr12
        fr[6]=fr[8]; // 8C63915C fmov fr8,fr6
        fr[7]=fr[9]; // 8C63915E fmov fr9,fr7
        fp.binary<FpuBinaryOperation::Multiply,1u,6u>(); // 8C639160 fmul fr1,fr6
        fp.binary<FpuBinaryOperation::Multiply,3u,7u>(); // 8C639162 fmul fr3,fr7
        fp.binary<FpuBinaryOperation::Multiply,0u,12u>(); // 8C639164 fmul fr0,fr12
        fp.binary<FpuBinaryOperation::Add,7u,6u>(); // 8C639166 fadd fr7,fr6
        fp.binary<FpuBinaryOperation::Multiply,2u,11u>(); // 8C639168 fmul fr2,fr11
        fp.binary<FpuBinaryOperation::Add,12u,6u>(); // 8C63916A fadd fr12,fr6
        fp.binary<FpuBinaryOperation::Multiply,4u,9u>(); // 8C63916C fmul fr4,fr9
        fp.binary<FpuBinaryOperation::Subtract,11u,6u>(); // 8C63916E fsub fr11,fr6
        fp.binary<FpuBinaryOperation::Multiply,5u,8u>(); // 8C639170 fmul fr5,fr8
        fp.binary<FpuBinaryOperation::Subtract,9u,6u>(); // 8C639172 fsub fr9,fr6
        fp.binary<FpuBinaryOperation::Subtract,8u,6u>(); // 8C639174 fsub fr8,fr6
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639176 fschg 
        fr[8]=xf[0];fr[9]=xf[1]; // 8C639178 fmov fr1,fr8
        fr[10]=xf[2];fr[11]=xf[3]; // 8C63917A fmov fr3,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C63917C fschg 
        fp.binary<FpuBinaryOperation::Multiply,8u,1u>(); // 8C63917E fmul fr8,fr1
        fr[10]=fr[11]; // 8C639180 fmov fr11,fr10
        fp.binary<FpuBinaryOperation::Multiply,9u,3u>(); // 8C639182 fmul fr9,fr3
        fp.binary<FpuBinaryOperation::Multiply,0u,10u>(); // 8C639184 fmul fr0,fr10
        fp.binary<FpuBinaryOperation::Add,3u,1u>(); // 8C639186 fadd fr3,fr1
        fp.binary<FpuBinaryOperation::Multiply,2u,11u>(); // 8C639188 fmul fr2,fr11
        fp.binary<FpuBinaryOperation::Add,10u,1u>(); // 8C63918A fadd fr10,fr1
        fp.binary<FpuBinaryOperation::Multiply,9u,4u>(); // 8C63918C fmul fr9,fr4
        fp.binary<FpuBinaryOperation::Subtract,11u,1u>(); // 8C63918E fsub fr11,fr1
        fp.binary<FpuBinaryOperation::Multiply,8u,5u>(); // 8C639190 fmul fr8,fr5
        fp.binary<FpuBinaryOperation::Subtract,4u,1u>(); // 8C639192 fsub fr4,fr1
        fp.binary<FpuBinaryOperation::Subtract,5u,1u>(); // 8C639194 fsub fr5,fr1
        fr[3]=fr[6]; // 8C639196 fmov fr6,fr3
        fr[1]^=0x80000000u; // 8C639198 fneg fr1
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C63919A fschg 
        fr[4]=xf[8];fr[5]=xf[9]; // 8C63919C fmov fr9,fr4
        fr[6]=xf[10];fr[7]=xf[11]; // 8C63919E fmov fr11,fr6
        fr[8]=xf[12];fr[9]=xf[13]; // 8C6391A0 fmov fr13,fr8
        fr[10]=xf[14];fr[11]=xf[15]; // 8C6391A2 fmov fr15,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6391A4 fschg 
        fp.binary<FpuBinaryOperation::Multiply,10u,5u>(); // 8C6391A6 fmul fr10,fr5
        fp.binary<FpuBinaryOperation::Multiply,6u,8u>(); // 8C6391A8 fmul fr6,fr8
        fp.binary<FpuBinaryOperation::Multiply,10u,4u>(); // 8C6391AA fmul fr10,fr4
        fr[7]=fr[8]; // 8C6391AC fmov fr8,fr7
        fp.binary<FpuBinaryOperation::Multiply,9u,6u>(); // 8C6391AE fmul fr9,fr6
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6391B0 fschg 
        fr[8]=xf[4];fr[9]=xf[5]; // 8C6391B2 fmov fr5,fr8
        fr[10]=xf[6];fr[11]=xf[7]; // 8C6391B4 fmov fr7,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6391B6 fschg 
        fr[11]=fr[8]; // 8C6391B8 fmov fr8,fr11
        fr[12]=fr[9]; // 8C6391BA fmov fr9,fr12
        fr[13]=fr[10]; // 8C6391BC fmov fr10,fr13
        fp.binary<FpuBinaryOperation::Multiply,5u,11u>(); // 8C6391BE fmul fr5,fr11
        fp.binary<FpuBinaryOperation::Multiply,7u,12u>(); // 8C6391C0 fmul fr7,fr12
        fp.binary<FpuBinaryOperation::Multiply,0u,13u>(); // 8C6391C2 fmul fr0,fr13
        fp.binary<FpuBinaryOperation::Add,11u,12u>(); // 8C6391C4 fadd fr11,fr12
        fp.binary<FpuBinaryOperation::Multiply,2u,10u>(); // 8C6391C6 fmul fr2,fr10
        fp.binary<FpuBinaryOperation::Add,13u,12u>(); // 8C6391C8 fadd fr13,fr12
        fp.binary<FpuBinaryOperation::Multiply,4u,9u>(); // 8C6391CA fmul fr4,fr9
        fp.binary<FpuBinaryOperation::Subtract,10u,12u>(); // 8C6391CC fsub fr10,fr12
        fp.binary<FpuBinaryOperation::Multiply,6u,8u>(); // 8C6391CE fmul fr6,fr8
        fp.binary<FpuBinaryOperation::Subtract,9u,12u>(); // 8C6391D0 fsub fr9,fr12
        fp.binary<FpuBinaryOperation::Subtract,8u,12u>(); // 8C6391D2 fsub fr8,fr12
        fr[12]^=0x80000000u; // 8C6391D4 fneg fr12
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6391D6 fschg 
        fr[8]=xf[0];fr[9]=xf[1]; // 8C6391D8 fmov fr1,fr8
        fr[10]=xf[2];fr[11]=xf[3]; // 8C6391DA fmov fr3,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6391DC fschg 
        fp.binary<FpuBinaryOperation::Multiply,8u,5u>(); // 8C6391DE fmul fr8,fr5
        fp.binary<FpuBinaryOperation::Multiply,9u,7u>(); // 8C6391E0 fmul fr9,fr7
        fp.binary<FpuBinaryOperation::Multiply,10u,0u>(); // 8C6391E2 fmul fr10,fr0
        fp.binary<FpuBinaryOperation::Add,7u,5u>(); // 8C6391E4 fadd fr7,fr5
        fp.binary<FpuBinaryOperation::Multiply,10u,2u>(); // 8C6391E6 fmul fr10,fr2
        fp.binary<FpuBinaryOperation::Add,5u,0u>(); // 8C6391E8 fadd fr5,fr0
        fp.binary<FpuBinaryOperation::Multiply,9u,4u>(); // 8C6391EA fmul fr9,fr4
        fp.binary<FpuBinaryOperation::Subtract,2u,0u>(); // 8C6391EC fsub fr2,fr0
        fp.binary<FpuBinaryOperation::Multiply,8u,6u>(); // 8C6391EE fmul fr8,fr6
        fp.binary<FpuBinaryOperation::Subtract,4u,0u>(); // 8C6391F0 fsub fr4,fr0
        fp.binary<FpuBinaryOperation::Subtract,6u,0u>(); // 8C6391F2 fsub fr6,fr0
        fr[8]=cpu.fpul; // 8C6391F4 fsts fpul,fr8
        fp.binary<FpuBinaryOperation::Multiply,8u,3u>(); // 8C6391F6 fmul fr8,fr3
        fp.binary<FpuBinaryOperation::Multiply,8u,1u>(); // 8C6391F8 fmul fr8,fr1
        fp.binary<FpuBinaryOperation::Multiply,8u,12u>(); // 8C6391FA fmul fr8,fr12
        fp.binary<FpuBinaryOperation::Multiply,8u,0u>(); // 8C6391FC fmul fr8,fr0
        store32(0x8C6391FEu,r[15]-4u,fr[3],CodeWriteSource::Fpu);r[15]-=4u; // 8C6391FE fmov fr3,@-r15
        store32(0x8C639200u,r[15]-4u,fr[1],CodeWriteSource::Fpu);r[15]-=4u; // 8C639200 fmov fr1,@-r15
        store32(0x8C639202u,r[15]-4u,fr[12],CodeWriteSource::Fpu);r[15]-=4u; // 8C639202 fmov fr12,@-r15
        store32(0x8C639204u,r[15]-4u,fr[0],CodeWriteSource::Fpu);r[15]-=4u; // 8C639204 fmov fr0,@-r15
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639206 fschg 
        fr[0]=xf[0];fr[1]=xf[1]; // 8C639208 fmov fr1,fr0
        fr[2]=xf[2];fr[3]=xf[3]; // 8C63920A fmov fr3,fr2
        fr[4]=xf[4];fr[5]=xf[5]; // 8C63920C fmov fr5,fr4
        fr[6]=xf[6];fr[7]=xf[7]; // 8C63920E fmov fr7,fr6
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639210 fschg 
        fr[0]=fr[1]; // 8C639212 fmov fr1,fr0
        fr[4]=fr[7]; // 8C639214 fmov fr7,fr4
        fp.binary<FpuBinaryOperation::Multiply,6u,0u>(); // 8C639216 fmul fr6,fr0
        fp.binary<FpuBinaryOperation::Multiply,2u,4u>(); // 8C639218 fmul fr2,fr4
        fp.binary<FpuBinaryOperation::Multiply,7u,1u>(); // 8C63921A fmul fr7,fr1
        fp.binary<FpuBinaryOperation::Multiply,5u,2u>(); // 8C63921C fmul fr5,fr2
        fp.binary<FpuBinaryOperation::Multiply,3u,5u>(); // 8C63921E fmul fr3,fr5
        fp.binary<FpuBinaryOperation::Multiply,6u,3u>(); // 8C639220 fmul fr6,fr3
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639222 fschg 
        fr[6]=xf[12];fr[7]=xf[13]; // 8C639224 fmov fr13,fr6
        fr[8]=xf[14];fr[9]=xf[15]; // 8C639226 fmov fr15,fr8
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639228 fschg 
        fr[6]=fr[9]; // 8C63922A fmov fr9,fr6
        fr[10]=fr[7]; // 8C63922C fmov fr7,fr10
        fr[11]=fr[8]; // 8C63922E fmov fr8,fr11
        fp.binary<FpuBinaryOperation::Multiply,0u,6u>(); // 8C639230 fmul fr0,fr6
        fp.binary<FpuBinaryOperation::Multiply,4u,7u>(); // 8C639232 fmul fr4,fr7
        fp.binary<FpuBinaryOperation::Multiply,5u,8u>(); // 8C639234 fmul fr5,fr8
        fp.binary<FpuBinaryOperation::Add,7u,6u>(); // 8C639236 fadd fr7,fr6
        fp.binary<FpuBinaryOperation::Multiply,3u,10u>(); // 8C639238 fmul fr3,fr10
        fp.binary<FpuBinaryOperation::Add,8u,6u>(); // 8C63923A fadd fr8,fr6
        fp.binary<FpuBinaryOperation::Multiply,2u,9u>(); // 8C63923C fmul fr2,fr9
        fp.binary<FpuBinaryOperation::Subtract,10u,6u>(); // 8C63923E fsub fr10,fr6
        fp.binary<FpuBinaryOperation::Multiply,1u,11u>(); // 8C639240 fmul fr1,fr11
        fp.binary<FpuBinaryOperation::Subtract,9u,6u>(); // 8C639242 fsub fr9,fr6
        fp.binary<FpuBinaryOperation::Subtract,11u,6u>(); // 8C639244 fsub fr11,fr6
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639246 fschg 
        fr[8]=xf[8];fr[9]=xf[9]; // 8C639248 fmov fr9,fr8
        fr[10]=xf[10];fr[11]=xf[11]; // 8C63924A fmov fr11,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C63924C fschg 
        fr[8]=fr[9]; // 8C63924E fmov fr9,fr8
        fr[12]=fr[9]; // 8C639250 fmov fr9,fr12
        fp.binary<FpuBinaryOperation::Multiply,11u,0u>(); // 8C639252 fmul fr11,fr0
        fp.binary<FpuBinaryOperation::Multiply,4u,8u>(); // 8C639254 fmul fr4,fr8
        fp.binary<FpuBinaryOperation::Multiply,10u,5u>(); // 8C639256 fmul fr10,fr5
        fp.binary<FpuBinaryOperation::Add,8u,0u>(); // 8C639258 fadd fr8,fr0
        fp.binary<FpuBinaryOperation::Multiply,3u,9u>(); // 8C63925A fmul fr3,fr9
        fp.binary<FpuBinaryOperation::Add,5u,0u>(); // 8C63925C fadd fr5,fr0
        fp.binary<FpuBinaryOperation::Multiply,11u,2u>(); // 8C63925E fmul fr11,fr2
        fp.binary<FpuBinaryOperation::Subtract,9u,0u>(); // 8C639260 fsub fr9,fr0
        fp.binary<FpuBinaryOperation::Multiply,10u,1u>(); // 8C639262 fmul fr10,fr1
        fp.binary<FpuBinaryOperation::Subtract,2u,0u>(); // 8C639264 fsub fr2,fr0
        fp.binary<FpuBinaryOperation::Subtract,1u,0u>(); // 8C639266 fsub fr1,fr0
        fr[2]=fr[4]; // 8C639268 fmov fr4,fr2
        fr[1]=fr[6]; // 8C63926A fmov fr6,fr1
        fr[0]^=0x80000000u; // 8C63926C fneg fr0
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C63926E fschg 
        fr[4]=xf[0];fr[5]=xf[1]; // 8C639270 fmov fr1,fr4
        fr[6]=xf[2];fr[7]=xf[3]; // 8C639272 fmov fr3,fr6
        fr[8]=xf[4];fr[9]=xf[5]; // 8C639274 fmov fr5,fr8
        fr[10]=xf[6];fr[11]=xf[7]; // 8C639276 fmov fr7,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639278 fschg 
        fp.binary<FpuBinaryOperation::Multiply,8u,7u>(); // 8C63927A fmul fr8,fr7
        fp.binary<FpuBinaryOperation::Multiply,8u,6u>(); // 8C63927C fmul fr8,fr6
        fp.binary<FpuBinaryOperation::Multiply,4u,10u>(); // 8C63927E fmul fr4,fr10
        fp.binary<FpuBinaryOperation::Multiply,11u,4u>(); // 8C639280 fmul fr11,fr4
        fr[5]=fr[10]; // 8C639282 fmov fr10,fr5
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639284 fschg 
        fr[8]=xf[12];fr[9]=xf[13]; // 8C639286 fmov fr13,fr8
        fr[10]=xf[14];fr[11]=xf[15]; // 8C639288 fmov fr15,fr10
        fr[12]=xf[14];fr[13]=xf[15]; // 8C63928A fmov fr15,fr12
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C63928C fschg 
        fr[9]=fr[8]; // 8C63928E fmov fr8,fr9
        fp.binary<FpuBinaryOperation::Multiply,5u,11u>(); // 8C639290 fmul fr5,fr11
        fp.binary<FpuBinaryOperation::Multiply,2u,8u>(); // 8C639292 fmul fr2,fr8
        fp.binary<FpuBinaryOperation::Multiply,7u,10u>(); // 8C639294 fmul fr7,fr10
        fp.binary<FpuBinaryOperation::Add,11u,8u>(); // 8C639296 fadd fr11,fr8
        fp.binary<FpuBinaryOperation::Multiply,3u,9u>(); // 8C639298 fmul fr3,fr9
        fp.binary<FpuBinaryOperation::Add,10u,8u>(); // 8C63929A fadd fr10,fr8
        fp.binary<FpuBinaryOperation::Multiply,6u,13u>(); // 8C63929C fmul fr6,fr13
        fp.binary<FpuBinaryOperation::Subtract,9u,8u>(); // 8C63929E fsub fr9,fr8
        fp.binary<FpuBinaryOperation::Multiply,4u,12u>(); // 8C6392A0 fmul fr4,fr12
        fp.binary<FpuBinaryOperation::Subtract,13u,8u>(); // 8C6392A2 fsub fr13,fr8
        fp.binary<FpuBinaryOperation::Subtract,12u,8u>(); // 8C6392A4 fsub fr12,fr8
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6392A6 fschg 
        fr[10]=xf[8];fr[11]=xf[9]; // 8C6392A8 fmov fr9,fr10
        fr[12]=xf[10];fr[13]=xf[11]; // 8C6392AA fmov fr11,fr12
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6392AC fschg 
        fr[8]^=0x80000000u; // 8C6392AE fneg fr8
        fp.binary<FpuBinaryOperation::Multiply,13u,5u>(); // 8C6392B0 fmul fr13,fr5
        fp.binary<FpuBinaryOperation::Multiply,10u,2u>(); // 8C6392B2 fmul fr10,fr2
        fp.binary<FpuBinaryOperation::Multiply,12u,7u>(); // 8C6392B4 fmul fr12,fr7
        fp.binary<FpuBinaryOperation::Add,5u,2u>(); // 8C6392B6 fadd fr5,fr2
        fp.binary<FpuBinaryOperation::Multiply,10u,3u>(); // 8C6392B8 fmul fr10,fr3
        fp.binary<FpuBinaryOperation::Add,7u,2u>(); // 8C6392BA fadd fr7,fr2
        fp.binary<FpuBinaryOperation::Multiply,13u,6u>(); // 8C6392BC fmul fr13,fr6
        fp.binary<FpuBinaryOperation::Subtract,3u,2u>(); // 8C6392BE fsub fr3,fr2
        fp.binary<FpuBinaryOperation::Multiply,12u,4u>(); // 8C6392C0 fmul fr12,fr4
        fp.binary<FpuBinaryOperation::Subtract,6u,2u>(); // 8C6392C2 fsub fr6,fr2
        fr[3]=fr[8]; // 8C6392C4 fmov fr8,fr3
        fp.binary<FpuBinaryOperation::Subtract,4u,2u>(); // 8C6392C6 fsub fr4,fr2
        fr[4]=cpu.fpul; // 8C6392C8 fsts fpul,fr4
        fp.binary<FpuBinaryOperation::Multiply,4u,1u>(); // 8C6392CA fmul fr4,fr1
        fp.binary<FpuBinaryOperation::Multiply,4u,0u>(); // 8C6392CC fmul fr4,fr0
        fp.binary<FpuBinaryOperation::Multiply,4u,3u>(); // 8C6392CE fmul fr4,fr3
        fp.binary<FpuBinaryOperation::Multiply,4u,2u>(); // 8C6392D0 fmul fr4,fr2
        store32(0x8C6392D2u,r[15]-4u,fr[1],CodeWriteSource::Fpu);r[15]-=4u; // 8C6392D2 fmov fr1,@-r15
        store32(0x8C6392D4u,r[15]-4u,fr[0],CodeWriteSource::Fpu);r[15]-=4u; // 8C6392D4 fmov fr0,@-r15
        store32(0x8C6392D6u,r[15]-4u,fr[3],CodeWriteSource::Fpu);r[15]-=4u; // 8C6392D6 fmov fr3,@-r15
        store32(0x8C6392D8u,r[15]-4u,fr[2],CodeWriteSource::Fpu);r[15]-=4u; // 8C6392D8 fmov fr2,@-r15
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6392DA fschg 
        fr[0]=xf[0];fr[1]=xf[1]; // 8C6392DC fmov fr1,fr0
        fr[2]=xf[2];fr[3]=xf[3]; // 8C6392DE fmov fr3,fr2
        fr[4]=xf[4];fr[5]=xf[5]; // 8C6392E0 fmov fr5,fr4
        fr[6]=xf[6];fr[7]=xf[7]; // 8C6392E2 fmov fr7,fr6
        fr[8]=xf[0];fr[9]=xf[1]; // 8C6392E4 fmov fr1,fr8
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6392E6 fschg 
        fr[2]=fr[3]; // 8C6392E8 fmov fr3,fr2
        fp.binary<FpuBinaryOperation::Multiply,7u,8u>(); // 8C6392EA fmul fr7,fr8
        fp.binary<FpuBinaryOperation::Multiply,5u,0u>(); // 8C6392EC fmul fr5,fr0
        fp.binary<FpuBinaryOperation::Multiply,7u,1u>(); // 8C6392EE fmul fr7,fr1
        fp.binary<FpuBinaryOperation::Multiply,4u,2u>(); // 8C6392F0 fmul fr4,fr2
        fp.binary<FpuBinaryOperation::Multiply,5u,3u>(); // 8C6392F2 fmul fr5,fr3
        fp.binary<FpuBinaryOperation::Multiply,9u,4u>(); // 8C6392F4 fmul fr9,fr4
        fr[5]=fr[8]; // 8C6392F6 fmov fr8,fr5
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6392F8 fschg 
        fr[6]=xf[12];fr[7]=xf[13]; // 8C6392FA fmov fr13,fr6
        fr[8]=xf[14];fr[9]=xf[15]; // 8C6392FC fmov fr15,fr8
        fr[10]=xf[12];fr[11]=xf[13]; // 8C6392FE fmov fr13,fr10
        fr[12]=xf[14];fr[13]=xf[15]; // 8C639300 fmov fr15,fr12
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639302 fschg 
        fp.binary<FpuBinaryOperation::Multiply,0u,9u>(); // 8C639304 fmul fr0,fr9
        fp.binary<FpuBinaryOperation::Multiply,1u,6u>(); // 8C639306 fmul fr1,fr6
        fp.binary<FpuBinaryOperation::Multiply,2u,7u>(); // 8C639308 fmul fr2,fr7
        fp.binary<FpuBinaryOperation::Add,9u,6u>(); // 8C63930A fadd fr9,fr6
        fp.binary<FpuBinaryOperation::Multiply,3u,10u>(); // 8C63930C fmul fr3,fr10
        fp.binary<FpuBinaryOperation::Add,7u,6u>(); // 8C63930E fadd fr7,fr6
        fp.binary<FpuBinaryOperation::Multiply,4u,13u>(); // 8C639310 fmul fr4,fr13
        fp.binary<FpuBinaryOperation::Subtract,10u,6u>(); // 8C639312 fsub fr10,fr6
        fp.binary<FpuBinaryOperation::Multiply,5u,11u>(); // 8C639314 fmul fr5,fr11
        fp.binary<FpuBinaryOperation::Subtract,13u,6u>(); // 8C639316 fsub fr13,fr6
        fp.binary<FpuBinaryOperation::Subtract,11u,6u>(); // 8C639318 fsub fr11,fr6
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C63931A fschg 
        fr[8]=xf[8];fr[9]=xf[9]; // 8C63931C fmov fr9,fr8
        fr[10]=xf[10];fr[11]=xf[11]; // 8C63931E fmov fr11,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639320 fschg 
        fr[10]=fr[11]; // 8C639322 fmov fr11,fr10
        fp.binary<FpuBinaryOperation::Multiply,8u,1u>(); // 8C639324 fmul fr8,fr1
        fp.binary<FpuBinaryOperation::Multiply,0u,10u>(); // 8C639326 fmul fr0,fr10
        fp.binary<FpuBinaryOperation::Multiply,9u,2u>(); // 8C639328 fmul fr9,fr2
        fp.binary<FpuBinaryOperation::Add,10u,1u>(); // 8C63932A fadd fr10,fr1
        fp.binary<FpuBinaryOperation::Multiply,3u,8u>(); // 8C63932C fmul fr3,fr8
        fp.binary<FpuBinaryOperation::Add,2u,1u>(); // 8C63932E fadd fr2,fr1
        fp.binary<FpuBinaryOperation::Multiply,4u,11u>(); // 8C639330 fmul fr4,fr11
        fp.binary<FpuBinaryOperation::Subtract,8u,1u>(); // 8C639332 fsub fr8,fr1
        fp.binary<FpuBinaryOperation::Multiply,9u,5u>(); // 8C639334 fmul fr9,fr5
        fp.binary<FpuBinaryOperation::Subtract,11u,1u>(); // 8C639336 fsub fr11,fr1
        fr[3]=fr[4]; // 8C639338 fmov fr4,fr3
        fp.binary<FpuBinaryOperation::Subtract,5u,1u>(); // 8C63933A fsub fr5,fr1
        fr[2]=fr[6]; // 8C63933C fmov fr6,fr2
        fr[1]^=0x80000000u; // 8C63933E fneg fr1
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639340 fschg 
        fr[4]=xf[0];fr[5]=xf[1]; // 8C639342 fmov fr1,fr4
        fr[6]=xf[2];fr[7]=xf[3]; // 8C639344 fmov fr3,fr6
        fr[8]=xf[4];fr[9]=xf[5]; // 8C639346 fmov fr5,fr8
        fr[10]=xf[6];fr[11]=xf[7]; // 8C639348 fmov fr7,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C63934A fschg 
        fp.binary<FpuBinaryOperation::Multiply,10u,5u>(); // 8C63934C fmul fr10,fr5
        fp.binary<FpuBinaryOperation::Multiply,6u,8u>(); // 8C63934E fmul fr6,fr8
        fp.binary<FpuBinaryOperation::Multiply,10u,4u>(); // 8C639350 fmul fr10,fr4
        fp.binary<FpuBinaryOperation::Multiply,9u,6u>(); // 8C639352 fmul fr9,fr6
        fr[7]=fr[8]; // 8C639354 fmov fr8,fr7
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639356 fschg 
        fr[8]=xf[12];fr[9]=xf[13]; // 8C639358 fmov fr13,fr8
        fr[10]=xf[14];fr[11]=xf[15]; // 8C63935A fmov fr15,fr10
        fr[12]=xf[12];fr[13]=xf[13]; // 8C63935C fmov fr13,fr12
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C63935E fschg 
        fr[11]=fr[10]; // 8C639360 fmov fr10,fr11
        fp.binary<FpuBinaryOperation::Multiply,5u,8u>(); // 8C639362 fmul fr5,fr8
        fp.binary<FpuBinaryOperation::Multiply,0u,10u>(); // 8C639364 fmul fr0,fr10
        fp.binary<FpuBinaryOperation::Multiply,7u,9u>(); // 8C639366 fmul fr7,fr9
        fp.binary<FpuBinaryOperation::Add,10u,8u>(); // 8C639368 fadd fr10,fr8
        fp.binary<FpuBinaryOperation::Multiply,6u,12u>(); // 8C63936A fmul fr6,fr12
        fp.binary<FpuBinaryOperation::Add,9u,8u>(); // 8C63936C fadd fr9,fr8
        fp.binary<FpuBinaryOperation::Multiply,3u,11u>(); // 8C63936E fmul fr3,fr11
        fp.binary<FpuBinaryOperation::Subtract,12u,8u>(); // 8C639370 fsub fr12,fr8
        fp.binary<FpuBinaryOperation::Multiply,4u,13u>(); // 8C639372 fmul fr4,fr13
        fp.binary<FpuBinaryOperation::Subtract,11u,8u>(); // 8C639374 fsub fr11,fr8
        fp.binary<FpuBinaryOperation::Subtract,13u,8u>(); // 8C639376 fsub fr13,fr8
        fr[8]^=0x80000000u; // 8C639378 fneg fr8
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C63937A fschg 
        fr[10]=xf[8];fr[11]=xf[9]; // 8C63937C fmov fr9,fr10
        fr[12]=xf[10];fr[13]=xf[11]; // 8C63937E fmov fr11,fr12
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C639380 fschg 
        fp.binary<FpuBinaryOperation::Multiply,12u,0u>(); // 8C639382 fmul fr12,fr0
        fp.binary<FpuBinaryOperation::Multiply,10u,5u>(); // 8C639384 fmul fr10,fr5
        fp.binary<FpuBinaryOperation::Multiply,11u,7u>(); // 8C639386 fmul fr11,fr7
        fp.binary<FpuBinaryOperation::Add,5u,0u>(); // 8C639388 fadd fr5,fr0
        fp.binary<FpuBinaryOperation::Multiply,10u,6u>(); // 8C63938A fmul fr10,fr6
        fp.binary<FpuBinaryOperation::Add,7u,0u>(); // 8C63938C fadd fr7,fr0
        fp.binary<FpuBinaryOperation::Multiply,12u,3u>(); // 8C63938E fmul fr12,fr3
        fp.binary<FpuBinaryOperation::Subtract,6u,0u>(); // 8C639390 fsub fr6,fr0
        fp.binary<FpuBinaryOperation::Multiply,11u,4u>(); // 8C639392 fmul fr11,fr4
        fp.binary<FpuBinaryOperation::Subtract,3u,0u>(); // 8C639394 fsub fr3,fr0
        fp.binary<FpuBinaryOperation::Subtract,4u,0u>(); // 8C639396 fsub fr4,fr0
        fr[4]=cpu.fpul; // 8C639398 fsts fpul,fr4
        fr[3]=fr[1]; // 8C63939A fmov fr1,fr3
        fr[9]=fr[0]; // 8C63939C fmov fr0,fr9
        fp.binary<FpuBinaryOperation::Multiply,4u,2u>(); // 8C63939E fmul fr4,fr2
        fp.binary<FpuBinaryOperation::Multiply,4u,3u>(); // 8C6393A0 fmul fr4,fr3
        fp.binary<FpuBinaryOperation::Multiply,4u,8u>(); // 8C6393A2 fmul fr4,fr8
        fp.binary<FpuBinaryOperation::Multiply,4u,9u>(); // 8C6393A4 fmul fr4,fr9
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6393A6 fschg 
        xf[10]=fr[2];xf[11]=fr[3]; // 8C6393A8 fmov fr2,fr11
        xf[14]=fr[8];xf[15]=fr[9]; // 8C6393AA fmov fr8,fr15
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6393AC fschg 
        fr[1]=load32(r[15]);r[15]+=4u; // 8C6393AE fmov @r15+,fr1
        fr[0]=load32(r[15]);r[15]+=4u; // 8C6393B0 fmov @r15+,fr0
        fr[3]=load32(r[15]);r[15]+=4u; // 8C6393B2 fmov @r15+,fr3
        fr[2]=load32(r[15]);r[15]+=4u; // 8C6393B4 fmov @r15+,fr2
        fr[5]=load32(r[15]);r[15]+=4u; // 8C6393B6 fmov @r15+,fr5
        fr[4]=load32(r[15]);r[15]+=4u; // 8C6393B8 fmov @r15+,fr4
        fr[7]=load32(r[15]);r[15]+=4u; // 8C6393BA fmov @r15+,fr7
        fr[6]=load32(r[15]);r[15]+=4u; // 8C6393BC fmov @r15+,fr6
        fr[9]=load32(r[15]);r[15]+=4u; // 8C6393BE fmov @r15+,fr9
        fr[8]=load32(r[15]);r[15]+=4u; // 8C6393C0 fmov @r15+,fr8
        fr[11]=load32(r[15]);r[15]+=4u; // 8C6393C2 fmov @r15+,fr11
        fr[10]=load32(r[15]);r[15]+=4u; // 8C6393C4 fmov @r15+,fr10
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6393C6 fschg 
        xf[6]=fr[0];xf[7]=fr[1]; // 8C6393C8 fmov fr0,fr7
        xf[2]=fr[2];xf[3]=fr[3]; // 8C6393CA fmov fr2,fr3
        xf[12]=fr[4];xf[13]=fr[5]; // 8C6393CC fmov fr4,fr13
        xf[8]=fr[6];xf[9]=fr[7]; // 8C6393CE fmov fr6,fr9
        xf[4]=fr[8];xf[5]=fr[9]; // 8C6393D0 fmov fr8,fr5
        xf[0]=fr[10];xf[1]=fr[11]; // 8C6393D2 fmov fr10,fr1
        cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask); // 8C6393D4 fschg 
        r[0]=0x00000001u; // 8C6393D8 mov #1,r0
        goto L_8C6397E2; // 8C6393D6 bra
L_8C6393DA:
        r[3]=r[14]; // 8C6393DA mov r14,r3
        r[2]=r[14]; // 8C6393DC mov r14,r2
        r[3]+=0x00000020u; // 8C6393DE add #32,r3
        r[2]+=0x00000010u; // 8C6393E0 add #16,r2
        fr[0]=load32(r[3]);r[3]+=4u; // 8C6393E2 fmov @r3+,fr0
        fr[1]=load32(r[3]);r[3]+=4u; // 8C6393E4 fmov @r3+,fr1
        fr[2]=load32(r[3]);r[3]+=4u; // 8C6393E6 fmov @r3+,fr2
        fr[3]=load32(r[3]);r[3]+=4u; // 8C6393E8 fmov @r3+,fr3
        fr[4]=load32(r[3]);r[3]+=4u; // 8C6393EA fmov @r3+,fr4
        fr[5]=load32(r[3]);r[3]+=4u; // 8C6393EC fmov @r3+,fr5
        fr[6]=load32(r[3]);r[3]+=4u; // 8C6393EE fmov @r3+,fr6
        fr[7]=load32(r[3]);r[3]+=4u; // 8C6393F0 fmov @r3+,fr7
        fr[8]=load32(r[2]);r[2]+=4u; // 8C6393F2 fmov @r2+,fr8
        fr[9]=load32(r[2]);r[2]+=4u; // 8C6393F4 fmov @r2+,fr9
        fr[10]=load32(r[2]);r[2]+=4u; // 8C6393F6 fmov @r2+,fr10
        fr[11]=load32(r[2]);r[2]+=4u; // 8C6393F8 fmov @r2+,fr11
        fr[0]=fr[1]; // 8C6393FA fmov fr1,fr0
        fr[4]=fr[2]; // 8C6393FC fmov fr2,fr4
        fp.binary<FpuBinaryOperation::Multiply,5u,2u>(); // 8C6393FE fmul fr5,fr2
        fp.binary<FpuBinaryOperation::Multiply,7u,4u>(); // 8C639400 fmul fr7,fr4
        fp.binary<FpuBinaryOperation::Multiply,3u,5u>(); // 8C639402 fmul fr3,fr5
        fp.binary<FpuBinaryOperation::Multiply,6u,1u>(); // 8C639404 fmul fr6,fr1
        fp.binary<FpuBinaryOperation::Multiply,7u,0u>(); // 8C639406 fmul fr7,fr0
        fp.binary<FpuBinaryOperation::Multiply,6u,3u>(); // 8C639408 fmul fr6,fr3
        fr[6]=fr[9]; // 8C63940A fmov fr9,fr6
        fr[7]=fr[10]; // 8C63940C fmov fr10,fr7
        fp.binary<FpuBinaryOperation::Multiply,4u,6u>(); // 8C63940E fmul fr4,fr6
        fp.binary<FpuBinaryOperation::Multiply,5u,7u>(); // 8C639410 fmul fr5,fr7
        fp.binary<FpuBinaryOperation::Multiply,0u,10u>(); // 8C639412 fmul fr0,fr10
        fp.binary<FpuBinaryOperation::Add,7u,6u>(); // 8C639414 fadd fr7,fr6
        fp.binary<FpuBinaryOperation::Multiply,3u,9u>(); // 8C639416 fmul fr3,fr9
        fr[7]=fr[11]; // 8C639418 fmov fr11,fr7
        fp.binary<FpuBinaryOperation::Multiply,1u,11u>(); // 8C63941A fmul fr1,fr11
        fp.binary<FpuBinaryOperation::Multiply,2u,7u>(); // 8C63941C fmul fr2,fr7
        fp.binary<FpuBinaryOperation::Add,11u,6u>(); // 8C63941E fadd fr11,fr6
        fp.binary<FpuBinaryOperation::Subtract,7u,6u>(); // 8C639420 fsub fr7,fr6
        r[5]=r[14]; // 8C639422 mov r14,r5
        fp.binary<FpuBinaryOperation::Subtract,10u,6u>(); // 8C639424 fsub fr10,fr6
        fp.binary<FpuBinaryOperation::Subtract,9u,6u>(); // 8C639426 fsub fr9,fr6
        fr[8]=load32(r[5]);r[5]+=4u; // 8C639428 fmov @r5+,fr8
        fr[9]=load32(r[5]);r[5]+=4u; // 8C63942A fmov @r5+,fr9
        fr[10]=load32(r[5]);r[5]+=4u; // 8C63942C fmov @r5+,fr10
        fr[11]=load32(r[5]);r[5]+=4u; // 8C63942E fmov @r5+,fr11
        fr[8]=fr[9]; // 8C639430 fmov fr9,fr8
        fp.binary<FpuBinaryOperation::Multiply,10u,5u>(); // 8C639432 fmul fr10,fr5
        fp.binary<FpuBinaryOperation::Multiply,4u,8u>(); // 8C639434 fmul fr4,fr8
        fp.binary<FpuBinaryOperation::Multiply,11u,1u>(); // 8C639436 fmul fr11,fr1
        fp.binary<FpuBinaryOperation::Add,8u,5u>(); // 8C639438 fadd fr8,fr5
        fp.binary<FpuBinaryOperation::Multiply,10u,0u>(); // 8C63943A fmul fr10,fr0
        fp.binary<FpuBinaryOperation::Add,5u,1u>(); // 8C63943C fadd fr5,fr1
        fp.binary<FpuBinaryOperation::Multiply,11u,2u>(); // 8C63943E fmul fr11,fr2
        fp.binary<FpuBinaryOperation::Multiply,3u,9u>(); // 8C639440 fmul fr3,fr9
        fp.binary<FpuBinaryOperation::Subtract,2u,1u>(); // 8C639442 fsub fr2,fr1
        r[7]=r[14]; // 8C639444 mov r14,r7
        fp.binary<FpuBinaryOperation::Subtract,0u,1u>(); // 8C639446 fsub fr0,fr1
        r[7]+=0x00000010u; // 8C639448 add #16,r7
        r[6]=r[14]; // 8C63944A mov r14,r6
        fp.binary<FpuBinaryOperation::Subtract,9u,1u>(); // 8C63944C fsub fr9,fr1
        fr[0]=fr[6]; // 8C63944E fmov fr6,fr0
        r[6]+=0x00000020u; // 8C639450 add #32,r6
        fr[1]^=0x80000000u; // 8C639452 fneg fr1
        fr[6]=load32(r[6]);r[6]+=4u; // 8C639454 fmov @r6+,fr6
        fr[7]=load32(r[6]);r[6]+=4u; // 8C639456 fmov @r6+,fr7
        fr[8]=load32(r[6]);r[6]+=4u; // 8C639458 fmov @r6+,fr8
        fr[9]=load32(r[6]);r[6]+=4u; // 8C63945A fmov @r6+,fr9
        fr[10]=load32(r[6]);r[6]+=4u; // 8C63945C fmov @r6+,fr10
        fr[11]=load32(r[6]);r[6]+=4u; // 8C63945E fmov @r6+,fr11
        fr[12]=load32(r[6]);r[6]+=4u; // 8C639460 fmov @r6+,fr12
        fr[13]=load32(r[6]);r[6]+=4u; // 8C639462 fmov @r6+,fr13
        fp.binary<FpuBinaryOperation::Multiply,10u,9u>(); // 8C639464 fmul fr10,fr9
        fp.binary<FpuBinaryOperation::Multiply,6u,12u>(); // 8C639466 fmul fr6,fr12
        fr[2]=fr[9]; // 8C639468 fmov fr9,fr2
        fp.binary<FpuBinaryOperation::Multiply,10u,8u>(); // 8C63946A fmul fr10,fr8
        fr[5]=fr[12]; // 8C63946C fmov fr12,fr5
        fp.binary<FpuBinaryOperation::Multiply,13u,6u>(); // 8C63946E fmul fr13,fr6
        fr[7]=fr[8]; // 8C639470 fmov fr8,fr7
        fr[8]=load32(r[7]);r[7]+=4u; // 8C639472 fmov @r7+,fr8
        fr[9]=load32(r[7]);r[7]+=4u; // 8C639474 fmov @r7+,fr9
        fr[10]=load32(r[7]); // 8C639476 fmov @r7,fr10
        fr[12]=load32(r[7]);r[7]+=4u; // 8C639478 fmov @r7+,fr12
        fr[11]=load32(r[7]); // 8C63947A fmov @r7,fr11
        fr[13]=load32(r[7]); // 8C63947C fmov @r7,fr13
        fr[9]=fr[8]; // 8C63947E fmov fr8,fr9
        fp.binary<FpuBinaryOperation::Multiply,2u,12u>(); // 8C639480 fmul fr2,fr12
        fp.binary<FpuBinaryOperation::Multiply,4u,9u>(); // 8C639482 fmul fr4,fr9
        fp.binary<FpuBinaryOperation::Multiply,5u,13u>(); // 8C639484 fmul fr5,fr13
        fp.binary<FpuBinaryOperation::Multiply,7u,11u>(); // 8C639486 fmul fr7,fr11
        fp.binary<FpuBinaryOperation::Multiply,6u,10u>(); // 8C639488 fmul fr6,fr10
        fp.binary<FpuBinaryOperation::Multiply,3u,8u>(); // 8C63948A fmul fr3,fr8
        fp.binary<FpuBinaryOperation::Add,12u,9u>(); // 8C63948C fadd fr12,fr9
        fp.binary<FpuBinaryOperation::Add,13u,9u>(); // 8C63948E fadd fr13,fr9
        fp.binary<FpuBinaryOperation::Subtract,11u,9u>(); // 8C639490 fsub fr11,fr9
        r[6]=r[14]; // 8C639492 mov r14,r6
        fp.binary<FpuBinaryOperation::Subtract,10u,9u>(); // 8C639494 fsub fr10,fr9
        fp.binary<FpuBinaryOperation::Subtract,8u,9u>(); // 8C639496 fsub fr8,fr9
        fr[10]=load32(r[6]);r[6]+=4u; // 8C639498 fmov @r6+,fr10
        fr[11]=load32(r[6]);r[6]+=4u; // 8C63949A fmov @r6+,fr11
        fr[12]=load32(r[6]);r[6]+=4u; // 8C63949C fmov @r6+,fr12
        fr[13]=load32(r[6]);r[6]+=4u; // 8C63949E fmov @r6+,fr13
        fr[9]^=0x80000000u; // 8C6394A0 fneg fr9
        fp.binary<FpuBinaryOperation::Multiply,10u,4u>(); // 8C6394A2 fmul fr10,fr4
        fp.binary<FpuBinaryOperation::Multiply,12u,2u>(); // 8C6394A4 fmul fr12,fr2
        fp.binary<FpuBinaryOperation::Multiply,13u,5u>(); // 8C6394A6 fmul fr13,fr5
        fp.binary<FpuBinaryOperation::Multiply,12u,6u>(); // 8C6394A8 fmul fr12,fr6
        fp.binary<FpuBinaryOperation::Multiply,13u,7u>(); // 8C6394AA fmul fr13,fr7
        fp.binary<FpuBinaryOperation::Multiply,10u,3u>(); // 8C6394AC fmul fr10,fr3
        fp.binary<FpuBinaryOperation::Add,2u,4u>(); // 8C6394AE fadd fr2,fr4
        fp.binary<FpuBinaryOperation::Add,5u,4u>(); // 8C6394B0 fadd fr5,fr4
        fr[3]^=0x80000000u; // 8C6394B2 fneg fr3
        fp.binary<FpuBinaryOperation::Subtract,7u,4u>(); // 8C6394B4 fsub fr7,fr4
        fr[2]=fr[9]; // 8C6394B6 fmov fr9,fr2
        fp.binary<FpuBinaryOperation::Subtract,6u,4u>(); // 8C6394B8 fsub fr6,fr4
        fp.binary<FpuBinaryOperation::Add,4u,3u>(); // 8C6394BA fadd fr4,fr3
        fr[4]=cpu.fpul; // 8C6394BC fsts fpul,fr4
        r[5]=r[14]; // 8C6394BE mov r14,r5
        fp.binary<FpuBinaryOperation::Multiply,4u,0u>(); // 8C6394C0 fmul fr4,fr0
        r[6]=r[14]; // 8C6394C2 mov r14,r6
        fp.binary<FpuBinaryOperation::Multiply,4u,1u>(); // 8C6394C4 fmul fr4,fr1
        r[5]+=0x00000020u; // 8C6394C6 add #32,r5
        fp.binary<FpuBinaryOperation::Multiply,4u,2u>(); // 8C6394C8 fmul fr4,fr2
        r[6]+=0x00000010u; // 8C6394CA add #16,r6
        fp.binary<FpuBinaryOperation::Multiply,4u,3u>(); // 8C6394CC fmul fr4,fr3
        store32(0x8C6394CEu,r[15]-4u,fr[0],CodeWriteSource::Fpu);r[15]-=4u; // 8C6394CE fmov fr0,@-r15
        store32(0x8C6394D0u,r[15]-4u,fr[1],CodeWriteSource::Fpu);r[15]-=4u; // 8C6394D0 fmov fr1,@-r15
        store32(0x8C6394D2u,r[15]-4u,fr[2],CodeWriteSource::Fpu);r[15]-=4u; // 8C6394D2 fmov fr2,@-r15
        store32(0x8C6394D4u,r[15]-4u,fr[3],CodeWriteSource::Fpu);r[15]-=4u; // 8C6394D4 fmov fr3,@-r15
        fr[0]=load32(r[5]);r[5]+=4u; // 8C6394D6 fmov @r5+,fr0
        fr[1]=load32(r[5]);r[5]+=4u; // 8C6394D8 fmov @r5+,fr1
        fr[2]=load32(r[5]);r[5]+=4u; // 8C6394DA fmov @r5+,fr2
        fr[3]=load32(r[5]);r[5]+=4u; // 8C6394DC fmov @r5+,fr3
        fr[4]=load32(r[5]);r[5]+=4u; // 8C6394DE fmov @r5+,fr4
        fr[5]=load32(r[5]);r[5]+=4u; // 8C6394E0 fmov @r5+,fr5
        fr[6]=load32(r[5]);r[5]+=4u; // 8C6394E2 fmov @r5+,fr6
        fr[7]=load32(r[5]);r[5]+=4u; // 8C6394E4 fmov @r5+,fr7
        fr[8]=load32(r[6]);r[6]+=4u; // 8C6394E6 fmov @r6+,fr8
        fr[9]=load32(r[6]);r[6]+=4u; // 8C6394E8 fmov @r6+,fr9
        fr[10]=load32(r[6]);r[6]+=4u; // 8C6394EA fmov @r6+,fr10
        fr[11]=load32(r[6]);r[6]+=4u; // 8C6394EC fmov @r6+,fr11
        fr[2]=fr[1]; // 8C6394EE fmov fr1,fr2
        fr[6]=fr[5]; // 8C6394F0 fmov fr5,fr6
        fp.binary<FpuBinaryOperation::Multiply,7u,1u>(); // 8C6394F2 fmul fr7,fr1
        fp.binary<FpuBinaryOperation::Multiply,0u,7u>(); // 8C6394F4 fmul fr0,fr7
        fp.binary<FpuBinaryOperation::Multiply,5u,0u>(); // 8C6394F6 fmul fr5,fr0
        fp.binary<FpuBinaryOperation::Multiply,3u,5u>(); // 8C6394F8 fmul fr3,fr5
        fp.binary<FpuBinaryOperation::Multiply,4u,3u>(); // 8C6394FA fmul fr4,fr3
        fp.binary<FpuBinaryOperation::Multiply,4u,2u>(); // 8C6394FC fmul fr4,fr2
        fr[4]=fr[7]; // 8C6394FE fmov fr7,fr4
        fr[12]=fr[11]; // 8C639500 fmov fr11,fr12
        fr[6]=fr[8]; // 8C639502 fmov fr8,fr6
        fr[7]=fr[9]; // 8C639504 fmov fr9,fr7
        fp.binary<FpuBinaryOperation::Multiply,1u,6u>(); // 8C639506 fmul fr1,fr6
        fp.binary<FpuBinaryOperation::Multiply,3u,7u>(); // 8C639508 fmul fr3,fr7
        fp.binary<FpuBinaryOperation::Multiply,0u,12u>(); // 8C63950A fmul fr0,fr12
        fp.binary<FpuBinaryOperation::Add,7u,6u>(); // 8C63950C fadd fr7,fr6
        fp.binary<FpuBinaryOperation::Multiply,2u,11u>(); // 8C63950E fmul fr2,fr11
        fp.binary<FpuBinaryOperation::Add,12u,6u>(); // 8C639510 fadd fr12,fr6
        fp.binary<FpuBinaryOperation::Multiply,4u,9u>(); // 8C639512 fmul fr4,fr9
        fp.binary<FpuBinaryOperation::Subtract,11u,6u>(); // 8C639514 fsub fr11,fr6
        fp.binary<FpuBinaryOperation::Multiply,5u,8u>(); // 8C639516 fmul fr5,fr8
        fp.binary<FpuBinaryOperation::Subtract,9u,6u>(); // 8C639518 fsub fr9,fr6
        r[5]=r[14]; // 8C63951A mov r14,r5
        fp.binary<FpuBinaryOperation::Subtract,8u,6u>(); // 8C63951C fsub fr8,fr6
        fr[8]=load32(r[5]);r[5]+=4u; // 8C63951E fmov @r5+,fr8
        fr[9]=load32(r[5]);r[5]+=4u; // 8C639520 fmov @r5+,fr9
        fr[10]=load32(r[5]);r[5]+=4u; // 8C639522 fmov @r5+,fr10
        fr[11]=load32(r[5]);r[5]+=4u; // 8C639524 fmov @r5+,fr11
        fp.binary<FpuBinaryOperation::Multiply,8u,1u>(); // 8C639526 fmul fr8,fr1
        fr[10]=fr[11]; // 8C639528 fmov fr11,fr10
        fp.binary<FpuBinaryOperation::Multiply,9u,3u>(); // 8C63952A fmul fr9,fr3
        fp.binary<FpuBinaryOperation::Multiply,0u,10u>(); // 8C63952C fmul fr0,fr10
        fp.binary<FpuBinaryOperation::Add,3u,1u>(); // 8C63952E fadd fr3,fr1
        fp.binary<FpuBinaryOperation::Multiply,2u,11u>(); // 8C639530 fmul fr2,fr11
        fp.binary<FpuBinaryOperation::Add,10u,1u>(); // 8C639532 fadd fr10,fr1
        fp.binary<FpuBinaryOperation::Multiply,9u,4u>(); // 8C639534 fmul fr9,fr4
        fp.binary<FpuBinaryOperation::Subtract,11u,1u>(); // 8C639536 fsub fr11,fr1
        fp.binary<FpuBinaryOperation::Multiply,8u,5u>(); // 8C639538 fmul fr8,fr5
        fp.binary<FpuBinaryOperation::Subtract,4u,1u>(); // 8C63953A fsub fr4,fr1
        r[5]=r[14]; // 8C63953C mov r14,r5
        fp.binary<FpuBinaryOperation::Subtract,5u,1u>(); // 8C63953E fsub fr5,fr1
        r[5]+=0x00000020u; // 8C639540 add #32,r5
        fr[3]=fr[6]; // 8C639542 fmov fr6,fr3
        r[6]=r[14]; // 8C639544 mov r14,r6
        fr[1]^=0x80000000u; // 8C639546 fneg fr1
        r[6]+=0x00000010u; // 8C639548 add #16,r6
        fr[4]=load32(r[5]);r[5]+=4u; // 8C63954A fmov @r5+,fr4
        fr[5]=load32(r[5]);r[5]+=4u; // 8C63954C fmov @r5+,fr5
        fr[6]=load32(r[5]);r[5]+=4u; // 8C63954E fmov @r5+,fr6
        fr[7]=load32(r[5]);r[5]+=4u; // 8C639550 fmov @r5+,fr7
        fr[8]=load32(r[5]);r[5]+=4u; // 8C639552 fmov @r5+,fr8
        fr[9]=load32(r[5]);r[5]+=4u; // 8C639554 fmov @r5+,fr9
        fr[10]=load32(r[5]);r[5]+=4u; // 8C639556 fmov @r5+,fr10
        fr[11]=load32(r[5]);r[5]+=4u; // 8C639558 fmov @r5+,fr11
        fp.binary<FpuBinaryOperation::Multiply,10u,5u>(); // 8C63955A fmul fr10,fr5
        fp.binary<FpuBinaryOperation::Multiply,6u,8u>(); // 8C63955C fmul fr6,fr8
        fp.binary<FpuBinaryOperation::Multiply,10u,4u>(); // 8C63955E fmul fr10,fr4
        fr[7]=fr[8]; // 8C639560 fmov fr8,fr7
        fp.binary<FpuBinaryOperation::Multiply,9u,6u>(); // 8C639562 fmul fr9,fr6
        fr[8]=load32(r[6]);r[6]+=4u; // 8C639564 fmov @r6+,fr8
        fr[9]=load32(r[6]);r[6]+=4u; // 8C639566 fmov @r6+,fr9
        fr[10]=load32(r[6]);r[6]+=4u; // 8C639568 fmov @r6+,fr10
        fr[11]=load32(r[6]);r[6]+=4u; // 8C63956A fmov @r6+,fr11
        fr[11]=fr[8]; // 8C63956C fmov fr8,fr11
        fr[12]=fr[9]; // 8C63956E fmov fr9,fr12
        fr[13]=fr[10]; // 8C639570 fmov fr10,fr13
        fp.binary<FpuBinaryOperation::Multiply,5u,11u>(); // 8C639572 fmul fr5,fr11
        fp.binary<FpuBinaryOperation::Multiply,7u,12u>(); // 8C639574 fmul fr7,fr12
        fp.binary<FpuBinaryOperation::Multiply,0u,13u>(); // 8C639576 fmul fr0,fr13
        fp.binary<FpuBinaryOperation::Add,11u,12u>(); // 8C639578 fadd fr11,fr12
        fp.binary<FpuBinaryOperation::Multiply,2u,10u>(); // 8C63957A fmul fr2,fr10
        fp.binary<FpuBinaryOperation::Add,13u,12u>(); // 8C63957C fadd fr13,fr12
        fp.binary<FpuBinaryOperation::Multiply,4u,9u>(); // 8C63957E fmul fr4,fr9
        fp.binary<FpuBinaryOperation::Subtract,10u,12u>(); // 8C639580 fsub fr10,fr12
        fp.binary<FpuBinaryOperation::Multiply,6u,8u>(); // 8C639582 fmul fr6,fr8
        fp.binary<FpuBinaryOperation::Subtract,9u,12u>(); // 8C639584 fsub fr9,fr12
        r[5]=r[14]; // 8C639586 mov r14,r5
        fp.binary<FpuBinaryOperation::Subtract,8u,12u>(); // 8C639588 fsub fr8,fr12
        fr[12]^=0x80000000u; // 8C63958A fneg fr12
        fr[8]=load32(r[5]);r[5]+=4u; // 8C63958C fmov @r5+,fr8
        fr[9]=load32(r[5]);r[5]+=4u; // 8C63958E fmov @r5+,fr9
        fr[10]=load32(r[5]);r[5]+=4u; // 8C639590 fmov @r5+,fr10
        fr[11]=load32(r[5]);r[5]+=4u; // 8C639592 fmov @r5+,fr11
        fp.binary<FpuBinaryOperation::Multiply,8u,5u>(); // 8C639594 fmul fr8,fr5
        fp.binary<FpuBinaryOperation::Multiply,9u,7u>(); // 8C639596 fmul fr9,fr7
        fp.binary<FpuBinaryOperation::Multiply,10u,0u>(); // 8C639598 fmul fr10,fr0
        fp.binary<FpuBinaryOperation::Add,7u,5u>(); // 8C63959A fadd fr7,fr5
        fp.binary<FpuBinaryOperation::Multiply,10u,2u>(); // 8C63959C fmul fr10,fr2
        fp.binary<FpuBinaryOperation::Add,5u,0u>(); // 8C63959E fadd fr5,fr0
        fp.binary<FpuBinaryOperation::Multiply,9u,4u>(); // 8C6395A0 fmul fr9,fr4
        fp.binary<FpuBinaryOperation::Subtract,2u,0u>(); // 8C6395A2 fsub fr2,fr0
        fp.binary<FpuBinaryOperation::Multiply,8u,6u>(); // 8C6395A4 fmul fr8,fr6
        fp.binary<FpuBinaryOperation::Subtract,4u,0u>(); // 8C6395A6 fsub fr4,fr0
        fp.binary<FpuBinaryOperation::Subtract,6u,0u>(); // 8C6395A8 fsub fr6,fr0
        fr[8]=cpu.fpul; // 8C6395AA fsts fpul,fr8
        r[5]=r[14]; // 8C6395AC mov r14,r5
        fp.binary<FpuBinaryOperation::Multiply,8u,3u>(); // 8C6395AE fmul fr8,fr3
        fp.binary<FpuBinaryOperation::Multiply,8u,1u>(); // 8C6395B0 fmul fr8,fr1
        fp.binary<FpuBinaryOperation::Multiply,8u,12u>(); // 8C6395B2 fmul fr8,fr12
        fp.binary<FpuBinaryOperation::Multiply,8u,0u>(); // 8C6395B4 fmul fr8,fr0
        store32(0x8C6395B6u,r[15]-4u,fr[3],CodeWriteSource::Fpu);r[15]-=4u; // 8C6395B6 fmov fr3,@-r15
        store32(0x8C6395B8u,r[15]-4u,fr[1],CodeWriteSource::Fpu);r[15]-=4u; // 8C6395B8 fmov fr1,@-r15
        store32(0x8C6395BAu,r[15]-4u,fr[12],CodeWriteSource::Fpu);r[15]-=4u; // 8C6395BA fmov fr12,@-r15
        store32(0x8C6395BCu,r[15]-4u,fr[0],CodeWriteSource::Fpu);r[15]-=4u; // 8C6395BC fmov fr0,@-r15
        fr[0]=load32(r[5]);r[5]+=4u; // 8C6395BE fmov @r5+,fr0
        fr[1]=load32(r[5]);r[5]+=4u; // 8C6395C0 fmov @r5+,fr1
        fr[2]=load32(r[5]);r[5]+=4u; // 8C6395C2 fmov @r5+,fr2
        fr[3]=load32(r[5]);r[5]+=4u; // 8C6395C4 fmov @r5+,fr3
        fr[4]=load32(r[5]);r[5]+=4u; // 8C6395C6 fmov @r5+,fr4
        fr[5]=load32(r[5]);r[5]+=4u; // 8C6395C8 fmov @r5+,fr5
        fr[6]=load32(r[5]);r[5]+=4u; // 8C6395CA fmov @r5+,fr6
        fr[7]=load32(r[5]);r[5]+=4u; // 8C6395CC fmov @r5+,fr7
        fr[0]=fr[1]; // 8C6395CE fmov fr1,fr0
        fr[4]=fr[7]; // 8C6395D0 fmov fr7,fr4
        fp.binary<FpuBinaryOperation::Multiply,6u,0u>(); // 8C6395D2 fmul fr6,fr0
        fp.binary<FpuBinaryOperation::Multiply,2u,4u>(); // 8C6395D4 fmul fr2,fr4
        fp.binary<FpuBinaryOperation::Multiply,7u,1u>(); // 8C6395D6 fmul fr7,fr1
        r[5]+=0x00000010u; // 8C6395D8 add #16,r5
        fp.binary<FpuBinaryOperation::Multiply,5u,2u>(); // 8C6395DA fmul fr5,fr2
        fp.binary<FpuBinaryOperation::Multiply,3u,5u>(); // 8C6395DC fmul fr3,fr5
        fp.binary<FpuBinaryOperation::Multiply,6u,3u>(); // 8C6395DE fmul fr6,fr3
        fr[6]=load32(r[5]);r[5]+=4u; // 8C6395E0 fmov @r5+,fr6
        fr[7]=load32(r[5]);r[5]+=4u; // 8C6395E2 fmov @r5+,fr7
        fr[8]=load32(r[5]);r[5]+=4u; // 8C6395E4 fmov @r5+,fr8
        fr[9]=load32(r[5]);r[5]+=4u; // 8C6395E6 fmov @r5+,fr9
        fr[6]=fr[9]; // 8C6395E8 fmov fr9,fr6
        fr[10]=fr[7]; // 8C6395EA fmov fr7,fr10
        fr[11]=fr[8]; // 8C6395EC fmov fr8,fr11
        fp.binary<FpuBinaryOperation::Multiply,0u,6u>(); // 8C6395EE fmul fr0,fr6
        fp.binary<FpuBinaryOperation::Multiply,4u,7u>(); // 8C6395F0 fmul fr4,fr7
        fp.binary<FpuBinaryOperation::Multiply,5u,8u>(); // 8C6395F2 fmul fr5,fr8
        fp.binary<FpuBinaryOperation::Add,7u,6u>(); // 8C6395F4 fadd fr7,fr6
        fp.binary<FpuBinaryOperation::Multiply,3u,10u>(); // 8C6395F6 fmul fr3,fr10
        fp.binary<FpuBinaryOperation::Add,8u,6u>(); // 8C6395F8 fadd fr8,fr6
        fp.binary<FpuBinaryOperation::Multiply,2u,9u>(); // 8C6395FA fmul fr2,fr9
        fp.binary<FpuBinaryOperation::Subtract,10u,6u>(); // 8C6395FC fsub fr10,fr6
        fp.binary<FpuBinaryOperation::Multiply,1u,11u>(); // 8C6395FE fmul fr1,fr11
        fp.binary<FpuBinaryOperation::Subtract,9u,6u>(); // 8C639600 fsub fr9,fr6
        r[5]=r[14]; // 8C639602 mov r14,r5
        fp.binary<FpuBinaryOperation::Subtract,11u,6u>(); // 8C639604 fsub fr11,fr6
        r[5]+=0x00000020u; // 8C639606 add #32,r5
        fr[8]=load32(r[5]);r[5]+=4u; // 8C639608 fmov @r5+,fr8
        fr[9]=load32(r[5]);r[5]+=4u; // 8C63960A fmov @r5+,fr9
        fr[10]=load32(r[5]);r[5]+=4u; // 8C63960C fmov @r5+,fr10
        fr[11]=load32(r[5]);r[5]+=4u; // 8C63960E fmov @r5+,fr11
        fr[8]=fr[9]; // 8C639610 fmov fr9,fr8
        fr[12]=fr[9]; // 8C639612 fmov fr9,fr12
        fp.binary<FpuBinaryOperation::Multiply,11u,0u>(); // 8C639614 fmul fr11,fr0
        fp.binary<FpuBinaryOperation::Multiply,4u,8u>(); // 8C639616 fmul fr4,fr8
        fp.binary<FpuBinaryOperation::Multiply,10u,5u>(); // 8C639618 fmul fr10,fr5
        fp.binary<FpuBinaryOperation::Add,8u,0u>(); // 8C63961A fadd fr8,fr0
        fp.binary<FpuBinaryOperation::Multiply,3u,9u>(); // 8C63961C fmul fr3,fr9
        fp.binary<FpuBinaryOperation::Add,5u,0u>(); // 8C63961E fadd fr5,fr0
        fp.binary<FpuBinaryOperation::Multiply,11u,2u>(); // 8C639620 fmul fr11,fr2
        fp.binary<FpuBinaryOperation::Subtract,9u,0u>(); // 8C639622 fsub fr9,fr0
        fp.binary<FpuBinaryOperation::Multiply,10u,1u>(); // 8C639624 fmul fr10,fr1
        fp.binary<FpuBinaryOperation::Subtract,2u,0u>(); // 8C639626 fsub fr2,fr0
        fp.binary<FpuBinaryOperation::Subtract,1u,0u>(); // 8C639628 fsub fr1,fr0
        r[5]=r[14]; // 8C63962A mov r14,r5
        fr[2]=fr[4]; // 8C63962C fmov fr4,fr2
        fr[1]=fr[6]; // 8C63962E fmov fr6,fr1
        fr[0]^=0x80000000u; // 8C639630 fneg fr0
        fr[4]=load32(r[5]);r[5]+=4u; // 8C639632 fmov @r5+,fr4
        fr[5]=load32(r[5]);r[5]+=4u; // 8C639634 fmov @r5+,fr5
        fr[6]=load32(r[5]);r[5]+=4u; // 8C639636 fmov @r5+,fr6
        fr[7]=load32(r[5]);r[5]+=4u; // 8C639638 fmov @r5+,fr7
        fr[8]=load32(r[5]);r[5]+=4u; // 8C63963A fmov @r5+,fr8
        fr[9]=load32(r[5]);r[5]+=4u; // 8C63963C fmov @r5+,fr9
        fr[10]=load32(r[5]);r[5]+=4u; // 8C63963E fmov @r5+,fr10
        fr[11]=load32(r[5]);r[5]+=4u; // 8C639640 fmov @r5+,fr11
        fp.binary<FpuBinaryOperation::Multiply,8u,7u>(); // 8C639642 fmul fr8,fr7
        r[5]+=0x00000010u; // 8C639644 add #16,r5
        fp.binary<FpuBinaryOperation::Multiply,8u,6u>(); // 8C639646 fmul fr8,fr6
        fp.binary<FpuBinaryOperation::Multiply,4u,10u>(); // 8C639648 fmul fr4,fr10
        fp.binary<FpuBinaryOperation::Multiply,11u,4u>(); // 8C63964A fmul fr11,fr4
        fr[5]=fr[10]; // 8C63964C fmov fr10,fr5
        fr[8]=load32(r[5]);r[5]+=4u; // 8C63964E fmov @r5+,fr8
        fr[9]=load32(r[5]);r[5]+=4u; // 8C639650 fmov @r5+,fr9
        fr[10]=load32(r[5]); // 8C639652 fmov @r5,fr10
        fr[12]=load32(r[5]);r[5]+=4u; // 8C639654 fmov @r5+,fr12
        fr[11]=load32(r[5]); // 8C639656 fmov @r5,fr11
        fr[13]=load32(r[5]); // 8C639658 fmov @r5,fr13
        fr[9]=fr[8]; // 8C63965A fmov fr8,fr9
        fp.binary<FpuBinaryOperation::Multiply,5u,11u>(); // 8C63965C fmul fr5,fr11
        fp.binary<FpuBinaryOperation::Multiply,2u,8u>(); // 8C63965E fmul fr2,fr8
        fp.binary<FpuBinaryOperation::Multiply,7u,10u>(); // 8C639660 fmul fr7,fr10
        fp.binary<FpuBinaryOperation::Add,11u,8u>(); // 8C639662 fadd fr11,fr8
        fp.binary<FpuBinaryOperation::Multiply,3u,9u>(); // 8C639664 fmul fr3,fr9
        fp.binary<FpuBinaryOperation::Add,10u,8u>(); // 8C639666 fadd fr10,fr8
        fp.binary<FpuBinaryOperation::Multiply,6u,13u>(); // 8C639668 fmul fr6,fr13
        fp.binary<FpuBinaryOperation::Subtract,9u,8u>(); // 8C63966A fsub fr9,fr8
        fp.binary<FpuBinaryOperation::Multiply,4u,12u>(); // 8C63966C fmul fr4,fr12
        fp.binary<FpuBinaryOperation::Subtract,13u,8u>(); // 8C63966E fsub fr13,fr8
        r[5]=r[14]; // 8C639670 mov r14,r5
        fp.binary<FpuBinaryOperation::Subtract,12u,8u>(); // 8C639672 fsub fr12,fr8
        r[5]+=0x00000020u; // 8C639674 add #32,r5
        fr[10]=load32(r[5]);r[5]+=4u; // 8C639676 fmov @r5+,fr10
        fr[11]=load32(r[5]);r[5]+=4u; // 8C639678 fmov @r5+,fr11
        fr[12]=load32(r[5]);r[5]+=4u; // 8C63967A fmov @r5+,fr12
        fr[13]=load32(r[5]);r[5]+=4u; // 8C63967C fmov @r5+,fr13
        fr[8]^=0x80000000u; // 8C63967E fneg fr8
        fp.binary<FpuBinaryOperation::Multiply,13u,5u>(); // 8C639680 fmul fr13,fr5
        fp.binary<FpuBinaryOperation::Multiply,10u,2u>(); // 8C639682 fmul fr10,fr2
        fp.binary<FpuBinaryOperation::Multiply,12u,7u>(); // 8C639684 fmul fr12,fr7
        fp.binary<FpuBinaryOperation::Add,5u,2u>(); // 8C639686 fadd fr5,fr2
        fp.binary<FpuBinaryOperation::Multiply,10u,3u>(); // 8C639688 fmul fr10,fr3
        fp.binary<FpuBinaryOperation::Add,7u,2u>(); // 8C63968A fadd fr7,fr2
        fp.binary<FpuBinaryOperation::Multiply,13u,6u>(); // 8C63968C fmul fr13,fr6
        fp.binary<FpuBinaryOperation::Subtract,3u,2u>(); // 8C63968E fsub fr3,fr2
        fp.binary<FpuBinaryOperation::Multiply,12u,4u>(); // 8C639690 fmul fr12,fr4
        fp.binary<FpuBinaryOperation::Subtract,6u,2u>(); // 8C639692 fsub fr6,fr2
        fr[3]=fr[8]; // 8C639694 fmov fr8,fr3
        fp.binary<FpuBinaryOperation::Subtract,4u,2u>(); // 8C639696 fsub fr4,fr2
        fr[4]=cpu.fpul; // 8C639698 fsts fpul,fr4
        fp.binary<FpuBinaryOperation::Multiply,4u,1u>(); // 8C63969A fmul fr4,fr1
        r[5]=r[14]; // 8C63969C mov r14,r5
        fp.binary<FpuBinaryOperation::Multiply,4u,0u>(); // 8C63969E fmul fr4,fr0
        fp.binary<FpuBinaryOperation::Multiply,4u,3u>(); // 8C6396A0 fmul fr4,fr3
        fp.binary<FpuBinaryOperation::Multiply,4u,2u>(); // 8C6396A2 fmul fr4,fr2
        store32(0x8C6396A4u,r[15]-4u,fr[1],CodeWriteSource::Fpu);r[15]-=4u; // 8C6396A4 fmov fr1,@-r15
        store32(0x8C6396A6u,r[15]-4u,fr[0],CodeWriteSource::Fpu);r[15]-=4u; // 8C6396A6 fmov fr0,@-r15
        store32(0x8C6396A8u,r[15]-4u,fr[3],CodeWriteSource::Fpu);r[15]-=4u; // 8C6396A8 fmov fr3,@-r15
        store32(0x8C6396AAu,r[15]-4u,fr[2],CodeWriteSource::Fpu);r[15]-=4u; // 8C6396AA fmov fr2,@-r15
        fr[8]=load32(r[5]); // 8C6396AC fmov @r5,fr8
        fr[0]=load32(r[5]);r[5]+=4u; // 8C6396AE fmov @r5+,fr0
        fr[9]=load32(r[5]); // 8C6396B0 fmov @r5,fr9
        fr[1]=load32(r[5]);r[5]+=4u; // 8C6396B2 fmov @r5+,fr1
        fr[2]=load32(r[5]);r[5]+=4u; // 8C6396B4 fmov @r5+,fr2
        fr[3]=load32(r[5]);r[5]+=4u; // 8C6396B6 fmov @r5+,fr3
        fr[4]=load32(r[5]);r[5]+=4u; // 8C6396B8 fmov @r5+,fr4
        fr[5]=load32(r[5]);r[5]+=4u; // 8C6396BA fmov @r5+,fr5
        fr[6]=load32(r[5]);r[5]+=4u; // 8C6396BC fmov @r5+,fr6
        fr[7]=load32(r[5]);r[5]+=4u; // 8C6396BE fmov @r5+,fr7
        fr[2]=fr[3]; // 8C6396C0 fmov fr3,fr2
        fp.binary<FpuBinaryOperation::Multiply,7u,8u>(); // 8C6396C2 fmul fr7,fr8
        fp.binary<FpuBinaryOperation::Multiply,5u,0u>(); // 8C6396C4 fmul fr5,fr0
        fp.binary<FpuBinaryOperation::Multiply,7u,1u>(); // 8C6396C6 fmul fr7,fr1
        fp.binary<FpuBinaryOperation::Multiply,4u,2u>(); // 8C6396C8 fmul fr4,fr2
        r[5]+=0x00000010u; // 8C6396CA add #16,r5
        fp.binary<FpuBinaryOperation::Multiply,5u,3u>(); // 8C6396CC fmul fr5,fr3
        fp.binary<FpuBinaryOperation::Multiply,9u,4u>(); // 8C6396CE fmul fr9,fr4
        fr[5]=fr[8]; // 8C6396D0 fmov fr8,fr5
        fr[6]=load32(r[5]); // 8C6396D2 fmov @r5,fr6
        fr[10]=load32(r[5]);r[5]+=4u; // 8C6396D4 fmov @r5+,fr10
        fr[7]=load32(r[5]); // 8C6396D6 fmov @r5,fr7
        fr[11]=load32(r[5]);r[5]+=4u; // 8C6396D8 fmov @r5+,fr11
        fr[8]=load32(r[5]); // 8C6396DA fmov @r5,fr8
        fr[12]=load32(r[5]);r[5]+=4u; // 8C6396DC fmov @r5+,fr12
        fr[9]=load32(r[5]); // 8C6396DE fmov @r5,fr9
        fr[13]=load32(r[5]);r[5]+=4u; // 8C6396E0 fmov @r5+,fr13
        fp.binary<FpuBinaryOperation::Multiply,0u,9u>(); // 8C6396E2 fmul fr0,fr9
        fp.binary<FpuBinaryOperation::Multiply,1u,6u>(); // 8C6396E4 fmul fr1,fr6
        fp.binary<FpuBinaryOperation::Multiply,2u,7u>(); // 8C6396E6 fmul fr2,fr7
        fp.binary<FpuBinaryOperation::Add,9u,6u>(); // 8C6396E8 fadd fr9,fr6
        fp.binary<FpuBinaryOperation::Multiply,3u,10u>(); // 8C6396EA fmul fr3,fr10
        fp.binary<FpuBinaryOperation::Add,7u,6u>(); // 8C6396EC fadd fr7,fr6
        fp.binary<FpuBinaryOperation::Multiply,4u,13u>(); // 8C6396EE fmul fr4,fr13
        fp.binary<FpuBinaryOperation::Subtract,10u,6u>(); // 8C6396F0 fsub fr10,fr6
        fp.binary<FpuBinaryOperation::Multiply,5u,11u>(); // 8C6396F2 fmul fr5,fr11
        fp.binary<FpuBinaryOperation::Subtract,13u,6u>(); // 8C6396F4 fsub fr13,fr6
        r[5]+=0xFFFFFFE0u; // 8C6396F6 add #-32,r5
        fp.binary<FpuBinaryOperation::Subtract,11u,6u>(); // 8C6396F8 fsub fr11,fr6
        fr[8]=load32(r[5]);r[5]+=4u; // 8C6396FA fmov @r5+,fr8
        fr[9]=load32(r[5]);r[5]+=4u; // 8C6396FC fmov @r5+,fr9
        fr[10]=load32(r[5]);r[5]+=4u; // 8C6396FE fmov @r5+,fr10
        fr[11]=load32(r[5]);r[5]+=4u; // 8C639700 fmov @r5+,fr11
        fr[10]=fr[11]; // 8C639702 fmov fr11,fr10
        fp.binary<FpuBinaryOperation::Multiply,8u,1u>(); // 8C639704 fmul fr8,fr1
        r[5]=r[14]; // 8C639706 mov r14,r5
        fp.binary<FpuBinaryOperation::Multiply,0u,10u>(); // 8C639708 fmul fr0,fr10
        fp.binary<FpuBinaryOperation::Multiply,9u,2u>(); // 8C63970A fmul fr9,fr2
        fp.binary<FpuBinaryOperation::Add,10u,1u>(); // 8C63970C fadd fr10,fr1
        fp.binary<FpuBinaryOperation::Multiply,3u,8u>(); // 8C63970E fmul fr3,fr8
        fp.binary<FpuBinaryOperation::Add,2u,1u>(); // 8C639710 fadd fr2,fr1
        fp.binary<FpuBinaryOperation::Multiply,4u,11u>(); // 8C639712 fmul fr4,fr11
        fp.binary<FpuBinaryOperation::Subtract,8u,1u>(); // 8C639714 fsub fr8,fr1
        fp.binary<FpuBinaryOperation::Multiply,9u,5u>(); // 8C639716 fmul fr9,fr5
        fp.binary<FpuBinaryOperation::Subtract,11u,1u>(); // 8C639718 fsub fr11,fr1
        fr[3]=fr[4]; // 8C63971A fmov fr4,fr3
        fp.binary<FpuBinaryOperation::Subtract,5u,1u>(); // 8C63971C fsub fr5,fr1
        fr[2]=fr[6]; // 8C63971E fmov fr6,fr2
        fr[1]^=0x80000000u; // 8C639720 fneg fr1
        fr[4]=load32(r[5]);r[5]+=4u; // 8C639722 fmov @r5+,fr4
        fr[5]=load32(r[5]);r[5]+=4u; // 8C639724 fmov @r5+,fr5
        fr[6]=load32(r[5]);r[5]+=4u; // 8C639726 fmov @r5+,fr6
        fr[7]=load32(r[5]);r[5]+=4u; // 8C639728 fmov @r5+,fr7
        fr[8]=load32(r[5]);r[5]+=4u; // 8C63972A fmov @r5+,fr8
        fr[9]=load32(r[5]);r[5]+=4u; // 8C63972C fmov @r5+,fr9
        fr[10]=load32(r[5]);r[5]+=4u; // 8C63972E fmov @r5+,fr10
        fr[11]=load32(r[5]);r[5]+=4u; // 8C639730 fmov @r5+,fr11
        fp.binary<FpuBinaryOperation::Multiply,10u,5u>(); // 8C639732 fmul fr10,fr5
        r[5]+=0x00000010u; // 8C639734 add #16,r5
        fp.binary<FpuBinaryOperation::Multiply,6u,8u>(); // 8C639736 fmul fr6,fr8
        fp.binary<FpuBinaryOperation::Multiply,10u,4u>(); // 8C639738 fmul fr10,fr4
        fp.binary<FpuBinaryOperation::Multiply,9u,6u>(); // 8C63973A fmul fr9,fr6
        fr[7]=fr[8]; // 8C63973C fmov fr8,fr7
        fr[8]=load32(r[5]); // 8C63973E fmov @r5,fr8
        fr[12]=load32(r[5]);r[5]+=4u; // 8C639740 fmov @r5+,fr12
        fr[9]=load32(r[5]); // 8C639742 fmov @r5,fr9
        fr[13]=load32(r[5]);r[5]+=4u; // 8C639744 fmov @r5+,fr13
        fr[10]=load32(r[5]);r[5]+=4u; // 8C639746 fmov @r5+,fr10
        fr[11]=load32(r[5]); // 8C639748 fmov @r5,fr11
        fr[11]=fr[10]; // 8C63974A fmov fr10,fr11
        fp.binary<FpuBinaryOperation::Multiply,5u,8u>(); // 8C63974C fmul fr5,fr8
        fp.binary<FpuBinaryOperation::Multiply,0u,10u>(); // 8C63974E fmul fr0,fr10
        fp.binary<FpuBinaryOperation::Multiply,7u,9u>(); // 8C639750 fmul fr7,fr9
        fp.binary<FpuBinaryOperation::Add,10u,8u>(); // 8C639752 fadd fr10,fr8
        fp.binary<FpuBinaryOperation::Multiply,6u,12u>(); // 8C639754 fmul fr6,fr12
        fp.binary<FpuBinaryOperation::Add,9u,8u>(); // 8C639756 fadd fr9,fr8
        fp.binary<FpuBinaryOperation::Multiply,3u,11u>(); // 8C639758 fmul fr3,fr11
        fp.binary<FpuBinaryOperation::Subtract,12u,8u>(); // 8C63975A fsub fr12,fr8
        fp.binary<FpuBinaryOperation::Multiply,4u,13u>(); // 8C63975C fmul fr4,fr13
        fp.binary<FpuBinaryOperation::Subtract,11u,8u>(); // 8C63975E fsub fr11,fr8
        r[5]=r[14]; // 8C639760 mov r14,r5
        fp.binary<FpuBinaryOperation::Subtract,13u,8u>(); // 8C639762 fsub fr13,fr8
        r[5]+=0x00000020u; // 8C639764 add #32,r5
        fr[8]^=0x80000000u; // 8C639766 fneg fr8
        fr[10]=load32(r[5]);r[5]+=4u; // 8C639768 fmov @r5+,fr10
        fr[11]=load32(r[5]);r[5]+=4u; // 8C63976A fmov @r5+,fr11
        fr[12]=load32(r[5]);r[5]+=4u; // 8C63976C fmov @r5+,fr12
        fr[13]=load32(r[5]);r[5]+=4u; // 8C63976E fmov @r5+,fr13
        fp.binary<FpuBinaryOperation::Multiply,12u,0u>(); // 8C639770 fmul fr12,fr0
        fp.binary<FpuBinaryOperation::Multiply,10u,5u>(); // 8C639772 fmul fr10,fr5
        fp.binary<FpuBinaryOperation::Multiply,11u,7u>(); // 8C639774 fmul fr11,fr7
        fp.binary<FpuBinaryOperation::Add,5u,0u>(); // 8C639776 fadd fr5,fr0
        fp.binary<FpuBinaryOperation::Multiply,10u,6u>(); // 8C639778 fmul fr10,fr6
        fp.binary<FpuBinaryOperation::Add,7u,0u>(); // 8C63977A fadd fr7,fr0
        fp.binary<FpuBinaryOperation::Multiply,12u,3u>(); // 8C63977C fmul fr12,fr3
        fp.binary<FpuBinaryOperation::Subtract,6u,0u>(); // 8C63977E fsub fr6,fr0
        fp.binary<FpuBinaryOperation::Multiply,11u,4u>(); // 8C639780 fmul fr11,fr4
        fp.binary<FpuBinaryOperation::Subtract,3u,0u>(); // 8C639782 fsub fr3,fr0
        r[5]=r[14]; // 8C639784 mov r14,r5
        fp.binary<FpuBinaryOperation::Subtract,4u,0u>(); // 8C639786 fsub fr4,fr0
        fr[4]=cpu.fpul; // 8C639788 fsts fpul,fr4
        fr[3]=fr[1]; // 8C63978A fmov fr1,fr3
        fr[9]=fr[0]; // 8C63978C fmov fr0,fr9
        fp.binary<FpuBinaryOperation::Multiply,4u,2u>(); // 8C63978E fmul fr4,fr2
        r[0]=0x0000002Cu; // 8C639790 mov #44,r0
        fp.binary<FpuBinaryOperation::Multiply,4u,3u>(); // 8C639792 fmul fr4,fr3
        r[6]=r[14]; // 8C639794 mov r14,r6
        fp.binary<FpuBinaryOperation::Multiply,4u,8u>(); // 8C639796 fmul fr4,fr8
        r[6]+=0x00000040u; // 8C639798 add #64,r6
        fp.binary<FpuBinaryOperation::Multiply,4u,9u>(); // 8C63979A fmul fr4,fr9
        store32(0x8C63979Cu,r[0]+r[5],fr[3],CodeWriteSource::Fpu); // 8C63979C fmov fr3,@(r0,r5)
        store32(0x8C63979Eu,r[6]-4u,fr[9],CodeWriteSource::Fpu);r[6]-=4u; // 8C63979E fmov fr9,@-r6
        r[0]=0x00000028u; // 8C6397A0 mov #40,r0
        store32(0x8C6397A2u,r[6]-4u,fr[8],CodeWriteSource::Fpu);r[6]-=4u; // 8C6397A2 fmov fr8,@-r6
        store32(0x8C6397A4u,r[0]+r[5],fr[2],CodeWriteSource::Fpu); // 8C6397A4 fmov fr2,@(r0,r5)
        fr[1]=load32(r[15]);r[15]+=4u; // 8C6397A6 fmov @r15+,fr1
        r[5]=r[14]; // 8C6397A8 mov r14,r5
        fr[0]=load32(r[15]);r[15]+=4u; // 8C6397AA fmov @r15+,fr0
        r[6]=r[14]; // 8C6397AC mov r14,r6
        fr[3]=load32(r[15]);r[15]+=4u; // 8C6397AE fmov @r15+,fr3
        r[6]+=0x00000038u; // 8C6397B0 add #56,r6
        fr[2]=load32(r[15]);r[15]+=4u; // 8C6397B2 fmov @r15+,fr2
        r[5]+=0x00000028u; // 8C6397B4 add #40,r5
        fr[5]=load32(r[15]);r[15]+=4u; // 8C6397B6 fmov @r15+,fr5
        fr[4]=load32(r[15]);r[15]+=4u; // 8C6397B8 fmov @r15+,fr4
        fr[7]=load32(r[15]);r[15]+=4u; // 8C6397BA fmov @r15+,fr7
        fr[6]=load32(r[15]);r[15]+=4u; // 8C6397BC fmov @r15+,fr6
        fr[9]=load32(r[15]);r[15]+=4u; // 8C6397BE fmov @r15+,fr9
        fr[8]=load32(r[15]);r[15]+=4u; // 8C6397C0 fmov @r15+,fr8
        fr[11]=load32(r[15]);r[15]+=4u; // 8C6397C2 fmov @r15+,fr11
        fr[10]=load32(r[15]);r[15]+=4u; // 8C6397C4 fmov @r15+,fr10
        r[7]=r[14]; // 8C6397C6 mov r14,r7
        store32(0x8C6397C8u,r[5]-4u,fr[7],CodeWriteSource::Fpu);r[5]-=4u; // 8C6397C8 fmov fr7,@-r5
        store32(0x8C6397CAu,r[5]-4u,fr[6],CodeWriteSource::Fpu);r[5]-=4u; // 8C6397CA fmov fr6,@-r5
        store32(0x8C6397CCu,r[5]-4u,fr[1],CodeWriteSource::Fpu);r[5]-=4u; // 8C6397CC fmov fr1,@-r5
        store32(0x8C6397CEu,r[5]-4u,fr[0],CodeWriteSource::Fpu);r[5]-=4u; // 8C6397CE fmov fr0,@-r5
        store32(0x8C6397D0u,r[5]-4u,fr[9],CodeWriteSource::Fpu);r[5]-=4u; // 8C6397D0 fmov fr9,@-r5
        store32(0x8C6397D2u,r[5]-4u,fr[8],CodeWriteSource::Fpu);r[5]-=4u; // 8C6397D2 fmov fr8,@-r5
        store32(0x8C6397D4u,r[5]-4u,fr[3],CodeWriteSource::Fpu);r[5]-=4u; // 8C6397D4 fmov fr3,@-r5
        store32(0x8C6397D6u,r[5]-4u,fr[2],CodeWriteSource::Fpu);r[5]-=4u; // 8C6397D6 fmov fr2,@-r5
        store32(0x8C6397D8u,r[5]-4u,fr[11],CodeWriteSource::Fpu);r[5]-=4u; // 8C6397D8 fmov fr11,@-r5
        store32(0x8C6397DAu,r[5]-4u,fr[10],CodeWriteSource::Fpu);r[5]-=4u; // 8C6397DA fmov fr10,@-r5
        store32(0x8C6397DCu,r[6]-4u,fr[5],CodeWriteSource::Fpu);r[6]-=4u; // 8C6397DC fmov fr5,@-r6
        store32(0x8C6397DEu,r[6]-4u,fr[4],CodeWriteSource::Fpu);r[6]-=4u; // 8C6397DE fmov fr4,@-r6
        r[0]=load32(0x8C6397F0u); // 8C6397E0 mov.l 0x8c6397f0,r0
L_8C6397E2:
        fr[14]=load32(r[15]);r[15]+=4u; // 8C6397E2 fmov @r15+,fr14
        fr[13]=load32(r[15]);r[15]+=4u; // 8C6397E4 fmov @r15+,fr13
        fr[12]=load32(r[15]);r[15]+=4u; // 8C6397E6 fmov @r15+,fr12
L_8C6397E8:
        cpu.pr=load32(r[15]);r[15]+=4u; // 8C6397E8 lds.l @r15+,pr
        {const auto target=cpu.pr;
        r[14]=load32(r[15]);r[15]+=4u; // 8C6397EC mov.l @r15+,r14
        cpu.pc=target;return;}
    };
    invert();
    return true;
}
} // namespace sonic::matrix_inverse
