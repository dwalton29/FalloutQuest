#include "../../app/src/main/cpp/ui/interaction/fo3-font-layout.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
using namespace fo3font;
void Put(std::vector<uint8_t>& b,size_t at,uint32_t v){for(int i=0;i<4;++i)b[at+i]=uint8_t(v>>(i*8));}
void Float(std::vector<uint8_t>& b,size_t at,float f){uint32_t v;std::memcpy(&v,&f,4);Put(b,at,v);}
std::vector<uint8_t> Fixture(){
    // Entirely synthetic metrics; no Bethesda bytes or captured glyph atlas.
    std::vector<uint8_t> b(FontFileBytes);Float(b,0,40);Put(b,4,1);std::memcpy(b.data()+12,"test_atlas",11);
    for(size_t i=0;i<256;++i){size_t at=GlyphTableOffset+i*56;
        float uv[]{.1f,.2f,.3f,.2f,.1f,.5f,.3f,.5f};
        for(int j=0;j<8;++j)Float(b,at+4+4*j,uv[j]);
        Float(b,at+36,20);Float(b,at+40,25);Float(b,at+44,2);Float(b,at+48,-3);Float(b,at+52,25);
    }
    size_t space=GlyphTableOffset+32*56;for(size_t j=4;j<56;j+=4)Float(b,space+j,0);Float(b,space+48,9);
    return b;
}
int main(int argc,char**argv){
    static_assert(GlyphRecordBytes==0x38&&GlyphCount==256&&GlyphTableOffset==0x128&&FontFileBytes==0x3928);
    auto bytes=Fixture();Metrics f;assert(ParseFalloutFont(bytes,f));
    assert(f.baseLine==40&&f.glyphs['A'].leadingEdge==2&&f.glyphs['A'].spacing==-3&&f.glyphs['A'].topEdge==25);
    const auto rawSpace=f.glyphs[' '];ApplyFalloutLoadSemantics(f);
    assert(f.glyphs[' '].width==rawSpace.spacing&&f.glyphs[' '].spacing==rawSpace.width);
    assert(GlyphRenderAdvance(f.glyphs[' '])==9&&!Drawable(f,f.glyphs[' ']));
    assert(f.glyphs[160].width==9&&f.glyphs[0].width==0&&f.glyphs[127].width==f.glyphs['|'].width);
    assert(GlyphIndex(32)==32&&GlyphIndex(45)==45&&GlyphIndex(46)==46);
    assert(GlyphIndex(0x91)==39&&GlyphIndex(0x92)==39&&GlyphIndex(0x93)==34&&GlyphIndex(0x94)==34);
    auto g=f.glyphs['A'];assert(GlyphRenderAdvance(g)==19);g.width=0;assert(GlyphRenderAdvance(g)==2&&GlyphMeasurementAdvance(g)==-1);
    // Spacing affects X only; topEdge and height independently determine Y.
    auto a=f.glyphs['A'];auto q=GlyphQuad(f,a,10,7);assert(q.left==12&&q.top==22&&q.bottom==47);
    a.spacing=100;auto same=GlyphQuad(f,a,10,7);assert(q.top==same.top&&q.bottom==same.bottom);
    a.topEdge=30;a.height=20;auto raised=GlyphQuad(f,a,10,7);assert(raised.top==17&&raised.bottom==37);
    f.glyphs['g'].height=30;f.glyphs['g'].topEdge=25; // intentional descender
    f.glyphs['.'].height=4;f.glyphs['.'].topEdge=4;
    f.glyphs['-'].height=3;f.glyphs['-'].topEdge=15;
    assert(GlyphQuad(f,f.glyphs['g'],0,0).bottom==45);
    assert(GlyphQuad(f,f.glyphs['.'],0,0).bottom==40);
    assert(GlyphQuad(f,f.glyphs['-'],0,0).top==25);
    for(const char* text:{"ABCDEFGHIJKLMNOPQRSTUVWXYZ","abcdefghijklmnopqrstuvwxyz","0123456789",".,:;!?-+/","Super-Duper Mart","Open Super-Duper Mart","Take 10mm Pistol","Bottlecap (123)"}){
        size_t quads=0;float end=AppendText(f,text,12,7,[&](uint8_t index,const Glyph& glyph,const Quad& rect){
            ++quads;assert(index!=' ');assert(rect.top==7+f.baseLine-glyph.topEdge);assert(rect.bottom==rect.top+glyph.height);
        });
        assert(std::abs((end-12)-MeasureText(f,text))<.001f);assert(FitText(f,text,MeasureText(f,text))==std::strlen(text));assert(quads>0);
    }
    assert(FitText(f,"AA A",38)==2);
    assert(GlyphQuad(f,f.glyphs['A'],0,0).top==GlyphQuad(f,f.glyphs['A'],200,0).top);
    // Table alignment, finite values, truncated/extra data and filename bounds.
    bytes.pop_back();assert(!ParseFalloutFont(bytes,f));bytes=Fixture();bytes.push_back(0);assert(!ParseFalloutFont(bytes,f));
    bytes=Fixture();Float(bytes,GlyphTableOffset+44,std::numeric_limits<float>::quiet_NaN());assert(!ParseFalloutFont(bytes,f));
    bytes=Fixture();Put(bytes,4,9);assert(!ParseFalloutFont(bytes,f));
    bytes=Fixture();std::memset(bytes.data()+12,'x',32);assert(!ParseFalloutFont(bytes,f));
    if(argc>1){
        std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> original{std::istreambuf_iterator<char>(file),{}};
        assert(ParseFalloutFont(original,f));assert(f.textureCount==1);
        for(unsigned char ch:std::string(" .-01AMaegty")){const auto& v=f.glyphs[ch];
            std::printf("raw glyph %02X texture=%d w=%.1f h=%.1f leading=%.1f spacing=%.1f top=%.1f\n",ch,v.textureIndex,v.width,v.height,v.leadingEdge,v.spacing,v.topEdge);
            assert(v.textureIndex==0);if(ch!=' ')assert(Drawable(f,v));
        }
        const auto space=f.glyphs[32];ApplyFalloutLoadSemantics(f);
        assert(GlyphRenderAdvance(f.glyphs[32])==space.leadingEdge+space.spacing+(space.spacing>0?space.width:0));
        assert(!Drawable(f,f.glyphs[32]));
        std::printf("original font baseline %.1f space advance %.1f\n",f.baseLine,GlyphRenderAdvance(f.glyphs[32]));
    }
    std::puts("Fallout font parser/layout checks passed");
}
