// #include <vector>
#include <set>
#include <iostream>
// #include <optional>


namespace tns {
namespace tcp {


template <typename T>
struct RightOpenInterval {  // [begin, end)
    T begin;
    T end;

    // TODO: throw in constructor if begin >= end
    RightOpenInterval(T begin, T end) : begin{begin}, end{end}
    {
        if (begin >= end)
            throw std::invalid_argument("RightOpenInterval: begin >= end");
    }

    auto operator<=>(const RightOpenInterval<T> &other) const = default;

};

template <typename T>
class RightOpenIntervalSet {
public:
    RightOpenIntervalSet() = default;

    T emplaceMerge(const RightOpenInterval<T> &interval)
    {
        const auto &[center, success] = intervals_.emplace(interval);

        if (!success)
            return interval.end;

        // Merged interval: [begin, end)
        auto begin = interval.begin;
        auto end   = interval.end;

        // Iterators to the range of intervals [*left..*right) that can be merged
        // Can merge at most one interval on the left
        auto left = center;
        
        if (left != intervals_.begin()) {
        if (const auto ll = std::prev(left); ll->end >= begin) {
            // merge!
            begin = ll->begin;
            end = std::max(end, ll->end);
            left = ll;
        }
        }

        // Merge with intervals on the right until no more merging is possible
        auto right = intervals_.lower_bound({end + 1, end + 2});

        // Replace all merged intervals *left..*right by the new interval [begin, end)
        if (right != std::next(left)) {
            end = std::max(end, std::prev(right)->end);
            const auto pos = intervals_.erase(left, right);
            intervals_.emplace_hint(pos, begin, end);
        }

        return end;
    }

    T mergeRemove(const RightOpenInterval<T> &interval)
    {
        auto end = interval.end;

        auto left = intervals_.lower_bound(interval);  // first >= interval
        
        if (left != intervals_.begin()) {
        if (auto ll = std::prev(left); ll->end >= interval.begin) {
            end = std::max(end, ll->end);
            left = ll;
        }
        }

        auto right = intervals_.lower_bound({end + 1, end + 2});  // everything up to right (exclusive) can be merged

        // Replace all merged intervals *left..*right by the new interval [begin, end)
        if (right != intervals_.begin()) {
            end = std::max(end, std::prev(right)->end);
            intervals_.erase(left, right);
        }

        return end;
    }
    
    // // debug
    // void print() const
    // {
    //     std::cout << "RightOpenIntervalSet:\n";
    //     for (const auto &interval : intervals_)
    //         std::cout << "  [" << interval.begin << ", " << interval.end << ")\n";
    // }

private:
    std::set <RightOpenInterval<T>> intervals_;

};


} // namespace tcp
} // namespace tns