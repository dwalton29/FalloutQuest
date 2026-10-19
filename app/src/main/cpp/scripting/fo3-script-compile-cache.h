#pragma once
#include "dialogue/fo3-dialogue-results.h"
#include <tuple>
#include <map>

namespace fo3script {
// A Player's catalogue is frozen. Only command decoding and immutable name
// bindings are cached; expression values, gates, execution and publication are
// always evaluated against the current transaction candidate.
class CompileCache {
public:
  struct Program {
    std::vector<fo3dialogue::Command> commands;
    fo3dialogue::CommandFormCapture forms;
    std::string error;
    bool valid=false;
  };
  static constexpr size_t MaxEntries=256,MaxBytes=2*1024*1024;
  explicit CompileCache(const fo3pipdata::Definitions& definitions):definitions_(definitions){}
  std::shared_ptr<const Program> Get(const fo3pipdata::ResultScript& script,
      uint32_t owner,uint32_t localScript,bool capture,size_t limit=0) {
    Key key{owner,localScript,capture,limit,script.source.empty()&&!script.compiled.empty(),script.source};
    const auto found=entries_.find(key);
    if(found!=entries_.end()){++hits;return found->second;}
    ++misses;
    auto result=std::make_shared<Program>();result->forms.limit=limit;
    result->valid=fo3dialogue::CompileResult(definitions_,script,result->commands,result->error,
        owner,localScript,capture?&result->forms:nullptr);
    size_t bytes=sizeof(Key)+sizeof(Program)+key.source.capacity()+result->error.capacity();
    bytes+=result->commands.capacity()*sizeof(fo3dialogue::Command);
    for(const auto& command:result->commands)bytes+=command.expression.capacity();
    bytes+=result->forms.bindings.capacity()*sizeof(result->forms.bindings[0]);
    bytes+=result->forms.locals.capacity()*sizeof(result->forms.locals[0]);
    bytes+=result->forms.variables.capacity()*sizeof(result->forms.variables[0]);
    bytes+=result->forms.globals.capacity()*sizeof(result->forms.globals[0]);
    for(const auto& binding:result->forms.bindings)bytes+=binding.second.capacity();
    for(const auto& local:result->forms.locals)bytes+=local.second.capacity();
    for(const auto& variable:result->forms.variables)
      bytes+=variable.ownerName.capacity()+variable.variableName.capacity();
    // Include map nodes and allocator overhead conservatively. A caller keeps
    // its immutable program alive even if a nested script clears this cache.
    bytes+=256;
    if(bytes<=MaxBytes){
      if(entries_.size()>=MaxEntries||bytes>MaxBytes-retainedBytes){entries_.clear();retainedBytes=0;}
      entries_.emplace(std::move(key),result);retainedBytes+=bytes;
    }
    return result;
  }
  size_t Size() const{return entries_.size();}
  size_t retainedBytes=0;
  uint64_t hits=0,misses=0;
private:
  struct Key {
    uint32_t owner,script;bool capture;size_t limit;bool compiledOnly;std::string source;
    bool operator<(const Key& b) const {
      return std::tie(owner,script,capture,limit,compiledOnly,source)<
          std::tie(b.owner,b.script,b.capture,b.limit,b.compiledOnly,b.source);
    }
  };
  const fo3pipdata::Definitions& definitions_;
  std::map<Key,std::shared_ptr<const Program>> entries_;
};
} // namespace fo3script
