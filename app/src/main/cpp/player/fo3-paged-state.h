#pragma once
#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace fo3player {
// Mutable save records with value semantics. Transaction candidates share
// untouched pages; an edit detaches only its page. Reads, even on a mutable
// State, never detach. No mutable iterators escape across a State copy.
// As with unordered_map, callers must not retain editable references across
// copies or publication. Player's mutation methods consume them immediately.
template<class T, size_t PageCount=64> class PagedStateMap {
    static_assert(PageCount && !(PageCount&(PageCount-1)),"power-of-two pages");
    using Page=std::unordered_map<uint32_t,T>;
    std::array<std::shared_ptr<Page>,PageCount> pages_{};
    size_t size_=0;
    static size_t Index(uint32_t key) {
        key^=key>>16;key*=0x7feb352du;key^=key>>15;
        return key&(PageCount-1);
    }
    Page& Write(size_t index) {
        auto& p=pages_[index];
        if(!p)p=std::make_shared<Page>();
        else if(!p.unique())p=std::make_shared<Page>(*p);
        return *p;
    }
public:
    class const_iterator {
        friend class PagedStateMap;
        const PagedStateMap* owner_=nullptr;
        size_t page_=PageCount;
        typename Page::const_iterator it_{};
        const_iterator(const PagedStateMap* owner,size_t page):owner_(owner),page_(page) { Seek(); }
        void Seek() {
            while(page_<PageCount) {
                const auto& p=owner_->pages_[page_];
                if(p&&!p->empty()){it_=p->cbegin();return;}
                ++page_;
            }
        }
    public:
        using iterator_category=std::forward_iterator_tag;
        using value_type=typename Page::value_type;
        using difference_type=std::ptrdiff_t;
        using pointer=const value_type*;
        using reference=const value_type&;
        const_iterator()=default;
        reference operator*()const{return *it_;}
        pointer operator->()const{return &*it_;}
        const_iterator& operator++(){
            if(++it_==owner_->pages_[page_]->cend()){++page_;Seek();}
            return *this;
        }
        const_iterator operator++(int){auto old=*this;++*this;return old;}
        bool operator==(const const_iterator& b)const {
            return owner_==b.owner_&&page_==b.page_&&(page_==PageCount||it_==b.it_);
        }
        bool operator!=(const const_iterator& b)const{return !(*this==b);}
    };
    size_t size()const noexcept{return size_;}
    bool empty()const noexcept{return !size_;}
    const_iterator begin()const{return {this,0};}
    const_iterator end()const{return {this,PageCount};}
    const_iterator find(uint32_t key)const {
        const auto index=Index(key);const auto& p=pages_[index];
        if(!p)return end();
        auto it=p->find(key);if(it==p->end())return end();
        const_iterator result;result.owner_=this;result.page_=index;result.it_=it;return result;
    }
    size_t count(uint32_t key)const{return find(key)!=end();}
    const T& at(uint32_t key)const {
        auto it=find(key);if(it==end())throw std::out_of_range("state record");return it->second;
    }
    T* Edit(uint32_t key) {
        if(!count(key))return nullptr;
        return &Write(Index(key)).at(key);
    }
    T& operator[](uint32_t key) {
        auto& p=Write(Index(key));auto result=p.try_emplace(key);
        if(result.second)++size_;
        return result.first->second;
    }
    std::pair<const_iterator,bool> emplace(uint32_t key,const T& value) {
        if(count(key))return {find(key),false};
        Write(Index(key)).emplace(key,value);++size_;return {find(key),true};
    }
    void clear()noexcept{pages_={};size_=0;}
    void swap(PagedStateMap& other)noexcept{pages_.swap(other.pages_);std::swap(size_,other.size_);}
    friend void swap(PagedStateMap& a,PagedStateMap& b)noexcept{a.swap(b);}
    // Default moves would leave the source's count nonzero after moving pages.
    PagedStateMap()=default;
    PagedStateMap(const PagedStateMap&)=default;
    PagedStateMap& operator=(const PagedStateMap&)=default;
    PagedStateMap(PagedStateMap&& other)noexcept{swap(other);}
    PagedStateMap& operator=(PagedStateMap&& other)noexcept {
        if(this!=&other){clear();swap(other);}return *this;
    }
};
}
