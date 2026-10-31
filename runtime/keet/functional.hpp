#pragma once

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

namespace keet {

// Forward declarations
template <typename C, typename Pred>
struct filter_view;

template <typename C, typename MapFn>
struct map_view;

template <typename C, typename Pred, typename MapFn>
struct filter_map_view;

// --- Filter View ---
template <typename C, typename Pred>
struct filter_view {
    const C& container;
    Pred pred;

    using value_type = typename C::value_type;

    operator std::vector<value_type>() const {
        std::vector<value_type> res;
        res.reserve(container.size() / 2);
        const size_t sz = container.size();
        const value_type* in = container.data();
        for (size_t i = 0; i < sz; ++i) {
            if (pred(in[i])) {
                res.push_back(in[i]);
            }
        }
        return res;
    }
};

// --- Map View ---
template <typename C, typename MapFn>
struct map_view {
    const C& container;
    MapFn map_fn;

    using RetT = std::decay_t<decltype(std::declval<MapFn>()(std::declval<typename C::value_type>()))>;
    using value_type = RetT;

    operator std::vector<RetT>() const {
        std::vector<RetT> res;
        res.reserve(container.size());
        const size_t sz = container.size();
        const auto* in = container.data();
        for (size_t i = 0; i < sz; ++i) {
            res.push_back(map_fn(in[i]));
        }
        return res;
    }
};

// --- Filter-Map Fused View ---
template <typename C, typename Pred, typename MapFn>
struct filter_map_view {
    const C& container;
    Pred pred;
    MapFn map_fn;

    using RetT = std::decay_t<decltype(std::declval<MapFn>()(std::declval<typename C::value_type>()))>;
    using value_type = RetT;

    operator std::vector<RetT>() const {
        std::vector<RetT> res;
        res.reserve(container.size() / 2);
        const size_t sz = container.size();
        const auto* in = container.data();
        for (size_t i = 0; i < sz; ++i) {
            if (pred(in[i])) {
                res.push_back(map_fn(in[i]));
            }
        }
        return res;
    }
};

// --- keet::filter ---

template <typename T, typename Pred>
inline filter_view<std::vector<T>, std::decay_t<Pred>> filter(const std::vector<T>& c, Pred&& pred) {
    return filter_view<std::vector<T>, std::decay_t<Pred>>{c, std::forward<Pred>(pred)};
}

// --- keet::map ---

template <typename T, typename MapFn>
inline map_view<std::vector<T>, std::decay_t<MapFn>> map(const std::vector<T>& c, MapFn&& map_fn) {
    return map_view<std::vector<T>, std::decay_t<MapFn>>{c, std::forward<MapFn>(map_fn)};
}

template <typename C, typename Pred, typename MapFn>
inline filter_map_view<C, Pred, std::decay_t<MapFn>> map(filter_view<C, Pred> fv, MapFn&& map_fn) {
    return filter_map_view<C, Pred, std::decay_t<MapFn>>{
        fv.container, std::move(fv.pred), std::forward<MapFn>(map_fn)
    };
}

// --- keet::reduce ---

template <typename T, typename Acc, typename ReduceFn>
inline Acc reduce(const std::vector<T>& c, Acc init, ReduceFn&& rfn) {
    Acc acc = std::move(init);
    const size_t sz = c.size();
    const T* in = c.data();
    for (size_t i = 0; i < sz; ++i) {
        acc = rfn(std::move(acc), in[i]);
    }
    return acc;
}

template <typename C, typename Pred, typename MapFn, typename Acc, typename ReduceFn>
inline Acc reduce(filter_map_view<C, Pred, MapFn> fmv, Acc init, ReduceFn&& rfn) {
    using ItemT = typename filter_map_view<C, Pred, MapFn>::RetT;
    std::vector<ItemT> filtered;
    filtered.reserve(fmv.container.size() / 2);
    const size_t sz = fmv.container.size();
    const auto* in = fmv.container.data();
    for (size_t i = 0; i < sz; ++i) {
        if (fmv.pred(in[i])) {
            filtered.push_back(fmv.map_fn(in[i]));
        }
    }
    Acc acc = std::move(init);
    for (const auto& item : filtered) {
        acc = rfn(std::move(acc), item);
    }
    return acc;
}

template <typename C, typename Pred, typename Acc, typename ReduceFn>
inline Acc reduce(filter_view<C, Pred> fv, Acc init, ReduceFn&& rfn) {
    using ItemT = typename filter_view<C, Pred>::value_type;
    std::vector<ItemT> filtered;
    filtered.reserve(fv.container.size() / 2);
    const size_t sz = fv.container.size();
    const auto* in = fv.container.data();
    for (size_t i = 0; i < sz; ++i) {
        if (fv.pred(in[i])) {
            filtered.push_back(in[i]);
        }
    }
    Acc acc = std::move(init);
    for (const auto& item : filtered) {
        acc = rfn(std::move(acc), item);
    }
    return acc;
}

template <typename C, typename MapFn, typename Acc, typename ReduceFn>
inline Acc reduce(map_view<C, MapFn> mv, Acc init, ReduceFn&& rfn) {
    Acc acc = std::move(init);
    const size_t sz = mv.container.size();
    const auto* in = mv.container.data();
    for (size_t i = 0; i < sz; ++i) {
        acc = rfn(std::move(acc), mv.map_fn(in[i]));
    }
    return acc;
}

} // namespace keet
