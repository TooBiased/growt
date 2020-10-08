/*******************************************************************************
 * wrapper/tbb_um_wrapper.h
 *
 * Wrapper to use tbb's tbb::concurrent_unordered_map in our benchmarks
 *
 * Part of Project growt - https://github.com/TooBiased/growt.git
 *
 * Copyright (C) 2015-2016 Tobias Maier <t.maier@kit.edu>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#ifndef TBB_UM_WRAPPER
#define TBB_UM_WRAPPER

#include <tbb/concurrent_unordered_map.h>
#include <atomic>
#include <memory>

#include "data-structures/returnelement.h"

using namespace growt;

template<class Hasher>
class TBBUMWrapper
{
private:
    using HashType = tbb::concurrent_unordered_map<size_t, size_t, Hasher>;

    HashType hash;

public:

    using key_type           = size_t;
    using mapped_type        = size_t;
    using value_type         = typename std::pair<const key_type, mapped_type>;
    using iterator           = typename HashType::iterator;
    using const_iterator     = typename HashType::const_iterator;
    using insert_return_type = std::pair<iterator, bool>;

    TBBUMWrapper() = default;
    TBBUMWrapper(size_t capacity_) : hash(capacity_) { }
    TBBUMWrapper(const TBBUMWrapper&) = delete;
    TBBUMWrapper& operator=(const TBBUMWrapper&) = delete;

    TBBUMWrapper(TBBUMWrapper&& rhs) = default;
    TBBUMWrapper& operator=(TBBUMWrapper&& rhs) = default;

    using Handle = TBBUMWrapper&;
    Handle get_handle() { return *this; }


    inline iterator find(const key_type& k)
    {
        return hash.find(k);
        // auto temp = hash.find(k);
        // if (temp == end(hash)) return ReturnElement::getEmpty();
        // else return ReturnElement(k, temp->second);
    }

    inline insert_return_type insert(const key_type& k, const mapped_type& d)
    {
        return hash.insert(std::make_pair(k,d));
        // auto ret = hash.insert(std::make_pair(k,d)).second;
        // return (ret) ? ReturnCode::SUCCESS_IN : ReturnCode::UNSUCCESS_ALREADY_USED;
    }

    template<class F, class ... Types>
    inline insert_return_type update(const key_type& k, F f, Types&& ... args)
    {
        auto result = hash.find(k);
        if (result != hash.end())
        {
            f.atomic(result->second, std::forward<Types>(args)...);
        }
        return std::make_pair(result, result != hash.end());
    }

    template<class F, class ... Types>
    inline insert_return_type insert_or_update(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    {
        auto ret = hash.insert(std::make_pair(k,d));
        if (! ret.second)
        {
            f.atomic(ret.first->second, std::forward<Types>(args)...);
        }
        return ret;
    }

    template<class F, class ... Types>
    inline insert_return_type update_unsafe(const key_type& k, F f, Types&& ... args)
    {
        return update(k,f, std::forward<Types>(args)...);
    }

    template<class F, class ... Types>
    inline insert_return_type insert_or_update_unsafe(const key_type& k, const mapped_type& d, F f, Types&& ... args)
    {
        return insert_or_update(k,d,f, std::forward<Types>(args)...);
    }

    inline size_t erase(const key_type&)
    {
        //static_assert(false, "tbb_um .remove(key) is not implemented")
        return 0;
    }


    inline iterator        end()         { return hash.end();  }
    inline const_iterator  end()   const { return hash.cend(); }
    inline const_iterator cend()   const { return hash.cend(); }

    inline iterator        begin()       { return hash.begin();  }
    inline const_iterator  begin() const { return hash.cbegin(); }
    inline const_iterator cbegin() const { return hash.cbegin(); }
};

#endif // TBB_UM_WRAPPER
