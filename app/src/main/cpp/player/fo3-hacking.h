#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
namespace fo3hacking {
// Original word lengths: 005B3E50: 4 + tier*2 + (base record flags & 1), capped 12.
inline int WordLength(int tier,bool odd){return std::clamp(4+2*tier+int(odd),4,12);}
// 00925781..0092586A. Science bonus entry point 30 modifies the input skill.
inline int WordCount(float science,int requirement,int minimum=5,int maximum=20){
  maximum=std::clamp(maximum,1,20);minimum=std::clamp(minimum,1,maximum);
  const float fraction=requirement<100?1-std::clamp((science-requirement)/(100-requirement),0.f,1.f):.5f;
  return std::clamp(minimum+int(std::floor((maximum-minimum)*fraction+.5f)),minimum,maximum);
}
inline std::array<std::vector<std::string>,13> Dictionary(const std::string& original){
  std::array<std::vector<std::string>,13> out;std::unordered_set<std::string> seen;
  std::istringstream in(original);std::string word;
  while(in>>word){if(word.size()<4||word.size()>12)continue;
    if(!std::all_of(word.begin(),word.end(),[](char c){return c>='A'&&c<='Z';}))continue;
    if(seen.insert(word).second)out[word.size()].push_back(word);}
  return out;
}
struct Game {
  static constexpr int Rows=16,Columns=12,Half=Rows*Columns,Size=Half*2;
  struct Word {std::string text;int offset=0;bool removed=false,guessed=false;};
  enum class Event {None,Denied,Granted,Lockout,Dud,Reset};
  std::string dump;std::vector<Word> words;size_t password=0;
  int attempts=4,cursor=0,likeness=0;uint32_t address=0;bool replenished=false;
  std::mt19937 random;
  bool Begin(const std::vector<std::string>& dictionary,int count,uint32_t seed){
    *this={};random.seed(seed);if(dictionary.empty())return false;
    const size_t length=dictionary.front().size();if(length<4||length>12)return false;
    auto candidates=dictionary;std::sort(candidates.begin(),candidates.end());
    candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
    candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](const auto& w){return w.size()!=length;}),candidates.end());
    count=std::min(std::clamp(count,1,20),int(candidates.size()));if(!count)return false;
    std::shuffle(candidates.begin(),candidates.end(),random);
    // Original glyph alphabet and two-column memory dump; no word list is embedded.
    static constexpr char noise[]="!@#$%^&*()_+-={}[]|:;,.<>?/";
    dump.resize(Size);for(char& c:dump)c=noise[random()%(sizeof(noise)-1)];
    const int stride=Size/count;
    for(int i=0;i<count;++i){const int start=i*stride+int(random()%std::max(1,stride-int(length)-1));
      words.push_back({candidates[i],start,false,false});dump.replace(size_t(start),length,candidates[i]);}
    password=random()%words.size();address=0xf000+(random()%0xff)*16;return true;
  }
  static int Likeness(const std::string& a,const std::string& b){int n=0;for(size_t i=0;i<std::min(a.size(),b.size());++i)n+=a[i]==b[i];return n;}
  int WordAt(int at) const {for(size_t i=0;i<words.size();++i){const auto& w=words[i];if(!w.removed&&at>=w.offset&&at<w.offset+int(w.text.size()))return int(i);}return -1;}
  std::pair<int,int> Span(int at) const {
    if(at<0||at>=Size)return {-1,-1};int word=WordAt(at);
    if(word>=0)return {words[word].offset,words[word].offset+int(words[word].text.size())-1};
    const std::string opens="([{<",closes=")]}>";auto bracket=opens.find(dump[at]);
    if(bracket==std::string::npos)return {at,at};
    // Paired punctuation never spans rows or includes a password candidate.
    for(int i=at+1;i<(at/Columns+1)*Columns;++i){if(WordAt(i)>=0)break;if(dump[i]==closes[bracket])return {at,i};}
    return {at,at};
  }
  void Move(int dx,int dy){int half=cursor/Half,row=(cursor%Half)/Columns,col=cursor%Columns;
    col+=dx;if(col<0){half=1-half;col=Columns-1;}if(col>=Columns){half=1-half;col=0;}
    row=(row+dy+Rows)%Rows;cursor=half*Half+row*Columns+col;}
  Event Select(){
    if(attempts<=0)return Event::None;
    const int index=WordAt(cursor);
    if(index>=0){auto& w=words[size_t(index)];if(w.guessed)return Event::None;
      w.guessed=true;likeness=Likeness(w.text,words[password].text);
      if(size_t(index)==password)return Event::Granted;
      return --attempts?Event::Denied:Event::Lockout;}
    const auto span=Span(cursor);if(span.second<=span.first)return Event::None;
    std::fill(dump.begin()+span.first,dump.begin()+span.second+1,'.');
    std::vector<size_t> duds;for(size_t i=0;i<words.size();++i)if(i!=password&&!words[i].removed&&!words[i].guessed)duds.push_back(i);
    if(!replenished&&(duds.empty()||random()%2==0)){attempts=4;replenished=true;return Event::Reset;}
    if(duds.empty())return Event::None;
    auto& w=words[duds[random()%duds.size()]];w.removed=true;dump.replace(size_t(w.offset),w.text.size(),std::string(w.text.size(),'.'));return Event::Dud;
  }
};
}
