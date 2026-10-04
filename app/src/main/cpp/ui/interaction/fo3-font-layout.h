#pragma once
// FO3 bitmap metrics and geometry. CPU-only; no asset bytes or GL dependency.
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace fo3font {
constexpr size_t GlyphCount = 256, GlyphTableOffset = 0x128,
                 GlyphRecordBytes = 0x38, FontFileBytes = 0x3928;
struct Glyph {
    int32_t textureIndex = 0;
    std::array<float,8> uv{}; // TL, TR, BL, BR
    float width = 0, height = 0, leadingEdge = 0, spacing = 0, topEdge = 0;
};
static_assert(sizeof(Glyph) == GlyphRecordBytes);
struct Metrics {
    float baseLine = 0;
    uint32_t textureCount = 0;
    std::array<std::string,8> textureFiles{};
    std::array<Glyph,GlyphCount> glyphs{};
};
inline uint32_t U32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
inline float F32(const uint8_t* p) {
    const uint32_t bits=U32(p);float f;std::memcpy(&f,&bits,4);return f;
}
// Parse disk metrics separately from the executable's post-load substitutions.
inline bool ParseFalloutFont(const std::vector<uint8_t>& bytes, Metrics& out) {
    out={};if(bytes.size()!=FontFileBytes)return false;
    Metrics parsed;parsed.baseLine=F32(bytes.data());parsed.textureCount=U32(bytes.data()+4);
    if(!std::isfinite(parsed.baseLine)||parsed.baseLine<=0||parsed.textureCount<1||parsed.textureCount>8)return false;
    for(size_t i=0;i<parsed.textureCount;++i) {
        const char* name=reinterpret_cast<const char*>(bytes.data()+12+i*36);
        // Fallout's texture filename is a fixed 32-byte field. Vanilla
        // Baked-in_Monofonto_Large uses all 32 bytes for
        // "baked-in_monofonto_large_0_lod_a"; the following entry/padding
        // supplies the terminating zero when Fallout3.exe formats %s.TEX.
        // A full field is therefore valid and must not be rejected.
        size_t length=0;while(length<32&&name[length])++length;
        if(length==0)return false;
        parsed.textureFiles[i].assign(name,length);
    }
    for(size_t i=0;i<GlyphCount;++i) {
        const uint8_t* p=bytes.data()+GlyphTableOffset+i*GlyphRecordBytes;auto& g=parsed.glyphs[i];
        g.textureIndex=static_cast<int32_t>(U32(p));
        for(size_t k=0;k<8;++k){g.uv[k]=F32(p+4+4*k);if(!std::isfinite(g.uv[k]))return false;}
        g.width=F32(p+36);g.height=F32(p+40);g.leadingEdge=F32(p+44);g.spacing=F32(p+48);g.topEdge=F32(p+52);
        for(float f:{g.width,g.height,g.leadingEdge,g.spacing,g.topEdge})if(!std::isfinite(f))return false;
        if(g.width<0||g.height<0)return false;
    }
    out=std::move(parsed);return true;
}
// Original Fallout3.exe B57F90..B58126: font-wide extents, space width/spacing
// swap, NBSP metrics, DEL from '|', and invisible NUL. Call once after parsing.
inline void ApplyFalloutLoadSemantics(Metrics& f) {
    float maxHeight=0, minTopMinusHeight=0;
    for(const auto& g:f.glyphs){maxHeight=std::max(maxHeight,g.height);minTopMinusHeight=std::min(minTopMinusHeight,g.topEdge-g.height);}
    auto& space=f.glyphs[0x20];std::swap(space.width,space.spacing);
    space.height=maxHeight;space.topEdge=maxHeight+minTopMinusHeight;
    auto& nbsp=f.glyphs[0xa0];nbsp.width=space.width;nbsp.spacing=space.spacing;nbsp.height=space.height;nbsp.topEdge=space.topEdge;
    auto& del=f.glyphs[0x7f];const auto& pipe=f.glyphs[0x7c];
    del.width=pipe.width;del.height=pipe.height;del.leadingEdge=pipe.leadingEdge;del.spacing=pipe.spacing;del.topEdge=pipe.topEdge;
    auto& nul=f.glyphs[0];nul.width=0;nul.spacing=0;nul.height=maxHeight;nul.topEdge=space.topEdge;nul.uv.fill(0);
}
inline uint8_t GlyphIndex(uint8_t byte) {
    // MakeString B58C87 and measurement B56500 normalize CP1252 smart quotes.
    if(byte==0x91||byte==0x92)return '\'';
    if(byte==0x93||byte==0x94)return '"';
    return byte;
}
inline float GlyphRenderAdvance(const Glyph& g) {
    // AddChar B555FD, B55792..B557AC; metrics are already post-load.
    return g.leadingEdge+g.width+(g.width>0 ? g.spacing : 0);
}
inline float GlyphMeasurementAdvance(const Glyph& g) {
    // FontManager measurement B56529..B56535 is unconditional. Current single
    // line UI uses render advance so fitting, centering and emitted pens agree.
    return g.leadingEdge+g.width+g.spacing;
}
inline float MeasureText(const Metrics& f,std::string_view text) {
    float width=0;for(unsigned char c:text)width+=GlyphRenderAdvance(f.glyphs[GlyphIndex(c)]);return width;
}
inline size_t FitText(const Metrics& f,std::string_view text,float maxWidth) {
    float width=0;size_t n=0;for(unsigned char c:text){float next=width+GlyphRenderAdvance(f.glyphs[GlyphIndex(c)]);if(next>maxWidth)break;width=next;++n;}return n;
}
struct Quad { float left,top,right,bottom; };
inline Quad GlyphQuad(const Metrics& f,const Glyph& g,float penX,float lineTop) {
    // AddChar emits Z=lineZ+topEdge and Z-height. Expressing its upward font
    // coordinates relative to the stored baseline in downward UI pixels:
    const float top=lineTop+f.baseLine-g.topEdge;
    return {penX+g.leadingEdge,top,penX+g.leadingEdge+g.width,top+g.height};
}
inline bool Drawable(const Metrics& f,const Glyph& g) {
    // Degenerate atlas rectangles contain no glyph. Skipping their quad prevents
    // sampling a border texel as punctuation; this applies to every glyph.
    return g.width>0&&g.height>0&&g.textureIndex>=0&&uint32_t(g.textureIndex)<f.textureCount&&
        (g.uv[0]!=g.uv[2]||g.uv[4]!=g.uv[6])&&(g.uv[1]!=g.uv[5]||g.uv[3]!=g.uv[7]);
}
template<class Emit> inline float AppendText(const Metrics& f,std::string_view text,float penX,float lineTop,Emit emit) {
    for(unsigned char c:text){uint8_t index=GlyphIndex(c);const auto& g=f.glyphs[index];
        if(Drawable(f,g))emit(index,g,GlyphQuad(f,g,penX,lineTop));
        penX+=GlyphRenderAdvance(g);
    }return penX;
}
} // namespace fo3font
