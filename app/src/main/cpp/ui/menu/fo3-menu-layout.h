#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fo3menu {
// Gamebryo menu XML is a fragment language (including &-GMST; entities), not
// strict XML. Keep source traits and expressions; never flatten to guessed XYs.
inline std::string Trim(std::string s) {
  auto p = s.find_first_not_of(" \t\r\n");
  if (p == s.npos)
    return {};
  return s.substr(p, s.find_last_not_of(" \t\r\n") - p + 1);
}
struct Xml {
  std::string tag, text;
  std::map<std::string, std::string> attrs;
  std::vector<Xml> children;
  std::string Attr(const char *key) const {
    auto i = attrs.find(key);
    return i == attrs.end() ? "" : i->second;
  }
};
inline bool Parse(const std::string &source, std::vector<Xml> &roots,
                  std::string &error) {
  size_t at = 0;
  size_t count = 0;
  std::function<bool(std::vector<Xml> &, const std::string &, unsigned)> read =
      [&](std::vector<Xml> &out, const std::string &close, unsigned depth) {
        if (depth > 128) {
          error = "menu XML nesting limit";
          return false;
        }
        while (at < source.size()) {
          if (source.compare(at, 4, "<!--") == 0) {
            auto end = source.find("-->", at + 4);
            if (end == source.npos) {
              error = "unterminated menu comment";
              return false;
            }
            at = end + 3;
            continue;
          }
          if (source[at] != '<') {
            ++at;
            continue;
          }
          auto end = source.find('>', at);
          if (end == source.npos) {
            error = "unterminated menu tag";
            return false;
          }
          auto token = Trim(source.substr(at + 1, end - at - 1));
          at = end + 1;
          if (token.empty() || token[0] == '?' || token[0] == '!')
            continue;
          if (token[0] == '/') {
            if (Trim(token.substr(1)) != close) {
              error = "mismatched menu close: " + token;
              return false;
            }
            return true;
          }
          if (++count > 150000) {
            error = "menu XML node limit";
            return false;
          }
          bool self = token.back() == '/';
          if (self)
            token.pop_back();
          Xml node;
          auto space = token.find_first_of(" \t\r\n");
          node.tag = token.substr(0, space);
          size_t pos = space;
          while (pos != token.npos && pos < token.size()) {
            pos = token.find_first_not_of(" \t\r\n", pos);
            if (pos == token.npos)
              break;
            auto eq = token.find('=', pos);
            if (eq == token.npos)
              break;
            auto key = Trim(token.substr(pos, eq - pos));
            pos = token.find_first_not_of(" \t\r\n", eq + 1);
            if (pos == token.npos ||
                (token[pos] != '\"' && token[pos] != '\'')) {
              error = "invalid menu attribute";
              return false;
            }
            auto finish = token.find(token[pos], pos + 1);
            if (finish == token.npos)
              return false;
            node.attrs[key] = token.substr(pos + 1, finish - pos - 1);
            pos = finish + 1;
          }
          if (!self) {
            size_t textStart = at;
            while (at < source.size() && source[at] != '<')
              ++at;
            node.text = Trim(source.substr(textStart, at - textStart));
            if (!read(node.children, node.tag, depth + 1))
              return false;
          }
          out.push_back(std::move(node));
        }
        if (!close.empty()) {
          error = "missing menu close: " + close;
          return false;
        }
        return true;
      };
  roots.clear();
  return read(roots, "", 0);
}
inline bool Tile(const std::string &tag) {
  return tag == "menu" || tag == "rect" || tag == "image" || tag == "text" ||
         tag == "hotrect" || tag == "nif";
}
struct Value {
  std::string text;
  Value() = default;
  Value(const std::string &s) : text(s) {}
  Value(const char *s) : text(s) {}
  Value(double n) {
    if (std::isfinite(n)) {
      char s[48];
      std::snprintf(s, sizeof(s), "%.9g", n);
      text = s;
    }
  }
  bool Numeric() const {
    if (text.empty())
      return true;
    char *end = nullptr;
    std::strtod(text.c_str(), &end);
    return end != text.c_str() && *end == 0;
  }
  bool Boolean() const { return Numeric() ? Number() != 0 : !text.empty(); }
  double Number() const {
    if (text == "true")
      return 1;
    if (text == "right")
      return 2;
    if (text == "center")
      return 1;
    if (text == "scale")
      return 100;
    char *end = nullptr;
    auto n = std::strtod(text.c_str(), &end);
    return end != text.c_str() && std::isfinite(n) ? n : 0;
  }
};
struct Node {
  std::string tag, name;
  int parent = -1;
  std::vector<int> children;
  std::map<std::string, Xml> traits;
  std::map<std::string, Value> bound;
  bool dynamic = false;
};
struct Box {
  float x = 0, y = 0, w = 0, h = 0;
};
struct Draw {
  int node = -1;
  std::string type, text, file, atlas;
  Box box, clip;
  int font = 2, justify = 0, colour = 0;
  float alpha = 1, brightness = 1, depth = 0, rotation = 0;
};
class Layout {
  using Reader = std::function<bool(const std::string &, std::string &)>;
  Reader reader_;
  std::vector<Node> nodes_;
  std::map<std::string, Xml> templates_;
  std::unordered_map<std::string, std::vector<Xml>> files_;
  std::map<std::pair<int, std::string>, Value> cache_;
  std::set<std::pair<int, std::string>> evaluating_;
  std::unordered_set<std::string> unsupported_;
  int root_ = -1, globals_ = -1;
  size_t staticSize_ = 0;
  std::string error_;
  bool File(const std::string &path, const Reader &read,
            std::vector<Xml> &result) {
    auto i = files_.find(path);
    if (i != files_.end()) {
      result = i->second;
      return true;
    }
    std::string source;
    if (!read(path, source)) {
      error_ = "missing menu definition: " + path;
      return false;
    }
    if (!Parse(source, result, error_))
      return false;
    files_[path] = result;
    return true;
  }
  bool Expand(const std::vector<Xml> &source, int parent, const Reader &read,
              unsigned depth = 0) {
    if (depth > 32) {
      error_ = "menu include nesting limit";
      return false;
    }
    for (const auto &xml : source) {
      if (xml.tag == "include") {
        std::vector<Xml> included;
        auto path = xml.Attr("src");
        if (path.find("\\") == path.npos)
          path = "menus\\prefabs\\" + path;
        if (!File(path, read, included) ||
            !Expand(included, parent, read, depth + 1))
          return false;
      } else if (xml.tag == "template")
        templates_[xml.Attr("name")] = xml;
      else if (Tile(xml.tag) || xml.tag == "globals") {
        int id = int(nodes_.size());
        nodes_.push_back(
            {xml.tag, xml.Attr("name"), parent, {}, {}, {}, false});
        if (parent >= 0)
          nodes_[parent].children.push_back(id);
        if (xml.tag == "menu")
          root_ = id;
        if (xml.tag == "globals" || xml.Attr("name") == "globals")
          globals_ = id;
        if (!Expand(xml.children, id, read, depth))
          return false;
      } else if (parent >= 0)
        nodes_[parent].traits[xml.tag] = xml;
    }
    return true;
  }
  int Descend(int parent, const std::string &name) const {
    if (parent < 0)
      return -1;
    if (nodes_[parent].name == name)
      return parent;
    for (auto c : nodes_[parent].children) {
      auto found = Descend(c, name);
      if (found >= 0)
        return found;
    }
    return -1;
  }
  int Source(int id, const std::string &src, double previous) {
    if (src == "me()" || src.empty())
      return id;
    if (src == "io()")
      return root_;
    if (src == "globals()")
      return globals_;
    if (src == "screen()")
      return -2;
    if (src == "parent()")
      return nodes_[id].parent;
    if (src == "grandparent()")
      return nodes_[id].parent < 0 ? -1 : nodes_[nodes_[id].parent].parent;
    if (src == "child()") {
      const auto &c = nodes_[id].children;
      return previous >= 0 && size_t(previous) < c.size() ? c[size_t(previous)]
                                                          : -1;
    }
    auto open = src.find('(');
    auto name =
        open == src.npos ? src : src.substr(open + 1, src.size() - open - 2);
    if (src.rfind("child(", 0) == 0)
      return Descend(id, name);
    for (int scope = nodes_[id].parent; scope >= 0;
         scope = nodes_[scope].parent) {
      auto found = Descend(scope, name);
      if (found >= 0)
        return found;
    }
    return Descend(root_, name);
  }
  Value Literal(std::string text) {
    text = Trim(text);
    if (text.rfind("&-", 0) == 0) {
      auto key = Trim(text.substr(2));
      if (!key.empty() && key.back() == ';')
        key.pop_back();
      return strings ? strings(key) : Value{};
    }
    if (text.size() > 2 && text.front() == '&' && text.back() == ';') {
      auto key = text.substr(1, text.size() - 2);
      if (key == "xbox")
        return 1.;
      if (key == "console")
        return 1.;
      if (key == "pc")
        return 0.;
      if (key == "true")
        return 1.;
      if (key == "false" || key == "nosystemcolor")
        return 0.;
      if (key == "left")
        return 0.;
      if (key == "center")
        return 1.;
      if (key == "right")
        return 2.;
      if (key == "scale")
        return 100.;
      if (key == "xbuttona")
        return "glow_general_button_a.dds";
      if (key == "xbuttonb")
        return "glow_general_button_b.dds";
      if (key == "xbuttonx")
        return "glow_general_button_x.dds";
      if (key == "xbuttony")
        return "glow_general_button_y.dds";
      if (key == "xbuttonrb")
        return "glow_general_button_rb.dds";
      if (key == "highdef")
        return 1.; // These VR surfaces use the original HD menu layout.
      if (key == "hudmain" || key == "pipboy" || key == "Pipboy")
        return 1.;
      return key;
    }
    return text;
  }
  Value Expression(int id, const Xml &expression, Value initial = {}) {
    Value value = expression.text.empty() ? initial : Literal(expression.text);
    for (const auto &op : expression.children) {
      Value operand;
      if (!op.Attr("src").empty()) {
        auto trait = op.Attr("trait");
        if (!trait.empty() && trait.back() == '_')
          trait += Value(value.Number()).text;
        operand = Get(Source(id, op.Attr("src"), value.Number()), trait);
        if (!op.children.empty()) {
          Xml nested = op;
          nested.attrs.clear();
          operand = Expression(id, nested, operand);
        }
      } else
        operand = Expression(id, op);
      auto a = value.Number(), b = operand.Number();
      const auto &kind = op.tag;
      if (kind == "copy" || kind == "ref")
        value = operand;
      else if (kind == "add")
        value = a + b;
      else if (kind == "sub")
        value = a - b;
      else if (kind == "mul" || kind == "mult")
        value = a * b;
      else if (kind == "div")
        value = b ? a / b : 0.;
      else if (kind == "min")
        value = std::min(a, b);
      else if (kind == "max")
        value = std::max(a, b);
      else if (kind == "eq")
        value = double(value.Numeric() && operand.Numeric()
                           ? a == b
                           : value.text == operand.text);
      else if (kind == "neq")
        value = double(value.Numeric() && operand.Numeric()
                           ? a != b
                           : value.text != operand.text);
      else if (kind == "gt")
        value = double(a > b);
      else if (kind == "gte")
        value = double(a >= b);
      else if (kind == "lt")
        value = double(a < b);
      else if (kind == "lte")
        value = double(a <= b);
      else if (kind == "and")
        value = double(value.Boolean() && operand.Boolean());
      else if (kind == "or")
        value = double(value.Boolean() || operand.Boolean());
      else if (kind == "not")
        value = double(!operand.Boolean());
      else if (kind == "onlyif") {
        if (!operand.Boolean())
          value = 0.;
      } else if (kind == "onlyifnot") {
        if (operand.Boolean())
          value = 0.;
      } else if (kind == "floor")
        value = std::floor(op.text.empty() && op.children.empty() ? a : b);
      else if (kind == "abs")
        value = std::abs(op.text.empty() && op.children.empty() ? a : b);
      else if (kind == "round")
        value = b ? std::round(a / b) * b : std::round(a);
      else if (kind == "mod")
        value = b ? std::fmod(a, b) : 0.;
      else
        unsupported_.insert(kind);
    }
    return value;
  }

public:
  float width = 1024, height = 768;
  std::function<Value(const std::string &)> strings;
  std::function<Box(int, const std::string &, float)> measure;
  std::function<Box(const std::string &, const std::string &)> imageSize;
  const std::string &Error() const { return error_; }
  const auto &Unsupported() const { return unsupported_; }
  const auto &Nodes() const { return nodes_; }
  int Root() const { return root_; }
  int Globals() const { return globals_; }
  bool Load(const std::string &path, const Reader &read) {
    reader_ = read;
    nodes_.clear();
    templates_.clear();
    files_.clear();
    root_ = globals_ = -1;
    std::vector<Xml> xml;
    if (!File("menus\\globals.xml", read, xml) || !Expand(xml, -1, read))
      return false;
    globals_ = 0;
    if (!File(path, read, xml) || !Expand(xml, -1, read) || root_ < 0)
      return false;
    staticSize_ = nodes_.size();
    Reset();
    return true;
  }
  void Reset() {
    nodes_.resize(staticSize_);
    for (auto &n : nodes_) {
      n.bound.clear();
      n.children.erase(
          std::remove_if(n.children.begin(), n.children.end(),
                         [&](int i) { return size_t(i) >= staticSize_; }),
          n.children.end());
    }
    Invalidate();
  }
  void Invalidate() {
    cache_.clear();
    evaluating_.clear();
    unsupported_.clear();
  }
  int Find(const std::string &name, int scope = -1) const {
    return Descend(scope < 0 ? root_ : scope, name);
  }
  void Set(int id, const std::string &trait, Value v) {
    if (id >= 0) {
      nodes_[id].bound[trait] = std::move(v);
      cache_.clear();
    }
  }
  void Set(const std::string &name, const std::string &trait, Value v) {
    Set(Find(name), trait, std::move(v));
  }
  Value Get(int id, const std::string &trait) {
    if (id == -2) {
      if (trait == "resolutionconverter")
        return 1.;
      if (trait == "width")
        return width;
      if (trait == "height")
        return height;
      return 0.;
    }
    if (id < 0 || size_t(id) >= nodes_.size())
      return 0.;
    auto &node = nodes_[id];
    auto bound = node.bound.find(trait);
    if (bound != node.bound.end())
      return bound->second;
    auto key = std::make_pair(id, trait);
    auto found = cache_.find(key);
    if (found != cache_.end())
      return found->second;
    if (!evaluating_.insert(key).second)
      return 0.;
    Value result;
    const auto source = node.traits.find(trait);
    if (source != node.traits.end())
      result = Expression(id, source->second);
    else if (trait == "childcount")
      result = double(node.children.size());
    else if (trait == "visible")
      result = 1.;
    else if (trait == "alpha" || trait == "brightness")
      result = 255.;
    else if (trait == "zoom")
      result = 100.;
    else if (trait == "font")
      result = node.tag == "text" ? 2. : 0.;
    else if ((trait == "filewidth" || trait == "fileheight") && imageSize) {
      const auto size = imageSize(S(id, "filename"), S(id, "texatlas"));
      result = (trait == "filewidth" ? size.w : size.h) * N(id, "zoom") / 100.f;
    } else if (node.tag == "text" && (trait == "width" || trait == "height") &&
               measure) {
      auto extent =
          measure(int(Get(id, "font").Number()), Get(id, "string").text,
                  float(Get(id, "wrapwidth").Number()));
      result = trait == "width" ? extent.w : extent.h;
    } else if (trait == "systemcolor" && node.parent >= 0)
      result = Get(node.parent, trait);
    evaluating_.erase(key);
    cache_[key] = result;
    return result;
  }
  float N(int id, const std::string &trait) {
    return float(Get(id, trait).Number());
  }
  float N(const std::string &name, const std::string &trait) {
    return N(Find(name), trait);
  }
  std::string S(int id, const std::string &trait) {
    return Get(id, trait).text;
  }
  std::string S(const std::string &name, const std::string &trait) {
    return S(Find(name), trait);
  }
  Box Bounds(int id) {
    if (id < 0)
      return {};
    float x = N(id, "x"), y = N(id, "y");
    for (int p = nodes_[id].parent; p >= 0; p = nodes_[p].parent)
      if (N(p, "locus")) {
        x += N(p, "x");
        y += N(p, "y");
      }
    return {x, y, N(id, "width"), N(id, "height")};
  }
  int Instantiate(const std::string &type, int parent) {
    auto it = templates_.find(type);
    if (it == templates_.end() || parent < 0)
      return -1;
    const int first = int(nodes_.size());
    if (!Expand(it->second.children, parent, reader_))
      return -1;
    for (size_t i = first; i < nodes_.size(); ++i)
      nodes_[i].dynamic = true;
    Invalidate();
    return int(nodes_[parent].children.back());
  }
  std::vector<Draw> Paint() {
    std::vector<Draw> out;
    const Box full{0, 0, width, height};
    std::function<void(int, Box)> visit = [&](int id, Box clip) {
      if (!Get(id, "visible").Boolean())
        return;
      auto box = Bounds(id);
      auto &n = nodes_[id];
      if (n.tag == "text" || n.tag == "image" || n.tag == "nif" ||
          (n.tag == "hotrect" && n.traits.count("filename") &&
           !S(id, "filename").empty())) {
        Draw d;
        d.node = id;
        d.type = n.tag == "hotrect" ? "image" : n.tag;
        d.box = box;
        d.clip = clip;
        d.font = int(N(id, "font"));
        d.justify = int(N(id, "justify"));
        d.colour = int(N(id, "systemcolor"));
        d.alpha = N(id, "alpha") / 255;
        d.brightness = N(id, "brightness") / 255;
        d.depth = N(id, "depth");
        d.rotation = N(id, "_rotation");
        for (int p = n.parent; p >= 0; p = nodes_[p].parent)
          d.depth += N(p, "depth");
        d.text = S(id, "string");
        d.file = S(id, "filename");
        d.atlas = S(id, "texatlas");
        out.push_back(std::move(d));
      }
      if (N(id, "clipwindow")) {
        auto right = std::min(clip.x + clip.w, box.x + box.w),
             bottom = std::min(clip.y + clip.h, box.y + box.h);
        clip.x = std::max(clip.x, box.x);
        clip.y = std::max(clip.y, box.y);
        clip.w = std::max(0.f, right - clip.x);
        clip.h = std::max(0.f, bottom - clip.y);
      }
      for (auto child : n.children)
        visit(child, clip);
    };
    visit(root_, full);
    std::stable_sort(out.begin(), out.end(), [](const Draw &a, const Draw &b) {
      return a.depth < b.depth;
    });
    return out;
  }
};
} // namespace fo3menu
