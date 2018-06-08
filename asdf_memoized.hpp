#ifndef ASDF_MEMOIZED_HPP
#define ASDF_MEMOIZED_HPP

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace ASDF {

using namespace std;

template <typename T> class memoized {
  function<shared_ptr<T>()> fun;
  mutable bool have_value;
  mutable shared_ptr<T> value;

public:
  memoized() : have_value(false) {}
  memoized(function<shared_ptr<T>()> fun1)
      : fun(move(fun1)), have_value(false) {}

  bool valid() const { return bool(fun); }
  void reset() { *this = memoized(); }

  bool cached() const { return have_value; }
  void fill_cache() const {
    if (have_value)
      return;
    value = fun();
    have_value = true;
  }
  void forget() const {
    if (!have_value)
      return;
    value.reset();
    have_value = false;
  }

  shared_ptr<T> get() const {
    fill_cache();
    return value;
  }

  const T &operator*() const {
    fill_cache();
    return *value;
  }
  T &operator*() {
    fill_cache();
    return *value;
  }
  const T *operator->() const {
    fill_cache();
    return value.get();
  }
  T *operator->() {
    fill_cache();
    return value.get();
  }
};

// // Modelled after std::make_shared
// template <typename T, class... Args>
// memoized<T> make_memoized(const Args &... args) {
//   return memoized<T>(
//       [=]() { return make_shared<T>(forward<Args...>(args...)); });
// }

// Modelled after std::experimental::make_ready_future
template <typename T>
memoized<typename decay<T>::type> make_fixed_memoized(T &&arg) {
  typedef typename decay<T>::type R;
  auto parg = make_shared<R>(forward<T>(arg));
  return memoized<R>([=]() { return parg; });
}

// Modelled after std::experimental::make_ready_future
template <typename T>
memoized<T> make_constant_memoized(const shared_ptr<T> &arg) {
  return memoized<T>([=]() { return arg; });
}

} // namespace ASDF

#endif // #ifndef ASDF_MEMOIZED_HPP
